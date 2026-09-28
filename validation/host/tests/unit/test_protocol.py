import pytest

from hal_ti_validation.protocol import (
    ProtocolError,
    format_command,
    format_value,
    is_event_line,
    is_final_line,
    normalize_pin,
    parse_event,
    parse_hex,
    parse_number,
    parse_pin_map,
    parse_response,
)


def test_ok_with_values():
    response = parse_response("OK pos=0x10 dir=fwd speed=12 res=4095")
    assert response.ok
    assert response.reason is None
    assert response.as_int("pos") == 16
    assert response["dir"] == "fwd"
    assert response.as_int("res") == 4095


def test_err_reason():
    response = parse_response("ERR busy")
    assert not response.ok
    assert response.reason == "busy"


def test_unrecognized_command_is_an_error():
    response = parse_response("Unrecognized command.")
    assert not response.ok
    assert response.reason == "unrecognized"


def test_typed_helpers():
    response = parse_response("OK samples=1,2,0x10 data=a55a0102 empty=")
    assert response.as_ints("samples") == [1, 2, 16]
    assert response.as_bytes("data") == b"\xa5\x5a\x01\x02"
    assert response.as_list("empty") == []
    assert response.as_bytes("empty") == b""
    with pytest.raises(KeyError):
        response["missing"]


def test_event():
    event = parse_event("EVT can index=0 id=0x123 ext=0 data=0102")
    assert event.kind == "can"
    assert event.as_int("id") == 0x123
    assert not event.as_bool("ext")
    assert event.as_bytes("data") == b"\x01\x02"


def test_line_classification():
    assert is_final_line("OK")
    assert is_final_line("OK value=1")
    assert is_final_line("ERR usage")
    assert not is_final_line("OKAY")
    assert is_event_line("EVT boot board=x")
    assert not is_event_line("EVTX")


@pytest.mark.parametrize(
    ("text", "value"),
    [("0", 0), ("42", 42), ("0x2A", 42), ("0x7ff", 0x7FF), ("-5", -5)],
)
def test_numbers(text, value):
    assert parse_number(text) == value


def test_bad_number_and_hex():
    with pytest.raises(ProtocolError):
        parse_number("12a")
    with pytest.raises(ProtocolError):
        parse_hex("abc")
    assert parse_hex("-") == b""


def test_format_command():
    line = format_command("pwm.open", 0, gens=[0, 1, 2], freq=20000, dead="off", inva=True, sync=None)
    assert line == "pwm.open 0 gens=0,1,2 freq=20000 dead=off inva=1"


def test_format_values():
    assert format_value(12.5) == "12.5"
    assert format_value(100.0) == "100"
    assert format_value(0.0) == "0"
    assert format_value(b"\x00\xff") == "00ff"
    assert format_value(b"") == "-"
    assert format_command("spi.xfer", 0, b"\xa5", continue_=False) == "spi.xfer 0 a5 continue=0"


def test_format_rejects_spaces():
    with pytest.raises(ProtocolError):
        format_command("gpio.cfg", "PF 1", "out")


def test_pins():
    assert normalize_pin("pf1") == "PF1"
    assert normalize_pin("PQ3") == "PQ3"
    assert normalize_pin("pwm1a") == "pwm1a"
    assert normalize_pin("PWM1A", {"pwm1a": "PB6"}) == "PB6"
    with pytest.raises(ProtocolError):
        normalize_pin("PX9")
    with pytest.raises(ProtocolError):
        normalize_pin("nosuchalias")


def test_pin_map():
    response = parse_response("OK terminaltx=PA1,terminalrx=PA0,phasea=PE3,ledop=PF1")
    assert parse_pin_map(response) == {"terminaltx": "PA1", "terminalrx": "PA0", "phasea": "PE3", "ledop": "PF1"}
    spaced = parse_response("OK phasea=PE3 vbus=PE0")
    assert parse_pin_map(spaced) == {"phasea": "PE3", "vbus": "PE0"}
