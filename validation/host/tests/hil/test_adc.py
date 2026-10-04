"""ADC (`hal::tiva::Adc` / `SynchronousAdc`): wavegen DC levels against raw 12-bit codes.

Wiring set `bundle1`: W1/W2 on the two inputs of `tests.adc.inputs`; the scope on the same pins (optional) measures
the actual level, which then replaces the programmed one as reference. Asynchronous sequencers convert on a PWM
generator trigger (the driver has no processor trigger), which each test opens itself.

Scenarios: features/adc.feature.
"""

from __future__ import annotations

import statistics
import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when

from hal_ti_validation import expect

EXTERNAL_REFERENCE_GAP = "driver never writes the external reference selection"
DEPTHS = (8, 4, 4, 1)
ORDINALS = {"first": 0, "second": 1}


@pytest.fixture
def adc_cfg(board_cfg):
    return board_cfg.param("adc")


def apply_level(ad3, need, pin, volts):
    """Drive `pin` to `volts`; returns the measured level (scope) or the programmed one.

    The scope reads on a 5 V range centred on mid-supply: the default 10 V range is some 25 mV off, most of the ADC tolerance.
    It averages 100 ms, whole periods of both 50 and 60 Hz: a 10 ms window put mains hum into the mean, ±25 mV between
    acquisitions on the bench.
    """
    ad3.wavegen.dc(need.wavegen(pin), volts)
    time.sleep(0.02)
    scope = need.optional_scope(pin)
    if scope is None:
        return volts
    return statistics.fmean(ad3.scope.acquire([scope], rate=5e4, samples=5000, range_v=5.0, offset_v=1.65)[scope])


def start_trigger(fw, adc_cfg, sync):
    """Asynchronous sequencers convert only on a PWM trigger: run one generator with `trigger=zero`.

    Returns the `trigger` option of `adc.open`, or None for a synchronous sequencer.
    """
    if sync:
        return None
    trigger = adc_cfg["trigger"]
    fw.pwm.open(trigger["module"], gens=[trigger["gen"]], freq=trigger["freq"], trigger="zero")
    fw.pwm.duty(trigger["module"], 50)
    return f"pwm{trigger['gen']}"


def runs(adc_cfg, steps):
    """`adc.measure` returns at most 64 values."""
    return max(1, min(adc_cfg["samples"], 64 // steps))


def check_codes(samples, volts, adc_cfg, vref=None):
    expected = analysis.adc_code(volts, vref or adc_cfg["vref"], adc_cfg["bits"])
    result = analysis.stats(samples)
    assert result.mean == pytest.approx(expected, abs=adc_cfg["tolerance_codes"]), f"{volts:.3f} V: {result}"
    assert result.maximum - result.minimum <= adc_cfg.get("spread_codes", 2 * adc_cfg["tolerance_codes"]), f"noisy samples {result}"


def timing_valid(values):
    return not (values.get("sync") and values.get("delay", "off") != "off")


def band_valid(values):
    return not (values.get("band") == "mid" and str(values.get("mode", "")).startswith("hyst"))


def dcmp_levels(adc_cfg, band):
    """(level inside the band, level outside it) for the `dcmp_window` thresholds."""
    levels = adc_cfg["dcmp_levels_v"]
    outside = {"low": levels["high"], "mid": levels["low"], "high": levels["low"]}
    return levels[band], outside[band]


@pytest.mark.matrix("adc.levels")
@scenario("adc.feature", "A DC level reads as its code")
def test_dc_levels(adc, pin, level, sync):
    pass


@pytest.mark.matrix("adc.sequencers")
@scenario("adc.feature", "Every sequencer reads its inputs with all its steps")
def test_sequencer_full_depth(adc, seq, sync):
    pass


@pytest.mark.board_params("seq", values=[0, 1, 2, 3])
@scenario("adc.feature", "A sequencer refuses more steps than its depth")
def test_sequencer_depth_limit(seq):
    pass


@pytest.mark.matrix("adc.timing")
@pytest.mark.constraint(valid=timing_valid)
@scenario("adc.feature", "A level reads as its code with any sample and hold time, averaging and sampling delay")
def test_sample_hold_averaging_delay(sh, avg, delay, sync):
    pass


@scenario("adc.feature", "An asynchronous sequencer needs a trigger and every sequencer needs pins")
def test_async_requires_trigger():
    pass


@pytest.mark.board_params(
    "option",
    values=[{"delay": 4}, {"trigger": "pwm0"}, {"dcmp": [(0, 0, 4095)]}, {"ref": "ext"}],
)
@scenario("adc.feature", "A synchronous sequencer refuses the asynchronous options")
def test_sync_rejects_async_options(option):
    pass


@scenario("adc.feature", "An asynchronous sequencer converts only while its PWM generator runs")
def test_async_times_out_without_trigger():
    pass


@pytest.mark.matrix("adc.dcmp")
@pytest.mark.constraint(valid=band_valid)
@scenario("adc.feature", "A digital comparator step stays out of the FIFO and reports its band as a PWM fault")
def test_digital_comparator(band, mode, comparator):
    pass


@pytest.mark.board_params(
    "entry,reason",
    values=[
        [[(0, 0, 4095), (1, 0, 4095)], "range"],
        [[(8, 0, 4095)], "range"],
        [[(0, 3000, 1000)], "range"],
        [[(0, 0, 4096)], "range"],
    ],
)
@scenario("adc.feature", "Invalid digital comparator entries are refused")
def test_digital_comparator_limits(entry, reason):
    pass


@pytest.mark.board_params("order", "adc.priorities")
@scenario("adc.feature", "Two synchronous sequencers with priorities read their own inputs")
def test_two_sequencers_with_priorities(order):
    pass


@scenario("adc.feature", "A level reads as its code with the internal reference")
def test_internal_reference():
    pass


@pytest.mark.xfail(strict=False, reason=EXTERNAL_REFERENCE_GAP)
@scenario("adc.feature", "With the external reference the codes follow VREFA+")
def test_external_reference():
    pass


@given("the pin is driven to the level", target_fixture="volts")
def pin_at_level(ad3, need, pin, level):
    return apply_level(ad3, need, pin, level)


@given(parsers.parse("the first input is driven to {target:g} V"), target_fixture="volts")
def first_input_at(ad3, need, adc_cfg, target):
    return apply_level(ad3, need, adc_cfg["inputs"][0], target)


@given(parsers.parse("the first input is driven to {first_v:g} V and the second to {second_v:g} V"), target_fixture="levels")
def both_inputs_at(ad3, need, adc_cfg, first_v, second_v):
    first, second = adc_cfg["inputs"]
    return [apply_level(ad3, need, first, first_v), apply_level(ad3, need, second, second_v)]


@given("the board file gives the VREFA+ voltage", target_fixture="reference")
def external_reference_configured(adc_cfg):
    reference = adc_cfg.get("external_reference_v")
    if reference is None:
        pytest.skip("set tests.adc.external_reference_v to the VREFA+ voltage")
    return reference


@given(
    parsers.parse("the first input is driven to the lower of {cap:g} V and {fraction:g} times the VREFA+ voltage"), target_fixture="volts"
)
def first_input_below_reference(ad3, need, adc_cfg, reference, cap, fraction):
    return apply_level(ad3, need, adc_cfg["inputs"][0], min(cap, reference * fraction))


@given("the first input is driven outside the band", target_fixture="dcmp_wavegen")
def first_input_outside_band(ad3, need, adc_cfg, band):
    _, outside = dcmp_levels(adc_cfg, band)
    wavegen = need.wavegen(adc_cfg["inputs"][0])
    ad3.wavegen.dc(wavegen, outside)
    return wavegen


@given("the trigger PWM generator runs unless the sequencer is synchronous", target_fixture="trigger_option")
def trigger_unless_synchronous(fw, adc_cfg, sync):
    return start_trigger(fw, adc_cfg, sync)


@given("the trigger PWM generator runs", target_fixture="trigger_option")
def trigger_runs(fw, adc_cfg):
    return start_trigger(fw, adc_cfg, sync=False)


@given("the trigger PWM generator is open with no duty set")
def trigger_open(fw, adc_cfg):
    trigger = adc_cfg["trigger"]
    fw.pwm.open(trigger["module"], gens=[trigger["gen"]], freq=trigger["freq"], trigger="zero")


@given(parsers.parse("sequencer {sequencer:d} of ADC {converter:d} is open on the first input, on the trigger"))
def sequencer_open_on_trigger(fw, adc_cfg, sequencer, converter):
    fw.adc.open(converter, sequencer, pins=[adc_cfg["inputs"][0]], trigger=f"pwm{adc_cfg['trigger']['gen']}")


@given(
    parsers.parse(
        "sequencer {sequencer:d} of ADC {converter:d} is open with two steps on the first input, on the trigger, "
        "the second routed to the comparator with the window, the band and the mode"
    )
)
def sequencer_open_with_comparator(fw, adc_cfg, band, mode, comparator, sequencer, converter):
    pin = adc_cfg["inputs"][0]
    low, high = adc_cfg["dcmp_window"]
    fw.adc.open(
        converter, sequencer, pins=[pin, pin], trigger=f"pwm{adc_cfg['trigger']['gen']}", dcmp=[(comparator, low, high, band, mode)]
    )


@given("the comparator is a fault input of the trigger PWM generator")
def comparator_faults_trigger(fw, adc_cfg, comparator):
    trigger = adc_cfg["trigger"]
    fw.pwm.fault(trigger["module"], gens=[trigger["gen"]], comparators=1 << comparator)


@when(parsers.parse("sequencer {sequencer:d} of the ADC opens on the pin, synchronous or on the trigger"))
def sequencer_opens_on_pin(fw, adc, pin, sync, trigger_option, sequencer):
    fw.adc.open(adc, sequencer, pins=[pin], trigger=trigger_option, sync=sync)


@when(
    "the sequencer of the ADC opens with all its steps alternating the first and the second input, synchronous or on the trigger",
    target_fixture="sequence_pins",
)
def sequencer_opens_full(fw, adc_cfg, adc, seq, sync, trigger_option):
    first, second = adc_cfg["inputs"]
    pins = [(first, second)[step % 2] for step in range(DEPTHS[seq])]
    fw.adc.open(adc, seq, pins=pins, trigger=trigger_option, sync=sync)
    return pins


@when(
    parsers.parse(
        "sequencer {sequencer:d} of ADC {converter:d} opens on the first input with the sample and hold time, the averaging "
        "and the delay, synchronous or on the trigger"
    )
)
def sequencer_opens_with_timing(fw, adc_cfg, sh, avg, delay, sync, trigger_option, sequencer, converter):
    fw.adc.open(converter, sequencer, pins=[adc_cfg["inputs"][0]], sh=sh, avg=avg, delay=delay, trigger=trigger_option, sync=sync)


@when(
    parsers.parse(
        "sequencer {sequencer:d} of ADC {converter:d} opens synchronously on the {position} input with the {rank} priority of the order"
    ),
    converters={"position": ORDINALS.__getitem__, "rank": ORDINALS.__getitem__},
)
def sequencer_opens_with_priority(fw, adc_cfg, order, sequencer, converter, position, rank):
    fw.adc.open(converter, sequencer, pins=[adc_cfg["inputs"][position]], prio=order[rank], sync=True)


@when(parsers.parse('sequencer {sequencer:d} of ADC {converter:d} opens on the first input, on the trigger, with reference "{ref_option}"'))
def sequencer_opens_with_reference(fw, adc_cfg, trigger_option, sequencer, converter, ref_option):
    fw.adc.open(converter, sequencer, pins=[adc_cfg["inputs"][0]], trigger=trigger_option, ref=ref_option)


@when(parsers.parse("the trigger PWM generator runs at {duty:d} % duty"))
def trigger_starts(fw, adc_cfg, duty):
    fw.pwm.duty(adc_cfg["trigger"]["module"], duty)


@when("the first input is driven inside the band")
def first_input_inside_band(ad3, adc_cfg, band, dcmp_wavegen):
    inside, _ = dcmp_levels(adc_cfg, band)
    ad3.wavegen.dc(dcmp_wavegen, inside)


@then(parsers.parse("sequencer {sequencer:d} of the ADC reads the level"))
def instance_reads_level(fw, adc_cfg, adc, volts, sequencer):
    check_codes(fw.adc.measure(adc, sequencer, n=adc_cfg["samples"]), volts, adc_cfg)


@then(parsers.parse("sequencer {sequencer:d} of ADC {converter:d} reads the level"))
def reads_level(fw, adc_cfg, volts, sequencer, converter):
    check_codes(fw.adc.measure(converter, sequencer, n=adc_cfg["samples"]), volts, adc_cfg)


@then(
    parsers.parse("sequencer {sequencer:d} of ADC {converter:d} reads the level of the {position} input"),
    converters={"position": ORDINALS.__getitem__},
)
def reads_input_level(fw, adc_cfg, levels, sequencer, converter, position):
    check_codes(fw.adc.measure(converter, sequencer, n=adc_cfg["samples"]), levels[position], adc_cfg)


@then(parsers.parse("sequencer {sequencer:d} of ADC {converter:d} reads the level against the VREFA+ voltage"))
def reads_level_against_reference(fw, adc_cfg, volts, reference, sequencer, converter):
    check_codes(fw.adc.measure(converter, sequencer, n=adc_cfg["samples"]), volts, adc_cfg, vref=reference)


@then("the sequencer of the ADC returns all steps of as many of the runs as fit in 64 values, each step reading the level of its input")
def reads_every_step(fw, adc_cfg, adc, seq, levels, sequence_pins):
    first, second = adc_cfg["inputs"]
    by_pin = {first: levels[0], second: levels[1]}
    steps = DEPTHS[seq]
    count = runs(adc_cfg, steps)
    samples = fw.adc.measure(adc, seq, n=count)
    assert len(samples) == count * steps
    for pin, channel in zip(sequence_pins, analysis.deinterleave(samples, steps)):
        check_codes(channel, by_pin[pin], adc_cfg)


@then(
    parsers.parse(
        "opening the sequencer of ADC {converter:d} synchronously with one step more than its depth on the first input "
        'fails with "{error_reason}"'
    )
)
def deeper_than_sequencer_refused(fw, adc_cfg, seq, converter, error_reason):
    pins = [adc_cfg["inputs"][0]] * (DEPTHS[seq] + 1)
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(converter, seq, pins=pins, sync=True)
    assert error.value.reason == error_reason


@then(
    parsers.parse('opening sequencer {sequencer:d} of ADC {converter:d} on the first input without a trigger fails with "{error_reason}"')
)
def open_without_trigger_refused(fw, adc_cfg, sequencer, converter, error_reason):
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(converter, sequencer, pins=[adc_cfg["inputs"][0]])
    assert error.value.reason == error_reason


@then(parsers.parse('opening sequencer {sequencer:d} of ADC {converter:d} synchronously without pins fails with "{error_reason}"'))
def open_without_pins_refused(fw, sequencer, converter, error_reason):
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(converter, sequencer, sync=True)
    assert error.value.reason == error_reason, "pins is required"


@then(
    parsers.parse(
        "opening sequencer {sequencer:d} of ADC {converter:d} synchronously with two steps on the first input and the option "
        'fails with "{error_reason}"'
    )
)
def synchronous_option_refused(fw, adc_cfg, option, sequencer, converter, error_reason):
    pin = adc_cfg["inputs"][0]
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(converter, sequencer, pins=[pin, pin], sync=True, **option)
    assert error.value.reason == error_reason


@then(
    parsers.parse(
        "opening sequencer {sequencer:d} of ADC {converter:d} with two steps on the first input, on the trigger, "
        "with the comparator entries fails with the reason"
    )
)
def comparator_entries_refused(fw, adc_cfg, entry, reason, sequencer, converter):
    pin = adc_cfg["inputs"][0]
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(converter, sequencer, pins=[pin, pin], trigger=f"pwm{adc_cfg['trigger']['gen']}", dcmp=entry)
    assert error.value.reason == reason


@then(
    parsers.parse(
        'measuring {count:d} run of sequencer {sequencer:d} of ADC {converter:d} within {seconds:g} s fails with "{error_reason}"'
    )
)
def measure_times_out(fw, count, sequencer, converter, seconds, error_reason):
    with pytest.raises(FirmwareError) as error:
        fw.adc.measure(converter, sequencer, n=count, cmd_timeout=seconds)
    assert error.value.reason == error_reason


@then(
    parsers.parse("measuring {count:d} runs of sequencer {sequencer:d} of ADC {converter:d} within {seconds:g} s returns {values:d} values")
)
def measure_returns(fw, count, sequencer, converter, seconds, values):
    assert len(fw.adc.measure(converter, sequencer, n=count, cmd_timeout=seconds)) == values


@then(
    parsers.parse(
        "measuring {count:d} runs of sequencer {sequencer:d} of ADC {converter:d} within {seconds:g} s returns only the step not routed "
        "to the comparator"
    )
)
def comparator_step_not_in_fifo(fw, count, sequencer, converter, seconds):
    samples = fw.adc.measure(converter, sequencer, n=count, cmd_timeout=seconds)
    assert len(samples) == count * expect.adc_steps_per_run(2, 1), "the comparator step must not reach the FIFO"


@then(parsers.parse("after {ms:d} ms the trigger PWM module has reported no fault"))
def no_fault_outside_band(fw, adc_cfg, band, comparator, ms):
    fw.system.delay(ms)
    assert not fw.pwm.faults(adc_cfg["trigger"]["module"]), f"comparator {comparator} reported while outside the {band} band"


@then("the trigger PWM module reports a fault of the comparator")
def comparator_fault_reported(fw, board_cfg, adc_cfg, comparator):
    event = fw.pwm.wait_fault(adc_cfg["trigger"]["module"], timeout=board_cfg.param("pwm.fault.timeout_s", 1.0))
    assert event.comparators & (1 << comparator), event.raw
