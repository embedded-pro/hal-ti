import pytest

from hal_ti_validation import expect


def test_pwm_quantisation():
    assert expect.pwm_load(40e6, 20000, "center") == 1000
    assert expect.pwm_load(40e6, 20000, "edge") == 1999
    assert expect.pwm_frequency(40e6, 20000, "edge") == pytest.approx(20000)
    assert expect.pwm_frequency(40e6, 30000, "center") == pytest.approx(40e6 / 2 / 666)
    assert expect.pwm_fits(80e6, 1000, "center")
    assert not expect.pwm_fits(80e6, 1000, "edge")
    assert not expect.pwm_fits(80e6, 80e6, "center"), "LOAD 0"
    assert not expect.pwm_fits(1e6, 2e6, "edge"), "period 0"
    assert expect.pwm_duty_step(40e6, 20000, "center") == pytest.approx(0.1)
    assert expect.pwm_duty_step(40e6, 20000, "edge") == pytest.approx(0.05)


@pytest.mark.parametrize(
    "mode,duty,resolvable",
    [("edge", 90, False), ("edge", 50, True), ("edge", 12.5, True), ("center", 90, False), ("center", 50, True), ("center", 12.5, True)],
)
def test_pwm_duty_resolvable(mode, duty, resolvable):
    """120 MHz / 64 at 200 kHz: 9 counts per period, LOAD 8 edge-aligned and 4 center-aligned."""
    assert expect.pwm_duty_resolvable(120e6 / 64, 200000, mode, duty) is resolvable
    assert expect.pwm_duty_resolvable(120e6, 10000, mode, duty)


def test_pwm_divisor_and_dead_time():
    assert expect.pwm_divisor_for(80e6, 1000, "edge") == 2
    assert expect.pwm_divisor_for(80e6, 1000, "center") == 1
    assert expect.pwm_divisor_for(120e6, 100, "edge") == 32
    with pytest.raises(ValueError):
        expect.pwm_divisor_for(80e6, 1, "edge")
    assert expect.pwm_dead_time(1000, 40e6) == pytest.approx(1e-6)
    assert expect.pwm_dead_time(260, 40e6) == pytest.approx(10 / 40e6)
    assert expect.pwm_dead_clocks(51200, 80e6) == 4096
    assert not expect.pwm_dead_fits(51200, 80e6)
    assert expect.pwm_dead_fits(1_000_000, 80e6 / 64)
    assert not expect.pwm_dead_fits(1_000_001, 80e6 / 64)


@pytest.mark.parametrize(
    ("source", "mode", "events"),
    [
        ("none", "edge", 0),
        ("zero", "edge", 1),
        ("load", "center", 1),
        ("cmpau", "edge", 0),
        ("cmpau", "center", 1),
        ("cmpbd", "edge", 1),
        ("cmpbu", "edge", 0),
    ],
)
def test_pwm_events_per_period(source, mode, events):
    assert expect.pwm_events_per_period(source, mode) == events


def test_comparator_reference():
    reference = {"low": {"offset_v": 0.731, "step_v": 0.0968}, "high": {"offset_v": 0.0, "step_v": 0.1375}}
    assert expect.comparator_reference(reference, "low", 0) == pytest.approx(0.731)
    assert expect.comparator_reference(reference, "high", 15) == pytest.approx(2.0625)


@pytest.mark.parametrize(("delta", "modulus", "wrapped"), [(10, 100, 10), (-10, 100, -10), (95, 100, -5), (50, 100, 50), (-60, 100, 40)])
def test_wrap_delta(delta, modulus, wrapped):
    assert expect.wrap_delta(delta, modulus) == wrapped


def test_can_uart_adc_and_payload_limits():
    assert expect.can_timing_bitrate(80e6, 12, 3, 1, 10) == pytest.approx(500000)
    assert expect.can_timing_bitrate(120e6, 11, 3, 4, 8) == pytest.approx(1e6)
    assert expect.adc_steps_per_run(8, 3) == 5
    assert expect.uart_frame_bits("even", 2) == 12
    assert expect.uart_transfer_time(10, 1000) == pytest.approx(0.1)
    assert expect.max_hex_payload(255, "uart.send 1") == 121
