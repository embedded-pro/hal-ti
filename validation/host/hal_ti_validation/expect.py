"""Expected values derived from TM4C hardware behaviour, used by the HIL tests (pure functions)."""

from __future__ import annotations

import math
from collections.abc import Mapping
from typing import Any, Literal

PWM_LOAD_MAX = 0xFFFF
PWM_DEAD_MAX_CLOCKS = 4095
PWM_DEAD_MAX_NS = 1_000_000

PwmMode = Literal["edge", "center"]


def pwm_period_clocks(pwmclk: float, frequency: float) -> int:
    return int(pwmclk // frequency)


def pwm_load(pwmclk: float, frequency: float, mode: PwmMode) -> int:
    """Generator LOAD as the driver writes it: period - 1 when counting down (edge), period / 2 up/down (center)."""
    period = pwm_period_clocks(pwmclk, frequency)
    return period // 2 if mode == "center" else period - 1


def pwm_fits(pwmclk: float, frequency: float, mode: PwmMode) -> bool:
    return pwm_period_clocks(pwmclk, frequency) > 0 and 1 <= pwm_load(pwmclk, frequency, mode) <= PWM_LOAD_MAX


def pwm_frequency(pwmclk: float, frequency: float, mode: PwmMode) -> float:
    """Frequency after LOAD quantisation."""
    load = pwm_load(pwmclk, frequency, mode)
    return pwmclk / (2 * load if mode == "center" else load + 1)


def pwm_duty_step(pwmclk: float, frequency: float, mode: PwmMode) -> float:
    """Duty resolution in percent: one comparator count of the period."""
    load = pwm_load(pwmclk, frequency, mode)
    return 100 / (load if mode == "center" else load + 1)


def pwm_duty_resolvable(pwmclk: float, frequency: float, mode: PwmMode, duty: float) -> bool:
    """Whether a duty between 0 and 100 % keeps both edges: the driver rounds it to whole counts
    (`DutyCycle::ToCounts`) and drives a static level when that reaches 0 or LOAD."""
    load = pwm_load(pwmclk, frequency, mode)
    counts = math.floor((load if mode == "center" else load + 1) * duty / 100 + 0.5)
    return 0 < counts < load


PWM_DIVISORS = (1, 2, 4, 8, 16, 32, 64)


def pwm_divisor_for(sysclk: float, frequency: float, mode: PwmMode) -> int:
    """Smallest PWM clock divisor whose LOAD register holds `frequency`."""
    for divisor in PWM_DIVISORS:
        if pwm_fits(sysclk // divisor, frequency, mode):
            return divisor
    raise ValueError(f"{frequency} Hz does not fit any PWM clock divisor")


def pwm_dead_clocks(dead_ns: float, pwmclk: float) -> int:
    return int(dead_ns * pwmclk // 1_000_000_000)


def pwm_dead_time(dead_ns: float, pwmclk: float) -> float:
    """Dead-band delay after truncation to PWM clock ticks, in seconds."""
    return pwm_dead_clocks(dead_ns, pwmclk) / pwmclk


def pwm_dead_fits(dead_ns: float, pwmclk: float) -> bool:
    return dead_ns <= PWM_DEAD_MAX_NS and pwm_dead_clocks(dead_ns, pwmclk) <= PWM_DEAD_MAX_CLOCKS


def pwm_events_per_period(source: str, mode: PwmMode) -> int:
    """Generator events per PWM period for a 0 < duty < 100 % output: a down counter (edge mode) never matches
    a comparator while counting up."""
    if source == "none":
        return 0
    if source in ("cmpau", "cmpbu"):
        return 1 if mode == "center" else 0
    return 1


def comparator_reference(reference: Mapping[str, Any], range_name: str, step: int) -> float:
    """Internal reference voltage from the board YAML `comparator.reference.<range>` offset/step."""
    entry = reference[range_name]
    return float(entry["offset_v"]) + step * float(entry["step_v"])


def wrap_delta(delta: int, modulus: int) -> int:
    """Signed difference of two positions on a counter that wraps at `modulus`, in (-modulus/2, modulus/2]."""
    delta %= modulus
    return delta - modulus if delta > modulus // 2 else delta


def can_timing_bitrate(sysclk: float, tseg1: int, tseg2: int, sjw: int, brp: int) -> float:
    """C_CAN bit rate of an explicit `timing=<tseg1>,<tseg2>,<sjw>,<brp>`; `sjw` does not change it."""
    return sysclk / (brp * (1 + tseg1 + tseg2))


def adc_steps_per_run(steps: int, comparators: int) -> int:
    """`adc.measure` values per sequence run: steps routed to digital comparators do not reach the FIFO."""
    return steps - comparators


def uart_frame_bits(parity: str, stop_bits: int, data_bits: int = 8) -> int:
    return 1 + data_bits + (parity != "none") + stop_bits


def uart_transfer_time(size: int, baud: int, parity: str = "none", stop_bits: int = 1) -> float:
    return size * uart_frame_bits(parity, stop_bits) / baud


def max_hex_payload(max_command_length: int, command_prefix: str) -> int:
    """Largest byte count whose hex encoding still fits the firmware's command line."""
    return max(0, (max_command_length - len(command_prefix) - 1) // 2)
