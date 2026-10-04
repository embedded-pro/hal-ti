"""PWM (`hal::tiva::Pwm` / `SynchronousPwm`): waveforms, outputs, dead band, update modes, interrupts, ADC
triggers and the fault path.

Wiring set `bundle1`: the A/B outputs of `tests.pwm.generators` and the fault pin on DIOs, W1 on the ADC input of
the digital comparator fault path. Interrupt and ADC-trigger tests need no wiring.

Scenarios: features/pwm.feature.
"""

from __future__ import annotations

import statistics
import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when

from hal_ti_validation import expect

# Step texts list one value per generator as "20, 40, 60 and 80".
LISTS = {
    "Names": lambda text: text.replace(" and ", ", ").split(", "),
    "Numbers": lambda text: [int(item) for item in text.replace(" and ", ", ").split(", ")],
}


@pytest.fixture
def pwm(board_cfg):
    return board_cfg.param("pwm")


@pytest.fixture(scope="module")
def sysclk(fw):
    return fw.system.info().sysclk


@pytest.fixture
def state():
    """What the steps of one scenario hand on to the later ones."""
    return {}


def configured(pwm, count):
    if count > len(pwm["generators"]):
        pytest.skip(f"tests.pwm.generators lists fewer than {count} generators")
    return pwm["generators"][:count]


def outputs_of(generator, output):
    return (generator["a"] if output in ("ab", "a") else None, generator["b"] if output in ("ab", "b") else None)


def open_generators(fw, pwm, generators, output="ab", **options):
    pins = [outputs_of(generator, output) for generator in generators]
    return fw.pwm.open(pwm["module"], gens=[generator["gen"] for generator in generators], pins=pins, **options)


def open_or_range(fits, call):
    """Run an open that must succeed exactly when `fits`; returns False when it was (correctly) refused."""
    try:
        result = call()
    except FirmwareError as error:
        assert error.reason == "range" and not fits, f"ERR {error.reason} although the setting fits"
        return False
    assert fits, "the firmware accepted a setting outside the PWM limits"
    return result


def record(ad3, pwm, frequency, trigger_dio=None, periods=None):
    periods = periods or pwm["capture_periods"]
    trigger = None if trigger_dio is None else (trigger_dio, "rising")
    return ad3.logic.record_for(periods / frequency, trigger=trigger, timeout=periods / frequency + 2)


def tolerance(pwm, key):
    return pwm["tolerance"][key]


def check_waveform(capture, dio, frequency, duty, pwm, step=0.0):
    """`step` is the duty resolution of the generator in percent."""
    if duty in (0, 100):
        bits = capture.channel(dio)
        level = 1 if duty == 100 else 0
        fraction = bits.count(level) / len(bits)
        assert fraction >= tolerance(pwm, "static_fraction"), f"DIO{dio}: {fraction:.4f} of the samples at {level} for duty {duty}"
        return
    assert capture.frequency(dio) == pytest.approx(frequency, rel=tolerance(pwm, "frequency")), f"DIO{dio} frequency"
    quantisation = 100 * 2 * frequency / capture.rate + step
    assert capture.duty(dio) * 100 == pytest.approx(duty, abs=tolerance(pwm, "duty") + quantisation), f"DIO{dio} duty"


def check_aligned(capture, dios, feature, allowed):
    def times(dio):
        bits = capture.channel(dio)
        if feature == "rising":
            return analysis.rising_times(bits, capture.rate)
        return analysis.pulse_centers(bits, capture.rate)

    reference = times(dios[0])[1:-1]
    assert reference, "no complete pulses captured"
    for dio in dios[1:]:
        offsets = analysis.nearest_offsets(reference, times(dio))
        worst = max(abs(offset) for offset in offsets)
        assert worst <= allowed + 2 / capture.rate, f"DIO{dio} {feature} edges off by {worst * 1e9:.0f} ns"


def logical(capture, dio, inverted):
    bits = capture.channel(dio)
    return [1 - bit for bit in bits] if inverted else bits


def interrupt_window(fw, pwm, generator):
    """Count events over `irq_window_ms`; returns the count and the shortest and longest window it can cover.

    The firmware clears and reads the counter somewhere within the round trips of those commands, so the counted window lies
    between the time from the clear's reply to the read's request and the time from the clear's request to the read's reply.
    """
    started = time.monotonic()
    fw.pwm.count(pwm["module"], generator, clear=True)
    cleared = time.monotonic()
    fw.system.delay(pwm["irq_window_ms"])
    reading = time.monotonic()
    count = fw.pwm.count(pwm["module"], generator)
    ended = time.monotonic()
    return count, (reading - cleared, ended - started)


def check_count(count, frequency, per_period, window_s, pwm):
    shortest, longest = window_s
    slack = frequency * per_period * 0.02
    lowest = frequency * shortest * per_period * (1 - pwm["irq_tolerance"])
    highest = frequency * longest * per_period * (1 + pwm["irq_tolerance"]) + slack
    assert lowest <= count <= highest, f"{count} events, expected {lowest:.0f} to {highest:.0f}"


def check_adc_trigger(fw, pwm, generator, source, mode):
    config = pwm["adc_trigger_input"]
    frequency, runs = config["freq"], config["runs"]
    fw.adc.open(config["adc"], config["seq"], pins=[config["pin"]], trigger=f"pwm{generator}")
    try:
        if expect.pwm_events_per_period(source, mode) == 0:
            with pytest.raises(FirmwareError) as error:
                fw.adc.measure(config["adc"], config["seq"], n=1, cmd_timeout=3.0)
            assert error.value.reason == "timeout", f"{source} in {mode} mode must not trigger"
            return
        start = time.monotonic()
        samples = fw.adc.measure(config["adc"], config["seq"], n=runs, cmd_timeout=3.0 + runs / frequency)
        elapsed = time.monotonic() - start
        assert len(samples) == runs
        assert elapsed >= 0.8 * (runs - 1) / frequency, f"{runs} conversions in {elapsed * 1000:.1f} ms: faster than the trigger"
    finally:
        fw.adc.close(config["adc"], config["seq"])


def fault_generators(pwm, which):
    selected = [generator["gen"] for generator in pwm["generators"]]
    return selected[:1] if which == "first" else selected


def generator_mask(generators):
    return sum(1 << generator for generator in generators)


@pytest.mark.matrix("pwm.waveform")
@scenario("pwm.feature", "Both outputs run at the set frequency and duty")
def test_waveform(mode, div, freq, duty, sync):
    pass


@pytest.mark.matrix("pwm.outputs")
@scenario("pwm.feature", "The selected outputs run in step while the unused ones stay low")
def test_outputs(generators, output, sync):
    pass


@pytest.mark.matrix("pwm.dead_time")
@scenario("pwm.feature", "The dead band delays each output after the other one switches off")
def test_dead_time(mode, dead, inversion, sync):
    pass


@pytest.mark.board_params("sync", values=[0, 1])
@scenario("pwm.feature", "Dead bands beyond the limits are refused")
def test_dead_time_limits(sync):
    pass


@pytest.mark.matrix("pwm.update")
@scenario("pwm.feature", "The generators run in lock step and take new duties with the update mode")
def test_update_and_alignment(update, mode, generators, sync):
    pass


@pytest.mark.board_params("mode", values=["edge", "center"])
@pytest.mark.board_params("sync", values=[0, 1])
@scenario("pwm.feature", "The outputs follow frequency changes and stop with the PWM")
def test_frequency_change_and_stop(mode, sync):
    pass


@pytest.mark.matrix("pwm.irq")
@scenario("pwm.feature", "The interrupt counts the events of its source")
def test_interrupt_count(source, mode, freq):
    pass


@pytest.mark.board_params("mode", values=["edge", "center"])
@scenario("pwm.feature", "Each generator counts the events of its own interrupt source")
def test_interrupt_source_per_generator(mode):
    pass


@scenario("pwm.feature", "Interrupts need the asynchronous driver")
def test_interrupts_need_async():
    pass


@pytest.mark.matrix("pwm.adc_trigger")
@scenario("pwm.feature", "The generator triggers the ADC on the events of its source")
def test_adc_trigger(source, gen, mode):
    pass


@scenario("pwm.feature", "Only generators with a trigger source trigger the ADC")
def test_adc_trigger_per_generator():
    pass


@scenario("pwm.feature", "The ADC trigger stops with the PWM")
def test_adc_trigger_stops_with_the_pwm():
    pass


@pytest.mark.matrix("pwm.fault.options")
@scenario("pwm.feature", "A high fault pin raises a fault naming its input and the generators set up for it")
def test_fault_input_pin(latch, minperiod, gens):
    pass


@scenario("pwm.feature", "A fault stops both outputs")
def test_fault_forces_outputs():
    pass


@scenario("pwm.feature", "Repeated faults are reported once until the PWM stops")
def test_fault_reported_once_until_stop():
    pass


@scenario("pwm.feature", "A digital comparator above its window raises a fault")
def test_fault_digital_comparator():
    pass


@scenario("pwm.feature", "Invalid fault settings are refused")
def test_fault_errors():
    pass


@scenario("pwm.feature", "Invalid opens and duties are refused")
def test_open_errors():
    pass


@given("the system clock of the board")
def system_clock(sysclk, state):
    state["sysclk"] = sysclk


@given("the A and B outputs of the first generator are wired")
def first_generator_wired(need, pwm, state):
    generator = configured(pwm, 1)[0]
    state["generator"] = generator
    state["a"], state["b"] = need.dio(generator["a"]), need.dio(generator["b"])


@given("the duty does not round to a static level at the system clock over the divisor, unless it is 0 or 100 % or the period does not fit")
def duty_resolvable(state, mode, div, freq, duty):
    pwmclk = state["sysclk"] // div
    fits = expect.pwm_fits(pwmclk, freq, mode)
    if fits and duty not in (0, 100) and not expect.pwm_duty_resolvable(pwmclk, freq, mode, duty):
        pytest.skip(f"{duty} % rounds to a static level at {freq} Hz with pwmclk {pwmclk} Hz ({mode})")
    state["pwmclk"], state["fits"] = pwmclk, fits


@given(
    'the generator is opened with the mode, divisor, frequency and sync and no dead band, which fails with "range" exactly when the period '
    "does not fit"
)
def open_for_waveform(fw, pwm, state, mode, div, freq, sync):
    generator = state["generator"]
    state["reported"] = open_or_range(
        state["fits"], lambda: open_generators(fw, pwm, [generator], freq=freq, mode=mode, div=div, dead="off", sync=sync)
    )


@given("both outputs of as many generators as the generator count are wired")
def generator_outputs_wired(need, pwm, state, generators, output):
    selected = configured(pwm, generators)
    used, unused = [], []
    for generator in selected:
        for pin, active in zip((generator["a"], generator["b"]), (output in ("ab", "a"), output in ("ab", "b"))):
            (used if active else unused).append((pin, need.dio(pin)))
    state["selected"], state["used"], state["unused"] = selected, used, unused


@given("the outputs the output option leaves out are configured as GPIO outputs")
def unused_outputs_are_gpio(fw, state):
    for pin, _ in state["unused"]:
        fw.gpio.cfg(pin, "out")


@given(
    parsers.parse(
        "the generators are opened on the outputs of the output option at {frequency:d} Hz in {pwm_mode} mode with the sync "
        "and no dead band"
    )
)
def open_for_outputs(fw, pwm, state, output, sync, frequency, pwm_mode):
    state["frequency"], state["pwm_mode"] = frequency, pwm_mode
    state["pwmclk"] = open_generators(fw, pwm, state["selected"], output, freq=frequency, mode=pwm_mode, dead="off", sync=sync)


@given(
    'the generator is opened at the dead-time frequency with the mode, dead band, inversion and sync, which fails with "range" exactly '
    "when a delay is beyond the dead-band limits"
)
def open_for_dead_time(fw, pwm, state, mode, dead, inversion, sync):
    generator, sysclk = state["generator"], state["sysclk"]
    rise, fall = (dead, dead) if isinstance(dead, (int, float)) else dead
    inva, invb = inversion
    frequency = pwm["dead_time_frequency"]
    fits = expect.pwm_dead_fits(rise, sysclk) and expect.pwm_dead_fits(fall, sysclk)
    pwmclk = open_or_range(
        fits,
        lambda: open_generators(fw, pwm, [generator], freq=frequency, mode=mode, dead=dead, inva=inva, invb=invb, sync=sync),
    )
    state.update(rise=rise, fall=fall, inva=inva, invb=invb, frequency=frequency, pwmclk=pwmclk)


@given("the first configured generator")
def first_configured_generator(pwm, state):
    state["generator"] = configured(pwm, 1)[0]


@given("the A outputs of as many generators as the generator count are wired")
def a_outputs_wired(need, pwm, state, generators):
    selected = configured(pwm, generators)
    state["selected"] = selected
    state["dios"] = [need.dio(generator["a"]) for generator in selected]


@given(
    parsers.parse("the generators are opened on their A outputs at {frequency:d} Hz with the update mode, mode and sync and no dead band")
)
def open_for_update(fw, pwm, state, update, mode, sync, frequency):
    state["frequency"] = frequency
    state["pwmclk"] = open_generators(fw, pwm, state["selected"], "a", freq=frequency, mode=mode, dead="off", update=update, sync=sync)


@given("the generator is opened at the first of the frequency changes with the mode and sync and no dead band")
def open_for_frequency_changes(fw, pwm, state, mode, sync):
    changes = pwm["frequency_changes"]
    state["pwmclk"] = open_generators(fw, pwm, [state["generator"]], freq=changes[0], mode=mode, dead="off", sync=sync)


@given(parsers.parse("the duty is {percent:d} %"))
@when(parsers.parse("the duty is {percent:d} %"))
def set_duty(fw, pwm, percent):
    fw.pwm.duty(pwm["module"], percent)


@given(
    "the first configured generator is opened at the frequency in the mode with the interrupt source, at the smallest divisor that holds "
    "the period"
)
def open_for_interrupt_count(fw, pwm, sysclk, state, source, mode, freq):
    generator = configured(pwm, 1)[0]["gen"]
    div = expect.pwm_divisor_for(sysclk, freq, mode)
    fw.pwm.open(pwm["module"], gens=[generator], freq=freq, mode=mode, div=div, irq=source)
    state["gen"] = generator


@given(
    parsers.parse(
        "all configured generators are opened at {frequency:d} Hz in the mode with the interrupt sources {irq_sources:Names} in order, "
        "at the smallest divisor that holds the period",
        extra_types=LISTS,
    )
)
def open_with_interrupt_sources(fw, pwm, sysclk, state, mode, frequency, irq_sources):
    selected = [generator["gen"] for generator in pwm["generators"]]
    sources = irq_sources[: len(selected)]
    div = expect.pwm_divisor_for(sysclk, frequency, mode)
    fw.pwm.open(pwm["module"], gens=selected, freq=frequency, mode=mode, div=div, irq=sources)
    state.update(selected=selected, sources=sources, frequency=frequency)


@given(parsers.parse("the generator is opened at the ADC trigger frequency in the mode with divisor {divisor:d} and the trigger source"))
def open_for_adc_trigger(fw, pwm, source, gen, mode, divisor):
    frequency = pwm["adc_trigger_input"]["freq"]
    fw.pwm.open(pwm["module"], gens=[gen], freq=frequency, mode=mode, div=divisor, trigger=source)


@given(
    parsers.parse(
        "all configured generators are opened at the ADC trigger frequency in {pwm_mode} mode with divisor {divisor:d} and the trigger "
        "sources {trigger_sources:Names} in order",
        extra_types=LISTS,
    )
)
def open_with_trigger_sources(fw, pwm, state, pwm_mode, divisor, trigger_sources):
    selected = [generator["gen"] for generator in pwm["generators"]]
    sources = trigger_sources[: len(selected)]
    fw.pwm.open(pwm["module"], gens=selected, freq=pwm["adc_trigger_input"]["freq"], mode=pwm_mode, div=divisor, trigger=sources)
    state.update(selected=selected, sources=sources, pwm_mode=pwm_mode)


@given(
    parsers.parse(
        "the first configured generator is opened at the ADC trigger frequency with divisor {divisor:d} and the {trigger} trigger"
    )
)
def open_for_adc_trigger_stop(fw, pwm, state, divisor, trigger):
    generator = configured(pwm, 1)[0]["gen"]
    config = pwm["adc_trigger_input"]
    fw.pwm.open(pwm["module"], gens=[generator], freq=config["freq"], div=divisor, trigger=trigger)
    state["gen"] = generator


@given(parsers.parse("an ADC sequencer triggered by the generator converts {count:d} samples"))
def adc_converts(fw, pwm, state, count):
    config = pwm["adc_trigger_input"]
    fw.adc.open(config["adc"], config["seq"], pins=[config["pin"]], trigger=f"pwm{state['gen']}")
    fw.adc.measure(config["adc"], config["seq"], n=count, cmd_timeout=3.0)


@given("the fault pin is wired and driven low")
def fault_pin_wired_low(ad3, need, pwm, state):
    dio = need.dio(pwm["fault"]["pin"])
    ad3.dio.drive(dio, 0)
    state["fault_dio"] = dio


@given(parsers.parse("all configured generators are opened at {frequency:d} Hz"))
def open_all_generators(fw, pwm, frequency):
    opened = [generator["gen"] for generator in pwm["generators"]]
    fw.pwm.open(pwm["module"], gens=opened, freq=frequency)


@given("the fault pin input is enabled for the generators the gens option selects, with the latch and minperiod options")
def enable_fault_pin_for_selection(fw, pwm, state, latch, minperiod, gens):
    fault = pwm["fault"]
    selected = fault_generators(pwm, gens)
    fw.pwm.fault(pwm["module"], gens=selected, inputs=fault["inputs"], pin=fault["pin"], latch=latch, minperiod=minperiod)
    state["selected"] = selected


@given(parsers.parse("the first configured generator is opened at {frequency:d} Hz on both outputs"))
def open_first_on_both_outputs(fw, pwm, frequency):
    open_generators(fw, pwm, configured(pwm, 1), freq=frequency)


@given("the fault pin input is enabled")
def enable_fault_pin(fw, pwm):
    fault = pwm["fault"]
    fw.pwm.fault(pwm["module"], inputs=fault["inputs"], pin=fault["pin"])


@given("the digital comparator input is wired to a wavegen at the safe level")
def comparator_input_safe(ad3, need, pwm, state):
    config = pwm["fault"]["dcmp"]
    wavegen = need.wavegen(config["pin"])
    ad3.wavegen.dc(wavegen, config["safe_v"])
    state["wavegen"] = wavegen


@given(parsers.parse("the first configured generator is opened at {frequency:d} Hz with the {trigger} ADC trigger"))
def open_with_adc_trigger(fw, pwm, state, frequency, trigger):
    generator = configured(pwm, 1)[0]["gen"]
    fw.pwm.open(pwm["module"], gens=[generator], freq=frequency, trigger=trigger)
    state["gen"] = generator


@given("an ADC sequencer triggered by the generator samples the input twice with the digital comparator window")
def comparator_sequencer(fw, pwm, state):
    config = pwm["fault"]["dcmp"]
    fw.adc.open(
        config["adc"],
        config["seq"],
        pins=[config["pin"], config["pin"]],
        trigger=f"pwm{state['gen']}",
        dcmp=[(config["comparator"], config["low"], config["high"])],
    )


@given("the digital comparator fault is enabled for the generator")
def enable_comparator_fault(fw, pwm, state):
    fw.pwm.fault(pwm["module"], gens=[state["gen"]], comparators=1 << pwm["fault"]["dcmp"]["comparator"])


@when("the duty is set and both outputs are recorded, triggered on A unless the duty is 0 or 100 %, if the generator opened")
def record_waveform(fw, ad3, pwm, state, freq, duty):
    if state["reported"] is False:
        return
    fw.pwm.duty(pwm["module"], duty)
    state["capture"] = record(ad3, pwm, freq, None if duty in (0, 100) else state["a"])


@when(
    parsers.parse(
        "the generators get the duties {duty_list:Numbers} % in order and the outputs are recorded, triggered on the first used one",
        extra_types=LISTS,
    )
)
def record_outputs(fw, ad3, pwm, state, generators, duty_list):
    duties = duty_list[:generators]
    fw.pwm.duty(pwm["module"], *duties)
    state["duties"] = duties
    state["capture"] = record(ad3, pwm, state["frequency"], state["used"][0][1])


@when(parsers.parse("the duty is set to {percent:d} % and both outputs are recorded, if the generator opened"))
def record_dead_time(fw, ad3, pwm, state, percent):
    if state["pwmclk"] is False:
        return
    fw.pwm.duty(pwm["module"], percent)
    state["capture"] = record(ad3, pwm, state["frequency"])


@when(
    parsers.parse(
        "the generators get the duties {duty_list:Numbers} % in order and are recorded {settle_ms:d} ms later, triggered on the first one",
        extra_types=LISTS,
    )
)
def record_update(fw, ad3, pwm, state, generators, duty_list, settle_ms):
    fw.pwm.duty(pwm["module"], *duty_list[:generators])
    time.sleep(settle_ms / 1000)
    state["duties"] = duty_list
    state["capture"] = record(ad3, pwm, state["frequency"], state["dios"][0])


@when("the PWM stops")
def stop_pwm(fw, pwm):
    fw.pwm.stop(pwm["module"])


@when("the outputs are recorded at the last of the frequency changes")
def record_last_frequency(ad3, pwm, state):
    state["capture"] = record(ad3, pwm, pwm["frequency_changes"][-1])


@when("the interrupts of the generator are counted over the interrupt window")
def count_interrupts(fw, pwm, state):
    state["count"], state["window"] = interrupt_window(fw, pwm, state["gen"])


@when("the PWM closes")
def close_pwm(fw, pwm):
    fw.pwm.close(pwm["module"])


@when("the fault pin is driven high")
def fault_pin_high(ad3, state):
    ad3.dio.drive(state["fault_dio"], 1)


@when("the fault pin is driven low")
def fault_pin_low(ad3, state):
    ad3.dio.drive(state["fault_dio"], 0)


@when("the fault pin is driven low and the fault detection is switched off")
def fault_pin_low_and_detection_off(fw, ad3, pwm, state):
    ad3.dio.drive(state["fault_dio"], 0)
    fw.pwm.fault(pwm["module"], False)


@when(parsers.parse("the outputs are recorded at {frequency:d} Hz"))
def record_at(ad3, pwm, state, frequency):
    state["capture"] = record(ad3, pwm, frequency)


@when("a fault event arrives")
@then("a fault event arrives")
def fault_event_arrives(fw, pwm):
    fw.pwm.wait_fault(pwm["module"], timeout=pwm["fault"]["timeout_s"])


@when(parsers.parse("the fault pin pulses high {pulses:d} times, each time {high_ms:d} ms high and {low_ms:d} ms low"))
def fault_pin_pulses(ad3, state, pulses, high_ms, low_ms):
    for _ in range(pulses):
        ad3.dio.drive(state["fault_dio"], 1)
        time.sleep(high_ms / 1000)
        ad3.dio.drive(state["fault_dio"], 0)
        time.sleep(low_ms / 1000)


@when("the input goes to the trip level")
def comparator_input_trips(ad3, pwm, state):
    ad3.wavegen.dc(state["wavegen"], pwm["fault"]["dcmp"]["trip_v"])


@when("the input returns to the safe level")
def comparator_input_safe_again(ad3, pwm, state):
    ad3.wavegen.dc(state["wavegen"], pwm["fault"]["dcmp"]["safe_v"])


@when("the first generator opens")
def open_first_generator(fw, pwm, state):
    selected = [generator["gen"] for generator in pwm["generators"][:1]]
    fw.pwm.open(pwm["module"], gens=selected)
    state["selected"] = selected


@when("the PWM closes and the first generator opens synchronously")
def reopen_synchronously(fw, pwm, state):
    fw.pwm.close(pwm["module"])
    fw.pwm.open(pwm["module"], gens=state["selected"], sync=True)


@when("the PWM opens with the A pin of the first generator only")
def open_a_pin_only(fw, pwm):
    first = pwm["generators"][0]
    fw.pwm.open(pwm["module"], pins=[(first["a"], None)])


@then("it reports the system clock over the divisor as PWM clock, if it opened")
def reports_pwm_clock(state):
    if state["reported"] is False:
        return
    assert state["reported"] == state["pwmclk"]


@then("both outputs run at the expected frequency and duty, if the generator opened")
def waveform_matches(pwm, state, mode, freq, duty):
    if state["reported"] is False:
        return
    pwmclk = state["pwmclk"]
    expected = expect.pwm_frequency(pwmclk, freq, mode)
    step = expect.pwm_duty_step(pwmclk, freq, mode)
    for dio in (state["a"], state["b"]):
        check_waveform(state["capture"], dio, expected, duty, pwm, step)


@then("every used output runs at the expected frequency with the duty of its generator")
def used_outputs_match(pwm, state, output):
    capture, duties = state["capture"], state["duties"]
    expected = expect.pwm_frequency(state["pwmclk"], state["frequency"], state["pwm_mode"])
    per_generator = 2 if output == "ab" else 1
    for position, (_, dio) in enumerate(state["used"]):
        check_waveform(capture, dio, expected, duties[position // per_generator], pwm)


@then("the unused outputs stay low without an edge")
def unused_outputs_stay_low(state):
    capture = state["capture"]
    for pin, dio in state["unused"]:
        assert capture.edge_count(dio) == 0 and capture.channel(dio)[0] == 0, f"unused output {pin} changed"


@then("the rising edges of the used outputs line up")
def used_outputs_aligned(pwm, state):
    check_aligned(state["capture"], [dio for _, dio in state["used"]], "rising", tolerance(pwm, "alignment_s"))


@then("A and B with the inversion undone are never active together, if the generator opened")
def no_shoot_through(state):
    if state["pwmclk"] is False:
        return
    capture = state["capture"]
    bits_a, bits_b = logical(capture, state["a"], state["inva"]), logical(capture, state["b"], state["invb"])
    rate = capture.rate
    assert analysis.overlap_samples(bits_a, bits_b) == 0, "A and B are active together (shoot-through)"
    state.update(bits_a=bits_a, bits_b=bits_b, rate=rate)


@then("A off -> B on takes the falling delay and B off -> A on the rising delay, if the generator opened")
def dead_times_match(pwm, state):
    if state["pwmclk"] is False:
        return
    pwmclk, rate = state["pwmclk"], state["rate"]
    split = analysis.dead_times_split(state["bits_a"], state["bits_b"], rate)
    allowed = tolerance(pwm, "dead_ticks") / pwmclk + 2 / rate
    for name, times, nanoseconds in (
        ("A off -> B on", split.a_off_to_b_on, state["fall"]),
        ("B off -> A on", split.b_off_to_a_on, state["rise"]),
    ):
        assert times, f"no {name} transitions captured"
        assert statistics.median(times) == pytest.approx(expect.pwm_dead_time(nanoseconds, pwmclk), abs=allowed), name
    state["allowed"] = allowed


@then("the high times of A and B plus both delays add up to one period, if the generator opened")
def period_adds_up(state, mode):
    if state["pwmclk"] is False:
        return
    pwmclk, rate, allowed = state["pwmclk"], state["rate"], state["allowed"]
    high_a = statistics.median(analysis.high_low_times(state["bits_a"], rate)[0])
    high_b = statistics.median(analysis.high_low_times(state["bits_b"], rate)[0])
    period = 1 / expect.pwm_frequency(pwmclk, state["frequency"], mode)
    dead_total = expect.pwm_dead_time(state["rise"], pwmclk) + expect.pwm_dead_time(state["fall"], pwmclk)
    assert high_a + high_b + dead_total == pytest.approx(period, abs=2 * allowed + 4 / rate)


@then(
    parsers.parse(
        "opening it with the sync and a dead band just over the nanosecond limit, a dead band just over the clock limit or no rising and a "
        'falling delay just over the clock limit fails with "{reason}"'
    )
)
def dead_band_limits_refused(fw, pwm, state, sync, reason):
    generator = state["generator"]
    too_many_clocks = int((expect.PWM_DEAD_MAX_CLOCKS + 1) * 1e9 / state["sysclk"]) + 1
    for dead in (expect.PWM_DEAD_MAX_NS + 1, too_many_clocks, (0, too_many_clocks)):
        with pytest.raises(FirmwareError) as error:
            fw.pwm.open(pwm["module"], gens=[generator["gen"]], dead=dead, sync=sync)
        assert error.value.reason == reason, f"dead={dead}"


@then(parsers.parse("it opens with the sync at divisor {divisor:d} with the largest dead band"))
def largest_dead_band_opens(fw, pwm, state, sync, divisor):
    fw.pwm.open(pwm["module"], gens=[state["generator"]["gen"]], div=divisor, dead=expect.PWM_DEAD_MAX_NS, sync=sync)


@then("every A output runs at the expected frequency with the duty of its generator")
def a_outputs_match(pwm, state, mode):
    expected = expect.pwm_frequency(state["pwmclk"], state["frequency"], mode)
    for dio, duty in zip(state["dios"], state["duties"]):
        check_waveform(state["capture"], dio, expected, duty, pwm)


@then("the A outputs line up on their rising edges in edge mode and on their pulse centres in center mode")
def a_outputs_aligned(pwm, state, mode):
    feature = "center" if mode == "center" else "rising"
    check_aligned(state["capture"], state["dios"], feature, tolerance(pwm, "alignment_s"))


@then(parsers.parse("A runs at the new frequency with {percent:d} % duty after each of the frequency changes"))
def a_follows_frequency_changes(fw, ad3, pwm, state, mode, percent):
    a = state["a"]
    for frequency in pwm["frequency_changes"]:
        fw.pwm.freq(pwm["module"], frequency)
        capture = record(ad3, pwm, frequency, a)
        check_waveform(capture, a, expect.pwm_frequency(state["pwmclk"], frequency, mode), percent, pwm)


@then("neither output has an edge")
def outputs_stopped(state):
    capture = state["capture"]
    assert capture.edge_count(state["a"]) == 0 and capture.edge_count(state["b"]) == 0, "outputs toggle after stop"


@then(parsers.parse('setting the frequency to {hz:d} Hz fails with "{reason}"'))
def frequency_refused(fw, pwm, hz, reason):
    with pytest.raises(FirmwareError) as error:
        fw.pwm.freq(pwm["module"], hz)
    assert error.value.reason == reason


@then("the count matches the events of the source per period at the frequency")
def count_matches_source(pwm, state, source, mode, freq):
    check_count(state["count"], freq, expect.pwm_events_per_period(source, mode), state["window"], pwm)


@then("the interrupts of each generator, counted over the interrupt window one after the other, match the events of its source per period")
def counts_match_sources(fw, pwm, state, mode):
    for generator, source in zip(state["selected"], state["sources"]):
        count, window = interrupt_window(fw, pwm, generator)
        check_count(count, state["frequency"], expect.pwm_events_per_period(source, mode), window, pwm)


@then(parsers.parse('opening it synchronously with the {interrupt} interrupt fails with "{reason}"'))
def synchronous_interrupts_refused(fw, pwm, state, interrupt, reason):
    with pytest.raises(FirmwareError) as error:
        fw.pwm.open(pwm["module"], gens=[state["generator"]["gen"]], irq=interrupt, sync=True)
    assert error.value.reason == reason


@then(
    "an ADC sequencer triggered by the generator converts no faster than the trigger, or times out if the source has no events in the mode"
)
def adc_follows_trigger(fw, pwm, source, gen, mode):
    check_adc_trigger(fw, pwm, gen, source, mode)


@then(
    "an ADC sequencer triggered by each generator in turn converts no faster than the trigger, or times out if its source has no events in "
    "that mode"
)
def adc_follows_each_trigger(fw, pwm, state):
    for generator, source in zip(state["selected"], state["sources"]):
        check_adc_trigger(fw, pwm, generator, source, state["pwm_mode"])


@then(parsers.parse('converting {count:d} sample on the sequencer fails with "{reason}"'))
def conversion_refused(fw, pwm, count, reason):
    config = pwm["adc_trigger_input"]
    with pytest.raises(FirmwareError) as error:
        fw.adc.measure(config["adc"], config["seq"], n=count, cmd_timeout=3.0)
    assert error.value.reason == reason, "the sequencer converted without its PWM trigger"


@then(parsers.parse("no fault is reported after {delay_ms:d} ms while the input is low"))
def no_fault_while_low(fw, pwm, delay_ms):
    fw.system.delay(delay_ms)
    assert not fw.pwm.faults(pwm["module"]), "fault reported while the input is low"


@then(parsers.parse("no fault is reported after {delay_ms:d} ms while the input is inside the safe band"))
def no_fault_while_safe(fw, pwm, delay_ms):
    fw.system.delay(delay_ms)
    assert not fw.pwm.faults(pwm["module"]), "fault reported while the input is inside the safe band"


@then(parsers.parse("no further fault is reported after {delay_ms:d} ms"))
def no_second_fault(fw, pwm, delay_ms):
    fw.system.delay(delay_ms)
    assert not fw.pwm.faults(pwm["module"]), "more than one fault event before pwm.stop"


@then("a fault event arrives naming a fault input, a selected generator and no other generator")
def fault_names_input_and_generators(fw, pwm, state):
    fault = pwm["fault"]
    selected = state["selected"]
    event = fw.pwm.wait_fault(pwm["module"], timeout=fault["timeout_s"])
    assert event.inputs & fault["inputs"], event.raw
    assert event.gens & generator_mask(selected), event.raw
    assert not event.gens & ~generator_mask(selected) & 0xF, f"generators without fault configuration reported: {event.raw}"


@then("A switches")
def a_switches(state):
    assert state["capture"].edge_count(state["a"]) > 0, "outputs must run before the fault"


@then("neither output switched during the fault")
def outputs_forced(state):
    capture = state["capture"]
    assert capture.edge_count(state["a"]) == 0 and capture.edge_count(state["b"]) == 0, "outputs keep switching during a fault"


@then("a fault event arrives naming the comparator and the generator")
def fault_names_comparator(fw, pwm, state):
    fault = pwm["fault"]
    event = fw.pwm.wait_fault(pwm["module"], timeout=fault["timeout_s"])
    assert event.comparators & (1 << fault["dcmp"]["comparator"]), event.raw
    assert event.gens & (1 << state["gen"]), event.raw


@then(parsers.parse('enabling the fault with input mask {mask:d} fails with "{reason}"'))
def fault_input_refused(fw, pwm, mask, reason):
    with pytest.raises(FirmwareError) as error:
        fw.pwm.fault(pwm["module"], inputs=mask)
    assert error.value.reason == reason


@then("every invalid fault setting fails with its reason")
def fault_settings_refused(fw, pwm, state):
    missing = next(generator for generator in range(4) if generator not in state["selected"])
    cases = [
        ({}, "usage"),
        ({"comparators": 0, "inputs": 0}, "usage"),
        ({"inputs": 16}, "range"),
        ({"comparators": 256}, "range"),
        ({"inputs": 1, "minperiod": 65536}, "range"),
        ({"inputs": 1, "gens": [missing]}, "usage"),
        ({"inputs": 1, "pin": "PB2"}, "pin"),
    ]
    for options, reason in cases:
        with pytest.raises(FirmwareError) as error:
            fw.pwm.fault(pwm["module"], **options)
        assert error.value.reason == reason, options


@then(parsers.parse('switching the fault detection off with a latch fails with "{reason}"'))
def fault_off_with_latch_refused(fw, pwm, reason):
    with pytest.raises(FirmwareError) as error:
        fw.pwm.fault(pwm["module"], False, latch=True)
    assert error.value.reason == reason


@then(parsers.parse("the fault inputs with comparator mask {comparators:d} can be enabled and switched off"))
def fault_enabled_and_switched_off(fw, pwm, comparators):
    fw.pwm.fault(pwm["module"], inputs=pwm["fault"]["inputs"], comparators=comparators)
    fw.pwm.fault(pwm["module"], False)


@then("every invalid open setting fails with its reason")
def open_settings_refused(fw, pwm):
    module = pwm["module"]
    first = pwm["generators"][0]
    cases = [
        ({}, "usage"),
        ({"gens": [4]}, "range"),
        ({"gens": [first["gen"], first["gen"]]}, "usage"),
        ({"gens": [0, 1, 2, 3, 0]}, "usage"),
        ({"gens": [first["gen"]], "pins": [(first["a"], first["b"])] * 2}, "usage"),
        ({"pins": [(None, None)]}, "usage"),
        ({"pins": [("PB2", None)]}, "pin"),
        ({"gens": [first["gen"]], "trigger": ["zero", "load"]}, "usage"),
        ({"gens": [first["gen"]], "irq": "sometimes"}, "usage"),
        ({"gens": [first["gen"]], "freq": 1}, "range"),
        ({"gens": [first["gen"]], "div": 3}, "usage"),
        ({"gens": [first["gen"]], "mode": "sideways"}, "usage"),
    ]
    for options, reason in cases:
        with pytest.raises(FirmwareError) as error:
            fw.pwm.open(module, **options)
        assert error.value.reason == reason, options


@then(parsers.parse('setting a duty of {percent:d} % fails with "{reason}"'))
def duty_refused(fw, pwm, percent, reason):
    with pytest.raises(FirmwareError) as error:
        fw.pwm.duty(pwm["module"], percent)
    assert error.value.reason == reason


@then(parsers.parse('setting {count:d} duties of {percent:d} % fails with "{reason}"'))
def duties_refused(fw, pwm, count, percent, reason):
    with pytest.raises(FirmwareError) as error:
        fw.pwm.duty(pwm["module"], *[percent] * count)
    assert error.value.reason == reason, "one duty per opened generator"


@then(parsers.parse('opening the first generator fails with "{reason}"'))
def reopen_refused(fw, pwm, reason):
    first = pwm["generators"][0]
    with pytest.raises(FirmwareError) as error:
        fw.pwm.open(pwm["module"], gens=[first["gen"]])
    assert error.value.reason == reason
