"""Offline double of the validation firmware: `FakeFirmware` answers a subset of the PROTOCOL.md commands
behind the generic `FakeTerminalDevice` (echo, prompt, `EVT` lines); connect it with `FakeSerial`."""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass, field

from ad3_waveforms_bench.fake_terminal import FakeSerial, FakeTerminalDevice, Handler
from ad3_waveforms_bench.protocol import format_hex, parse_hex, parse_number

__all__ = ["FakeFirmware", "FakeSerial"]


@dataclass
class FakeFirmware(FakeTerminalDevice):
    """Emulates the validation firmware behind `services::TerminalWithCommandsImpl`; `style`, `noise` and
    `emit()` come from `FakeTerminalDevice`."""

    board: str = "ek_tm4c123gxl"
    family: str = "tm4c123"
    sysclk: int = 80_000_000
    pins: dict[str, str] = field(default_factory=lambda: {"ledop": "PF1", "pwm1a": "PB6", "pwm1b": "PB7"})
    eeprom_size: int = 2048
    gpio_levels: dict[str, int] = field(default_factory=dict)
    gpio_counts: dict[str, int] = field(default_factory=dict)
    opened: set[tuple[str, str]] = field(default_factory=set)
    reset_cause: str = "por"

    def __post_init__(self) -> None:
        self.eeprom = bytearray(b"\xff" * self.eeprom_size)
        super().__post_init__()

    def boot(self) -> None:
        self.opened.clear()
        super().boot()

    def boot_message(self) -> str:
        return f"EVT boot board={self.board} family={self.family} sysclk={self.sysclk} reset={self.reset_cause}"

    def lookup(self, name: str) -> Handler | None:
        handler = self.handlers.get(name) or _DEFAULT_HANDLERS.get(name)
        if handler is None and "." in name:
            action = name.split(".", 1)[1]
            if action == "open":
                handler = _generic_open
            elif action == "close":
                handler = _generic_close
        return handler


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
    "ping": _ok,
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
