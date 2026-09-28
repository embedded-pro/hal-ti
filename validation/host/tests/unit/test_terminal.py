import pytest

from hal_ti_validation.instruments.fake import FakeFirmware, FakeSerial
from hal_ti_validation.terminal import (
    FirmwareError,
    FirmwareTerminal,
    LineAssembler,
    TerminalTimeout,
    clean_line,
)


def make_terminal(style="line", chunk=0, noise=False, **kwargs):
    firmware = FakeFirmware(style=style, noise=noise)
    serial = FakeSerial(firmware, chunk=chunk)
    return FirmwareTerminal(serial=serial, timeout=0.5, **kwargs), firmware


def test_clean_line_strips_prompt_escape_and_backspace():
    assert clean_line("> > pingx\b") == "ping"
    assert clean_line("\x1b[2K\x1b[1;5HOK\a value=1\x1b[K") == "OK value=1"
    assert clean_line(">") == ""


def test_assembler_final_followed_by_prompt():
    assembler = LineAssembler()
    assert assembler.feed(b"> ping\r\n") == ["ping"]
    assert assembler.feed(b"\r\nOK") == []
    assert assembler.feed(b"> ") == ["OK"]


def test_assembler_releases_unterminated_event_after_idle():
    assembler = LineAssembler(idle_flush=0.05)
    assert assembler.feed(b"\r\nEVT boot board=x", now=0.0) == []
    assert assembler.poll(now=0.01) == []
    assert assembler.poll(now=0.1) == ["EVT boot board=x"]


def test_assembler_keeps_partial_escape_sequence():
    assembler = LineAssembler(idle_flush=0.0)
    assert assembler.feed(b"OK value=1\x1b[", now=0.0) == []
    assert assembler.feed(b"K\r\n", now=1.0) == ["OK value=1"]


@pytest.mark.parametrize("style", ["line", "trace"])
@pytest.mark.parametrize("chunk", [0, 1, 3, 7])
@pytest.mark.parametrize("noise", [False, True])
def test_command_framing(style, chunk, noise):
    terminal, firmware = make_terminal(style, chunk, noise)
    terminal.wait_boot(1.0)
    assert terminal.command("ping").ok
    firmware.gpio_levels["PF1"] = 1
    assert terminal.command("gpio.get PF1").as_int("value") == 1
    info = terminal.command("info")
    assert info["board"] == "ek_tm4c123gxl"
    assert info["sysclk"] == "80000000"


@pytest.mark.parametrize("style", ["line", "trace"])
def test_events_between_command_and_final_line(style):
    terminal, firmware = make_terminal(style, chunk=5)
    firmware.emit("EVT gpio pin=PF1 count=3")
    firmware.emit("EVT wdt index=0 warning=1")
    assert terminal.command("ping").ok
    event = terminal.wait_event("wdt", timeout=0.1)
    assert event.as_int("warning") == 1
    assert [event.raw for event in terminal.drain_events("gpio")] == ["EVT gpio pin=PF1 count=3"]


def test_asynchronous_final_after_prompt():
    terminal, firmware = make_terminal("line")
    firmware.handlers["delay"] = _prompt_only
    pending = terminal.begin("delay 10")
    terminal.pump()
    firmware.output += b"\r\nOK\r\n"
    assert pending.wait().ok


def _prompt_only(fw, args, options):
    fw._write("> ")
    return None


def test_error_raises_firmware_error():
    terminal, _ = make_terminal()
    terminal.command("can.open 0")
    with pytest.raises(FirmwareError) as error:
        terminal.command("can.open 0")
    assert error.value.reason == "busy"
    assert error.value.command == "can.open 0"
    assert terminal.command("can.open 0", check=False).reason == "busy"


def test_unrecognized_command():
    terminal, _ = make_terminal()
    with pytest.raises(FirmwareError) as error:
        terminal.command("nosuch.command")
    assert error.value.reason == "unrecognized"


def test_timeout_without_final_line():
    terminal, firmware = make_terminal()
    firmware.handlers["hang"] = _prompt_only
    with pytest.raises(TerminalTimeout):
        terminal.command("hang", timeout=0.1)
    assert terminal.command("ping").ok


def test_reset_and_boot_event():
    terminal, _ = make_terminal("trace")
    terminal.wait_boot(1.0)
    terminal.send_nowait("reset")
    boot = terminal.wait_boot(1.0)
    assert boot["reset"] == "sw"


def test_sync_clears_partial_input():
    terminal, firmware = make_terminal()
    terminal.write_raw(b"gpio.se")
    terminal.sync()
    assert firmware.received[-1] == "ping"


def test_command_validation():
    terminal, _ = make_terminal(max_command_length=20)
    with pytest.raises(ValueError):
        terminal.command("x" * 21)
    with pytest.raises(ValueError):
        terminal.command("ping\r")


def test_context_manager_closes_serial():
    firmware = FakeFirmware()
    serial = FakeSerial(firmware)
    with FirmwareTerminal(serial=serial) as terminal:
        terminal.command("ping")
    assert serial.closed
