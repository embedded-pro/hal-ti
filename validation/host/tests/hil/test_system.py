"""General commands, framing and error semantics (no AD3 needed)."""

import time

import pytest
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation.protocol import is_alias


def test_ping(fw):
    fw.system.ping()


def test_info_matches_board(fw, board_cfg):
    info = fw.system.info()
    assert board_cfg.matches_firmware_name(info.board), info.raw
    assert info.family == board_cfg.family
    if board_cfg.sysclk:
        assert info.sysclk == board_cfg.sysclk


def test_board_pins_match_yaml(fw, board_cfg):
    """`board.pins` and the board file's `pins` are the same alias table, in both directions."""
    reported = fw.system.pins()
    mismatches = {alias: (pin, reported.get(alias)) for alias, pin in board_cfg.pins.items() if reported.get(alias) != pin}
    assert not mismatches, f"alias: (yaml, firmware) {mismatches}"
    extra = {alias: pin for alias, pin in reported.items() if alias not in board_cfg.pins}
    assert not extra, f"aliases the board file lacks: {extra}"


def test_aliases_are_generic(board_cfg):
    assert all(is_alias(alias) for alias in board_cfg.pins), sorted(board_cfg.pins)
    assert {"terminaltx", "terminalrx"} <= set(board_cfg.pins)


def test_every_alias_is_accepted_as_pin(fw, board_cfg):
    """Each alias names its pin in commands; the terminal pins stay reserved."""
    for alias, pin in board_cfg.pins.items():
        if pin in board_cfg.terminal.pins:
            with pytest.raises(FirmwareError) as error:
                fw.command("gpio.cfg", alias, "in")
            assert error.value.reason == "busy", alias
            continue
        fw.command("gpio.cfg", alias, "in")
        fw.command("gpio.get", pin)
        fw.command("gpio.release", alias)


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
    fw.adc.open(1, 3, pins=["ain0"], sync=True)
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(1, 3, pins=["ain0"], sync=True)
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
