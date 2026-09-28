"""Analog comparator (`hal::tiva::AnalogComparator` / `SynchronousAnalogComparator`).

Wiring set `comparator`: W1 on the positive input, W2 on the negative input, the output pin on a DIO.
The output is high while VIN- < VIN+ (inverted with `invert=1`).
"""

import time

import pytest

from hal_ti_validation import expect

pytestmark = pytest.mark.ad3


@pytest.fixture
def comp_cfg(board_cfg):
    return board_cfg.param("comparator")


def settle():
    time.sleep(0.005)


@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.board_params("pos_level", "comparator.pos_levels_v")
@pytest.mark.parametrize("invert", [0, 1])
@pytest.mark.board_params("sync", "comparator.sync")
def test_output_follows_inputs(fw, ad3, need, comp_cfg, instance, pos_level, invert, sync):
    neg_level = comp_cfg["neg_level_v"]
    if abs(pos_level - neg_level) < 0.1:
        pytest.skip("inputs too close to compare reliably")
    ad3.wavegen.dc(need.wavegen(instance["pos"]), pos_level)
    ad3.wavegen.dc(need.wavegen(instance["neg"]), neg_level)
    settle()
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], src="pin", invert=invert, sync=sync)
    settle()
    expected = int(pos_level > neg_level) ^ invert
    assert fw.comp.read(instance["index"]) == expected
    assert ad3.dio.read(need.dio(instance["out"])) == expected, "output pin"


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


@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.board_params("range_name,step", "comparator.ref_steps")
def test_internal_reference(fw, ad3, need, board_cfg, comp_cfg, instance, range_name, step):
    expected = expect.comparator_reference(comp_cfg["reference"], range_name, step)
    low_limit, high_limit = board_cfg.ad3.analog_min, board_cfg.ad3.analog_max
    if not low_limit + 0.05 < expected < high_limit - 0.05:
        pytest.skip(f"reference {expected:.3f} V is outside the wavegen range")
    wavegen = need.wavegen(instance["neg"])
    ad3.wavegen.dc(wavegen, low_limit)
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], src="ref", ref=(range_name, step))
    threshold = find_threshold(fw, ad3, instance["index"], wavegen, low_limit, high_limit, comp_cfg["ref_search_step_v"])
    assert threshold == pytest.approx(expected, abs=comp_cfg["ref_tolerance_v"])


@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.board_params("edge", "comparator.irq.edges")
@pytest.mark.board_params("cycles", "comparator.irq.cycles")
def test_interrupt_counts(fw, ad3, need, comp_cfg, instance, edge, cycles):
    irq = comp_cfg["irq"]
    pos = need.wavegen(instance["pos"])
    ad3.wavegen.dc(need.wavegen(instance["neg"]), comp_cfg["neg_level_v"])
    ad3.wavegen.dc(pos, (irq["low_v"] + irq["high_v"]) / 2)
    settle()
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], src="pin")
    fw.comp.irq(instance["index"], edge)
    fw.comp.count(instance["index"], clear=True)
    ad3.wavegen.square(pos, irq["low_v"], irq["high_v"], irq["frequency_hz"], cycles=cycles)
    ad3.wavegen.wait_done(pos, timeout=cycles / irq["frequency_hz"] + 2)
    fw.system.delay(10)
    expected = {"rising": cycles, "falling": cycles, "both": 2 * cycles}[edge]
    assert fw.comp.count(instance["index"]) == expected
    fw.comp.irq(instance["index"], "off")
