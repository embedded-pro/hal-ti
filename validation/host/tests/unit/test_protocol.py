import pytest
from ad3_waveforms_bench.protocol import ProtocolError, parse_response

from hal_ti_validation.protocol import PIN_ALIASES, is_alias, is_pin, normalize_pin, parse_pin_map


def test_pins():
    assert normalize_pin("pf1") == "PF1"
    assert normalize_pin("PQ3") == "PQ3"
    assert normalize_pin("m0pwm0") == "m0pwm0"
    assert normalize_pin("M0PWM0", {"m0pwm0": "PB6"}) == "PB6"
    assert normalize_pin("qei0idx", {"m0pwm0": "PB6"}) == "qei0idx", "a generic alias passes through unresolved"
    assert is_pin("pj0")
    assert not is_pin("led0")
    with pytest.raises(ProtocolError):
        normalize_pin("PX9")
    with pytest.raises(ProtocolError):
        normalize_pin("nosuchalias")
    with pytest.raises(ProtocolError):
        normalize_pin("qei0idx", {"m0pwm0": "PB6"}, strict=True)


def test_generic_aliases():
    for alias in ("terminaltx", "terminalrx", "ain0", "ain11", "m0pwm7", "m1pwm0", "qei0a", "qei1idx", "can0rx", "can1tx", "led2", "gpio6"):
        assert is_alias(alias), alias
    for name in ("phasea", "vbus", "itotal", "enca", "pwm1a", "canrx", "ledop", "perf", "halla", "id0", "pwrstatus"):
        assert not is_alias(name), name
    assert "GPIO0".lower() in PIN_ALIASES


def test_pin_map():
    response = parse_response("OK terminaltx=PA1,terminalrx=PA0,ain0=PE3,led0=PF1")
    assert parse_pin_map(response) == {"terminaltx": "PA1", "terminalrx": "PA0", "ain0": "PE3", "led0": "PF1"}
    spaced = parse_response("OK ain0=PE3 ain3=PE0")
    assert parse_pin_map(spaced) == {"ain0": "PE3", "ain3": "PE0"}
