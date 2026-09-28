"""Quadrature encoder (`hal::tiva::QuadratureEncoder`) driven by the AD3 pattern generator.

Wiring set `qei`: A, B and index of each `tests.qei.instances` entry on DIOs (`--with qei1` adds QEI1 on the
TM4C123). The pattern generator produces an exact number of 4-state cycles (A leads B for `fwd`).
"""

from __future__ import annotations

import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation.expect import wrap_delta


@pytest.fixture
def qei(board_cfg):
    return board_cfg.param("qei")


def instance_dios(need, instance):
    return {key: need.dio(instance[key]) for key in ("a", "b", "idx")}


def open_qei(fw, qei, instance, **options):
    options.setdefault("res", qei["resolution"])
    fw.qei.open(instance["index"], a=instance["a"], b=instance["b"], idx=instance["idx"], **options)


def run(ad3, dios, frequency, cycles, direction="fwd", **options):
    ad3.pattern.quadrature(dios["a"], dios["b"], frequency, cycles, direction, **options)
    ad3.pattern.wait_done(timeout=cycles / frequency + 2)


def run_inverted(ad3, dios, frequency, cycles, invert_a, invert_b, direction="fwd"):
    """Quadrature with physically inverted phases, rotated so the run ends in the idle (low, low) state."""
    pattern = analysis.quadrature_pattern(1, direction)
    states = [(a ^ invert_a, b ^ invert_b) for a, b in zip(pattern["a"], pattern["b"])]
    while states[-1] != (0, 0):
        states = states[1:] + states[:1]
    ad3.pattern.custom({dios["a"]: [a for a, _ in states], dios["b"]: [b for _, b in states]}, frequency * 4, run_samples=4 * cycles)
    ad3.pattern.wait_done(timeout=cycles / frequency + 2)


@pytest.mark.ad3
@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.position")
def test_position_counts(fw, ad3, need, qei, instance, freq, cycles, direction, cap, inva, invb):
    """Counts per cycle follow the capture mode; inverting exactly one phase reverses the direction."""
    dios = instance_dios(need, instance)
    open_qei(fw, qei, instance, cap=cap, inva=inva, invb=invb, reset="max")
    before = fw.qei.read(instance["index"]).pos
    run(ad3, dios, freq, cycles, direction)
    after = fw.qei.read(instance["index"])
    reversed_direction = bool(inva) != bool(invb)
    forward = (direction == "fwd") != reversed_direction
    expected = qei["counts_per_cycle"][cap] * cycles * (1 if forward else -1)
    assert wrap_delta(after.pos - before, qei["resolution"]) == wrap_delta(expected, qei["resolution"])
    assert after.dir == ("fwd" if forward else "rev")


@pytest.mark.ad3
@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.board_params("inva,invb", values=[[1, 0], [0, 1], [1, 1]])
def test_inverted_inputs_restore_the_signal(fw, ad3, need, qei, instance, inva, invb):
    """Physically inverted phases with the matching `inva`/`invb` count like the plain signal."""
    dios = instance_dios(need, instance)
    open_qei(fw, qei, instance, inva=inva, invb=invb)
    before = fw.qei.read(instance["index"]).pos
    cycles = 25
    run_inverted(ad3, dios, 1000, cycles, inva, invb)
    after = fw.qei.read(instance["index"])
    assert wrap_delta(after.pos - before, qei["resolution"]) == qei["counts_per_cycle"]["ab"] * cycles
    assert after.dir == "fwd"


@pytest.mark.ad3
@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.rollover")
def test_offset_and_resolution_rollover(fw, ad3, need, qei, instance, res, offset):
    dios = instance_dios(need, instance)
    open_qei(fw, qei, instance, res=res, offset=offset, reset="max")
    reading = fw.qei.read(instance["index"])
    assert reading.pos == offset
    assert reading.res in (res, res - 1)
    cycles = 30
    run(ad3, dios, 1000, cycles)
    assert fw.qei.read(instance["index"]).pos == (offset + 4 * cycles) % res


@pytest.mark.ad3
@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.index")
def test_index_resets_position(fw, ad3, need, qei, instance, direction, invi, every):
    """With `reset=index` the position restarts on every index pulse (`invi` moves the reset to the pulse's
    other edge, within the tolerance)."""
    dios = instance_dios(need, instance)
    extra = 5
    cycles = 3 * every + extra
    open_qei(fw, qei, instance, reset="index", invi=invi)
    ad3.pattern.quadrature(dios["a"], dios["b"], 1000, cycles, direction, z=dios["idx"], index_every=every)
    ad3.pattern.wait_done(timeout=cycles / 1000 + 2)
    position = fw.qei.read(instance["index"]).pos
    expected = 4 * extra * (1 if direction == "fwd" else -1)
    assert wrap_delta(position - expected, qei["resolution"]) == pytest.approx(0, abs=qei["index_tolerance"])


@pytest.mark.ad3
@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.velocity")
def test_speed(fw, ad3, need, qei, instance, freq, vel):
    dios = instance_dios(need, instance)
    open_qei(fw, qei, instance, vel=vel, cap="ab")
    ad3.pattern.quadrature(dios["a"], dios["b"], freq, 0)
    time.sleep(3 * vel / 1e6 + 0.05)
    speed = fw.qei.read(instance["index"]).speed
    ad3.pattern.stop()
    expected = qei["counts_per_cycle"]["ab"] * freq * vel / 1e6
    assert speed == pytest.approx(expected, rel=qei["speed_tolerance"], abs=1)


@pytest.mark.ad3
@pytest.mark.board_params("instance", "qei.instances")
def test_clock_and_direction_mode(fw, ad3, need, qei, instance):
    dios = instance_dios(need, instance)
    open_qei(fw, qei, instance, sig="clkdir")
    pulses = 50
    deltas = []
    for level in (0, 1):
        ad3.dio.drive(dios["b"], level)
        before = fw.qei.read(instance["index"]).pos
        ad3.pattern.pulses(dios["a"], pulses, 1000)
        ad3.pattern.wait_done(timeout=pulses / 1000 + 2)
        deltas.append(wrap_delta(fw.qei.read(instance["index"]).pos - before, qei["resolution"]))
    ad3.dio.release(dios["b"])
    assert deltas[0] == -deltas[1], f"direction levels must count opposite ways: {deltas}"
    assert abs(deltas[0]) == pulses * qei["clkdir_counts_per_pulse"]


def test_default_pins_and_errors(fw, qei):
    """QEI0 opens on its default pins without arguments; other instances need `a` and `b`."""
    fw.qei.open(0)
    fw.qei.close(0)
    with pytest.raises(FirmwareError) as error:
        fw.qei.open(0, res=100, offset=100)
    assert error.value.reason == "range"
    for instance in qei["instances"]:
        if instance["index"] != 0:
            with pytest.raises(FirmwareError) as error:
                fw.qei.open(instance["index"])
            assert error.value.reason == "usage"
            with pytest.raises(FirmwareError) as error:
                fw.qei.open(instance["index"], a=instance["a"])
            assert error.value.reason == "usage"
