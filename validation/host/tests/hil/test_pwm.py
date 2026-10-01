"""PWM (`hal::tiva::Pwm` / `SynchronousPwm`): waveforms, outputs, dead band, update modes, interrupts, ADC
triggers and the fault path.

Wiring set `bundle1`: the A/B outputs of `tests.pwm.generators` and the fault pin on DIOs, W1 on the ADC input of
the digital comparator fault path. Interrupt and ADC-trigger tests need no wiring.
"""

from __future__ import annotations

import statistics
import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation import expect

PWMFAULTVAL_GAP = "driver does not program PWMFAULTVAL"


@pytest.fixture
def pwm(board_cfg):
    return board_cfg.param("pwm")


@pytest.fixture(scope="module")
def sysclk(fw):
    return fw.system.info().sysclk


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


@pytest.mark.ad3
@pytest.mark.matrix("pwm.waveform")
def test_waveform(fw, ad3, need, pwm, sysclk, mode, div, freq, duty, sync):
    generator = configured(pwm, 1)[0]
    a, b = need.dio(generator["a"]), need.dio(generator["b"])
    pwmclk = sysclk // div
    fits = expect.pwm_fits(pwmclk, freq, mode)
    reported = open_or_range(fits, lambda: open_generators(fw, pwm, [generator], freq=freq, mode=mode, div=div, dead="off", sync=sync))
    if reported is False:
        return
    assert reported == pwmclk
    fw.pwm.duty(pwm["module"], duty)
    capture = record(ad3, pwm, freq, None if duty in (0, 100) else a)
    expected = expect.pwm_frequency(pwmclk, freq, mode)
    step = expect.pwm_duty_step(pwmclk, freq, mode)
    for dio in (a, b):
        check_waveform(capture, dio, expected, duty, pwm, step)


@pytest.mark.ad3
@pytest.mark.matrix("pwm.outputs")
def test_outputs(fw, ad3, need, pwm, sysclk, generators, output, sync):
    """1-4 generators with both outputs, only A (`<pin>:-`) or only B (`-:<pin>`); the unused pin stays a
    GPIO driven low, so the driver must not take it over."""
    selected = configured(pwm, generators)
    duties = [20, 40, 60, 80][:generators]
    used, unused = [], []
    for generator in selected:
        for pin, active in zip((generator["a"], generator["b"]), (output in ("ab", "a"), output in ("ab", "b"))):
            (used if active else unused).append((pin, need.dio(pin)))
    for pin, _ in unused:
        fw.gpio.cfg(pin, "out")
    frequency = 10000
    pwmclk = open_generators(fw, pwm, selected, output, freq=frequency, mode="edge", dead="off", sync=sync)
    fw.pwm.duty(pwm["module"], *duties)
    capture = record(ad3, pwm, frequency, used[0][1])
    expected = expect.pwm_frequency(pwmclk, frequency, "edge")
    per_generator = 2 if output == "ab" else 1
    for position, (_, dio) in enumerate(used):
        check_waveform(capture, dio, expected, duties[position // per_generator], pwm)
    for pin, dio in unused:
        assert capture.edge_count(dio) == 0 and capture.channel(dio)[0] == 0, f"unused output {pin} changed"
    check_aligned(capture, [dio for _, dio in used], "rising", tolerance(pwm, "alignment_s"))


def logical(capture, dio, inverted):
    bits = capture.channel(dio)
    return [1 - bit for bit in bits] if inverted else bits


@pytest.mark.ad3
@pytest.mark.matrix("pwm.dead_time")
def test_dead_time(fw, ad3, need, pwm, sysclk, mode, dead, inversion, sync):
    """Dead band: A off -> B on is the falling-edge delay, B off -> A on the rising-edge delay; inverted outputs
    are compared after undoing the inversion."""
    generator = configured(pwm, 1)[0]
    a, b = need.dio(generator["a"]), need.dio(generator["b"])
    rise, fall = (dead, dead) if isinstance(dead, (int, float)) else dead
    inva, invb = inversion
    frequency = pwm["dead_time_frequency"]
    fits = expect.pwm_dead_fits(rise, sysclk) and expect.pwm_dead_fits(fall, sysclk)
    pwmclk = open_or_range(
        fits,
        lambda: open_generators(fw, pwm, [generator], freq=frequency, mode=mode, dead=dead, inva=inva, invb=invb, sync=sync),
    )
    if pwmclk is False:
        return
    fw.pwm.duty(pwm["module"], 40)
    capture = record(ad3, pwm, frequency)
    bits_a, bits_b = logical(capture, a, inva), logical(capture, b, invb)
    rate = capture.rate
    assert analysis.overlap_samples(bits_a, bits_b) == 0, "A and B are active together (shoot-through)"
    split = analysis.dead_times_split(bits_a, bits_b, rate)
    allowed = tolerance(pwm, "dead_ticks") / pwmclk + 2 / rate
    for name, times, nanoseconds in (("A off -> B on", split.a_off_to_b_on, fall), ("B off -> A on", split.b_off_to_a_on, rise)):
        assert times, f"no {name} transitions captured"
        assert statistics.median(times) == pytest.approx(expect.pwm_dead_time(nanoseconds, pwmclk), abs=allowed), name
    high_a = statistics.median(analysis.high_low_times(bits_a, rate)[0])
    high_b = statistics.median(analysis.high_low_times(bits_b, rate)[0])
    period = 1 / expect.pwm_frequency(pwmclk, frequency, mode)
    dead_total = expect.pwm_dead_time(rise, pwmclk) + expect.pwm_dead_time(fall, pwmclk)
    assert high_a + high_b + dead_total == pytest.approx(period, abs=2 * allowed + 4 / rate)


@pytest.mark.board_params("sync", values=[0, 1])
def test_dead_time_limits(fw, pwm, sysclk, sync):
    generator = configured(pwm, 1)[0]
    too_many_clocks = int((expect.PWM_DEAD_MAX_CLOCKS + 1) * 1e9 / sysclk) + 1
    for dead in (expect.PWM_DEAD_MAX_NS + 1, too_many_clocks, (0, too_many_clocks)):
        with pytest.raises(FirmwareError) as error:
            fw.pwm.open(pwm["module"], gens=[generator["gen"]], dead=dead, sync=sync)
        assert error.value.reason == "range", f"dead={dead}"
    fw.pwm.open(pwm["module"], gens=[generator["gen"]], div=64, dead=expect.PWM_DEAD_MAX_NS, sync=sync)


@pytest.mark.ad3
@pytest.mark.matrix("pwm.update")
def test_update_and_alignment(fw, ad3, need, pwm, update, mode, generators, sync):
    """Generators run in lock step (common edges or centres) and take new duties with local and global update."""
    selected = configured(pwm, generators)
    dios = [need.dio(generator["a"]) for generator in selected]
    frequency = 20000
    pwmclk = open_generators(fw, pwm, selected, "a", freq=frequency, mode=mode, dead="off", update=update, sync=sync)
    expected = expect.pwm_frequency(pwmclk, frequency, mode)
    feature = "center" if mode == "center" else "rising"
    for duties in ([20, 50, 80, 35], [60, 30, 45, 70]):
        fw.pwm.duty(pwm["module"], *duties[:generators])
        time.sleep(0.01)
        capture = record(ad3, pwm, frequency, dios[0])
        for dio, duty in zip(dios, duties):
            check_waveform(capture, dio, expected, duty, pwm)
        check_aligned(capture, dios, feature, tolerance(pwm, "alignment_s"))


@pytest.mark.ad3
@pytest.mark.board_params("mode", values=["edge", "center"])
@pytest.mark.board_params("sync", values=[0, 1])
def test_frequency_change_and_stop(fw, ad3, need, pwm, mode, sync):
    generator = configured(pwm, 1)[0]
    a, b = need.dio(generator["a"]), need.dio(generator["b"])
    changes = pwm["frequency_changes"]
    pwmclk = open_generators(fw, pwm, [generator], freq=changes[0], mode=mode, dead="off", sync=sync)
    fw.pwm.duty(pwm["module"], 50)
    for frequency in changes:
        fw.pwm.freq(pwm["module"], frequency)
        capture = record(ad3, pwm, frequency, a)
        check_waveform(capture, a, expect.pwm_frequency(pwmclk, frequency, mode), 50, pwm)
    fw.pwm.stop(pwm["module"])
    capture = record(ad3, pwm, changes[-1])
    assert capture.edge_count(a) == 0 and capture.edge_count(b) == 0, "outputs toggle after stop"
    with pytest.raises(FirmwareError) as error:
        fw.pwm.freq(pwm["module"], 1)
    assert error.value.reason == "range"


def interrupt_window(fw, pwm, generator):
    window_ms = pwm["irq_window_ms"]
    fw.pwm.count(pwm["module"], generator, clear=True)
    fw.system.delay(window_ms)
    return fw.pwm.count(pwm["module"], generator), window_ms / 1000


def check_count(count, frequency, per_period, window_s, pwm):
    expected = frequency * window_s * per_period
    slack = frequency * per_period * 0.02
    assert expected * (1 - pwm["irq_tolerance"]) <= count <= expected * (1 + pwm["irq_tolerance"]) + slack, (
        f"{count} events, expected {expected:.0f}"
    )


@pytest.mark.matrix("pwm.irq")
def test_interrupt_count(fw, pwm, sysclk, source, mode, freq):
    generator = configured(pwm, 1)[0]["gen"]
    div = expect.pwm_divisor_for(sysclk, freq, mode)
    fw.pwm.open(pwm["module"], gens=[generator], freq=freq, mode=mode, div=div, irq=source)
    fw.pwm.duty(pwm["module"], 50)
    count, window = interrupt_window(fw, pwm, generator)
    check_count(count, freq, expect.pwm_events_per_period(source, mode), window, pwm)


@pytest.mark.board_params("mode", values=["edge", "center"])
def test_interrupt_source_per_generator(fw, pwm, sysclk, mode):
    """`irq` with one source per generator: every generator counts its own events, `none` counts nothing."""
    selected = [generator["gen"] for generator in pwm["generators"]]
    sources = ["zero", "none", "load", "cmpbd"][: len(selected)]
    frequency = 5000
    div = expect.pwm_divisor_for(sysclk, frequency, mode)
    fw.pwm.open(pwm["module"], gens=selected, freq=frequency, mode=mode, div=div, irq=sources)
    fw.pwm.duty(pwm["module"], 50)
    for generator, source in zip(selected, sources):
        count, window = interrupt_window(fw, pwm, generator)
        check_count(count, frequency, expect.pwm_events_per_period(source, mode), window, pwm)


def test_interrupts_need_async(fw, pwm):
    generator = configured(pwm, 1)[0]["gen"]
    with pytest.raises(FirmwareError) as error:
        fw.pwm.open(pwm["module"], gens=[generator], irq="zero", sync=True)
    assert error.value.reason == "unsupported"


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


@pytest.mark.matrix("pwm.adc_trigger")
def test_adc_trigger(fw, pwm, source, gen, mode):
    """Generator `gen` triggers an asynchronous ADC sequencer (`trigger=pwm<gen>`) once per `source` event."""
    frequency = pwm["adc_trigger_input"]["freq"]
    fw.pwm.open(pwm["module"], gens=[gen], freq=frequency, mode=mode, div=64, trigger=source)
    fw.pwm.duty(pwm["module"], 50)
    check_adc_trigger(fw, pwm, gen, source, mode)


def test_adc_trigger_per_generator(fw, pwm):
    """`trigger` with one source per generator: only generators with a source trigger the ADC."""
    selected = [generator["gen"] for generator in pwm["generators"]]
    sources = ["zero", "none", "load", "none"][: len(selected)]
    fw.pwm.open(pwm["module"], gens=selected, freq=pwm["adc_trigger_input"]["freq"], mode="center", div=64, trigger=sources)
    fw.pwm.duty(pwm["module"], 50)
    for generator, source in zip(selected, sources):
        check_adc_trigger(fw, pwm, generator, source, "center")


def test_adc_trigger_stops_with_the_pwm(fw, pwm):
    generator = configured(pwm, 1)[0]["gen"]
    config = pwm["adc_trigger_input"]
    fw.pwm.open(pwm["module"], gens=[generator], freq=config["freq"], div=64, trigger="zero")
    fw.pwm.duty(pwm["module"], 50)
    fw.adc.open(config["adc"], config["seq"], pins=[config["pin"]], trigger=f"pwm{generator}")
    fw.adc.measure(config["adc"], config["seq"], n=2, cmd_timeout=3.0)
    fw.pwm.close(pwm["module"])
    with pytest.raises(FirmwareError) as error:
        fw.adc.measure(config["adc"], config["seq"], n=1, cmd_timeout=3.0)
    assert error.value.reason == "timeout", "the sequencer converted without its PWM trigger"


def fault_generators(pwm, which):
    selected = [generator["gen"] for generator in pwm["generators"]]
    return selected[:1] if which == "first" else selected


def generator_mask(generators):
    return sum(1 << generator for generator in generators)


@pytest.mark.ad3
@pytest.mark.matrix("pwm.fault.options")
def test_fault_input_pin(fw, ad3, need, pwm, latch, minperiod, gens):
    """A high level on the fault pin raises `EVT pwm` naming the pin input and the configured generators."""
    fault = pwm["fault"]
    dio = need.dio(fault["pin"])
    ad3.dio.drive(dio, 0)
    opened = [generator["gen"] for generator in pwm["generators"]]
    selected = fault_generators(pwm, gens)
    fw.pwm.open(pwm["module"], gens=opened, freq=10000)
    fw.pwm.fault(pwm["module"], gens=selected, inputs=fault["inputs"], pin=fault["pin"], latch=latch, minperiod=minperiod)
    fw.pwm.duty(pwm["module"], 50)
    fw.system.delay(50)
    assert not fw.pwm.faults(pwm["module"]), "fault reported while the input is low"
    ad3.dio.drive(dio, 1)
    event = fw.pwm.wait_fault(pwm["module"], timeout=fault["timeout_s"])
    assert event.inputs & fault["inputs"], event.raw
    assert event.gens & generator_mask(selected), event.raw
    assert not event.gens & ~generator_mask(selected) & 0xF, f"generators without fault configuration reported: {event.raw}"
    ad3.dio.drive(dio, 0)
    fw.pwm.fault(pwm["module"], False)


@pytest.mark.ad3
@pytest.mark.xfail(strict=False, reason=PWMFAULTVAL_GAP)
def test_fault_forces_outputs(fw, ad3, need, pwm):
    fault = pwm["fault"]
    generator = configured(pwm, 1)[0]
    a, b = need.dio(generator["a"]), need.dio(generator["b"])
    dio = need.dio(fault["pin"])
    ad3.dio.drive(dio, 0)
    open_generators(fw, pwm, [generator], freq=10000)
    fw.pwm.fault(pwm["module"], inputs=fault["inputs"], pin=fault["pin"])
    fw.pwm.duty(pwm["module"], 50)
    running = record(ad3, pwm, 10000)
    assert running.edge_count(a) > 0, "outputs must run before the fault"
    ad3.dio.drive(dio, 1)
    fw.pwm.wait_fault(pwm["module"], timeout=fault["timeout_s"])
    stopped = record(ad3, pwm, 10000)
    ad3.dio.drive(dio, 0)
    assert stopped.edge_count(a) == 0 and stopped.edge_count(b) == 0, "outputs keep switching during a fault"


@pytest.mark.ad3
def test_fault_digital_comparator(fw, ad3, need, pwm):
    """An ADC digital comparator (`adc.open dcmp=`) above its window raises `EVT pwm` with its comparator bit."""
    fault = pwm["fault"]
    config = fault["dcmp"]
    wavegen = need.wavegen(config["pin"])
    ad3.wavegen.dc(wavegen, config["safe_v"])
    generator = configured(pwm, 1)[0]["gen"]
    fw.pwm.open(pwm["module"], gens=[generator], freq=10000, trigger="zero")
    fw.adc.open(
        config["adc"],
        config["seq"],
        pins=[config["pin"], config["pin"]],
        trigger=f"pwm{generator}",
        dcmp=[(config["comparator"], config["low"], config["high"])],
    )
    fw.pwm.fault(pwm["module"], gens=[generator], comparators=1 << config["comparator"])
    fw.pwm.duty(pwm["module"], 50)
    fw.system.delay(50)
    assert not fw.pwm.faults(pwm["module"]), "fault reported while the input is inside the safe band"
    ad3.wavegen.dc(wavegen, config["trip_v"])
    event = fw.pwm.wait_fault(pwm["module"], timeout=fault["timeout_s"])
    assert event.comparators & (1 << config["comparator"]), event.raw
    assert event.gens & (1 << generator), event.raw
    ad3.wavegen.dc(wavegen, config["safe_v"])


def test_fault_errors(fw, pwm):
    fault = pwm["fault"]
    selected = [generator["gen"] for generator in pwm["generators"][:1]]
    missing = next(generator for generator in range(4) if generator not in selected)
    with pytest.raises(FirmwareError) as error:
        fw.pwm.fault(pwm["module"], inputs=1)
    assert error.value.reason == "notopen"
    fw.pwm.open(pwm["module"], gens=selected)
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
    with pytest.raises(FirmwareError) as error:
        fw.pwm.fault(pwm["module"], False, latch=True)
    assert error.value.reason == "usage"
    fw.pwm.fault(pwm["module"], inputs=fault["inputs"], comparators=1)
    fw.pwm.fault(pwm["module"], False)
    fw.pwm.close(pwm["module"])
    fw.pwm.open(pwm["module"], gens=selected, sync=True)
    with pytest.raises(FirmwareError) as error:
        fw.pwm.fault(pwm["module"], inputs=1)
    assert error.value.reason == "unsupported"


def test_open_errors(fw, pwm):
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
    with pytest.raises(FirmwareError) as error:
        fw.pwm.duty(module, 50)
    assert error.value.reason == "notopen"
    fw.pwm.open(module, pins=[(first["a"], None)])
    with pytest.raises(FirmwareError) as error:
        fw.pwm.duty(module, 50, 50)
    assert error.value.reason == "usage", "one duty per opened generator"
    with pytest.raises(FirmwareError) as error:
        fw.pwm.open(module, gens=[first["gen"]])
    assert error.value.reason == "busy"
