"""General commands, framing and error semantics (no AD3 needed)."""

import time

import pytest

from hal_ti_validation.terminal import FirmwareError


def test_ping(fw):
    fw.system.ping()


def test_info_matches_board(fw, board_cfg):
    info = fw.system.info()
    assert board_cfg.matches_firmware_name(info.board), info.raw
    assert info.family == board_cfg.family
    if board_cfg.sysclk:
        assert info.sysclk == board_cfg.sysclk


def test_board_pins_match_yaml(fw, board_cfg):
    reported = fw.system.pins()
    mismatches = {alias: (pin, reported.get(alias)) for alias, pin in board_cfg.pins.items() if reported.get(alias) != pin}
    assert not mismatches, f"alias: (yaml, firmware) {mismatches}"


def test_alias_is_accepted_as_pin(fw):
    fw.command("gpio.cfg", "perf", "out")
    fw.command("gpio.set", "perf", 0)
    fw.command("gpio.release", "perf")


def test_terminal_pins_are_reserved(fw, board_cfg):
    for pin in board_cfg.terminal.pins:
        with pytest.raises(FirmwareError) as error:
            fw.gpio.cfg(pin, "in")
        assert error.value.reason == "busy"


@pytest.mark.board_params("index", "system.reserved_uarts")
def test_terminal_uart_is_reserved(fw, index):
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(index)
    assert error.value.reason == "busy"


def test_unknown_command_and_key(fw):
    with pytest.raises(FirmwareError) as error:
        fw.command("no.such.command")
    assert error.value.reason in ("usage", "unrecognized")
    with pytest.raises(FirmwareError) as error:
        fw.command("ping", nosuchkey=1)
    assert error.value.reason == "usage"


@pytest.mark.board_params("line", "system.missing_instances")
def test_nonexistent_instance(fw, line):
    with pytest.raises(FirmwareError) as error:
        fw.terminal.command(line)
    assert error.value.reason == "range"


@pytest.mark.parametrize(
    ("name", "args"),
    [("gpio.set", ()), ("ping", ("extra",)), ("delay", ("abc",)), ("gpio.cfg", ("PA7", "sideways"))],
)
def test_usage_errors(fw, name, args):
    with pytest.raises(FirmwareError) as error:
        fw.command(name, *args)
    assert error.value.reason in ("usage", "range")


def test_notopen_and_busy(fw):
    with pytest.raises(FirmwareError) as error:
        fw.adc.close(1, 3)
    assert error.value.reason == "notopen"
    fw.adc.open(1, 3, pins=["phasea"])
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(1, 3, pins=["phasea"])
    assert error.value.reason == "busy"
    fw.adc.close(1, 3)


@pytest.mark.parametrize("ms", [10, 200])
def test_delay(fw, ms):
    start = time.monotonic()
    fw.system.delay(ms)
    assert time.monotonic() - start >= ms / 1000 * 0.95


@pytest.mark.resets_board
def test_reset_reports_boot(fw, board_cfg):
    boot = fw.system.reset(timeout=board_cfg.param("system.boot_timeout", 5.0))
    assert board_cfg.matches_firmware_name(boot.board)
    assert boot.family == board_cfg.family
    assert boot.reset == "sw"
    fw.system.ping()
    assert fw.system.info().reset == "sw"
