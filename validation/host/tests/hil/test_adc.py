"""ADC (`hal::tiva::Adc` / `SynchronousAdc`): wavegen DC levels against raw 12-bit codes.

Wiring set `adc`: W1/W2 on the analog pins used below; the scope on the same pins (optional) measures the
actual level, which then replaces the programmed one as reference.
"""

import time

import pytest

from hal_ti_validation import analysis
from hal_ti_validation.terminal import FirmwareError

pytestmark = pytest.mark.ad3


@pytest.fixture
def adc_cfg(board_cfg):
    return board_cfg.param("adc")


def apply_level(ad3, need, pin, volts):
    """Drive `pin` to `volts`; returns the measured level (scope) or the programmed one."""
    ad3.wavegen.dc(need.wavegen(pin), volts)
    time.sleep(0.02)
    scope = need.optional_scope(pin)
    return ad3.scope.average(scope) if scope is not None else volts


def start_trigger(fw, board_cfg, sync):
    """Asynchronous sequencers convert only on their PWM trigger: run e-foc's PWM for them."""
    if not sync:
        module = board_cfg.param("pwm.module")
        fw.pwm.open(module)
        fw.pwm.duty(module, 50)


def runs(adc_cfg, steps):
    """`adc.measure` returns at most 64 values."""
    return max(1, min(adc_cfg["samples"], 64 // steps))


def check_codes(samples, volts, adc_cfg):
    expected = analysis.adc_code(volts, adc_cfg["vref"], adc_cfg["bits"])
    result = analysis.stats(samples)
    assert result.mean == pytest.approx(expected, abs=adc_cfg["tolerance_codes"]), f"{volts:.3f} V: {result}"
    assert result.maximum - result.minimum <= 2 * adc_cfg["tolerance_codes"], f"noisy samples {result}"


@pytest.mark.board_params("entry", "adc.single")
@pytest.mark.board_params("level", "adc.levels_v")
@pytest.mark.board_params("sync", "adc.sync")
def test_dc_levels(fw, ad3, need, board_cfg, adc_cfg, entry, level, sync):
    volts = apply_level(ad3, need, entry["pin"], level)
    start_trigger(fw, board_cfg, sync)
    fw.adc.open(entry["adc"], entry["seq"], pins=[entry["pin"]], sync=sync)
    check_codes(fw.adc.measure(entry["adc"], entry["seq"], n=adc_cfg["samples"]), volts, adc_cfg)


@pytest.mark.board_params("entry", "adc.single")
@pytest.mark.board_params("sh", "adc.sample_hold")
@pytest.mark.board_params("avg", "adc.oversampling")
def test_sample_hold_and_oversampling(fw, ad3, need, adc_cfg, entry, sh, avg):
    volts = apply_level(ad3, need, entry["pin"], 1.65)
    fw.adc.open(entry["adc"], entry["seq"], pins=[entry["pin"]], sh=sh, avg=avg, sync=True)
    check_codes(fw.adc.measure(entry["adc"], entry["seq"], n=adc_cfg["samples"]), volts, adc_cfg)


@pytest.mark.board_params("entry", "adc.single")
def test_delay_option(fw, ad3, need, board_cfg, adc_cfg, entry):
    volts = apply_level(ad3, need, entry["pin"], 2.0)
    start_trigger(fw, board_cfg, False)
    fw.adc.open(entry["adc"], entry["seq"], pins=[entry["pin"]], delay=4, sync=False)
    check_codes(fw.adc.measure(entry["adc"], entry["seq"], n=adc_cfg["samples"]), volts, adc_cfg)


@pytest.mark.board_params("sync", "adc.sync")
def test_multi_pin_sequence(fw, ad3, need, board_cfg, adc_cfg, sync):
    sequence = adc_cfg["sequence"]
    pins = sequence["pins"]
    levels = [0.8, 2.4, 1.6, 0.4][: len(pins)]
    volts = [apply_level(ad3, need, pin, level) for pin, level in zip(pins, levels)]
    start_trigger(fw, board_cfg, sync)
    fw.adc.open(sequence["adc"], sequence["seq"], pins=pins, sync=sync)
    samples = fw.adc.measure(sequence["adc"], sequence["seq"], n=runs(adc_cfg, len(pins)))
    for channel, expected in zip(analysis.deinterleave(samples, len(pins)), volts):
        check_codes(channel, expected, adc_cfg)


def test_efoc_phase_sequencer_is_pwm_triggered(fw, ad3, need, adc_cfg, board_cfg):
    phase = adc_cfg["efoc_phase"]
    volts = apply_level(ad3, need, phase["driven"], 1.2)
    driven = board_cfg.resolve_pin(phase["driven"])
    position = [board_cfg.resolve_pin(pin) for pin in phase["pins"]].index(driven)
    module = board_cfg.param("pwm.module")
    fw.pwm.open(module)
    fw.pwm.duty(module, 50, 50, 50)
    fw.adc.open(0, 0)
    samples = fw.adc.measure(0, 0, n=runs(adc_cfg, len(phase["pins"])))
    check_codes(analysis.deinterleave(samples, len(phase["pins"]))[position], volts, adc_cfg)
    fw.pwm.close(module)
    with pytest.raises(FirmwareError) as error:
        fw.adc.measure(0, 0, n=1, cmd_timeout=5.0)
    assert error.value.reason == "timeout", "the PWM-triggered sequencer converted without its trigger"


def test_efoc_supply_sequencer(fw, ad3, need, adc_cfg, board_cfg):
    supply = adc_cfg["efoc_supply"]
    volts = apply_level(ad3, need, supply["driven"], 2.2)
    fw.adc.open(1, 0)
    samples = fw.adc.measure(1, 0, n=runs(adc_cfg, len(supply["pins"])))
    position = [board_cfg.resolve_pin(pin) for pin in supply["pins"]].index(board_cfg.resolve_pin(supply["driven"]))
    check_codes(analysis.deinterleave(samples, len(supply["pins"]))[position], volts, adc_cfg)
