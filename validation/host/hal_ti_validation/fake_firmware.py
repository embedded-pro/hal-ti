"""Offline double of the validation firmware: `FakeFirmware` answers the PROTOCOL.md commands behind the generic
`FakeTerminalDevice` (echo, prompt, `EVT` lines); connect it with `FakeSerial`.

It validates arguments like the firmware (`usage`, `range`, `pin`, `unsupported`, `busy`, `notopen`) and models
what needs no hardware: pin ownership, open instances, EEPROM contents, PWM interrupt counts and ADC triggers
over time, CAN loopback with the acceptance filter, watchdog warnings and resets, and the board alias table.
Measured signals (DIO levels, analog codes, UART/SPI peers, faults) are not emulated.
"""

from __future__ import annotations

import re
import time
from collections.abc import Callable, Mapping
from dataclasses import dataclass, field
from typing import Any

from ad3_waveforms_bench.fake_terminal import FakeSerial as _FakeSerial
from ad3_waveforms_bench.fake_terminal import FakeTerminalDevice, Handler
from ad3_waveforms_bench.protocol import ProtocolError, format_hex, parse_hex, parse_number

__all__ = ["FakeFirmware", "FakeSerial", "TM4C123_PINS", "TM4C129_PINS"]

TM4C123_PINS: dict[str, str] = {
    "terminaltx": "PA1",
    "terminalrx": "PA0",
    "ain0": "PE3",
    "ain1": "PE2",
    "ain2": "PE1",
    "ain3": "PE0",
    "m0pwm0": "PB6",
    "m0pwm1": "PB7",
    "m0pwm2": "PB4",
    "m0pwm3": "PB5",
    "m0pwm4": "PE4",
    "m0pwm5": "PE5",
    "qei0a": "PD6",
    "qei0b": "PD7",
    "qei0idx": "PD3",
    "can0rx": "PF0",
    "can0tx": "PF3",
    "led0": "PF1",
    "gpio0": "PA2",
    "gpio1": "PA4",
    "gpio2": "PA5",
    "gpio3": "PA6",
}

TM4C129_PINS: dict[str, str] = {
    "terminaltx": "PD5",
    "terminalrx": "PD4",
    "ain0": "PE3",
    "ain1": "PE2",
    "ain2": "PE1",
    "ain10": "PB4",
    "ain11": "PB5",
    "m0pwm2": "PF2",
    "m0pwm3": "PF3",
    "m0pwm4": "PG0",
    "m0pwm5": "PG1",
    "m0pwm6": "PK4",
    "m0pwm7": "PK5",
    "qei0a": "PL1",
    "qei0b": "PL2",
    "qei0idx": "PL3",
    "can0rx": "PA0",
    "can0tx": "PA1",
    "led0": "PN3",
    "led1": "PN2",
    "led2": "PP2",
    "gpio0": "PN4",
    "gpio1": "PE4",
    "gpio2": "PE5",
    "gpio3": "PK0",
    "gpio4": "PK1",
    "gpio5": "PK2",
    "gpio6": "PC6",
}


@dataclass(frozen=True)
class _Family:
    ports: str
    terminal_uart: int
    pwm_modules: int
    qeis: int
    comparators: int
    default_uart: tuple[int, str, str] | None
    fault_pins: dict[str, int]
    ethernet: bool


_FAMILIES = {
    "tm4c123": _Family("ABCDEF", 0, 2, 2, 2, (1, "PB1", "PB0"), {"PD2": 0, "PD6": 0, "PF2": 0, "PF4": 1}, False),
    "tm4c129": _Family("ABCDEFGHJKLMNPQ", 2, 1, 1, 3, None, {"PF4": 0, "PK6": 0, "PK7": 0, "PL0": 0}, True),
}

# PWM channel pins per family and module (pinout table order, the first one is the default).
_PWM_PINS: dict[str, dict[int, tuple[tuple[str, ...], ...]]] = {
    "tm4c123": {
        0: (("PB6",), ("PB7",), ("PB4",), ("PB5",), ("PE4",), ("PE5",), ("PC4", "PD0"), ("PC5", "PD1")),
        1: (("PD0",), ("PD1",), ("PA6", "PE4"), ("PA7", "PE5"), ("PF0",), ("PF1",), ("PF2",), ("PF3",)),
    },
    "tm4c129": {0: (("PF0",), ("PF1",), ("PF2",), ("PF3",), ("PG0",), ("PG1",), ("PK4",), ("PK5",))},
}

_UART_BAUDS = frozenset({600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600})
_PWM_SOURCES = ("none", "zero", "load", "cmpau", "cmpad", "cmpbu", "cmpbd")
_PWM_DIVS = (1, 2, 4, 8, 16, 32, 64)
_SEQUENCER_DEPTHS = (8, 4, 4, 1)
_OPEN_LIMITS = {"pwm": 1, "uart": 1, "spi": 1, "adc": 2, "comp": 1, "qei": 1, "can": 1, "wdt": 1, "gpio": 8}
_PIN_RE = re.compile(r"^P([A-T])([0-7])$", re.IGNORECASE)


class FakeSerial(_FakeSerial):
    """`FakeSerial` that lets the device produce time-driven output (watchdog warnings and resets) on reads."""

    @property
    def in_waiting(self) -> int:
        self._poll()
        return super().in_waiting

    def read(self, size: int = 1) -> bytes:
        self._poll()
        return super().read(size)

    def write(self, data: bytes) -> int:
        self._poll()
        return super().write(data)

    def _poll(self) -> None:
        poll = getattr(self.device, "poll", None)
        if poll is not None:
            poll()


class _Error(Exception):
    def __init__(self, reason: str) -> None:
        super().__init__(reason)
        self.reason = reason


def _fail(reason: str) -> None:
    raise _Error(reason)


def _number(text: str | None, low: int = 0, high: int = 2**32 - 1) -> int:
    if text is None:
        _fail("usage")
    try:
        value = parse_number(text)
    except ProtocolError:
        raise _Error("usage") from None
    if not low <= value <= high:
        _fail("range")
    return value


def _flag(options: Mapping[str, str], key: str, default: bool = False) -> bool:
    if key not in options:
        return default
    if options[key] not in ("0", "1"):
        _fail("usage")
    return options[key] == "1"


def _choice(options: Mapping[str, str], key: str, choices: tuple[str, ...], default: str) -> str:
    value = options.get(key, default)
    if value not in choices:
        _fail("usage")
    return value


def _keys(options: Mapping[str, str], allowed: tuple[str, ...]) -> None:
    if any(key not in allowed for key in options):
        _fail("usage")


def _positionals(args: list[str], low: int, high: int | None = None) -> None:
    if not low <= len(args) <= (low if high is None else high):
        _fail("usage")


@dataclass
class FakeFirmware(FakeTerminalDevice):
    """Emulates the validation firmware behind `services::HilTerminal`; `style`, `noise` and `emit()` come from
    `FakeTerminalDevice`. `pins` defaults to the alias table of the family."""

    board: str = "ek_tm4c123gxl"
    family: str = "tm4c123"
    sysclk: int = 80_000_000
    pins: dict[str, str] | None = None
    eeprom_size: int = 2048
    gpio_levels: dict[str, int] = field(default_factory=dict)
    gpio_counts: dict[str, int] = field(default_factory=dict)
    adc_codes: dict[str, int] = field(default_factory=dict)
    uart_rx: dict[int, bytearray] = field(default_factory=dict)
    spi_miso: int = 0x00
    opened: dict[tuple[str, str], dict[str, Any]] = field(default_factory=dict)
    reset_cause: str = "por"
    clock: Callable[[], float] = time.monotonic
    sleep: Callable[[float], None] = time.sleep

    def __post_init__(self) -> None:
        if self.family not in _FAMILIES:
            raise ValueError(f"unknown family {self.family!r}")
        if self.pins is None:
            self.pins = dict(TM4C123_PINS if self.family == "tm4c123" else TM4C129_PINS)
        self.eeprom = bytearray(b"\xff" * self.eeprom_size)
        self.claims: dict[str, tuple[str, str]] = {}
        self.watchdog: dict[str, Any] | None = None
        super().__post_init__()

    @property
    def spec(self) -> _Family:
        return _FAMILIES[self.family]

    def boot(self) -> None:
        self.opened.clear()
        self.claims = {}
        self.watchdog = None
        super().boot()

    def event(self, line: str) -> None:
        """Print an asynchronous `EVT` line now, as a complete line whatever the output style."""
        self._write(f"\r\n{line}\r\n")

    def boot_message(self) -> str:
        return f"EVT boot board={self.board} family={self.family} sysclk={self.sysclk} reset={self.reset_cause}"

    def lookup(self, name: str) -> Handler | None:
        handler = self.handlers.get(name)
        if handler is not None:
            return handler
        method = getattr(self, "_cmd_" + name.replace(".", "_"), None)
        if method is None:
            return None

        def run(device: FakeTerminalDevice, args: list[str], options: dict[str, str]) -> str | list[str] | None:
            try:
                return method(args, options)
            except _Error as error:
                return f"ERR {error.reason}"

        return run

    # pins and instances

    def pin(self, text: str | None) -> str | None:
        if text is None:
            return None
        match = _PIN_RE.match(text)
        if match is None:
            resolved = (self.pins or {}).get(text.lower())
            if resolved is None:
                _fail("pin")
            match = _PIN_RE.match(resolved)
            assert match is not None
        port, index = match.group(1).upper(), match.group(2)
        if port not in self.spec.ports:
            _fail("pin")
        return f"P{port}{index}"

    def _reserved(self) -> set[str]:
        return {pin for alias, pin in (self.pins or {}).items() if alias in ("terminaltx", "terminalrx")}

    def _check_pins(self, owner: tuple[str, str], pins: list[str | None], analog: bool = False) -> None:
        for pin in pins:
            if pin is None:
                continue
            if pin in self._reserved():
                _fail("busy")
            holder = self.claims.get(pin)
            if holder is not None and holder != owner and not (analog and holder[0] == "adc"):
                _fail("busy")

    def _claim(self, owner: tuple[str, str], pins: list[str | None]) -> None:
        for pin in pins:
            if pin is not None:
                self.claims.setdefault(pin, owner)

    def _release(self, owner: tuple[str, str]) -> None:
        self.claims = {pin: holder for pin, holder in self.claims.items() if holder != owner}

    def _instance(self, text: str | None, count: int) -> int:
        return _number(text, 0, count - 1)

    def _open(self, group: str, key: str, state: dict[str, Any], pins: list[str | None], analog: bool = False) -> None:
        owner = (group, key)
        if owner in self.opened:
            _fail("busy")
        if sum(1 for opened_group, _ in self.opened if opened_group == group) >= _OPEN_LIMITS.get(group, 1):
            _fail("busy")
        self._check_pins(owner, pins, analog)
        self._claim(owner, pins)
        self.opened[owner] = state

    def _state(self, group: str, key: str) -> dict[str, Any]:
        state = self.opened.get((group, key))
        if state is None:
            _fail("notopen")
        return state  # type: ignore[return-value]

    def _close(self, group: str, key: str) -> str:
        self._state(group, key)
        del self.opened[(group, key)]
        self._release((group, key))
        return "OK"

    # general

    def _cmd_ping(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 0)
        _keys(options, ())
        return "OK"

    def _cmd_info(self, args: list[str], options: dict[str, str]) -> str:
        return f"OK board={self.board} family={self.family} sysclk={self.sysclk} reset={self.reset_cause} uid=none"

    def _cmd_board_pins(self, args: list[str], options: dict[str, str]) -> str:
        return "OK " + ",".join(f"{alias}={pin}" for alias, pin in (self.pins or {}).items())

    def _cmd_delay(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        self.sleep(_number(args[0], 0, 600000) / 1000)
        return "OK"

    def _cmd_reset(self, args: list[str], options: dict[str, str]) -> None:
        self.reset_cause = "sw"
        self.boot()
        return None

    # gpio

    def _cmd_gpio_cfg(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        _keys(options, ("pull", "drive"))
        pin = self.pin(args[0])
        assert pin is not None
        if args[1] not in ("in", "out", "od"):
            _fail("usage")
        pull = _choice(options, "pull", ("none", "up", "down"), "none")
        if "drive" in options and options["drive"] not in ("2", "4", "8"):
            _fail("usage")
        if args[1] == "od" and "pull" in options:
            _fail("usage")
        owner = ("gpio", pin)
        if owner not in self.opened:
            self._open("gpio", pin, {}, [pin])
        self.opened[owner].update(mode=args[1], pull=pull)
        level = 0 if args[1] == "out" else (1 if pull == "up" or args[1] == "od" else 0)
        self.gpio_levels.setdefault(pin, level)
        return "OK"

    def _gpio(self, text: str) -> tuple[str, dict[str, Any]]:
        pin = self.pin(text)
        assert pin is not None
        return pin, self._state("gpio", pin)

    def _cmd_gpio_set(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        pin, _ = self._gpio(args[0])
        self.gpio_levels[pin] = _number(args[1], 0, 1)
        return "OK"

    def _cmd_gpio_get(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        pin, _ = self._gpio(args[0])
        return f"OK value={self.gpio_levels.get(pin, 0)}"

    def _cmd_gpio_pulse(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 3)
        pin, state = self._gpio(args[0])
        _number(args[1], 1)
        _number(args[2], 1)
        if state.get("mode") == "in":
            _fail("usage")
        return "OK"

    def _cmd_gpio_irq(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        _keys(options, ("type",))
        pin, state = self._gpio(args[0])
        if args[1] not in ("rising", "falling", "both", "off"):
            _fail("usage")
        _choice(options, "type", ("immediate", "dispatched"), "dispatched")
        if pin[1] not in "ABCDEF":
            _fail("unsupported")
        state["irq"] = args[1]
        return "OK"

    def _cmd_gpio_count(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        pin, _ = self._gpio(args[0])
        count = self.gpio_counts.get(pin, 0)
        if _flag(options, "clear"):
            self.gpio_counts[pin] = 0
        return f"OK count={count}"

    def _cmd_gpio_release(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        pin = self.pin(args[0])
        assert pin is not None
        return self._close("gpio", pin)

    # pwm

    def _pwm_channels(self, module: int, options: Mapping[str, str]) -> list[dict[str, Any]]:
        gens_text, pins_text = options.get("gens"), options.get("pins")
        if gens_text is None and pins_text is None:
            _fail("usage")
        gen_tokens = gens_text.split(",") if gens_text is not None else []
        pin_tokens = pins_text.split(",") if pins_text is not None else []
        count = len(gen_tokens) if gens_text is not None else len(pin_tokens)
        if count == 0 or count > 4 or (gens_text is not None and pins_text is not None and len(gen_tokens) != len(pin_tokens)):
            _fail("usage")
        channels: list[dict[str, Any]] = []
        for i in range(count):
            a = b = None
            if pins_text is not None:
                pair = pin_tokens[i].split(":")
                if len(pair) != 2:
                    _fail("usage")
                a = None if pair[0] == "-" else self.pin(pair[0])
                b = None if pair[1] == "-" else self.pin(pair[1])
                if a is None and b is None:
                    _fail("usage")
            table = _PWM_PINS[self.family][module]
            generator = _number(gen_tokens[i], 0, 3) if gens_text is not None else self._pwm_generator_of(table, a or b)
            if pins_text is None:
                a, b = table[2 * generator][0], table[2 * generator + 1][0]
            for pin, channel in ((a, 2 * generator), (b, 2 * generator + 1)):
                if pin is not None and pin not in table[channel]:
                    _fail("pin")
            if any(channel["gen"] == generator for channel in channels):
                _fail("usage")
            channels.append({"gen": generator, "a": a, "b": b, "trigger": None, "irq": None})
        return channels

    @staticmethod
    def _pwm_generator_of(table: tuple[tuple[str, ...], ...], pin: str | None) -> int:
        for channel, pins in enumerate(table):
            if pin in pins:
                return channel // 2
        _fail("pin")
        return 0

    def _cmd_pwm_open(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("gens", "pins", "freq", "mode", "div", "dead", "inva", "invb", "update", "trigger", "irq", "sync"))
        module = self._instance(args[0], self.spec.pwm_modules)
        freq = _number(options.get("freq", "10000"), 1, self.sysclk)
        mode = _choice(options, "mode", ("edge", "center"), "edge")
        div_text = options.get("div", "1")
        if div_text not in {str(div) for div in _PWM_DIVS}:
            _fail("usage")
        div = int(div_text)
        _flag(options, "inva")
        _flag(options, "invb")
        _choice(options, "update", ("local", "global"), "local")
        sync = _flag(options, "sync")
        dead: tuple[int, int] | None = None
        if options.get("dead", "off") != "off":
            values = options["dead"].split(",")
            if not 1 <= len(values) <= 2:
                _fail("usage")
            numbers = [_number(value, 0, 1_000_000) for value in values]
            dead = (numbers[0], numbers[-1])
        channels = self._pwm_channels(module, options)
        for key in ("trigger", "irq"):
            if key in options:
                sources = options[key].split(",")
                if len(sources) not in (1, len(channels)) or any(source not in _PWM_SOURCES for source in sources):
                    _fail("usage")
                for i, channel in enumerate(channels):
                    source = sources[0 if len(sources) == 1 else i]
                    channel[key] = None if source == "none" else source
        pwmclk = self.sysclk // div
        if dead is not None and max(value * pwmclk // 1_000_000_000 for value in dead) > 4095:
            _fail("range")
        if not self._pwm_fits(pwmclk, freq, mode):
            _fail("range")
        if sync and any(channel["irq"] for channel in channels):
            _fail("unsupported")
        pins = [pin for channel in channels for pin in (channel["a"], channel["b"])]
        state = {"module": module, "channels": channels, "freq": freq, "mode": mode, "pwmclk": pwmclk, "sync": sync, "running": False}
        state["since"] = {channel["gen"]: self.clock() for channel in channels}
        state["fault"] = None
        self._open("pwm", str(module), state, pins)
        return f"OK pwmclk={pwmclk}"

    @staticmethod
    def _pwm_fits(pwmclk: int, freq: int, mode: str) -> bool:
        period = pwmclk // freq
        load = period // 2 if mode == "center" else period - 1
        return period != 0 and 1 <= load <= 0xFFFF

    def _pwm(self, text: str) -> dict[str, Any]:
        module = self._instance(text, self.spec.pwm_modules)
        return self._state("pwm", str(module))

    def _cmd_pwm_duty(self, args: list[str], options: dict[str, str]) -> str:
        _keys(options, ())
        if len(args) < 2:
            _fail("usage")
        state = self._pwm(args[0])
        duties = args[1:]
        if len(duties) not in (1, len(state["channels"])):
            _fail("usage")
        for duty in duties:
            try:
                value = float(duty)
            except ValueError:
                raise _Error("usage") from None
            if not 0 <= value <= 100:
                _fail("range")
        if not state["running"]:
            state["running"] = True
            state["since"] = {channel["gen"]: self.clock() for channel in state["channels"]}
        return "OK"

    def _cmd_pwm_freq(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        state = self._pwm(args[0])
        freq = _number(args[1], 1, self.sysclk)
        if not self._pwm_fits(state["pwmclk"], freq, state["mode"]):
            _fail("range")
        self._pwm_accumulate(state)
        state["freq"] = freq
        return "OK"

    def _pwm_accumulate(self, state: dict[str, Any]) -> None:
        state.setdefault("counts", {})
        for channel in state["channels"]:
            state["counts"][channel["gen"]] = self._pwm_count(state, channel)
            state["since"][channel["gen"]] = self.clock()

    def _cmd_pwm_stop(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        state = self._pwm(args[0])
        self._pwm_accumulate(state)
        state["running"] = False
        return "OK"

    def _pwm_count(self, state: dict[str, Any], channel: dict[str, Any]) -> int:
        base = state.get("counts", {}).get(channel["gen"], 0)
        source = channel["irq"]
        if not state["running"] or source is None:
            return base
        per_period = 0 if source in ("cmpau", "cmpbu") and state["mode"] == "edge" else 1
        return base + int((self.clock() - state["since"][channel["gen"]]) * state["freq"] * per_period)

    def _cmd_pwm_count(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        _keys(options, ("clear",))
        state = self._pwm(args[0])
        generator = _number(args[1], 0, 3)
        clear = _flag(options, "clear")
        channel = next((channel for channel in state["channels"] if channel["gen"] == generator), None)
        count = 0 if channel is None else self._pwm_count(state, channel)
        if clear and channel is not None:
            state.setdefault("counts", {})[generator] = 0
            state["since"][generator] = self.clock()
        return f"OK count={count}"

    def _cmd_pwm_fault(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        _keys(options, ("gens", "comparators", "inputs", "pin", "latch", "minperiod"))
        state = self._pwm(args[0])
        if args[1] not in ("on", "off"):
            _fail("usage")
        comparators = _number(options.get("comparators", "0"), 0, 255)
        inputs = _number(options.get("inputs", "0"), 0, 15)
        _number(options.get("minperiod", "0"), 0, 65535)
        _flag(options, "latch")
        pin = self.pin(options.get("pin"))
        opened = {channel["gen"] for channel in state["channels"]}
        if "gens" in options:
            generators = [_number(text, 0, 3) for text in options["gens"].split(",")]
            if any(generator not in opened for generator in generators):
                _fail("usage")
        if pin is not None and self.spec.fault_pins.get(pin) != state["module"]:
            _fail("pin")
        if args[1] == "off":
            if options:
                _fail("usage")
        elif comparators == 0 and inputs == 0:
            _fail("usage")
        if state["sync"]:
            _fail("unsupported")
        owner = ("pwm", str(state["module"]))
        previous = state.get("fault_pin")
        requested = pin if args[1] == "on" else None
        if requested is not None and requested != previous:
            self._check_pins(owner, [requested])
        if previous is not None and requested != previous:
            self.claims = {claimed: holder for claimed, holder in self.claims.items() if not (holder == owner and claimed == previous)}
        if requested is not None and requested != previous:
            self._claim(owner, [requested])
        state["fault_pin"] = requested
        state["fault"] = dict(options) if args[1] == "on" else None
        state["running"] = False
        return "OK"

    def _cmd_pwm_close(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        module = self._instance(args[0], self.spec.pwm_modules)
        return self._close("pwm", str(module))

    # uart

    def _cmd_uart_open(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("tx", "rx", "rts", "cts", "baud", "parity", "stop", "flow", "dma", "sync"))
        index = self._instance(args[0], 8)
        if index == self.spec.terminal_uart:
            _fail("busy")
        if options.get("baud", "115200") not in {str(baud) for baud in _UART_BAUDS}:
            _fail("usage")
        parity = _choice(options, "parity", ("none", "even", "odd"), "none")
        stop = _choice(options, "stop", ("1", "2"), "1")
        flow = _choice(options, "flow", ("none", "rts", "cts", "rtscts"), "none")
        dma, sync = _flag(options, "dma"), _flag(options, "sync")
        tx, rx, rts, cts = (self.pin(options.get(key)) for key in ("tx", "rx", "rts", "cts"))
        if tx is None and rx is None:
            default = self.spec.default_uart
            if default is None or default[0] != index:
                _fail("usage")
            assert default is not None
            tx, rx = default[1], default[2]
        if tx is None or rx is None or (flow in ("rts", "rtscts") and rts is None) or (flow in ("cts", "rtscts") and cts is None):
            _fail("usage")
        if dma and sync:
            _fail("usage")
        if sync and (parity != "none" or stop != "1"):
            _fail("unsupported")
        self._open("uart", str(index), {"flow": flow}, [tx, rx, rts, cts])
        self.uart_rx.setdefault(index, bytearray())
        return "OK"

    def _cmd_uart_send(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        index = self._instance(args[0], 8)
        self._state("uart", str(index))
        try:
            data = parse_hex(args[1])
        except ProtocolError:
            raise _Error("usage") from None
        if not 1 <= len(data) <= 112:
            _fail("range")
        return "OK"

    def _cmd_uart_recv(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("timeout", "len"))
        index = self._instance(args[0], 8)
        self._state("uart", str(index))
        _number(options.get("timeout", "1000"), 0, 10000)
        buffer = self.uart_rx.setdefault(index, bytearray())
        data = bytes(buffer[:256])
        del buffer[: len(data)]
        return f"OK data={format_hex(data) or '-'}"

    def _cmd_uart_close(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        return self._close("uart", str(self._instance(args[0], 8)))

    # spi

    def _cmd_spi_open(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("clk", "mosi", "miso", "cs", "baud", "mode", "sync"))
        index = self._instance(args[0], 4)
        clk, mosi, miso, cs = (self.pin(options.get(key)) for key in ("clk", "mosi", "miso", "cs"))
        _number(options.get("baud", "100000"), self.sysclk // 65024 + 1, self.sysclk // 2)
        _number(options.get("mode", "0"), 0, 3)
        sync = _flag(options, "sync")
        if clk is None or mosi is None or miso is None:
            _fail("usage")
        if sync and cs is not None:
            _fail("unsupported")
        self._open("spi", str(index), {}, [clk, mosi, miso, cs])
        return "OK"

    def _cmd_spi_xfer(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        _keys(options, ("rx", "continue"))
        index = self._instance(args[0], 4)
        self._state("spi", str(index))
        try:
            data = parse_hex(args[1])
        except ProtocolError:
            raise _Error("usage") from None
        size = _number(options.get("rx", str(len(data))), 0, 64)
        if len(data) > 64 or max(len(data), size) == 0:
            _fail("range")
        _flag(options, "continue")
        return f"OK rx={format_hex(bytes([self.spi_miso]) * size) or '-'}"

    def _cmd_spi_close(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        return self._close("spi", str(self._instance(args[0], 4)))

    # adc

    def _cmd_adc_open(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        _keys(options, ("pins", "sh", "avg", "delay", "trigger", "dcmp", "ref", "prio", "sync"))
        adc = self._instance(args[0], 2)
        seq = self._instance(args[1], 4)
        _choice(options, "sh", ("4", "8", "16", "32", "64", "128", "256"), "4")
        _choice(options, "avg", ("off", "2", "4", "8", "16", "32", "64"), "off")
        trigger = options.get("trigger")
        if trigger is not None and trigger not in ("pwm0", "pwm1", "pwm2", "pwm3"):
            _fail("usage")
        ref = _choice(options, "ref", ("int", "ext"), "int")
        sync = _flag(options, "sync")
        delay = options.get("delay", "off")
        if delay != "off":
            _number(delay, 0, 15)
        if "prio" in options:
            _number(options["prio"], 0, 3)
        if "pins" not in options or (not sync and trigger is None):
            _fail("usage")
        tokens = options["pins"].split(",")
        if not 1 <= len(tokens) <= _SEQUENCER_DEPTHS[seq]:
            _fail("range")
        pins = [self.pin(token) for token in tokens]
        if any(pin not in self._analog_pins() for pin in pins):
            _fail("pin")
        if sync and (delay != "off" or trigger is not None or ref == "ext" or "dcmp" in options):
            _fail("unsupported")
        comparators = self._dcmp(options["dcmp"], len(pins)) if "dcmp" in options else 0
        state = {"pins": pins, "trigger": trigger, "sync": sync, "fifo": len(pins) - comparators}
        self._open("adc", f"{adc} {seq}", state, list(pins), analog=True)
        return "OK"

    def _analog_pins(self) -> set[str]:
        common = {"PE3", "PE2", "PE1", "PE0", "PE5", "PE4", "PB4", "PB5"}
        if self.family == "tm4c123":
            return common | {"PD3", "PD2", "PD1", "PD0"}
        return common | {"PD7", "PD6", "PD5", "PD4", "PD3", "PD2", "PD1", "PD0", "PK0", "PK1", "PK2", "PK3"}

    @staticmethod
    def _dcmp(text: str, steps: int) -> int:
        entries = text.split(",")
        if not entries or len(entries) >= steps:
            _fail("range")
        used: set[int] = set()
        for entry in entries:
            fields = entry.split(":")
            if not 3 <= len(fields) <= 5:
                _fail("usage")
            if len(fields) > 3 and fields[3] not in ("low", "mid", "high"):
                _fail("usage")
            if len(fields) > 4 and fields[4] not in ("always", "once", "hyst", "hystonce"):
                _fail("usage")
            index, low, high = (_number(value) for value in fields[:3])
            if index > 7 or high > 4095 or low > high or index in used:
                _fail("range")
            used.add(index)
        return len(entries)

    def _cmd_adc_measure(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        _keys(options, ("n",))
        adc, seq = self._instance(args[0], 2), self._instance(args[1], 4)
        state = self._state("adc", f"{adc} {seq}")
        runs = _number(options.get("n", "1"), 1, 64)
        if runs * state["fifo"] > 64:
            _fail("range")
        if not state["sync"]:
            rate = self._adc_trigger_rate(state["trigger"])
            if rate == 0:
                return "ERR timeout"
            self.sleep(runs / rate)
        samples = [self.adc_codes.get(pin, 2048) for _ in range(runs) for pin in state["pins"][: state["fifo"]]]
        return "OK samples=" + ",".join(str(sample) for sample in samples)

    def _adc_trigger_rate(self, trigger: str) -> float:
        """Conversions per second of an asynchronous sequencer on generator `pwm<g>` of module 0."""
        pwm = self.opened.get(("pwm", "0"))
        if pwm is None or not pwm["running"]:
            return 0.0
        generator = int(trigger[3:])
        for channel in pwm["channels"]:
            if channel["gen"] == generator and channel["trigger"] is not None:
                return 0.0 if channel["trigger"] in ("cmpau", "cmpbu") and pwm["mode"] == "edge" else float(pwm["freq"])
        return 0.0

    def _cmd_adc_close(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        return self._close("adc", f"{self._instance(args[0], 2)} {self._instance(args[1], 4)}")

    # comparator

    def _cmd_comp_open(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("pos", "neg", "out", "src", "ref", "invert", "trigger", "sync"))
        index = self._instance(args[0], self.spec.comparators)
        pos, neg, out = (self.pin(options.get(key)) for key in ("pos", "neg", "out"))
        src = _choice(options, "src", ("pin", "c0", "ref"), "ref" if "ref" in options else "pin")
        _flag(options, "invert")
        _choice(options, "trigger", ("off", "rising", "falling", "both", "high", "low"), "off")
        sync = _flag(options, "sync")
        if "ref" in options:
            fields = options["ref"].split(",")
            if len(fields) != 2 or fields[0] not in ("low", "high"):
                _fail("usage")
            _number(fields[1], 0, 15)
        if neg is None or (src == "pin" and pos is None) or ((src == "ref") != ("ref" in options)):
            _fail("usage")
        self._open("comp", str(index), {"sync": sync, "irq": "off", "count": 0}, [pos, neg, out])
        return "OK"

    def _comp(self, text: str) -> dict[str, Any]:
        return self._state("comp", str(self._instance(text, self.spec.comparators)))

    def _cmd_comp_read(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        self._comp(args[0])
        return "OK out=0"

    def _cmd_comp_irq(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        state = self._comp(args[0])
        if args[1] not in ("rising", "falling", "both", "off"):
            _fail("usage")
        if state["sync"]:
            _fail("unsupported")
        state["irq"] = args[1]
        return "OK"

    def _cmd_comp_count(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        state = self._comp(args[0])
        count = state["count"]
        if _flag(options, "clear"):
            state["count"] = 0
        return f"OK count={count}"

    def _cmd_comp_close(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        return self._close("comp", str(self._instance(args[0], self.spec.comparators)))

    # qei

    def _cmd_qei_open(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("a", "b", "idx", "res", "offset", "inva", "invb", "invi", "reset", "cap", "sig", "vel"))
        index = self._instance(args[0], self.spec.qeis)
        res = _number(options.get("res", "1024"), 1)
        offset = _number(options.get("offset", "0"))
        for key in ("inva", "invb", "invi"):
            _flag(options, key)
        _choice(options, "reset", ("max", "index"), "max")
        _choice(options, "cap", ("a", "ab"), "ab")
        _choice(options, "sig", ("quad", "clkdir"), "quad")
        _number(options.get("vel", "1000"), 1)
        if offset >= res:
            _fail("range")
        a, b, idx = (self.pin(options.get(key)) for key in ("a", "b", "idx"))
        if a is None and b is None and idx is None:
            if index != 0:
                _fail("usage")
            pins = self.pins or {}
            a, b, idx = pins.get("qei0a"), pins.get("qei0b"), pins.get("qei0idx")
        if a is None or b is None:
            _fail("usage")
        self._open("qei", str(index), {"res": res, "pos": offset}, [a, b, idx])
        return "OK"

    def _cmd_qei_read(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        state = self._state("qei", str(self._instance(args[0], self.spec.qeis)))
        return f"OK pos={state['pos']} dir=fwd speed=0 res={state['res']}"

    def _cmd_qei_close(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        return self._close("qei", str(self._instance(args[0], self.spec.qeis)))

    # can

    def _cmd_can_open(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("rx", "tx", "bitrate", "timing", "filter", "loopback", "recover"))
        index = self._instance(args[0], 2)
        rx, tx = self.pin(options.get("rx")), self.pin(options.get("tx"))
        bitrate = _number(options.get("bitrate", "500000"), 1, 1_000_000)
        loopback = _flag(options, "loopback")
        _flag(options, "recover", True)
        can_filter = self._can_filter(options["filter"]) if "filter" in options else None
        if "timing" in options:
            if "bitrate" in options:
                _fail("usage")
            fields = options["timing"].split(",")
            if len(fields) != 4:
                _fail("usage")
            for text, high in zip(fields, (16, 8, 4, 1024)):
                _number(text, 1, high)
        elif not self._can_bitrate_achievable(bitrate):
            _fail("range")
        if rx is None and tx is None:
            if index != 0:
                _fail("usage")
            pins = self.pins or {}
            rx, tx = pins.get("can0rx"), pins.get("can0tx")
        if rx is None or tx is None:
            _fail("usage")
        self._open("can", str(index), {"loopback": loopback, "filter": can_filter}, [rx, tx])
        return "OK"

    def _can_bitrate_achievable(self, bitrate: int) -> bool:
        if bitrate == 0 or self.sysclk % bitrate:
            return False
        clocks = self.sysclk // bitrate
        for quanta in range(8, 26):
            if clocks % quanta or clocks // quanta > 1024:
                continue
            before = (875 * quanta + 500) // 1000
            segment1, segment2 = before - 1, quanta - before
            if before >= 3 and segment1 <= 16 and 1 <= segment2 <= 8:
                return True
        return False

    @staticmethod
    def _can_filter(text: str) -> tuple[int, int, bool, bool]:
        fields = text.split(",")
        if not 3 <= len(fields) <= 4:
            _fail("usage")
        ident, mask = _number(fields[0]), _number(fields[1])
        ext, match = _number(fields[2], 0, 1), _number(fields[3] if len(fields) == 4 else "1", 0, 1)
        limit = 0x1FFFFFFF if ext else 0x7FF
        if ident > limit or mask > limit:
            _fail("range")
        return ident, mask, bool(ext), bool(match)

    @staticmethod
    def can_accepts(can_filter: tuple[int, int, bool, bool] | None, ident: int, ext: bool) -> bool:
        """C_CAN acceptance: a standard id sits in arbitration bits 28:18; without `match` the id type is ignored."""
        if can_filter is None:
            return True
        filter_id, mask, filter_ext, match = can_filter
        if ext != filter_ext and match:
            return False
        frame = ident if ext else ident << 18
        reference = filter_id if filter_ext else filter_id << 18
        window = mask if filter_ext else (mask << 18) & 0x1FFC0000
        return (frame ^ reference) & window == 0

    def _cmd_can_send(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 3)
        _keys(options, ("ext",))
        index = self._instance(args[0], 2)
        state = self._state("can", str(index))
        ext = _flag(options, "ext")
        ident = _number(args[1], 0, 0x1FFFFFFF if ext else 0x7FF)
        try:
            data = parse_hex(args[2])
        except ProtocolError:
            raise _Error("usage") from None
        if len(data) > 8:
            _fail("range")
        if state["loopback"] and self.can_accepts(state["filter"], ident, ext):
            self.emit(f"EVT can index={index} id=0x{ident:x} ext={int(ext)} data={format_hex(data) or '-'}")
        return "OK"

    def _cmd_can_close(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        return self._close("can", str(self._instance(args[0], 2)))

    # eeprom

    def _cmd_eeprom_write(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        address, data = _number(args[0]), parse_hex(args[1])
        if len(data) > 112 or address + len(data) > len(self.eeprom):
            _fail("range")
        self.eeprom[address : address + len(data)] = data
        return "OK"

    def _cmd_eeprom_read(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 2)
        address, length = _number(args[0]), _number(args[1])
        if length > 112 or address + length > len(self.eeprom):
            _fail("range")
        return f"OK data={format_hex(self.eeprom[address : address + length]) or '-'}"

    def _cmd_eeprom_erase(self, args: list[str], options: dict[str, str]) -> str:
        self.eeprom[:] = b"\xff" * len(self.eeprom)
        return "OK"

    # watchdog

    def _cmd_wdt_start(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        _keys(options, ("timeout", "reset", "feed", "pin"))
        index = self._instance(args[0], 2)
        timeout = _number(options.get("timeout"), 1, 30000)
        reset = _flag(options, "reset", True)
        feed = _choice(options, "feed", ("auto", "manual"), "auto")
        pin = self.pin(options.get("pin"))
        if self.watchdog is not None:
            _fail("busy")
        owner = ("wdt", str(index))
        self._check_pins(owner, [pin])
        self._claim(owner, [pin])
        period = timeout / 1000
        self.watchdog = {
            "index": index,
            "period": period,
            "reset": reset,
            "feed": feed,
            "next": self.clock() + period,
            "warnings": 0,
            "warned": False,
        }
        return "OK"

    def _cmd_wdt_feed(self, args: list[str], options: dict[str, str]) -> str:
        _positionals(args, 1)
        index = self._instance(args[0], 2)
        if self.watchdog is None or self.watchdog["index"] != index:
            _fail("notopen")
        self.watchdog["next"] = self.clock() + self.watchdog["period"]
        self.watchdog["warned"] = False
        return "OK"

    def poll(self) -> None:
        """Early warnings every timeout; without feeding, `reset=1` resets the board at the second timeout."""
        watchdog = self.watchdog
        if watchdog is None:
            return
        now = self.clock()
        for _ in range(1000):
            if watchdog["next"] > now:
                return
            if watchdog["feed"] == "manual" and watchdog["reset"] and watchdog["warned"]:
                self.reset_cause = f"wdt{watchdog['index']}"
                self.boot()
                return
            watchdog["warnings"] += 1
            watchdog["warned"] = True
            watchdog["next"] += watchdog["period"]
            self.event(f"EVT wdt index={watchdog['index']} warning={watchdog['warnings']}")

    # ethernet

    def _ethernet(self) -> None:
        if not self.spec.ethernet:
            _fail("unsupported")

    def _cmd_eth_open(self, args: list[str], options: dict[str, str]) -> str:
        self._ethernet()
        _keys(options, ("phy", "speed"))
        _choice(options, "phy", ("internal",), "internal")
        _choice(options, "speed", ("auto", "10", "100"), "auto")
        self._open("eth", "", {}, [])
        return "OK"

    def _cmd_eth_status(self, args: list[str], options: dict[str, str]) -> str:
        self._ethernet()
        self._state("eth", "")
        return "OK link=down speed=10 duplex=half rx=0 tx=0"

    def _cmd_eth_close(self, args: list[str], options: dict[str, str]) -> str:
        self._ethernet()
        return self._close("eth", "")
