"""Quadrature encoder (`hal::tiva::QuadratureEncoder`) driven by the AD3 pattern generator.

Wiring set `qei`: A, B and index on DIOs. The pattern generator produces an exact number of 4-state
cycles (A leads B for `fwd`).
"""

import time

import pytest

from hal_ti_validation.expect import wrap_delta

pytestmark = pytest.mark.ad3


@pytest.fixture
def qei(board_cfg):
    return board_cfg.param("qei")


@pytest.fixture
def dios(qei, need):
    return {key: need.dio(qei[key]) for key in ("a", "b", "idx")}


def open_qei(fw, qei, **options):
    options.setdefault("res", qei["resolution"])
    fw.qei.open(qei["index"], a=qei["a"], b=qei["b"], idx=qei["idx"], **options)


def run(ad3, dios, frequency, cycles, direction="fwd", **options):
    ad3.pattern.quadrature(dios["a"], dios["b"], frequency, cycles, direction, **options)
    ad3.pattern.wait_done(timeout=cycles / frequency + 2)


@pytest.mark.board_params("frequency", "qei.frequencies_hz")
@pytest.mark.board_params("cycles", "qei.cycles")
@pytest.mark.board_params("direction", "qei.directions")
@pytest.mark.board_params("cap", "qei.capture")
def test_position_counts(fw, ad3, qei, dios, frequency, cycles, direction, cap):
    open_qei(fw, qei, cap=cap, reset="max")
    before = fw.qei.read(qei["index"]).pos
    run(ad3, dios, frequency, cycles, direction)
    after = fw.qei.read(qei["index"])
    per_cycle = qei["counts_per_cycle"][cap]
    expected = per_cycle * cycles * (1 if direction == "fwd" else -1)
    assert wrap_delta(after.pos - before, qei["resolution"]) == wrap_delta(expected, qei["resolution"])
    assert after.dir == direction


@pytest.mark.parametrize(("inva", "invb"), [(1, 0), (0, 1), (1, 1)])
def test_phase_inversion(fw, ad3, qei, dios, inva, invb):
    open_qei(fw, qei, inva=inva, invb=invb)
    before = fw.qei.read(qei["index"]).pos
    cycles = 10
    run(ad3, dios, 1000, cycles)
    after = fw.qei.read(qei["index"])
    reversed_direction = bool(inva) != bool(invb)
    expected = qei["counts_per_cycle"]["ab"] * cycles * (-1 if reversed_direction else 1)
    assert wrap_delta(after.pos - before, qei["resolution"]) == expected
    assert after.dir == ("rev" if reversed_direction else "fwd")


def test_offset_and_resolution_rollover(fw, ad3, qei, dios):
    resolution = 100
    open_qei(fw, qei, res=resolution, offset=50, reset="max")
    reading = fw.qei.read(qei["index"])
    assert reading.pos == 50
    assert reading.res in (resolution, resolution - 1)
    run(ad3, dios, 1000, 30)
    assert fw.qei.read(qei["index"]).pos == (50 + 4 * 30) % resolution


def test_index_resets_position(fw, ad3, qei, dios):
    every = qei["index_every"]
    extra = 5
    open_qei(fw, qei, reset="index")
    ad3.pattern.quadrature(dios["a"], dios["b"], 1000, 3 * every + extra, z=dios["idx"], index_every=every)
    ad3.pattern.wait_done(timeout=(3 * every + extra) / 1000 + 2)
    position = fw.qei.read(qei["index"]).pos
    assert wrap_delta(position - 4 * extra, qei["resolution"]) == pytest.approx(0, abs=qei["index_tolerance"])


@pytest.mark.board_params("frequency", "qei.speed_frequencies_hz")
@pytest.mark.board_params("velocity_us", "qei.velocity_us")
def test_speed(fw, ad3, qei, dios, frequency, velocity_us):
    open_qei(fw, qei, vel=velocity_us, cap="ab")
    ad3.pattern.quadrature(dios["a"], dios["b"], frequency, 0)
    time.sleep(3 * velocity_us / 1e6 + 0.05)
    speed = fw.qei.read(qei["index"]).speed
    ad3.pattern.stop()
    expected = qei["counts_per_cycle"]["ab"] * frequency * velocity_us / 1e6
    assert speed == pytest.approx(expected, rel=qei["speed_tolerance"], abs=1)


def test_clock_and_direction_mode(fw, ad3, qei, dios):
    open_qei(fw, qei, sig="clkdir")
    pulses = 50
    deltas = []
    for level in (0, 1):
        ad3.dio.drive(dios["b"], level)
        before = fw.qei.read(qei["index"]).pos
        ad3.pattern.pulses(dios["a"], pulses, 1000)
        ad3.pattern.wait_done(timeout=pulses / 1000 + 2)
        deltas.append(wrap_delta(fw.qei.read(qei["index"]).pos - before, qei["resolution"]))
    ad3.dio.release(dios["b"])
    assert deltas[0] == -deltas[1], f"direction levels must count opposite ways: {deltas}"
    assert abs(deltas[0]) == pulses * qei["clkdir_counts_per_pulse"]
