"""Offline doubles: a fake WaveForms library behind the real `AnalogDiscovery3` wrapper, and a fake serial
port that behaves like the EMIL terminal of the validation firmware (echo, prompt, `EVT` lines)."""

from __future__ import annotations

import ctypes
import random
from collections import deque
from collections.abc import Callable, Sequence
from dataclasses import dataclass, field
from types import SimpleNamespace
from typing import Any, Literal

from ..protocol import format_hex, parse_hex, parse_number
from .ad3 import AnalogDiscovery3
from .dwf import _FALLBACK_CONSTANTS

# --------------------------------------------------------------------------- WaveForms library


def _target(arg: Any) -> Any:
    return getattr(arg, "_obj", arg)


def _value(arg: Any) -> Any:
    return getattr(_target(arg), "value", arg)


class FakeDwfApi:
    """Records every `FDwf*` call and answers the out-parameters the wrapper reads.

    DigitalIO loops outputs back to inputs; `inputs` supplies levels of undriven DIOs. `logic_samples`,
    `uart_rx`, `can_rx` and `scope_levels` feed the corresponding acquisitions.
    """

    def __init__(self) -> None:
        self.constants = SimpleNamespace(**_FALLBACK_CONSTANTS)
        self.calls: list[tuple[str, tuple[Any, ...]]] = []
        self.clock_hz = 100e6
        self.buffer_size = 32768
        self.pattern_buffer = 32768
        self.counter_max = 0x7FFF
        self.inputs = 0
        self.output_enable = 0
        self.outputs = 0
        self.logic_samples: list[int] = []
        self.uart_rx = bytearray()
        self.uart_tx = bytearray()
        self.can_rx: deque[tuple[int, bool, bytes]] = deque()
        self.can_tx: list[tuple[int, bool, bytes]] = []
        self.scope_levels = {0: 0.0, 1: 0.0}
        self.serial = "SN:210415ABCDEF"

    def names(self) -> list[str]:
        return [name for name, _ in self.calls]

    def calls_to(self, name: str) -> list[tuple[Any, ...]]:
        return [args for called, args in self.calls if called == name]

    def last_error(self) -> str:
        return ""

    def __getattr__(self, name: str) -> Callable[..., int]:
        if not name.startswith("FDwf"):
            raise AttributeError(name)

        def call(*args: Any) -> int:
            self.calls.append((name, tuple(_value(arg) if not isinstance(arg, ctypes.Array) else arg for arg in args)))
            handler = getattr(self, "_" + name, None)
            if handler is not None:
                handler(*args)
            return 1

        return call

    @staticmethod
    def _set(arg: Any, value: Any) -> None:
        _target(arg).value = value

    def _FDwfEnum(self, _filter: Any, count: Any) -> None:
        self._set(count, 1)

    def _FDwfEnumDeviceName(self, _index: Any, buffer: Any) -> None:
        buffer.value = b"Analog Discovery 3"

    def _FDwfEnumSN(self, _index: Any, buffer: Any) -> None:
        buffer.value = self.serial.encode()

    def _FDwfDeviceOpen(self, _index: Any, handle: Any) -> None:
        self._set(handle, 1)

    def _FDwfDigitalOutInternalClockInfo(self, _h: Any, value: Any) -> None:
        self._set(value, self.clock_hz)

    _FDwfDigitalInInternalClockInfo = _FDwfDigitalOutInternalClockInfo

    def _FDwfDigitalInBufferSizeInfo(self, _h: Any, value: Any) -> None:
        self._set(value, self.buffer_size)

    def _FDwfDigitalOutDataInfo(self, _h: Any, _dio: Any, value: Any) -> None:
        self._set(value, self.pattern_buffer)

    def _FDwfDigitalOutCounterInfo(self, _h: Any, _dio: Any, low: Any, high: Any) -> None:
        self._set(low, 0)
        self._set(high, self.counter_max)

    def _done(self, *args: Any) -> None:
        self._set(args[-1], 2)

    _FDwfDigitalOutStatus = _done
    _FDwfDigitalInStatus = _done
    _FDwfAnalogInStatus = _done
    _FDwfAnalogOutStatus = _done

    def _FDwfDigitalIOOutputEnableSet(self, _h: Any, mask: Any) -> None:
        self.output_enable = _value(mask)

    def _FDwfDigitalIOOutputSet(self, _h: Any, mask: Any) -> None:
        self.outputs = _value(mask)

    def _FDwfDigitalIOInputStatus(self, _h: Any, value: Any) -> None:
        self._set(value, (self.outputs & self.output_enable) | (self.inputs & ~self.output_enable & 0xFFFF))

    def _FDwfDigitalInStatusData(self, _h: Any, buffer: Any, _size: Any) -> None:
        target = _target(buffer)
        for i in range(min(len(target), len(self.logic_samples))):
            target[i] = self.logic_samples[i]

    def _FDwfAnalogInStatusData(self, _h: Any, index: Any, buffer: Any, _size: Any) -> None:
        target = _target(buffer)
        for i in range(len(target)):
            target[i] = self.scope_levels[_value(index)]

    def _FDwfDigitalUartTx(self, _h: Any, buffer: Any, size: Any) -> None:
        if buffer is not None:
            self.uart_tx += ctypes.string_at(buffer, _value(size))

    def _FDwfDigitalUartRx(self, _h: Any, buffer: Any, size: Any, count: Any, parity: Any) -> None:
        taken = bytes(self.uart_rx[: _value(size)]) if buffer is not None else b""
        del self.uart_rx[: len(taken)]
        if taken:
            ctypes.memmove(buffer, taken, len(taken))
        self._set(count, len(taken))
        self._set(parity, 0)

    def _FDwfDigitalCanTx(self, _h: Any, ident: Any, ext: Any, _remote: Any, dlc: Any, buffer: Any) -> None:
        if _value(ident) >= 0 and buffer is not None:
            self.can_tx.append((_value(ident), bool(_value(ext)), bytes(buffer[: _value(dlc)])))

    def _FDwfDigitalCanRx(self, _h: Any, ident: Any, ext: Any, remote: Any, dlc: Any, buffer: Any, size: Any, status: Any) -> None:
        if _value(size) and self.can_rx:
            frame_id, frame_ext, data = self.can_rx.popleft()
            for i, byte in enumerate(data):
                buffer[i] = byte
            self._set(ident, frame_id)
            self._set(ext, int(frame_ext))
            self._set(remote, 0)
            self._set(dlc, len(data))
            self._set(status, 1)
        else:
            self._set(status, 0)


def fake_ad3(api: FakeDwfApi | None = None, analog_limits: tuple[float, float] = (0.0, 3.3)) -> AnalogDiscovery3:
    """An opened `AnalogDiscovery3` running on `FakeDwfApi`."""
    backend = api or FakeDwfApi()
    device = AnalogDiscovery3(analog_limits=analog_limits, api_factory=lambda: backend)
    device.open()
    return device


# --------------------------------------------------------------------------- firmware terminal

Handler = Callable[["FakeFirmware", list[str], dict[str, str]], "str | list[str] | None"]


@dataclass
class FakeFirmware:
    """Emulates the validation firmware behind `services::TerminalWithCommandsImpl`.

    `style="trace"` prints each line as `\\r\\n<line>` (Tracer::Trace), `"line"` as `<line>\\r\\n`. `noise`
    sprinkles bells and ANSI erase sequences into the output. Lines queued with `emit()` are printed before
    the next final line (or immediately with `emit(..., now=True)`).
    """

    board: str = "ek_tm4c123gxl"
    family: str = "tm4c123"
    sysclk: int = 80_000_000
    pins: dict[str, str] = field(default_factory=lambda: {"ledop": "PF1", "pwm1a": "PB6", "pwm1b": "PB7"})
    style: Literal["trace", "line"] = "trace"
    noise: bool = False
    eeprom_size: int = 2048
    output: bytearray = field(default_factory=bytearray)
    handlers: dict[str, Handler] = field(default_factory=dict)
    received: list[str] = field(default_factory=list)
    gpio_levels: dict[str, int] = field(default_factory=dict)
    gpio_counts: dict[str, int] = field(default_factory=dict)
    opened: set[tuple[str, str]] = field(default_factory=set)
    reset_cause: str = "por"

    def __post_init__(self) -> None:
        self._line = ""
        self.command_name = ""
        self._pending_events: list[str] = []
        self.eeprom = bytearray(b"\xff" * self.eeprom_size)
        self.boot()

    def boot(self) -> None:
        self._line = ""
        self.opened.clear()
        self._write("> ")
        self._print(f"EVT boot board={self.board} family={self.family} sysclk={self.sysclk} reset={self.reset_cause}")

    def emit(self, line: str, now: bool = False) -> None:
        if now:
            self._print(line)
        else:
            self._pending_events.append(line)

    def feed(self, data: bytes) -> None:
        for char in data.decode("latin-1"):
            if char == "\r":
                self._enter()
            elif char == "\n":
                continue
            elif char == "\x03":
                self._line = ""
                self._write("\r> ")
            elif char in "\b\x7f":
                if self._line:
                    self._line = self._line[:-1]
                    self._write("\b \b")
            elif " " <= char <= "~":
                self._line += char
                self._write(char)
            else:
                self._write("\a")

    def _enter(self) -> None:
        line, self._line = self._line, ""
        self._write("\r\n")
        if line:
            self.received.append(line)
            result = self._execute(line)
            for event in self._pending_events:
                self._print(event)
            self._pending_events.clear()
            if result is None:
                return
            for item in [result] if isinstance(result, str) else result:
                self._print(item)
        self._write("> ")

    def _execute(self, line: str) -> str | list[str] | None:
        tokens = line.split()
        name, rest = tokens[0], tokens[1:]
        self.command_name = name
        args = [token for token in rest if "=" not in token]
        options = dict(token.split("=", 1) for token in rest if "=" in token)
        handler = self.handlers.get(name) or _DEFAULT_HANDLERS.get(name)
        if handler is None and "." in name:
            group, action = name.split(".", 1)
            if action == "open":
                handler = _generic_open
            elif action == "close":
                handler = _generic_close
        if handler is None:
            return "Unrecognized command."
        try:
            return handler(self, args, options)
        except (IndexError, ValueError, KeyError):
            return "ERR usage"

    def _print(self, line: str) -> None:
        if self.noise:
            line = line.replace(" ", " \a", 1) + "\x1b[K"
        self._write(f"\r\n{line}" if self.style == "trace" else f"{line}\r\n")

    def _write(self, text: str) -> None:
        self.output += text.encode("latin-1")


def _ping(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    return "OK"


def _info(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    return f"OK board={fw.board} family={fw.family} sysclk={fw.sysclk} reset={fw.reset_cause} uid=none"


def _pins(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    return "OK " + ",".join(f"{alias}={pin}" for alias, pin in fw.pins.items())


def _delay(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    parse_number(args[0])
    return "OK"


def _reset(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> None:
    fw.reset_cause = "sw"
    fw.boot()
    return None


def _gpio_cfg(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    if args[1] not in ("in", "out", "od"):
        return "ERR usage"
    fw.gpio_levels.setdefault(args[0], 0)
    return "OK"


def _gpio_set(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    fw.gpio_levels[args[0]] = parse_number(args[1])
    return "OK"


def _gpio_get(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    return f"OK value={fw.gpio_levels.get(args[0], 0)}"


def _gpio_count(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    count = fw.gpio_counts.get(args[0], 0)
    if options.get("clear") == "1":
        fw.gpio_counts[args[0]] = 0
    return f"OK count={count}"


def _ok(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    return "OK"


def _eeprom_write(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    address, data = parse_number(args[0]), parse_hex(args[1])
    if address + len(data) > len(fw.eeprom):
        return "ERR range"
    fw.eeprom[address : address + len(data)] = data
    return "OK"


def _eeprom_read(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    address, length = parse_number(args[0]), parse_number(args[1])
    if address + length > len(fw.eeprom):
        return "ERR range"
    return f"OK data={format_hex(fw.eeprom[address : address + length])}"


def _eeprom_erase(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    fw.eeprom[:] = b"\xff" * len(fw.eeprom)
    return "OK"


def _instance(name: str, args: Sequence[str]) -> tuple[str, str]:
    group = name.split(".")[0]
    key = " ".join(args[:2]) if group == "adc" else (args[0] if args else "")
    return group, key


def _generic_open(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    key = _instance(fw.command_name, args)
    if key in fw.opened:
        return "ERR busy"
    fw.opened.add(key)
    return "OK pwmclk=40000000" if key[0] == "pwm" else "OK"


def _generic_close(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    key = _instance(fw.command_name, args)
    if key not in fw.opened:
        return "ERR notopen"
    fw.opened.discard(key)
    return "OK"


def _can_send(fw: FakeFirmware, args: list[str], options: dict[str, str]) -> str:
    if ("can", args[0]) not in fw.opened:
        return "ERR notopen"
    ident = parse_number(args[1])
    fw.emit(f"EVT can index={args[0]} id=0x{ident:x} ext={options.get('ext', '0')} data={args[2] if args[2] != '-' else ''}")
    return "OK"


_DEFAULT_HANDLERS: dict[str, Handler] = {
    "ping": _ping,
    "info": _info,
    "board.pins": _pins,
    "delay": _delay,
    "reset": _reset,
    "gpio.cfg": _gpio_cfg,
    "gpio.set": _gpio_set,
    "gpio.get": _gpio_get,
    "gpio.count": _gpio_count,
    "gpio.irq": _ok,
    "gpio.pulse": _ok,
    "gpio.release": _ok,
    "eeprom.write": _eeprom_write,
    "eeprom.read": _eeprom_read,
    "eeprom.erase": _eeprom_erase,
    "can.send": _can_send,
}


class FakeSerial:
    """pyserial stand-in wired to a `FakeFirmware`; `chunk` > 0 returns output in random small pieces."""

    def __init__(self, firmware: FakeFirmware | None = None, chunk: int = 0, seed: int = 1) -> None:
        self.firmware = firmware or FakeFirmware()
        self.chunk = chunk
        self._random = random.Random(seed)
        self.written = bytearray()
        self.closed = False

    @property
    def in_waiting(self) -> int:
        return len(self.firmware.output)

    def write(self, data: bytes) -> int:
        self.written += data
        self.firmware.feed(data)
        return len(data)

    def read(self, size: int = 1) -> bytes:
        output = self.firmware.output
        if self.chunk:
            size = min(size, self._random.randint(1, self.chunk))
        data = bytes(output[:size])
        del output[:size]
        return data

    def close(self) -> None:
        self.closed = True
