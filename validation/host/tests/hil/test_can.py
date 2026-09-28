"""CAN (`hal::tiva::Can`): internal loopback test mode (no wiring) and logic-level traffic with the AD3.

The LaunchPads have no CAN transceiver. Wiring set `can` builds a wired-AND bus from diodes and a pull-up
(see the README), so the controller sees its own bits and the AD3 frames without transceivers. The AD3 CAN
receiver's acknowledge behaviour is unverified, so firmware-to-AD3 transfers accept `ERR failed` as outcome.
"""

from __future__ import annotations

import pytest
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation import expect
from hal_ti_validation.firmware import settle


@pytest.fixture
def can(board_cfg):
    return board_cfg.param("can")


def make_frame(can, ext, dlc):
    ids = can["ids"]["ext" if ext else "std"]
    return {"id": ids[dlc % len(ids)], "ext": bool(ext), "data": bytes((dlc * 16 + i) & 0xFF for i in range(dlc))}


def matches(frame):
    return lambda received: received.id == frame["id"] and received.ext == frame["ext"]


def open_can(fw, can, **options):
    fw.can.open(can["index"], rx=can["rx"], tx=can["tx"], **options)


def send_and_expect(fw, can, frame):
    fw.can.send(can["index"], frame["id"], frame["data"], ext=frame["ext"])
    received = fw.can.wait_frame(can["index"], matches(frame), timeout=1.0)
    assert received.data == frame["data"]


@pytest.mark.matrix("can.frames")
def test_loopback(fw, can, bitrate, ext, dlc):
    open_can(fw, can, bitrate=bitrate, loopback=True)
    send_and_expect(fw, can, make_frame(can, ext, dlc))


@pytest.mark.board_params("timing", "can.timings")
def test_loopback_with_explicit_timing(fw, can, timing):
    open_can(fw, can, timing=tuple(timing), loopback=True)
    for ext in (0, 1):
        send_and_expect(fw, can, make_frame(can, ext, 8))


@pytest.mark.board_params("case", "can.filters")
def test_acceptance_filter(fw, can, case):
    """Standard and extended filters, with and without `match` (id type ignored when 0)."""
    ident, mask, ext, match = case["filter"]
    open_can(fw, can, bitrate=500000, filter=(ident, mask, bool(ext), bool(match)), loopback=True)
    for frame_id, frame_ext in case["accepted"]:
        fw.can.send(can["index"], frame_id, b"\x01", ext=bool(frame_ext))
        received = fw.can.wait_frame(can["index"], lambda f, i=frame_id: f.id == i, timeout=1.0)
        assert (received.ext, received.data) == (bool(frame_ext), b"\x01")
    for frame_id, frame_ext in case["rejected"]:
        fw.can.send(can["index"], frame_id, b"\x02", ext=bool(frame_ext))
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


def ad3_bus(ad3, need, bitrate):
    tx, rx = need.dio(role="can_ad3_tx"), need.dio(role="can_ad3_rx")
    ad3.can.configure(tx=tx, rx=rx, bitrate=int(round(bitrate)))
    return tx


def firmware_to_ad3(fw, ad3, can, frame):
    command = format_command("can.send", can["index"], f"0x{frame['id']:x}", frame["data"], ext=frame["ext"])
    pending = fw.terminal.begin(command, timeout=3.0)
    try:
        received = ad3.can.receive(timeout=1.0)
    finally:
        response = settle(pending)
    outcome = "no answer" if response is None else response.reason or "ok"
    assert received is not None, "the AD3 did not receive the frame"
    assert (received.id, received.ext, received.data) == (frame["id"], frame["ext"], frame["data"])
    if can["ad3_acknowledges"]:
        assert outcome == "ok"
    else:
        assert outcome in ("ok", "failed", "timeout", "no answer")


@pytest.mark.ad3
@pytest.mark.matrix("can.frames")
def test_ad3_to_firmware(fw, ad3, need, can, bitrate, ext, dlc):
    frame = make_frame(can, ext, dlc)
    open_can(fw, can, bitrate=bitrate)
    ad3_bus(ad3, need, bitrate)
    ad3.can.send(frame["id"], frame["data"], ext=frame["ext"])
    received = fw.can.wait_frame(can["index"], matches(frame), timeout=1.0)
    assert received.data == frame["data"]


@pytest.mark.ad3
@pytest.mark.matrix("can.frames")
def test_firmware_to_ad3(fw, ad3, need, can, bitrate, ext, dlc):
    open_can(fw, can, bitrate=bitrate)
    ad3_bus(ad3, need, bitrate)
    firmware_to_ad3(fw, ad3, can, make_frame(can, ext, dlc))


@pytest.mark.ad3
@pytest.mark.board_params("timing", "can.timings")
def test_explicit_timing_on_the_bus(fw, ad3, need, can, timing):
    """The AD3 runs at sysclk / (brp * (1 + tseg1 + tseg2)): frames only decode at the programmed bit rate."""
    bitrate = expect.can_timing_bitrate(fw.system.info().sysclk, *timing)
    open_can(fw, can, timing=tuple(timing))
    ad3_bus(ad3, need, bitrate)
    frame = make_frame(can, 1, 8)
    ad3.can.send(frame["id"], frame["data"], ext=True)
    assert fw.can.wait_frame(can["index"], matches(frame), timeout=1.0).data == frame["data"]
    firmware_to_ad3(fw, ad3, can, make_frame(can, 0, 3))


@pytest.mark.ad3
def test_error_events(fw, ad3, need, can):
    """Dominant bursts forced onto the bus by the pattern generator are reported as `EVT can error=`."""
    burst = can["error_burst"]
    open_can(fw, can, bitrate=500000)
    tx = need.dio(role="can_ad3_tx")
    ad3.pattern.pulses(tx, burst["pulses"], burst["frequency_hz"], duty=burst["duty"], idle="high")
    ad3.pattern.wait_done(timeout=burst["pulses"] / burst["frequency_hz"] + 2)
    fw.system.delay(150)
    errors = fw.can.errors(can["index"])
    assert errors, "no error reported for dominant bursts on the bus"
    assert set(errors) <= {
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


@pytest.mark.ad3
@pytest.mark.board_params("recover", "can.recover")
def test_bus_off_and_recovery(fw, ad3, need, can, recover):
    """Dominant bits injected into a transmission drive the controller to bus off; with `recover=1` it returns
    to the bus by itself and the pending frame leaves, with `recover=0` it stays off."""
    bitrate = 500000
    open_can(fw, can, bitrate=bitrate, recover=recover)
    tx = need.dio(role="can_ad3_tx")
    ad3.pattern.pulses(tx, 20000, bitrate / 20, duty=0.1, idle="high")
    frame = make_frame(can, 0, 8)
    pending = fw.terminal.begin(format_command("can.send", can["index"], f"0x{frame['id']:x}", frame["data"]), timeout=3.0)
    try:
        fw.terminal.wait_event("can", lambda event: event.get("error") == "busOff", timeout=2.0)
    finally:
        ad3.pattern.stop()
        settle(pending)
    ad3_bus(ad3, need, bitrate)
    received = ad3.can.receive(timeout=1.0)
    if recover:
        assert received is not None and received.id == frame["id"], "no frame after the automatic bus-off recovery"
    else:
        assert received is None, "the controller left bus off although recover=0"
