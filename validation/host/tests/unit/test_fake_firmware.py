import pytest
from ad3_waveforms_bench.terminal import FirmwareError, FirmwareTerminal

from hal_ti_validation.fake_firmware import FakeFirmware, FakeSerial


def make_terminal(style="line", chunk=0, noise=False, **kwargs):
    firmware = FakeFirmware(style=style, noise=noise)
    serial = FakeSerial(firmware, chunk=chunk)
    return FirmwareTerminal(serial=serial, timeout=0.5, **kwargs), firmware


@pytest.mark.parametrize("style", ["line", "trace"])
@pytest.mark.parametrize("chunk", [0, 3])
@pytest.mark.parametrize("noise", [False, True])
def test_command_framing(style, chunk, noise):
    terminal, firmware = make_terminal(style, chunk, noise)
    boot = terminal.wait_boot(1.0)
    assert boot["board"] == "ek_tm4c123gxl"
    assert boot["reset"] == "por"
    assert terminal.command("ping").ok
    firmware.gpio_levels["PF1"] = 1
    assert terminal.command("gpio.get PF1").as_int("value") == 1
    info = terminal.command("info")
    assert info["board"] == "ek_tm4c123gxl"
    assert info["sysclk"] == "80000000"


def test_open_close_busy_and_notopen():
    terminal, _ = make_terminal()
    terminal.command("can.open 0")
    with pytest.raises(FirmwareError) as error:
        terminal.command("can.open 0")
    assert error.value.reason == "busy"
    assert terminal.command("can.close 0").ok
    assert terminal.command("can.close 0", check=False).reason == "notopen"
    assert terminal.command("pwm.open 0")["pwmclk"] == "40000000"


def test_unrecognized_command():
    terminal, _ = make_terminal()
    with pytest.raises(FirmwareError) as error:
        terminal.command("nosuch.command")
    assert error.value.reason == "unrecognized"


def test_reset_reports_sw_and_closes_instances():
    terminal, firmware = make_terminal("trace")
    terminal.wait_boot(1.0)
    terminal.command("can.open 0")
    terminal.send_nowait("reset")
    boot = terminal.wait_boot(1.0)
    assert boot["reset"] == "sw"
    assert not firmware.opened


def test_sync_clears_partial_input():
    terminal, firmware = make_terminal()
    terminal.write_raw(b"gpio.se")
    terminal.sync()
    assert firmware.received[-1] == "ping"
