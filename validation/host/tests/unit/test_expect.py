import pytest

from hal_ti_validation import expect


def test_pwm_quantisation():
    assert expect.pwm_load(40e6, 20000, "center") == 1000
    assert expect.pwm_load(40e6, 20000, "edge") == 2000
    assert expect.pwm_frequency(40e6, 30000, "center") == pytest.approx(40e6 / 2 / 667)
    assert expect.pwm_fits(80e6, 1000, "center")
    assert not expect.pwm_fits(80e6, 1000, "edge")
    assert expect.pwm_dead_time(1000, 40e6) == pytest.approx(1e-6)
    assert expect.pwm_dead_time(260, 40e6) == pytest.approx(10 / 40e6)


def test_comparator_reference():
    reference = {"low": {"offset_v": 0.731, "step_v": 0.0968}, "high": {"offset_v": 0.0, "step_v": 0.1375}}
    assert expect.comparator_reference(reference, "low", 0) == pytest.approx(0.731)
    assert expect.comparator_reference(reference, "high", 15) == pytest.approx(2.0625)


@pytest.mark.parametrize(("delta", "modulus", "wrapped"), [(10, 100, 10), (-10, 100, -10), (95, 100, -5), (50, 100, 50), (-60, 100, 40)])
def test_wrap_delta(delta, modulus, wrapped):
    assert expect.wrap_delta(delta, modulus) == wrapped


def test_uart_and_payload_limits():
    assert expect.uart_frame_bits("even", 2) == 12
    assert expect.uart_transfer_time(10, 1000) == pytest.approx(0.1)
    assert expect.max_hex_payload(255, "uart.send 1") == 121
