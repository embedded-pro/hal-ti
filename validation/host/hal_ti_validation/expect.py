"""Expected values derived from TM4C hardware behaviour, used by the HIL tests (pure functions)."""

from __future__ import annotations

from collections.abc import Mapping
from typing import Any, Literal

PWM_LOAD_MAX = 0xFFFF


def pwm_load(pwmclk: float, frequency: float, mode: Literal["edge", "center"]) -> int:
    """Generator LOAD value: the counter counts down (edge) or up/down (center)."""
    return round(pwmclk / frequency / (2 if mode == "center" else 1))


def pwm_fits(pwmclk: float, frequency: float, mode: Literal["edge", "center"]) -> bool:
    return 2 <= pwm_load(pwmclk, frequency, mode) <= PWM_LOAD_MAX


def pwm_frequency(pwmclk: float, frequency: float, mode: Literal["edge", "center"]) -> float:
    """Frequency after LOAD quantisation."""
    return pwmclk / pwm_load(pwmclk, frequency, mode) / (2 if mode == "center" else 1)


def pwm_dead_time(dead_ns: float, pwmclk: float) -> float:
    """Dead-band delay after quantisation to PWM clock ticks, in seconds."""
    return round(dead_ns * 1e-9 * pwmclk) / pwmclk


def comparator_reference(reference: Mapping[str, Any], range_name: str, step: int) -> float:
    """Internal reference voltage from the board YAML `comparator.reference.<range>` offset/step."""
    entry = reference[range_name]
    return float(entry["offset_v"]) + step * float(entry["step_v"])


def wrap_delta(delta: int, modulus: int) -> int:
    """Signed difference of two positions on a counter that wraps at `modulus`, in (-modulus/2, modulus/2]."""
    delta %= modulus
    return delta - modulus if delta > modulus // 2 else delta


def uart_frame_bits(parity: str, stop_bits: int, data_bits: int = 8) -> int:
    return 1 + data_bits + (parity != "none") + stop_bits


def uart_transfer_time(size: int, baud: int, parity: str = "none", stop_bits: int = 1) -> float:
    return size * uart_frame_bits(parity, stop_bits) / baud


def max_hex_payload(max_command_length: int, command_prefix: str) -> int:
    """Largest byte count whose hex encoding still fits the firmware's command line."""
    return max(0, (max_command_length - len(command_prefix) - 1) // 2)
