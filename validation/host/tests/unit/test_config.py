import pytest

from hal_ti_validation.config import ConfigError, available_boards, load_board, parse_board

BOARDS = ["ek_tm4c123gxl", "ek_tm4c1294xl"]


def test_available_boards():
    assert set(BOARDS) <= set(available_boards())


@pytest.mark.parametrize("name", BOARDS)
def test_board_files_load(name):
    board = load_board(name)
    assert board.name == name
    assert board.family in ("tm4c123", "tm4c129")
    assert board.terminal.baud == 921600
    assert board.ad3.vplus is None
    assert board.ad3.analog_max == 3.3
    assert board.matches_firmware_name(name.upper().replace("_", "-"))
    for alias in ("pwm1a", "enca", "canrx", "ledop", "perf", "vbus", "phasea"):
        assert board.resolve_pin(alias).startswith("P")


@pytest.mark.parametrize("name", BOARDS)
def test_parameters_reference_wired_pins(name):
    """Pins listed in the test parameters are wired in the matching wiring set."""
    board = load_board(name)

    def wired(set_name, pin, kind="dio"):
        return board.wiring([set_name]).channel(kind, board.resolve_pin(pin)) is not None

    for pin in board.param("gpio.loop_pins") + board.param("gpio.locked_pins") + board.param("gpio.output_only_pins"):
        assert wired("gpio", pin), pin
    for channel in board.param("pwm.channels"):
        assert wired("pwm", channel["a"]) and wired("pwm", channel["b"])
    for instance in board.param("uart.instances"):
        assert wired("uart", instance["tx"]) and wired("uart", instance["rx"])
    for instance in board.param("spi.instances"):
        assert all(wired("spi", instance[key]) for key in ("clk", "cs", "mosi", "miso"))
    for key in ("a", "b", "idx"):
        assert wired("qei", board.param(f"qei.{key}"))
    for instance in board.param("comparator.instances"):
        assert wired("comparator", instance["pos"], "wavegen")
        assert wired("comparator", instance["neg"], "wavegen")
        assert wired("comparator", instance["out"])
    for entry in board.param("adc.single"):
        assert wired("adc", entry["pin"], "wavegen")
    can = board.wiring(["can"])
    assert can.dio(role="can_ad3_tx") is not None and can.dio(role="can_ad3_rx") is not None


def test_wiring_merge_and_optional_connections():
    board = load_board("ek_tm4c123gxl")
    wiring = board.wiring(["pwm", "adc"])
    assert wiring.dio(board.resolve_pin("pwm1a")) == 0
    assert wiring.wavegen(board.resolve_pin("phasea")) == 1
    assert wiring.scope(board.resolve_pin("vbus")) == 2
    uart = board.wiring(["uart"])
    assert uart.dio(role="uart_rts") is None
    flow = board.wiring(["uart"], ["flow"])
    assert flow.dio(role="uart_rts") == 2
    assert flow.has("flow")
    assert "DIO0" in flow.describe()


def test_conflicting_wiring_sets():
    board = load_board("ek_tm4c123gxl")
    with pytest.raises(ConfigError):
        board.wiring(["pwm", "gpio"])
    with pytest.raises(ConfigError):
        board.wiring(["nosuchset"])


def test_params_and_overrides():
    board = load_board("ek_tm4c123gxl")
    assert board.param("pwm.efoc.frequency_hz") == 20000
    assert board.param("pwm.missing", 5) == 5
    with pytest.raises(ConfigError):
        board.param("pwm.missing")
    board.apply_overrides(["pwm.frequencies_hz=[20000]", "new.value=1.5"])
    assert board.param("pwm.frequencies_hz") == [20000]
    assert board.param("new.value") == 1.5
    with pytest.raises(ConfigError):
        board.apply_overrides(["novalue"])


def test_invalid_channels_rejected():
    raw = {"board": "x", "family": "tm4c123", "wiring_sets": {"bad": {"dio": {16: "PA0"}}}}
    with pytest.raises(ConfigError):
        parse_board(raw)
    raw = {"board": "x", "family": "tm4c123", "wiring_sets": {"bad": {"wavegen": {3: "PA0"}}}}
    with pytest.raises(ConfigError):
        parse_board(raw)
