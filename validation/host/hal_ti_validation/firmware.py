"""Typed API over the validation firmware commands; keyword arguments map 1:1 to protocol options."""

from __future__ import annotations

from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from typing import Literal

from .protocol import Event, Response, format_command, normalize_pin, parse_pin_map
from .terminal import FirmwareError, FirmwareTerminal

Pin = str
Level = Literal[0, 1]
Edge = Literal["rising", "falling", "both", "off"]


@dataclass(frozen=True)
class BootInfo:
    board: str
    family: str
    sysclk: int
    reset: str
    raw: str

    @classmethod
    def from_values(cls, source: Response | Event) -> BootInfo:
        return cls(
            board=source["board"],
            family=source["family"],
            sysclk=source.as_int("sysclk"),
            reset=source["reset"],
            raw=source.raw,
        )


@dataclass(frozen=True)
class Info(BootInfo):
    uid: str | None = None


@dataclass(frozen=True)
class QeiReading:
    pos: int
    dir: str
    speed: int
    res: int


@dataclass(frozen=True)
class CanFrame:
    index: int
    id: int
    ext: bool
    data: bytes


@dataclass(frozen=True)
class EthStatus:
    link: str
    speed: int
    duplex: str
    rx: int
    tx: int


class _Group:
    prefix = ""

    def __init__(self, firmware: Firmware) -> None:
        self._fw = firmware

    def _cmd(self, name: str, *args: object, cmd_timeout: float | None = None, **options: object) -> Response:
        return self._fw.command(f"{self.prefix}.{name}", *args, cmd_timeout=cmd_timeout, **options)

    def _pin(self, pin: Pin | None) -> str | None:
        return None if pin is None else self._fw.pin(pin)

    def _pins(self, pins: Sequence[Pin] | None) -> list[str] | None:
        return None if pins is None else [self._fw.pin(pin) for pin in pins]


class System(_Group):
    def ping(self) -> None:
        self._fw.command("ping")

    def info(self) -> Info:
        response = self._fw.command("info")
        uid = response.get("uid")
        base = BootInfo.from_values(response)
        return Info(**base.__dict__, uid=None if uid in (None, "none") else uid)

    def pins(self) -> dict[str, str]:
        return parse_pin_map(self._fw.command("board.pins"))

    def delay(self, ms: int) -> None:
        self._fw.command("delay", ms, cmd_timeout=self._fw.terminal.timeout + ms / 1000)

    def reset(self, timeout: float = 5.0) -> BootInfo:
        terminal = self._fw.terminal
        terminal.drain_events("boot")
        terminal.send_nowait("reset")
        self._fw.forget_open()
        return BootInfo.from_values(terminal.wait_boot(timeout))

    def wait_boot(self, timeout: float = 5.0) -> BootInfo:
        self._fw.forget_open()
        return BootInfo.from_values(self._fw.terminal.wait_boot(timeout))


class Gpio(_Group):
    prefix = "gpio"

    def cfg(self, pin: Pin, mode: Literal["in", "out", "od"], pull: str | None = None, drive: int | None = None) -> None:
        pin = self._fw.pin(pin)
        self._cmd("cfg", pin, mode, pull=pull, drive=drive)
        self._fw.track(("gpio", pin), "gpio.release", pin)

    def set(self, pin: Pin, value: int | bool) -> None:
        self._cmd("set", self._fw.pin(pin), int(bool(value)))

    def get(self, pin: Pin) -> int:
        return self._cmd("get", self._fw.pin(pin)).as_int("value")

    def pulse(self, pin: Pin, count: int, period_ms: int, cmd_timeout: float | None = None) -> None:
        duration = count * period_ms / 1000
        self._cmd("pulse", self._fw.pin(pin), count, period_ms, cmd_timeout=cmd_timeout or self._fw.terminal.timeout + duration * 1.5)

    def irq(self, pin: Pin, edge: Edge, type: Literal["immediate", "dispatched"] | None = None) -> None:
        self._cmd("irq", self._fw.pin(pin), edge, type=type)

    def count(self, pin: Pin, clear: bool | None = None) -> int:
        return self._cmd("count", self._fw.pin(pin), clear=clear).as_int("count")

    def release(self, pin: Pin) -> None:
        pin = self._fw.pin(pin)
        self._cmd("release", pin)
        self._fw.untrack(("gpio", pin))


class Pwm(_Group):
    prefix = "pwm"

    def open(
        self,
        module: int,
        gens: Sequence[int] | None = None,
        pins: Sequence[tuple[Pin, Pin]] | None = None,
        freq: int | None = None,
        mode: Literal["edge", "center"] | None = None,
        div: int | None = None,
        dead: int | Literal["off"] | None = None,
        inva: bool | None = None,
        invb: bool | None = None,
        update: Literal["local", "global"] | None = None,
        trigger: Literal["zero", "load", "none"] | None = None,
        irq: str | None = None,
        sync: bool | None = None,
    ) -> int:
        """Returns the PWM clock in Hz."""
        pin_list = None if pins is None else [f"{self._fw.pin(a)}:{self._fw.pin(b)}" for a, b in pins]
        response = self._cmd(
            "open",
            module,
            gens=list(gens) if gens is not None else None,
            pins=pin_list,
            freq=freq,
            mode=mode,
            div=div,
            dead=dead,
            inva=inva,
            invb=invb,
            update=update,
            trigger=trigger,
            irq=irq,
            sync=sync,
        )
        self._fw.track(("pwm", module), "pwm.close", module)
        return response.as_int("pwmclk")

    def fault(self, module: int, on: bool) -> None:
        self._cmd("fault", module, "on" if on else "off")

    def duty(self, module: int, *duties: float) -> None:
        if not 1 <= len(duties) <= 3:
            raise ValueError("one to three duties")
        self._cmd("duty", module, *[float(duty) for duty in duties])

    def freq(self, module: int, hz: int) -> None:
        self._cmd("freq", module, hz)

    def stop(self, module: int) -> None:
        self._cmd("stop", module)

    def count(self, module: int, gen: int, clear: bool | None = None) -> int:
        return self._cmd("count", module, gen, clear=clear).as_int("count")

    def close(self, module: int) -> None:
        self._cmd("close", module)
        self._fw.untrack(("pwm", module))


class Uart(_Group):
    prefix = "uart"

    def open(
        self,
        index: int,
        tx: Pin | None = None,
        rx: Pin | None = None,
        rts: Pin | None = None,
        cts: Pin | None = None,
        baud: int | None = None,
        parity: Literal["none", "even", "odd"] | None = None,
        stop: int | None = None,
        flow: Literal["none", "rts", "cts", "rtscts"] | None = None,
        dma: bool | None = None,
        sync: bool | None = None,
    ) -> None:
        self._cmd(
            "open",
            index,
            tx=self._pin(tx),
            rx=self._pin(rx),
            rts=self._pin(rts),
            cts=self._pin(cts),
            baud=baud,
            parity=parity,
            stop=stop,
            flow=flow,
            dma=dma,
            sync=sync,
        )
        self._fw.track(("uart", index), "uart.close", index)

    def send(self, index: int, data: bytes, cmd_timeout: float | None = None) -> None:
        if not data:
            raise ValueError("uart.send needs at least one byte")
        self._cmd("send", index, bytes(data), cmd_timeout=cmd_timeout)

    def recv(self, index: int, timeout: int | None = None, len: int | None = None) -> bytes:
        wait = self._fw.terminal.timeout + (timeout or 0) / 1000
        return self._cmd("recv", index, timeout=timeout, len=len, cmd_timeout=wait).as_bytes("data")

    def close(self, index: int) -> None:
        self._cmd("close", index)
        self._fw.untrack(("uart", index))


class Spi(_Group):
    prefix = "spi"

    def open(
        self,
        index: int,
        clk: Pin,
        mosi: Pin,
        miso: Pin,
        cs: Pin | None = None,
        baud: int | None = None,
        mode: int | None = None,
        sync: bool | None = None,
    ) -> None:
        self._cmd(
            "open",
            index,
            clk=self._pin(clk),
            mosi=self._pin(mosi),
            miso=self._pin(miso),
            cs=self._pin(cs),
            baud=baud,
            mode=mode,
            sync=sync,
        )
        self._fw.track(("spi", index), "spi.close", index)

    def xfer(self, index: int, tx: bytes, rx: int | None = None, continue_: bool | None = None) -> bytes:
        return self._cmd("xfer", index, bytes(tx), rx=rx, continue_=continue_).as_bytes("rx")

    def close(self, index: int) -> None:
        self._cmd("close", index)
        self._fw.untrack(("spi", index))


class Adc(_Group):
    prefix = "adc"

    def open(
        self,
        adc: int,
        seq: int,
        pins: Sequence[Pin] | None = None,
        sh: int | None = None,
        avg: int | Literal["off"] | None = None,
        delay: int | Literal["off"] | None = None,
        trigger: str | None = None,
        dcmp: Sequence[tuple[int, int, int]] | None = None,
        sync: bool | None = None,
    ) -> None:
        dcmp_list = None if dcmp is None else [f"{index}:{low}:{high}" for index, low, high in dcmp]
        self._cmd(
            "open",
            adc,
            seq,
            pins=self._pins(pins),
            sh=sh,
            avg=avg,
            delay=delay,
            trigger=trigger,
            dcmp=dcmp_list,
            sync=sync,
        )
        self._fw.track(("adc", adc, seq), "adc.close", adc, seq)

    def measure(self, adc: int, seq: int, n: int | None = None, cmd_timeout: float | None = None) -> list[int]:
        return self._cmd("measure", adc, seq, n=n, cmd_timeout=cmd_timeout).as_ints("samples")

    def close(self, adc: int, seq: int) -> None:
        self._cmd("close", adc, seq)
        self._fw.untrack(("adc", adc, seq))


class Comparator(_Group):
    prefix = "comp"

    def open(
        self,
        index: int,
        pos: Pin,
        neg: Pin,
        out: Pin | None = None,
        src: Literal["pin", "c0", "ref"] | None = None,
        ref: tuple[Literal["low", "high"], int] | None = None,
        invert: bool | None = None,
        sync: bool | None = None,
    ) -> None:
        self._cmd(
            "open",
            index,
            pos=self._pin(pos),
            neg=self._pin(neg),
            out=self._pin(out),
            src=src,
            ref=None if ref is None else [ref[0], ref[1]],
            invert=invert,
            sync=sync,
        )
        self._fw.track(("comp", index), "comp.close", index)

    def read(self, index: int) -> int:
        return self._cmd("read", index).as_int("out")

    def irq(self, index: int, edge: Edge) -> None:
        self._cmd("irq", index, edge)

    def count(self, index: int, clear: bool | None = None) -> int:
        return self._cmd("count", index, clear=clear).as_int("count")

    def close(self, index: int) -> None:
        self._cmd("close", index)
        self._fw.untrack(("comp", index))


class Qei(_Group):
    prefix = "qei"

    def open(
        self,
        index: int,
        a: Pin | None = None,
        b: Pin | None = None,
        idx: Pin | None = None,
        res: int | None = None,
        offset: int | None = None,
        inva: bool | None = None,
        invb: bool | None = None,
        invi: bool | None = None,
        reset: Literal["max", "index"] | None = None,
        cap: Literal["a", "ab"] | None = None,
        sig: Literal["quad", "clkdir"] | None = None,
        vel: int | None = None,
    ) -> None:
        self._cmd(
            "open",
            index,
            a=self._pin(a),
            b=self._pin(b),
            idx=self._pin(idx),
            res=res,
            offset=offset,
            inva=inva,
            invb=invb,
            invi=invi,
            reset=reset,
            cap=cap,
            sig=sig,
            vel=vel,
        )
        self._fw.track(("qei", index), "qei.close", index)

    def read(self, index: int) -> QeiReading:
        response = self._cmd("read", index)
        return QeiReading(
            pos=response.as_int("pos"),
            dir=response["dir"],
            speed=response.as_int("speed"),
            res=response.as_int("res"),
        )

    def close(self, index: int) -> None:
        self._cmd("close", index)
        self._fw.untrack(("qei", index))


class Can(_Group):
    prefix = "can"

    def open(
        self,
        index: int,
        rx: Pin | None = None,
        tx: Pin | None = None,
        bitrate: int | None = None,
        filter: tuple[int, int, bool] | None = None,
        loopback: bool | None = None,
        recover: bool | None = None,
    ) -> None:
        filter_value = None if filter is None else [f"0x{filter[0]:x}", f"0x{filter[1]:x}", int(bool(filter[2]))]
        self._cmd(
            "open",
            index,
            rx=self._pin(rx),
            tx=self._pin(tx),
            bitrate=bitrate,
            filter=filter_value,
            loopback=loopback,
            recover=recover,
        )
        self._fw.track(("can", index), "can.close", index)

    def send(self, index: int, id: int, data: bytes, ext: bool | None = None, cmd_timeout: float | None = None) -> None:
        self._cmd("send", index, f"0x{id:x}", bytes(data), ext=ext, cmd_timeout=cmd_timeout)

    def wait_frame(self, index: int, predicate: Callable[[CanFrame], bool] | None = None, timeout: float = 1.0) -> CanFrame:
        def accept(event: Event) -> bool:
            if "error" in event or "id" not in event or event.as_int("index") != index:
                return False
            return predicate is None or predicate(self.frame(event))

        return self.frame(self._fw.terminal.wait_event("can", accept, timeout))

    def errors(self, index: int) -> list[str]:
        events = self._fw.terminal.drain_events("can")
        return [event["error"] for event in events if "error" in event and event.as_int("index") == index]

    @staticmethod
    def frame(event: Event) -> CanFrame:
        return CanFrame(
            index=event.as_int("index"),
            id=event.as_int("id"),
            ext=event.as_bool("ext"),
            data=event.as_bytes("data"),
        )

    def close(self, index: int) -> None:
        self._cmd("close", index)
        self._fw.untrack(("can", index))


class Eeprom(_Group):
    prefix = "eeprom"

    def write(self, address: int, data: bytes) -> None:
        self._cmd("write", address, bytes(data))

    def read(self, address: int, len: int) -> bytes:
        return self._cmd("read", address, len).as_bytes("data")

    def erase(self, cmd_timeout: float = 10.0) -> None:
        self._cmd("erase", cmd_timeout=cmd_timeout)


class Watchdog(_Group):
    prefix = "wdt"

    def start(
        self,
        index: int,
        timeout: int,
        reset: bool | None = None,
        feed: Literal["auto", "manual"] | None = None,
    ) -> None:
        self._cmd("start", index, timeout=timeout, reset=reset, feed=feed)

    def feed(self, index: int) -> None:
        self._cmd("feed", index)

    def wait_warning(self, index: int, timeout: float) -> Event:
        return self._fw.terminal.wait_event("wdt", lambda event: event.as_int("index") == index, timeout)


class Ethernet(_Group):
    prefix = "eth"

    def open(self, phy: str | None = None, speed: Literal["auto", "10", "100"] | int | None = None) -> None:
        self._cmd("open", phy=phy, speed=speed)
        self._fw.track(("eth",), "eth.close")

    def status(self) -> EthStatus:
        response = self._cmd("status")
        return EthStatus(
            link=response["link"],
            speed=response.as_int("speed"),
            duplex=response["duplex"],
            rx=response.as_int("rx"),
            tx=response.as_int("tx"),
        )

    def close(self) -> None:
        self._cmd("close")
        self._fw.untrack(("eth",))


class Firmware:
    """Entry point: `fw.gpio.set("ledop", 1)`, `fw.pwm.open(0, freq=20000)`, ...

    Open instances are tracked so `close_all()` can restore a clean state between tests.
    Pins are sent as `P<port><index>` after resolving aliases with `aliases` (when given).
    """

    def __init__(self, terminal: FirmwareTerminal, aliases: Mapping[str, str] | None = None) -> None:
        self.terminal = terminal
        self.aliases = dict(aliases or {})
        self._open: dict[tuple[object, ...], tuple[str, tuple[object, ...]]] = {}
        self.system = System(self)
        self.gpio = Gpio(self)
        self.pwm = Pwm(self)
        self.uart = Uart(self)
        self.spi = Spi(self)
        self.adc = Adc(self)
        self.comp = Comparator(self)
        self.qei = Qei(self)
        self.can = Can(self)
        self.eeprom = Eeprom(self)
        self.wdt = Watchdog(self)
        self.eth = Ethernet(self)

    def command(self, name: str, *args: object, cmd_timeout: float | None = None, **options: object) -> Response:
        return self.terminal.command(format_command(name, *args, **options), timeout=cmd_timeout)

    def pin(self, pin: Pin) -> str:
        return normalize_pin(pin, self.aliases or None)

    def track(self, key: tuple[object, ...], close_command: str, *args: object) -> None:
        self._open[key] = (close_command, args)

    def untrack(self, key: tuple[object, ...]) -> None:
        self._open.pop(key, None)

    def forget_open(self) -> None:
        self._open.clear()

    @property
    def open_instances(self) -> list[tuple[object, ...]]:
        return list(self._open)

    def close_all(self) -> list[str]:
        """Close everything opened through this object; returns the commands that failed."""
        failures: list[str] = []
        for key, (name, args) in reversed(list(self._open.items())):
            try:
                self.command(name, *args)
            except FirmwareError as error:
                if error.reason != "notopen":
                    failures.append(f"{error.command}: {error.reason}")
            self._open.pop(key, None)
        return failures
