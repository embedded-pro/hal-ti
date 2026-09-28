"""CAN (`hal::tiva::Can`): internal loopback test mode (no wiring) and logic-level traffic with the AD3.

The LaunchPads have no CAN transceiver. Wiring set `can` builds a wired-AND bus from diodes and a pull-up
(see the README), so the controller sees its own bits and the AD3 frames without transceivers.
"""

import pytest
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError, TerminalTimeout


@pytest.fixture
def can(board_cfg):
    return board_cfg.param("can")


def frame_id(frame):
    return int(frame["id"])


def frame_data(frame):
    return bytes.fromhex(frame["data"] or "")


def matches(frame):
    return lambda received: received.id == frame_id(frame) and received.ext == bool(frame["ext"])


@pytest.mark.board_params("bitrate", "can.bitrates")
@pytest.mark.board_params("frame", "can.frames")
def test_loopback(fw, can, bitrate, frame):
    fw.can.open(can["index"], rx=can["rx"], tx=can["tx"], bitrate=bitrate, loopback=True)
    fw.can.send(can["index"], frame_id(frame), frame_data(frame), ext=bool(frame["ext"]))
    received = fw.can.wait_frame(can["index"], matches(frame), timeout=1.0)
    assert received.data == frame_data(frame)


def test_acceptance_filter(fw, can):
    config = can["filter"]
    fw.can.open(
        can["index"],
        rx=can["rx"],
        tx=can["tx"],
        bitrate=can["bitrates"][0],
        filter=(config["id"], config["mask"], bool(config["ext"])),
        loopback=True,
    )
    for ident in config["accepted"]:
        fw.can.send(can["index"], ident, b"\x01")
        assert fw.can.wait_frame(can["index"], lambda f, i=ident: f.id == i, timeout=1.0).data == b"\x01"
    for ident in config["rejected"]:
        fw.can.send(can["index"], ident, b"\x02")
    fw.system.delay(100)
    leaked = [event.raw for event in fw.terminal.drain_events("can") if "id" in event]
    assert not leaked, f"frames outside the filter were received: {leaked}"


def test_send_requires_open(fw, can):
    with pytest.raises(FirmwareError) as error:
        fw.can.send(can["index"], 0x100, b"\x00")
    assert error.value.reason == "notopen"


@pytest.mark.ad3
@pytest.mark.board_params("bitrate", "can.bitrates")
@pytest.mark.board_params("frame", "can.frames")
def test_ad3_to_firmware(fw, ad3, need, can, bitrate, frame):
    ad3_tx, ad3_rx = need.dio(role="can_ad3_tx"), need.dio(role="can_ad3_rx")
    fw.can.open(can["index"], rx=can["rx"], tx=can["tx"], bitrate=bitrate)
    ad3.can.configure(tx=ad3_tx, rx=ad3_rx, bitrate=bitrate)
    ad3.can.send(frame_id(frame), frame_data(frame), ext=bool(frame["ext"]))
    received = fw.can.wait_frame(can["index"], matches(frame), timeout=1.0)
    assert received.data == frame_data(frame)


@pytest.mark.ad3
@pytest.mark.board_params("bitrate", "can.bitrates")
@pytest.mark.board_params("frame", "can.frames")
def test_firmware_to_ad3(fw, ad3, need, can, bitrate, frame):
    ad3_tx, ad3_rx = need.dio(role="can_ad3_tx"), need.dio(role="can_ad3_rx")
    fw.can.open(can["index"], rx=can["rx"], tx=can["tx"], bitrate=bitrate)
    ad3.can.configure(tx=ad3_tx, rx=ad3_rx, bitrate=bitrate)
    command = format_command("can.send", can["index"], f"0x{frame_id(frame):x}", frame_data(frame), ext=bool(frame["ext"]))
    pending = fw.terminal.begin(command, timeout=3.0)
    received = ad3.can.receive(timeout=1.0)
    try:
        outcome = pending.wait(check=False).reason or "ok"
    except TerminalTimeout:
        outcome = "no answer"
    assert received is not None, "the AD3 did not receive the frame"
    assert (received.id, received.ext, received.data) == (frame_id(frame), bool(frame["ext"]), frame_data(frame))
    if can["ad3_acknowledges"]:
        assert outcome == "ok"
    else:
        assert outcome in ("ok", "failed", "timeout", "no answer")
