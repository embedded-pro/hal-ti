"""EEPROM (`hal::tiva::Eeprom`): erase, write/read patterns, boundaries and persistence across reset."""

import pytest
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation import expect


@pytest.fixture
def eeprom(board_cfg):
    return board_cfg.param("eeprom")


def read_all(fw, size, chunk):
    return b"".join(fw.eeprom.read(address, min(chunk, size - address)) for address in range(0, size, chunk))


@pytest.mark.slow
def test_erase_clears_everything(fw, eeprom):
    fw.eeprom.write(0, b"\x00" * 8)
    fw.eeprom.erase()
    assert read_all(fw, eeprom["size"], eeprom["max_chunk"]) == b"\xff" * eeprom["size"]


@pytest.mark.board_params("address", "eeprom.addresses")
@pytest.mark.board_params("pattern", "eeprom.patterns")
def test_write_read(fw, eeprom, address, pattern):
    data = bytes.fromhex(pattern)
    if address + len(data) > eeprom["size"]:
        pytest.skip("pattern does not fit at this address")
    fw.eeprom.erase()
    fw.eeprom.write(address, data)
    assert fw.eeprom.read(address, len(data)) == data
    start = max(0, address - 4)
    end = min(eeprom["size"], address + len(data) + 4)
    around = fw.eeprom.read(start, end - start)
    assert around[: address - start] == b"\xff" * (address - start), "bytes before the write changed"
    assert around[address - start + len(data) :] == b"\xff" * (end - address - len(data)), "bytes after the write changed"


def test_overwrite_without_erase(fw):
    fw.eeprom.write(16, bytes.fromhex("00112233"))
    fw.eeprom.write(16, bytes.fromhex("ffeeddcc"))
    assert fw.eeprom.read(16, 4) == bytes.fromhex("ffeeddcc")


def test_largest_write(fw, board_cfg, eeprom):
    line_limit = expect.max_hex_payload(board_cfg.terminal.max_command_length, format_command("eeprom.write", eeprom["size"] - 1))
    size = min(line_limit, eeprom["max_transfer"])
    data = bytes((i * 13 + 1) & 0xFF for i in range(size))
    fw.eeprom.write(0, data)
    read = b"".join(fw.eeprom.read(offset, min(eeprom["max_chunk"], size - offset)) for offset in range(0, size, eeprom["max_chunk"]))
    assert read == data


def test_boundaries(fw, eeprom):
    size = eeprom["size"]
    fw.eeprom.write(size - 4, b"\xa1\xb2\xc3\xd4")
    assert fw.eeprom.read(size - 4, 4) == b"\xa1\xb2\xc3\xd4"
    for call in (lambda: fw.eeprom.write(size - 2, b"\x00" * 4), lambda: fw.eeprom.read(size - 2, 4), lambda: fw.eeprom.read(size, 1)):
        with pytest.raises(FirmwareError) as error:
            call()
        assert error.value.reason == "range"


@pytest.mark.resets_board
def test_persists_across_reset(fw, board_cfg):
    data = bytes.fromhex("c0ffee00deadbeef")
    fw.eeprom.write(64, data)
    fw.system.reset(timeout=board_cfg.param("system.boot_timeout", 5.0))
    assert fw.eeprom.read(64, len(data)) == data
