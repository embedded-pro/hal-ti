"""CAN (`hal::tiva::Can`) over the link `--can-mode` selects:

- `loopback`: the controller's internal test mode (`loopback=1`); needs no wiring;
- `bus`: CAN0 through a 3.3 V transceiver to a CANable (wiring set `can`, `--can-peer`); every frame goes
  from the firmware to the CANable and back.

The `test_bus_*` tests need the CANable whatever `--can-mode` is: a missing acknowledge (CANable listen-only),
receive errors and bus off (CANable at a wrong bit rate) and the automatic bus-off recovery.

Scenarios: features/can.feature.
"""

from __future__ import annotations

import pytest
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when

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


def send_on_link(fw, can, peer, frame_id, frame_ext, data):
    if peer is None:
        fw.can.send(can["index"], frame_id, data, ext=bool(frame_ext))
    else:
        peer.send(frame_id, data, ext=bool(frame_ext))


def begin_send(fw, can, frame):
    command = format_command("can.send", can["index"], f"0x{frame['id']:x}", frame["data"], ext=frame["ext"])
    return fw.terminal.begin(command, timeout=3.0)


@pytest.mark.matrix("can.frames")
@scenario("can.feature", "A frame of the id type and data length crosses the link at the bit rate")
def test_frames(link, bitrate, ext, dlc):
    pass


@pytest.mark.board_params("timing", "can.timings")
@scenario("can.feature", "Frames cross the link at the bit rate of an explicit timing")
def test_explicit_timing(link, timing):
    pass


@pytest.mark.board_params("case", "can.filters")
@scenario("can.feature", "The acceptance filter passes the frames it matches and drops the others")
def test_acceptance_filter(link, case):
    pass


@scenario("can.feature", "A frame cannot be sent before the controller is open")
def test_send_requires_open():
    pass


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
@scenario("can.feature", "Invalid options are refused with their reason")
def test_open_errors(options, reason):
    pass


@scenario("can.feature", "A timing needs four fields")
def test_timing_needs_four_fields():
    pass


@scenario("can.feature", "CAN0 opens on its default pins but not on a single pin")
def test_default_pins():
    pass


@scenario("can.feature", "A frame nobody acknowledges fails with ackError")
def test_bus_no_acknowledge(can_peer):
    pass


@scenario("can.feature", "Frames at a wrong bit rate are reported as receive errors")
def test_bus_receive_errors(can_peer):
    pass


@pytest.mark.board_params("recover", "can.recover")
@scenario("can.feature", "Bus off ends by itself only with the automatic recovery")
def test_bus_off_and_recovery(can_peer, recover):
    pass


@given("the CANable runs at the bit rate if the link is the bus")
def canable_at_bitrate(peer, bitrate):
    if peer is not None:
        attach(peer, bitrate)


@given("the controller is open on the link at the bit rate")
def open_at_bitrate(fw, can, link, bitrate):
    open_can(fw, can, link, bitrate=bitrate)


@given("the CANable runs at the bit rate of the timing at the sysclk the firmware reports, if the link is the bus")
def canable_at_timing_bitrate(fw, peer, timing):
    if peer is not None:
        attach(peer, expect.can_timing_bitrate(fw.system.info().sysclk, *timing))


@given("the controller is open on the link with the timing")
def open_with_timing(fw, can, link, timing):
    open_can(fw, can, link, timing=tuple(timing))


@given("the CANable runs at the bus bit rate if the link is the bus")
def canable_at_bus_bitrate_on_bus(can, peer):
    if peer is not None:
        attach(peer, can["bus"]["bitrate"])


@given("the controller is open on the link at the bus bit rate with the filter of the case")
def open_with_filter(fw, can, link, case):
    ident, mask, ext, match = case["filter"]
    open_can(fw, can, link, bitrate=can["bus"]["bitrate"], filter=(ident, mask, bool(ext), bool(match)))


@given("the bus is not the fake bus")
def real_bus(no_fake):
    pass


@given("the CANable's bit rate can be set")
def canable_adjustable(can_peer):
    if not can_peer.adjustable:
        pytest.skip(f"{can_peer} has a fixed bit rate")


@given("the CANable runs listen-only at the bus bit rate")
def canable_listen_only(can, can_peer):
    attach(can_peer, can["bus"]["bitrate"], listen_only=True)


@given("the CANable runs at the wrong bit rate")
def canable_at_wrong_bitrate(can, can_peer):
    attach(can_peer, can["bus"]["wrong_bitrate"])


@given("the controller is open at the bus bit rate")
def open_at_bus_bitrate(fw, can):
    open_can(fw, can, bitrate=can["bus"]["bitrate"])


@given("the controller is open at the bus bit rate with the recovery setting")
def open_with_recovery(fw, can, recover):
    open_can(fw, can, bitrate=can["bus"]["bitrate"], recover=recover)


@when(parsers.parse('each rejected frame of the case is sent on the link with data "{data}"'))
def send_rejected(fw, can, peer, case, data):
    for frame_id, frame_ext in case["rejected"]:
        send_on_link(fw, can, peer, frame_id, frame_ext, bytes.fromhex(data))


@when(
    parsers.parse(
        "the firmware sends the standard frame of {length:d} bytes in the background while the CANable waits up to {seconds:g} s for it"
    ),
    target_fixture="unacknowledged",
)
def send_unacknowledged(fw, can, can_peer, length, seconds):
    frame = make_frame(can, 0, length)
    pending = begin_send(fw, can, frame)
    try:
        received = can_peer.receive(timeout=seconds, predicate=matches(frame))
    finally:
        response = settle(pending)
    return {"frame": frame, "received": received, "response": response}


@when(parsers.parse("the CANable sends a standard frame with id 0x{ident:x} and {length:d} zero bytes"))
def canable_sends_zeros(can_peer, ident, length):
    can_peer.send(ident, bytes(length))


@when(
    parsers.parse(
        "the firmware sends a standard frame with id 0x{ident:x} and {length:d} zero bytes in the background while the controller "
        "reports busOff within {seconds:g} s"
    ),
    target_fixture="pending_frame",
)
def send_until_bus_off(fw, can, ident, length, seconds):
    frame = {"id": ident, "ext": False, "data": bytes(length)}
    pending = begin_send(fw, can, frame)
    try:
        fw.terminal.wait_event("can", lambda event: event.get("error") == "busOff", timeout=seconds)
    finally:
        settle(pending)
    return frame


@when("the CANable switches to the bus bit rate")
def canable_back_at_bus_bitrate(can, can_peer):
    attach(can_peer, can["bus"]["bitrate"])


@then("the frame of the id type and data length crosses the link")
def frame_crosses(fw, can, peer, ext, dlc):
    transfer(fw, can, peer, make_frame(can, ext, dlc))


@then(parsers.parse("the standard and then the extended frame of {length:d} bytes cross the link"))
def both_id_types_cross(fw, can, peer, length):
    for ext in (0, 1):
        transfer(fw, can, peer, make_frame(can, ext, length))


@then(
    parsers.parse(
        'each accepted frame of the case, sent on the link with data "{data}", arrives at the firmware within {seconds:g} s with its id '
        "type and that data"
    )
)
def accepted_frames_arrive(fw, can, peer, case, data, seconds):
    payload = bytes.fromhex(data)
    for frame_id, frame_ext in case["accepted"]:
        send_on_link(fw, can, peer, frame_id, frame_ext, payload)
        received = fw.can.wait_frame(can["index"], lambda f, i=frame_id: f.id == i, timeout=seconds)
        assert (received.ext, received.data) == (bool(frame_ext), payload)


@then(parsers.parse("after {ms:d} ms no other frame has arrived at the firmware"))
def no_frame_leaked(fw, ms):
    fw.system.delay(ms)
    leaked = [event.raw for event in fw.terminal.drain_events("can") if "id" in event]
    assert not leaked, f"frames outside the filter were received: {leaked}"


@then(parsers.parse('sending a frame with id 0x{ident:x} and data "{data}" on the controller fails with "{refusal}"'))
def send_unopened_refused(fw, can, ident, data, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.can.send(can["index"], ident, bytes.fromhex(data))
    assert error.value.reason == refusal


@then("opening the controller on its pins with the options fails with the reason")
def open_options_refused(fw, can, options, reason):
    with pytest.raises(FirmwareError) as error:
        open_can(fw, can, **options)
    assert error.value.reason == reason


@then(parsers.parse('opening the controller on its pins with timing "{fields}" fails with "{refusal}"'))
def short_timing_refused(fw, can, fields, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.command("can.open", can["index"], timing=fields, rx=fw.pin(can["rx"]), tx=fw.pin(can["tx"]))
    assert error.value.reason == refusal


@then(parsers.parse("CAN {number:d} opens in loopback mode without pins and closes again"))
def opens_on_default_pins(fw, number):
    fw.can.open(number, loopback=True)
    fw.can.close(number)


@then(parsers.parse('opening CAN {number:d} on the RX pin of the controller only fails with "{refusal}"'))
def single_pin_refused(fw, can, number, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.can.open(number, rx=can["rx"])
    assert error.value.reason == refusal


@then("the CANable has received the frame with its data")
def canable_saw_frame(unacknowledged):
    frame, received = unacknowledged["frame"], unacknowledged["received"]
    assert received is not None and received.data == frame["data"], "the listen-only CANable did not see the frame"


@then(parsers.parse('can.send has answered "{first}" or "{second}"'))
def send_failed(unacknowledged, first, second):
    response = unacknowledged["response"]
    assert response is not None and response.reason in (first, second), f"unacknowledged send answered {response}"


@then(parsers.parse('the controller has reported "{error_name}"'))
def error_reported(fw, can, error_name):
    errors = fw.can.errors(can["index"])
    assert error_name in errors, f"no {error_name} reported: {errors}"


@then(parsers.parse("after {ms:d} ms the controller has reported at least one error, all of them known CAN error names"))
def receive_errors_reported(fw, can, ms):
    fw.system.delay(ms)
    errors = fw.can.errors(can["index"])
    assert errors, "no error reported for frames at a wrong bit rate"
    assert set(errors) <= ERROR_NAMES


@then(
    parsers.parse(
        "the CANable receives a frame with its id and id type within {seconds:g} s if the recovery setting is on, and does not if it is off"
    )
)
def recovery_as_set(can_peer, recover, pending_frame, seconds):
    received = can_peer.receive(timeout=seconds, predicate=matches(pending_frame))
    if recover:
        assert received is not None, "no frame after the automatic bus-off recovery"
    else:
        assert received is None, "the controller left bus off although recover=0"
