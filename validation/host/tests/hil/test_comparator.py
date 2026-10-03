"""Analog comparator (`hal::tiva::AnalogComparator` / `SynchronousAnalogComparator`).

Wiring set `bundle2`: W1 on the positive input, W2 on the negative input (a jumper ties the second comparator's
input to the same channel), the output pin on a DIO when the instance has one; this covers the comparators whose
positive input is C0+ (`src=c0`) too.
The output is high while VIN- < VIN+ (inverted with `invert=1`).
"""

from __future__ import annotations

import time

import pytest
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation import expect


@pytest.fixture
def comp_cfg(board_cfg):
    return board_cfg.param("comparator")


def settle():
    time.sleep(0.005)


def check_output(fw, ad3, need, index, out, pos_level, neg_level, invert):
    expected = int(pos_level > neg_level) ^ invert
    assert fw.comp.read(index) == expected
    if out is not None:
        assert ad3.dio.read(need.dio(out)) == expected, "output pin"


@pytest.mark.ad3
@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.matrix("comparator.output")
def test_output_follows_inputs(fw, ad3, need, comp_cfg, instance, pos_level, invert, sync):
    neg_level = comp_cfg["neg_level_v"]
    if abs(pos_level - neg_level) < 0.1:
        pytest.skip("inputs too close to compare reliably")
    ad3.wavegen.dc(need.wavegen(instance["pos"]), pos_level)
    ad3.wavegen.dc(need.wavegen(instance["neg"]), neg_level)
    settle()
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], src="pin", invert=invert, sync=sync)
    settle()
    check_output(fw, ad3, need, instance["index"], instance["out"], pos_level, neg_level, invert)


@pytest.mark.ad3
@pytest.mark.board_params("instance", "comparator.c0_instances")
@pytest.mark.matrix("comparator.output")
def test_positive_input_from_c0(fw, ad3, need, comp_cfg, instance, pos_level, invert, sync):
    """`src=c0`: the comparator compares its negative input against C0+."""
    neg_level = comp_cfg["neg_level_v"]
    if abs(pos_level - neg_level) < 0.1:
        pytest.skip("inputs too close to compare reliably")
    ad3.wavegen.dc(need.wavegen(instance["c0"]), pos_level)
    ad3.wavegen.dc(need.wavegen(instance["neg"]), neg_level)
    settle()
    fw.comp.open(instance["index"], neg=instance["neg"], out=instance["out"], src="c0", invert=invert, sync=sync)
    settle()
    check_output(fw, ad3, need, instance["index"], instance["out"], pos_level, neg_level, invert)


def find_threshold(fw, ad3, index, wavegen, low, high, step):
    """Lowest VIN- at which the output drops (binary search; output is 1 below the threshold)."""
    while high - low > step:
        middle = (low + high) / 2
        ad3.wavegen.dc(wavegen, middle)
        settle()
        if fw.comp.read(index):
            low = middle
        else:
            high = middle
    return (low + high) / 2


@pytest.mark.ad3
@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.matrix("comparator.ladder")
@pytest.mark.board_params("sync", values=[0, 1])
def test_internal_reference(fw, ad3, need, board_cfg, comp_cfg, instance, ref_range, step, sync):
    expected = expect.comparator_reference(comp_cfg["reference"], ref_range, step)
    low_limit, high_limit = board_cfg.ad3.analog_min, board_cfg.ad3.analog_max
    if not low_limit + 0.05 < expected < high_limit - 0.05:
        pytest.skip(f"reference {expected:.3f} V is outside the wavegen range")
    wavegen = need.wavegen(instance["neg"])
    ad3.wavegen.dc(wavegen, low_limit)
    fw.comp.open(instance["index"], neg=instance["neg"], src="ref", ref=(ref_range, step), sync=sync)
    threshold = find_threshold(fw, ad3, instance["index"], wavegen, low_limit, high_limit, comp_cfg["ref_search_step_v"])
    assert threshold == pytest.approx(expected, abs=comp_cfg["ref_tolerance_v"])


@pytest.mark.ad3
@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.board_params("trigger", "comparator.triggers")
@pytest.mark.board_params("sync", values=[0, 1])
def test_trigger_sense_is_accepted(fw, ad3, need, comp_cfg, instance, trigger, sync):
    """Every ADC trigger sense opens and leaves the comparison intact; the trigger itself is not observed
    because the driver gives it no observable effect."""
    neg_level = comp_cfg["neg_level_v"]
    ad3.wavegen.dc(need.wavegen(instance["neg"]), neg_level)
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], trigger=trigger, sync=sync)
    for pos_level in (neg_level - 0.4, neg_level + 0.4):
        ad3.wavegen.dc(need.wavegen(instance["pos"]), pos_level)
        settle()
        check_output(fw, ad3, need, instance["index"], instance["out"], pos_level, neg_level, 0)


@pytest.mark.ad3
@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.matrix("comparator.irq")
def test_interrupt_counts(fw, ad3, need, comp_cfg, instance, edge, cycles, invert):
    pos = need.wavegen(instance["pos"])
    low, high, frequency = comp_cfg["irq_low_v"], comp_cfg["irq_high_v"], comp_cfg["irq_frequency_hz"]
    ad3.wavegen.dc(need.wavegen(instance["neg"]), comp_cfg["neg_level_v"])
    ad3.wavegen.dc(pos, (low + high) / 2)
    settle()
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], src="pin", invert=invert, sync=False)
    fw.comp.irq(instance["index"], edge)
    fw.comp.count(instance["index"], clear=True)
    ad3.wavegen.square(pos, low, high, frequency, cycles=cycles)
    ad3.wavegen.wait_done(pos, timeout=cycles / frequency + 2)
    fw.system.delay(10)
    expected = {"rising": cycles, "falling": cycles, "both": 2 * cycles}[edge]
    assert fw.comp.count(instance["index"]) == expected
    fw.comp.irq(instance["index"], "off")
    ad3.wavegen.square(pos, low, high, frequency, cycles=3)
    ad3.wavegen.wait_done(pos, timeout=3 / frequency + 2)
    assert fw.comp.count(instance["index"]) == expected, "counting must stop after irq off"


def test_interrupt_restrictions(fw, comp_cfg):
    """Only edge interrupts exist, and only on the asynchronous driver."""
    instance = comp_cfg["instances"][0]
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], sync=False)
    for sense in ("high", "low"):
        with pytest.raises(FirmwareError) as error:
            fw.comp.irq(instance["index"], sense)
        assert error.value.reason == "usage", sense
    fw.comp.close(instance["index"])
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], sync=True)
    with pytest.raises(FirmwareError) as error:
        fw.comp.irq(instance["index"], "rising")
    assert error.value.reason == "unsupported"


def test_open_errors(fw, comp_cfg):
    instance = comp_cfg["instances"][0]
    index = instance["index"]
    cases = [
        ({"pos": instance["pos"]}, "usage"),
        ({"neg": instance["neg"], "src": "pin"}, "usage"),
        ({"neg": instance["neg"], "src": "pin", "ref": ("low", 4)}, "usage"),
        ({"neg": instance["neg"], "ref": ("low", 16)}, "range"),
        ({"pos": instance["pos"], "neg": instance["neg"], "trigger": "sometimes"}, "usage"),
    ]
    for options, reason in cases:
        with pytest.raises(FirmwareError) as error:
            fw.command("comp.open", index, **{key: fw.pin(value) if key in ("pos", "neg") else value for key, value in options.items()})
        assert error.value.reason == reason, options
