"""ADC (`hal::tiva::Adc` / `SynchronousAdc`): wavegen DC levels against raw 12-bit codes.

Wiring set `adc`: W1/W2 on the two inputs of `tests.adc.inputs`; the scope on the same pins (optional) measures
the actual level, which then replaces the programmed one as reference. Asynchronous sequencers convert on a PWM
generator trigger (the driver has no processor trigger), which each test opens itself.
"""

from __future__ import annotations

import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation import expect

EXTERNAL_REFERENCE_GAP = "driver never writes the external reference selection"
DEPTHS = (8, 4, 4, 1)


@pytest.fixture
def adc_cfg(board_cfg):
    return board_cfg.param("adc")


def apply_level(ad3, need, pin, volts):
    """Drive `pin` to `volts`; returns the measured level (scope) or the programmed one."""
    ad3.wavegen.dc(need.wavegen(pin), volts)
    time.sleep(0.02)
    scope = need.optional_scope(pin)
    return ad3.scope.average(scope) if scope is not None else volts


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
    assert result.maximum - result.minimum <= 2 * adc_cfg["tolerance_codes"], f"noisy samples {result}"


@pytest.mark.ad3
@pytest.mark.matrix("adc.levels")
def test_dc_levels(fw, ad3, need, adc_cfg, adc, pin, level, sync):
    volts = apply_level(ad3, need, pin, level)
    trigger = start_trigger(fw, adc_cfg, sync)
    fw.adc.open(adc, 3, pins=[pin], trigger=trigger, sync=sync)
    check_codes(fw.adc.measure(adc, 3, n=adc_cfg["samples"]), volts, adc_cfg)


@pytest.mark.ad3
@pytest.mark.matrix("adc.sequencers")
def test_sequencer_full_depth(fw, ad3, need, adc_cfg, adc, seq, sync):
    """Every sequencer with all its steps (8/4/4/1), alternating the two driven inputs."""
    first, second = adc_cfg["inputs"]
    levels = {first: apply_level(ad3, need, first, 0.8), second: apply_level(ad3, need, second, 2.4)}
    steps = DEPTHS[seq]
    pins = [(first, second)[step % 2] for step in range(steps)]
    trigger = start_trigger(fw, adc_cfg, sync)
    fw.adc.open(adc, seq, pins=pins, trigger=trigger, sync=sync)
    count = runs(adc_cfg, steps)
    samples = fw.adc.measure(adc, seq, n=count)
    assert len(samples) == count * steps
    for pin, channel in zip(pins, analysis.deinterleave(samples, steps)):
        check_codes(channel, levels[pin], adc_cfg)


@pytest.mark.board_params("seq", values=[0, 1, 2, 3])
def test_sequencer_depth_limit(fw, adc_cfg, seq):
    pins = [adc_cfg["inputs"][0]] * (DEPTHS[seq] + 1)
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(0, seq, pins=pins, sync=True)
    assert error.value.reason == "range"


def timing_valid(values):
    return not (values.get("sync") and values.get("delay", "off") != "off")


@pytest.mark.ad3
@pytest.mark.matrix("adc.timing")
@pytest.mark.constraint(valid=timing_valid)
def test_sample_hold_averaging_delay(fw, ad3, need, adc_cfg, sh, avg, delay, sync):
    pin = adc_cfg["inputs"][0]
    volts = apply_level(ad3, need, pin, 1.65)
    trigger = start_trigger(fw, adc_cfg, sync)
    fw.adc.open(0, 3, pins=[pin], sh=sh, avg=avg, delay=delay, trigger=trigger, sync=sync)
    check_codes(fw.adc.measure(0, 3, n=adc_cfg["samples"]), volts, adc_cfg)


def test_async_requires_trigger(fw, adc_cfg):
    pin = adc_cfg["inputs"][0]
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(0, 3, pins=[pin])
    assert error.value.reason == "usage"
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(0, 3, sync=True)
    assert error.value.reason == "usage", "pins is required"


@pytest.mark.board_params(
    "option",
    values=[{"delay": 4}, {"trigger": "pwm0"}, {"dcmp": [(0, 0, 4095)]}, {"ref": "ext"}],
)
def test_sync_rejects_async_options(fw, adc_cfg, option):
    pin = adc_cfg["inputs"][0]
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(0, 1, pins=[pin, pin], sync=True, **option)
    assert error.value.reason == "unsupported"


def test_async_times_out_without_trigger(fw, adc_cfg):
    """An asynchronous sequencer whose PWM generator does not run never converts."""
    trigger = adc_cfg["trigger"]
    fw.pwm.open(trigger["module"], gens=[trigger["gen"]], freq=trigger["freq"], trigger="zero")
    fw.adc.open(0, 3, pins=[adc_cfg["inputs"][0]], trigger=f"pwm{trigger['gen']}")
    with pytest.raises(FirmwareError) as error:
        fw.adc.measure(0, 3, n=1, cmd_timeout=3.0)
    assert error.value.reason == "timeout"
    fw.pwm.duty(trigger["module"], 50)
    assert len(fw.adc.measure(0, 3, n=4, cmd_timeout=3.0)) == 4


def band_valid(values):
    return not (values.get("band") == "mid" and str(values.get("mode", "")).startswith("hyst"))


def dcmp_levels(adc_cfg, band):
    """(level inside the band, level outside it) for the `dcmp_window` thresholds."""
    levels = adc_cfg["dcmp_levels_v"]
    outside = {"low": levels["high"], "mid": levels["low"], "high": levels["low"]}
    return levels[band], outside[band]


@pytest.mark.ad3
@pytest.mark.matrix("adc.dcmp")
@pytest.mark.constraint(valid=band_valid)
def test_digital_comparator(fw, ad3, need, board_cfg, adc_cfg, band, mode, comparator):
    """A digital comparator step leaves the FIFO and, routed to the PWM fault inputs, reports its band."""
    pin = adc_cfg["inputs"][0]
    low, high = adc_cfg["dcmp_window"]
    inside, outside = dcmp_levels(adc_cfg, band)
    wavegen = need.wavegen(pin)
    ad3.wavegen.dc(wavegen, outside)
    trigger = adc_cfg["trigger"]
    module = trigger["module"]
    fw.pwm.open(module, gens=[trigger["gen"]], freq=trigger["freq"], trigger="zero")
    fw.adc.open(0, 1, pins=[pin, pin], trigger=f"pwm{trigger['gen']}", dcmp=[(comparator, low, high, band, mode)])
    fw.pwm.fault(module, gens=[trigger["gen"]], comparators=1 << comparator)
    fw.pwm.duty(module, 50)
    samples = fw.adc.measure(0, 1, n=4, cmd_timeout=3.0)
    assert len(samples) == 4 * expect.adc_steps_per_run(2, 1), "the comparator step must not reach the FIFO"
    fw.system.delay(50)
    assert not fw.pwm.faults(module), f"comparator {comparator} reported while outside the {band} band"
    ad3.wavegen.dc(wavegen, inside)
    event = fw.pwm.wait_fault(module, timeout=board_cfg.param("pwm.fault.timeout_s", 1.0))
    assert event.comparators & (1 << comparator), event.raw


@pytest.mark.board_params(
    "entry,reason",
    values=[
        [[(0, 0, 4095), (1, 0, 4095)], "range"],
        [[(8, 0, 4095)], "range"],
        [[(0, 3000, 1000)], "range"],
        [[(0, 0, 4096)], "range"],
    ],
)
def test_digital_comparator_limits(fw, adc_cfg, entry, reason):
    pin = adc_cfg["inputs"][0]
    trigger = adc_cfg["trigger"]
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(0, 1, pins=[pin, pin], trigger=f"pwm{trigger['gen']}", dcmp=entry)
    assert error.value.reason == reason


@pytest.mark.ad3
@pytest.mark.board_params("order", "adc.priorities")
def test_two_sequencers_with_priorities(fw, ad3, need, adc_cfg, order):
    """Two synchronous sequencers of the same ADC with explicit priorities convert their own inputs."""
    first, second = adc_cfg["inputs"]
    volts = [apply_level(ad3, need, first, 0.6), apply_level(ad3, need, second, 2.7)]
    fw.adc.open(0, 1, pins=[first], prio=order[0], sync=True)
    fw.adc.open(0, 2, pins=[second], prio=order[1], sync=True)
    check_codes(fw.adc.measure(0, 1, n=adc_cfg["samples"]), volts[0], adc_cfg)
    check_codes(fw.adc.measure(0, 2, n=adc_cfg["samples"]), volts[1], adc_cfg)


@pytest.mark.ad3
def test_internal_reference(fw, ad3, need, adc_cfg):
    pin = adc_cfg["inputs"][0]
    volts = apply_level(ad3, need, pin, 2.0)
    trigger = start_trigger(fw, adc_cfg, sync=False)
    fw.adc.open(0, 3, pins=[pin], trigger=trigger, ref="int")
    check_codes(fw.adc.measure(0, 3, n=adc_cfg["samples"]), volts, adc_cfg)


@pytest.mark.ad3
@pytest.mark.xfail(strict=False, reason=EXTERNAL_REFERENCE_GAP)
def test_external_reference(fw, ad3, need, adc_cfg):
    """With `ref=ext` the codes follow VREFA+ (`tests.adc.external_reference_v`)."""
    reference = adc_cfg.get("external_reference_v")
    if reference is None:
        pytest.skip("set tests.adc.external_reference_v to the VREFA+ voltage")
    pin = adc_cfg["inputs"][0]
    volts = apply_level(ad3, need, pin, min(2.0, reference * 0.8))
    trigger = start_trigger(fw, adc_cfg, sync=False)
    fw.adc.open(0, 3, pins=[pin], trigger=trigger, ref="ext")
    check_codes(fw.adc.measure(0, 3, n=adc_cfg["samples"]), volts, adc_cfg, vref=reference)
