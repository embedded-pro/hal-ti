"""Quadrature encoder (`hal::tiva::QuadratureEncoder`) driven by the AD3 pattern generator.

Wiring set `bundle1`: A, B and index of each `tests.qei.instances` entry on DIOs. The pattern generator produces
an exact number of 4-state cycles (A leads B for `fwd`).

Scenarios: features/qei.feature.
"""

from __future__ import annotations

import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when

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


def counts_forward(direction, inva, invb):
    reversed_direction = bool(inva) != bool(invb)
    return (direction == "fwd") != reversed_direction


@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.position")
@scenario("qei.feature", "The position moves by the counts of the capture mode, reversed by inverting exactly one phase")
def test_position_counts(instance, freq, cycles, direction, cap, inva, invb):
    pass


@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.board_params("inva,invb", values=[[1, 0], [0, 1], [1, 1]])
@scenario("qei.feature", "Inverted phases with the matching inversion count like the plain signal")
def test_inverted_inputs_restore_the_signal(instance, inva, invb):
    pass


@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.rollover")
@scenario("qei.feature", "The position starts at the offset and rolls over at the resolution")
def test_offset_and_resolution_rollover(instance, res, offset):
    pass


@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.index")
@scenario("qei.feature", "An index pulse restarts the position")
def test_index_resets_position(instance, direction, invi, every):
    pass


@pytest.mark.board_params("instance", "qei.instances")
@pytest.mark.matrix("qei.velocity")
@scenario("qei.feature", "The speed is the count rate over the velocity period")
def test_speed(instance, freq, vel):
    pass


@pytest.mark.board_params("instance", "qei.instances")
@scenario("qei.feature", "In clock and direction mode the B level sets the counting direction")
def test_clock_and_direction_mode(instance):
    pass


@scenario("qei.feature", "QEI0 opens on its default pins, other instances need A and B")
def test_default_pins_and_errors():
    pass


@given("the A, B and index pins of the instance are wired", target_fixture="dios")
def pins_wired(need, instance):
    return instance_dios(need, instance)


@given("the instance is open on its pins at the default resolution with the capture mode, the phase inversions and reset at the maximum")
def open_with_capture_and_inversions(fw, qei, instance, cap, inva, invb):
    open_qei(fw, qei, instance, cap=cap, inva=inva, invb=invb, reset="max")


@given("the instance is open on its pins at the default resolution with the phase inversions")
def open_with_inversions(fw, qei, instance, inva, invb):
    open_qei(fw, qei, instance, inva=inva, invb=invb)


@given("the instance is open on its pins at the resolution with the offset and reset at the maximum")
def open_with_resolution_and_offset(fw, qei, instance, res, offset):
    open_qei(fw, qei, instance, res=res, offset=offset, reset="max")


@given("the instance is open on its pins at the default resolution with the index inversion and reset on the index pulse")
def open_with_index_reset(fw, qei, instance, invi):
    open_qei(fw, qei, instance, reset="index", invi=invi)


@given(parsers.parse("the instance is open on its pins at the default resolution with the velocity period and {mode} capture"))
def open_with_velocity(fw, qei, instance, vel, mode):
    open_qei(fw, qei, instance, vel=vel, cap=mode)


@given("the instance is open on its pins at the default resolution in clock and direction mode")
def open_clock_and_direction(fw, qei, instance):
    open_qei(fw, qei, instance, sig="clkdir")


@given("the position is read", target_fixture="before")
def read_position(fw, instance):
    return fw.qei.read(instance["index"]).pos


@when("the pattern generator runs the cycles at the frequency in the direction")
def run_cycles(ad3, dios, freq, cycles, direction):
    run(ad3, dios, freq, cycles, direction)


@when(
    parsers.parse(
        "the pattern generator runs {n:d} {heading:w} cycles at {hz:d} Hz on the phases inverted as the instance inverts them, "
        "ending with both phases low"
    )
)
def run_inverted_cycles(ad3, dios, inva, invb, n, heading, hz):
    run_inverted(ad3, dios, hz, n, inva, invb, heading)


@when(parsers.parse("the pattern generator runs {n:d} {heading:w} cycles at {hz:d} Hz"), target_fixture="cycles_run")
def run_plain_cycles(ad3, dios, n, heading, hz):
    run(ad3, dios, hz, n, heading)
    return n


@when(
    parsers.parse(
        "the pattern generator runs {intervals:d} index intervals plus {extra:d} cycles at {hz:d} Hz in the direction, "
        "with an index pulse at the start of every index interval"
    ),
    target_fixture="cycles_run",
)
def run_with_index(ad3, dios, direction, every, intervals, extra, hz):
    total = intervals * every + extra
    ad3.pattern.quadrature(dios["a"], dios["b"], hz, total, direction, z=dios["idx"], index_every=every)
    ad3.pattern.wait_done(timeout=total / hz + 2)
    return total


@when("the pattern generator runs at the frequency until stopped")
def run_continuously(ad3, dios, freq):
    ad3.pattern.quadrature(dios["a"], dios["b"], freq, 0)


@when(parsers.parse("{periods:d} velocity periods plus {margin:g} s pass"))
def wait_velocity_periods(vel, periods, margin):
    time.sleep(periods * vel / 1e6 + margin)


@when("the speed is read", target_fixture="speed")
def read_speed(fw, instance):
    return fw.qei.read(instance["index"]).speed


@when("the pattern generator stops")
def stop_pattern(ad3):
    ad3.pattern.stop()


@when("the encoder is read", target_fixture="reading")
def read_encoder(fw, instance):
    return fw.qei.read(instance["index"])


@when(
    parsers.parse(
        "the position change modulo the default resolution over {pulses:d} pulses at {hz:d} Hz on A is measured "
        "with the AD3 driving B low, then high"
    ),
    target_fixture="deltas",
)
def measure_clock_and_direction(fw, ad3, qei, instance, dios, pulses, hz):
    deltas = []
    for level in (0, 1):
        ad3.dio.drive(dios["b"], level)
        before = fw.qei.read(instance["index"]).pos
        ad3.pattern.pulses(dios["a"], pulses, hz)
        ad3.pattern.wait_done(timeout=pulses / hz + 2)
        deltas.append(wrap_delta(fw.qei.read(instance["index"]).pos - before, qei["resolution"]))
    return deltas


@when("the AD3 releases B")
def release_b(ad3, dios):
    ad3.dio.release(dios["b"])


@when(parsers.parse("QEI {unit:d} is opened without pins and closed"))
def open_and_close_default(fw, unit):
    fw.qei.open(unit)
    fw.qei.close(unit)


@then(
    "the position moved by the counts per cycle of the capture mode times the cycles, modulo the default resolution, "
    "forwards for fwd and backwards for rev, the other way round when exactly one phase is inverted"
)
def moved_by_capture_counts(qei, cycles, direction, cap, inva, invb, before, reading):
    forward = counts_forward(direction, inva, invb)
    expected = qei["counts_per_cycle"][cap] * cycles * (1 if forward else -1)
    assert wrap_delta(reading.pos - before, qei["resolution"]) == wrap_delta(expected, qei["resolution"])


@then("the direction reads fwd when the position moved forwards, rev otherwise")
def direction_follows_inversion(direction, inva, invb, reading):
    forward = counts_forward(direction, inva, invb)
    assert reading.dir == ("fwd" if forward else "rev")


@then(parsers.parse("the position moved forwards by the counts per cycle of {mode} capture times {n:d}, modulo the default resolution"))
def moved_forwards(qei, before, reading, mode, n):
    assert wrap_delta(reading.pos - before, qei["resolution"]) == qei["counts_per_cycle"][mode] * n


@then("the direction reads fwd")
def direction_forwards(reading):
    assert reading.dir == "fwd"


@then("the position reads the offset and the encoder reports the resolution or one less")
def starts_at_offset(fw, instance, res, offset):
    reading = fw.qei.read(instance["index"])
    assert reading.pos == offset
    assert reading.res in (res, res - 1)


@then(parsers.parse("the position reads the offset plus {per:d} counts per cycle run, modulo the resolution"))
def rolled_over(fw, instance, res, offset, cycles_run, per):
    assert fw.qei.read(instance["index"]).pos == (offset + per * cycles_run) % res


@then(
    parsers.parse(
        "the position reads {per:d} counts per cycle since the last index pulse, negative for rev, "
        "within the index tolerance modulo the default resolution"
    )
)
def restarted_at_index(fw, qei, instance, direction, every, cycles_run, per):
    position = fw.qei.read(instance["index"]).pos
    # The pattern repeats every `every` cycles with the index at the start, so the last index can fall inside the extra cycles.
    expected = per * ((cycles_run - 1) % every + 1) * (1 if direction == "fwd" else -1)
    assert wrap_delta(position - expected, qei["resolution"]) == pytest.approx(0, abs=qei["index_tolerance"])


@then(
    parsers.parse(
        "the speed is the counts per cycle of {mode} capture times the frequency times the velocity period, "
        "within the speed tolerance or {slack:d}"
    )
)
def speed_matches(qei, freq, vel, speed, mode, slack):
    expected = qei["counts_per_cycle"][mode] * freq * vel / 1e6
    assert speed == pytest.approx(expected, rel=qei["speed_tolerance"], abs=slack)


@then("the two position changes are opposite")
def changes_opposite(deltas):
    assert deltas[0] == -deltas[1], f"direction levels must count opposite ways: {deltas}"


@then(parsers.parse("each position change is {pulses:d} times the counts per clock pulse"))
def changes_match_pulses(qei, deltas, pulses):
    assert abs(deltas[0]) == pulses * qei["clkdir_counts_per_pulse"]


@then(parsers.parse('opening QEI {unit:d} at resolution {at_res:d} with offset {at_offset:d} fails with "{refusal}"'))
def out_of_range_refused(fw, unit, at_res, at_offset, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.qei.open(unit, res=at_res, offset=at_offset)
    assert error.value.reason == refusal


@then(parsers.parse('opening each instance other than QEI {unit:d} without pins, then on its A pin only, fails with "{refusal}"'))
def other_instances_need_pins(fw, qei, unit, refusal):
    for instance in qei["instances"]:
        if instance["index"] != unit:
            with pytest.raises(FirmwareError) as error:
                fw.qei.open(instance["index"])
            assert error.value.reason == refusal
            with pytest.raises(FirmwareError) as error:
                fw.qei.open(instance["index"], a=instance["a"])
            assert error.value.reason == refusal
