"""EEPROM (`hal::tiva::Eeprom`): erase, write/read patterns, boundaries and persistence across reset.

Scenarios: features/eeprom.feature.
"""

import pytest
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when

from hal_ti_validation import expect


@pytest.fixture
def eeprom(board_cfg):
    return board_cfg.param("eeprom")


def read_all(fw, size, chunk):
    return b"".join(fw.eeprom.read(address, min(chunk, size - address)) for address in range(0, size, chunk))


@scenario("eeprom.feature", "Erasing clears every byte")
def test_erase_clears_everything():
    pass


@pytest.mark.board_params("address", "eeprom.addresses")
@pytest.mark.board_params("pattern", "eeprom.patterns")
@scenario("eeprom.feature", "A pattern reads back without touching its neighbours")
def test_write_read(address, pattern):
    pass


@scenario("eeprom.feature", "Writing without erasing replaces the old bytes")
def test_overwrite_without_erase():
    pass


@scenario("eeprom.feature", "The largest write a command line can carry reads back")
def test_largest_write():
    pass


@scenario("eeprom.feature", "Accesses past the end are refused")
def test_boundaries():
    pass


@scenario("eeprom.feature", "The content persists across a reset")
def test_persists_across_reset():
    pass


@given(parsers.parse('"{data}" is written at address {at:d}'))
@when(parsers.parse('"{data}" is written at address {at:d}'))
def write_at(fw, data, at):
    fw.eeprom.write(at, bytes.fromhex(data))


@given("the pattern fits at the address")
def pattern_fits(eeprom, address, pattern):
    if address + len(bytes.fromhex(pattern)) > eeprom["size"]:
        pytest.skip("pattern does not fit at this address")


@given("the EEPROM is erased")
@when("the EEPROM is erased")
def erase(fw):
    fw.eeprom.erase()


@when("the pattern is written at the address")
def write_pattern(fw, address, pattern):
    fw.eeprom.write(address, bytes.fromhex(pattern))


@when("the largest pattern one command line can carry is written at address 0", target_fixture="written")
def write_largest(fw, board_cfg, eeprom):
    line_limit = expect.max_hex_payload(board_cfg.terminal.max_command_length, format_command("eeprom.write", eeprom["size"] - 1))
    size = min(line_limit, eeprom["max_transfer"])
    data = bytes((i * 13 + 1) & 0xFF for i in range(size))
    fw.eeprom.write(0, data)
    return data


@when(parsers.parse('"{data}" is written in the last {count:d} bytes'))
def write_last(fw, eeprom, data, count):
    fw.eeprom.write(eeprom["size"] - count, bytes.fromhex(data))


@when("the board resets")
def reset_board(fw, board_cfg):
    fw.system.reset(timeout=board_cfg.param("system.boot_timeout", 5.0))


@then("every byte reads ff")
def all_erased(fw, eeprom):
    assert read_all(fw, eeprom["size"], eeprom["max_chunk"]) == b"\xff" * eeprom["size"]


@then("the pattern reads back at the address")
def pattern_reads_back(fw, address, pattern):
    data = bytes.fromhex(pattern)
    assert fw.eeprom.read(address, len(data)) == data


@then(parsers.parse("the {margin:d} bytes on each side of it still read ff"))
def neighbours_untouched(fw, eeprom, address, pattern, margin):
    size = len(bytes.fromhex(pattern))
    start = max(0, address - margin)
    end = min(eeprom["size"], address + size + margin)
    around = fw.eeprom.read(start, end - start)
    assert around[: address - start] == b"\xff" * (address - start), "bytes before the write changed"
    assert around[address - start + size :] == b"\xff" * (end - address - size), "bytes after the write changed"


@then(parsers.parse('{count:d} bytes at address {at:d} read "{data}"'))
def reads_at(fw, count, at, data):
    assert fw.eeprom.read(at, count) == bytes.fromhex(data)


@then("it reads back in chunks")
def reads_back_in_chunks(fw, eeprom, written):
    assert read_all(fw, len(written), eeprom["max_chunk"]) == written


@then(parsers.parse('the last {count:d} bytes read "{data}"'))
def last_bytes_read(fw, eeprom, count, data):
    assert fw.eeprom.read(eeprom["size"] - count, count) == bytes.fromhex(data)


def refused(call, reason):
    with pytest.raises(FirmwareError) as error:
        call()
    assert error.value.reason == reason


@then(parsers.parse('writing {count:d} bytes {back:d} before the end fails with "{reason}"'))
def write_before_end_refused(fw, eeprom, count, back, reason):
    refused(lambda: fw.eeprom.write(eeprom["size"] - back, b"\x00" * count), reason)


@then(parsers.parse('reading {count:d} bytes {back:d} before the end fails with "{reason}"'))
def read_before_end_refused(fw, eeprom, count, back, reason):
    refused(lambda: fw.eeprom.read(eeprom["size"] - back, count), reason)


@then(parsers.parse('reading {count:d} byte at the end fails with "{reason}"'))
def read_at_end_refused(fw, eeprom, count, reason):
    refused(lambda: fw.eeprom.read(eeprom["size"], count), reason)
