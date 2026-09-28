import pytest
from ad3_waveforms_bench.protocol import ProtocolError, parse_response

from hal_ti_validation.protocol import is_pin, normalize_pin, parse_pin_map


def test_pins():
    assert normalize_pin("pf1") == "PF1"
    assert normalize_pin("PQ3") == "PQ3"
    assert normalize_pin("pwm1a") == "pwm1a"
    assert normalize_pin("PWM1A", {"pwm1a": "PB6"}) == "PB6"
    assert is_pin("pj0")
    assert not is_pin("ledop")
    with pytest.raises(ProtocolError):
        normalize_pin("PX9")
    with pytest.raises(ProtocolError):
        normalize_pin("nosuchalias")


def test_pin_map():
    response = parse_response("OK terminaltx=PA1,terminalrx=PA0,phasea=PE3,ledop=PF1")
    assert parse_pin_map(response) == {"terminaltx": "PA1", "terminalrx": "PA0", "phasea": "PE3", "ledop": "PF1"}
    spaced = parse_response("OK phasea=PE3 vbus=PE0")
    assert parse_pin_map(spaced) == {"phasea": "PE3", "vbus": "PE0"}
