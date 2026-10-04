"""General commands, framing and error semantics (no AD3 needed).

Scenarios: features/system.feature.
"""

import time

import pytest
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import parsers, scenario, then, when

from hal_ti_validation.protocol import is_alias


@scenario("system.feature", "The board answers ping")
def test_ping():
    pass


@scenario("system.feature", "The board info matches the board file")
def test_info_matches_board():
    pass


@scenario("system.feature", "The firmware and the board file have the same pin aliases")
def test_board_pins_match_yaml():
    pass


@scenario("system.feature", "The board file uses generic aliases, including the terminal pins")
def test_aliases_are_generic():
    pass


@scenario("system.feature", "Every alias names its pin in commands")
def test_every_alias_is_accepted_as_pin():
    pass


@scenario("system.feature", "The terminal pins are reserved")
def test_terminal_pins_are_reserved():
    pass


@scenario("system.feature", "The debug LED is reserved")
def test_debug_led_is_reserved():
    pass


@pytest.mark.board_params("index", "system.reserved_uarts")
@scenario("system.feature", "The terminal UART is reserved")
def test_terminal_uart_is_reserved(index):
    pass


@scenario("system.feature", "Unknown commands and keys are refused")
def test_unknown_command_and_key():
    pass


@pytest.mark.board_params("line", "system.missing_instances")
@scenario("system.feature", "A command on a missing instance is refused")
def test_nonexistent_instance(line):
    pass


@pytest.mark.parametrize(
    ("name", "args"),
    [("gpio.set", ()), ("ping", ("extra",)), ("delay", ("abc",)), ("gpio.cfg", ("PA7", "sideways"))],
)
@scenario("system.feature", "Malformed commands are refused")
def test_usage_errors(name, args):
    pass


@scenario("system.feature", "Closing what is not open and opening what is open are refused")
def test_notopen_and_busy():
    pass


@pytest.mark.parametrize("ms", [10, 200])
@scenario("system.feature", "A delay lasts at least the requested time")
def test_delay(ms):
    pass


@scenario("system.feature", "A reset reports a software reset")
def test_reset_reports_boot():
    pass


@when("the board info is read", target_fixture="info")
def read_info(fw):
    return fw.system.info()


@when("the firmware lists its pin aliases", target_fixture="reported")
def list_pins(fw):
    return fw.system.pins()


@when(parsers.parse("ADC {adc_unit:d} sequencer {seq:d} is opened in synchronous mode on {pin}"))
def open_adc(fw, adc_unit, seq, pin):
    fw.adc.open(adc_unit, seq, pins=[pin], sync=True)


@when("the firmware delays for the time, timed on the host", target_fixture="elapsed")
def timed_delay(fw, ms):
    start = time.monotonic()
    fw.system.delay(ms)
    return time.monotonic() - start


@when("the board resets", target_fixture="boot")
def reset_board(fw, board_cfg):
    return fw.system.reset(timeout=board_cfg.param("system.boot_timeout", 5.0))


@then("the board answers ping")
def answers_ping(fw):
    fw.system.ping()


@then("it names the board of the board file")
def info_names_board(board_cfg, info):
    assert board_cfg.matches_firmware_name(info.board), info.raw


@then("it reports the family of the board file")
def info_reports_family(board_cfg, info):
    assert info.family == board_cfg.family


@then("it reports the system clock of the board file, if the board file gives one")
def info_reports_sysclk(board_cfg, info):
    if board_cfg.sysclk:
        assert info.sysclk == board_cfg.sysclk


@then("every alias of the board file names the same pin in the firmware")
def aliases_match(board_cfg, reported):
    mismatches = {alias: (pin, reported.get(alias)) for alias, pin in board_cfg.pins.items() if reported.get(alias) != pin}
    assert not mismatches, f"alias: (yaml, firmware) {mismatches}"


@then("the firmware has no alias the board file lacks")
def no_extra_aliases(board_cfg, reported):
    extra = {alias: pin for alias, pin in reported.items() if alias not in board_cfg.pins}
    assert not extra, f"aliases the board file lacks: {extra}"


@then("every alias of the board file is a generic alias")
def aliases_generic(board_cfg):
    assert all(is_alias(alias) for alias in board_cfg.pins), sorted(board_cfg.pins)


@then(parsers.parse("the board file has the aliases {first} and {second}"))
def has_terminal_aliases(board_cfg, first, second):
    assert {first, second} <= set(board_cfg.pins)


@then(
    parsers.parse(
        "every alias of the board file configures as a GPIO input, its pin reads and the alias releases, "
        'except that configuring an alias of a terminal pin fails with "{refusal}"'
    )
)
def aliases_accepted(fw, board_cfg, refusal):
    for alias, pin in board_cfg.pins.items():
        if pin in board_cfg.terminal.pins:
            with pytest.raises(FirmwareError) as error:
                fw.command("gpio.cfg", alias, "in")
            assert error.value.reason == refusal, alias
            continue
        fw.command("gpio.cfg", alias, "in")
        fw.command("gpio.get", pin)
        fw.command("gpio.release", alias)


@then(parsers.parse('configuring each terminal pin as a GPIO input fails with "{refusal}"'))
def terminal_pins_refused(fw, board_cfg, refusal):
    for pin in board_cfg.terminal.pins:
        with pytest.raises(FirmwareError) as error:
            fw.gpio.cfg(pin, "in")
        assert error.value.reason == refusal


@then(parsers.parse('configuring the debug LED pin as a GPIO input fails with "{refusal}"'))
def debug_led_refused(fw, board_cfg, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.gpio.cfg(board_cfg.param("system.debug_led"), "in")
    assert error.value.reason == refusal


@then(parsers.parse('opening the reserved UART fails with "{refusal}"'))
def terminal_uart_refused(fw, index, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(index)
    assert error.value.reason == refusal


@then(parsers.parse('the command "{command}" fails with "{one}" or "{other}"'))
def unknown_command_refused(fw, command, one, other):
    with pytest.raises(FirmwareError) as error:
        fw.command(command)
    assert error.value.reason in (one, other)


@then(parsers.parse('the command "{command}" with {key}={value:d} fails with "{refusal}"'))
def unknown_key_refused(fw, command, key, value, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.command(command, **{key: value})
    assert error.value.reason == refusal


@then(parsers.parse('the command line of the missing instance fails with "{refusal}"'))
def missing_instance_refused(fw, line, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.terminal.command(line)
    assert error.value.reason == refusal


@then(parsers.parse('the command with the arguments fails with "{one}" or "{other}"'))
def usage_refused(fw, name, args, one, other):
    with pytest.raises(FirmwareError) as error:
        fw.command(name, *args)
    assert error.value.reason in (one, other)


@then(parsers.parse('closing ADC {adc_unit:d} sequencer {seq:d} fails with "{refusal}"'))
def close_not_open_refused(fw, adc_unit, seq, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.adc.close(adc_unit, seq)
    assert error.value.reason == refusal


@then(parsers.parse('opening ADC {adc_unit:d} sequencer {seq:d} in synchronous mode on {pin} again fails with "{refusal}"'))
def reopen_refused(fw, adc_unit, seq, pin, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.adc.open(adc_unit, seq, pins=[pin], sync=True)
    assert error.value.reason == refusal


@then(parsers.parse("ADC {adc_unit:d} sequencer {seq:d} closes"))
def adc_closes(fw, adc_unit, seq):
    fw.adc.close(adc_unit, seq)


@then(parsers.parse("it took at least {percent:d} % of the time"))
def delay_long_enough(ms, elapsed, percent):
    assert elapsed >= ms / 1000 * (percent / 100)


@then(parsers.parse('the boot message names the board and the family of the board file and the reset cause "{cause}"'))
def boot_reports_reset(board_cfg, boot, cause):
    assert board_cfg.matches_firmware_name(boot.board)
    assert boot.family == board_cfg.family
    assert boot.reset == cause


@then(parsers.parse('the board info reports the reset cause "{cause}"'))
def info_reports_reset(fw, cause):
    assert fw.system.info().reset == cause
