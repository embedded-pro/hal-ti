"""CAN (`hal::tiva::Can`) over the link `--can-mode` selects:

- `loopback`: the controller's internal test mode (`loopback=1`); needs no wiring;
- `bus`: CAN0 through a 3.3 V transceiver to a CANable (wiring set `can`, `--can-peer`); every frame goes
  from the firmware to the CANable and back.

The `test_bus_*` tests need the CANable whatever `--can-mode` is: a missing acknowledge (CANable listen-only),
receive errors and bus off (CANable at a wrong bit rate) and the automatic bus-off recovery.
"""

from __future__ import annotations

import pytest
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation import expect
from hal_ti_validation.can_peer import CanPeer
from hal_ti_validation.firmware import settle

ERROR_NAMES = {
    "stuffError",
    "formError",
    "ackError",
    "bit1Error",
    "bit0Error",
    "crcError",
    "busOff",
    "errorWarning",
    "errorPassive",
    "messageLost",
}


@pytest.fixture
def can(board_cfg):
    return board_cfg.param("can")


@pytest.fixture
def peer(request, link) -> CanPeer | None:
    return request.getfixturevalue("can_peer") if link == "bus" else None


@pytest.fixture
def no_fake(pytestconfig):
    if pytestconfig.getoption("--fake"):
        pytest.skip("the fake bus does not model bit errors")


def make_frame(can, ext, dlc):
    ids = can["ids"]["ext" if ext else "std"]
    return {"id": ids[dlc % len(ids)], "ext": bool(ext), "data": bytes((dlc * 16 + i) & 0xFF for i in range(dlc))}


def matches(frame):
    return lambda received: received.id == frame["id"] and received.ext == frame["ext"]


def open_can(fw, can, link=None, **options):
    if link == "loopback":
        options["loopback"] = True
    fw.can.open(can["index"], rx=can["rx"], tx=can["tx"], **options)


def attach(peer, bitrate, listen_only=False):
    bitrate = int(round(bitrate))
    if not peer.supports(bitrate, listen_only):
        pytest.skip(f"{peer} cannot run at {bitrate} bit/s{' listen-only' if listen_only else ''}")
    peer.configure(bitrate, listen_only)


def transfer(fw, can, peer, frame):
    """Loopback: the controller receives its own frame. Bus: the CANable receives it and sends it back."""
    fw.can.send(can["index"], frame["id"], frame["data"], ext=frame["ext"])
    if peer is not None:
        received = peer.receive(timeout=1.0, predicate=matches(frame))
        assert received is not None, f"{peer} did not receive the frame"
        assert received.data == frame["data"]
        peer.send(frame["id"], frame["data"], ext=frame["ext"])
    received = fw.can.wait_frame(can["index"], matches(frame), timeout=1.0)
    assert received.data == frame["data"]


@pytest.mark.matrix("can.frames")
def test_frames(fw, can, link, peer, bitrate, ext, dlc):
    if peer is not None:
        attach(peer, bitrate)
    open_can(fw, can, link, bitrate=bitrate)
    transfer(fw, can, peer, make_frame(can, ext, dlc))


@pytest.mark.board_params("timing", "can.timings")
def test_explicit_timing(fw, can, link, peer, timing):
    """On the bus, the CANable runs at sysclk / (brp * (1 + tseg1 + tseg2)): frames only pass at that bit rate."""
    if peer is not None:
        attach(peer, expect.can_timing_bitrate(fw.system.info().sysclk, *timing))
    open_can(fw, can, link, timing=tuple(timing))
    for ext in (0, 1):
        transfer(fw, can, peer, make_frame(can, ext, 8))


@pytest.mark.board_params("case", "can.filters")
def test_acceptance_filter(fw, can, link, peer, case):
    """Standard and extended filters, with and without `match` (id type ignored when 0). On the bus the CANable
    sends the frames."""
    bitrate = can["bus"]["bitrate"]
    if peer is not None:
        attach(peer, bitrate)
    ident, mask, ext, match = case["filter"]
    open_can(fw, can, link, bitrate=bitrate, filter=(ident, mask, bool(ext), bool(match)))

    def send(frame_id, frame_ext, data):
        if peer is None:
            fw.can.send(can["index"], frame_id, data, ext=bool(frame_ext))
        else:
            peer.send(frame_id, data, ext=bool(frame_ext))

    for frame_id, frame_ext in case["accepted"]:
        send(frame_id, frame_ext, b"\x01")
        received = fw.can.wait_frame(can["index"], lambda f, i=frame_id: f.id == i, timeout=1.0)
        assert (received.ext, received.data) == (bool(frame_ext), b"\x01")
    for frame_id, frame_ext in case["rejected"]:
        send(frame_id, frame_ext, b"\x02")
    fw.system.delay(100)
    leaked = [event.raw for event in fw.terminal.drain_events("can") if "id" in event]
    assert not leaked, f"frames outside the filter were received: {leaked}"


def test_send_requires_open(fw, can):
    with pytest.raises(FirmwareError) as error:
        fw.can.send(can["index"], 0x100, b"\x00")
    assert error.value.reason == "notopen"


@pytest.mark.board_params(
    "options,reason",
    values=[
        [{"bitrate": 123457}, "range"],
        [{"bitrate": 500000, "timing": (12, 3, 1, 10)}, "usage"],
        [{"timing": (17, 3, 1, 10)}, "range"],
        [{"timing": (12, 9, 1, 10)}, "range"],
        [{"timing": (12, 3, 5, 10)}, "range"],
        [{"timing": (12, 3, 1, 1025)}, "range"],
        [{"filter": (0x800, 0x7FF, False)}, "range"],
    ],
)
def test_open_errors(fw, can, options, reason):
    with pytest.raises(FirmwareError) as error:
        open_can(fw, can, **options)
    assert error.value.reason == reason


def test_timing_needs_four_fields(fw, can):
    with pytest.raises(FirmwareError) as error:
        fw.command("can.open", can["index"], timing="12,3,1", rx=fw.pin(can["rx"]), tx=fw.pin(can["tx"]))
    assert error.value.reason == "usage"


def test_default_pins(fw, can):
    """CAN0 opens on `can0rx`/`can0tx` without pins; a single pin is not enough."""
    fw.can.open(0, loopback=True)
    fw.can.close(0)
    with pytest.raises(FirmwareError) as error:
        fw.can.open(0, rx=can["rx"])
    assert error.value.reason == "usage"


def begin_send(fw, can, frame):
    command = format_command("can.send", can["index"], f"0x{frame['id']:x}", frame["data"], ext=frame["ext"])
    return fw.terminal.begin(command, timeout=3.0)


def test_bus_no_acknowledge(fw, can, can_peer, no_fake):
    """A listen-only CANable sees the frame but does not acknowledge it: the send fails with `ackError`."""
    bitrate = can["bus"]["bitrate"]
    attach(can_peer, bitrate, listen_only=True)
    open_can(fw, can, bitrate=bitrate)
    frame = make_frame(can, 0, 4)
    pending = begin_send(fw, can, frame)
    try:
        received = can_peer.receive(timeout=1.0, predicate=matches(frame))
    finally:
        response = settle(pending)
    assert received is not None and received.data == frame["data"], "the listen-only CANable did not see the frame"
    assert response is not None and response.reason in ("failed", "timeout"), f"unacknowledged send answered {response}"
    errors = fw.can.errors(can["index"])
    assert "ackError" in errors, f"no ackError reported: {errors}"


def test_bus_receive_errors(fw, can, can_peer, no_fake):
    """Frames sent by the CANable at a wrong bit rate are reported as receive errors (`EVT can error=`)."""
    if not can_peer.adjustable:
        pytest.skip(f"{can_peer} has a fixed bit rate")
    attach(can_peer, can["bus"]["wrong_bitrate"])
    open_can(fw, can, bitrate=can["bus"]["bitrate"])
    can_peer.send(0x000, bytes(8))
    fw.system.delay(150)
    errors = fw.can.errors(can["index"])
    assert errors, "no error reported for frames at a wrong bit rate"
    assert set(errors) <= ERROR_NAMES


@pytest.mark.board_params("recover", "can.recover")
def test_bus_off_and_recovery(fw, can, can_peer, no_fake, recover):
    """The CANable at a wrong bit rate destroys every attempt of an all-dominant frame with error flags, so the
    controller's transmit error counter reaches bus off. Back at the right bit rate, `recover=1` returns to the
    bus by itself and the pending frame leaves; with `recover=0` it stays off."""
    if not can_peer.adjustable:
        pytest.skip(f"{can_peer} has a fixed bit rate")
    bitrate = can["bus"]["bitrate"]
    attach(can_peer, can["bus"]["wrong_bitrate"])
    open_can(fw, can, bitrate=bitrate, recover=recover)
    frame = {"id": 0x000, "ext": False, "data": bytes(8)}
    pending = begin_send(fw, can, frame)
    try:
        fw.terminal.wait_event("can", lambda event: event.get("error") == "busOff", timeout=2.0)
    finally:
        settle(pending)
    attach(can_peer, bitrate)
    received = can_peer.receive(timeout=1.0, predicate=matches(frame))
    if recover:
        assert received is not None, "no frame after the automatic bus-off recovery"
    else:
        assert received is None, "the controller left bus off although recover=0"
