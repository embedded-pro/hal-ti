import pytest

from hal_ti_validation.config import ConfigError, available_boards, load_board, parse_board
from hal_ti_validation.fake_firmware import TM4C123_PINS, TM4C129_PINS
from hal_ti_validation.pairwise import pairwise
from hal_ti_validation.protocol import is_alias

BOARDS = ["ek_tm4c123gxl", "ek_tm4c1294xl"]
FIRMWARE_TABLES = {"ek_tm4c123gxl": TM4C123_PINS, "ek_tm4c1294xl": TM4C129_PINS}


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
    assert all(is_alias(alias) for alias in board.pins)
    assert set(board.terminal.pins) == {board.pins["terminaltx"], board.pins["terminalrx"]}
    for alias in ("m0pwm2", "qei0a", "can0rx", "led0", "gpio0", "ain0"):
        assert board.resolve_pin(alias).startswith("P")


@pytest.mark.parametrize("name", BOARDS)
def test_alias_table_matches_protocol(name):
    """The board file's `pins` is the PROTOCOL.md table the fake firmware also serves."""
    assert load_board(name).pins == FIRMWARE_TABLES[name]


@pytest.mark.parametrize("name", BOARDS)
def test_parameters_reference_wired_pins(name):
    """Pins listed in the test parameters are wired in the matching wiring set (optional ones with their tag)."""
    board = load_board(name)
    tags = ["pwm4", "faultpin", "dcmp", "flow", "qei1", "loopback"]

    def wired(set_name, pin, kind="dio"):
        return board.wiring([set_name], tags).channel(kind, board.resolve_pin(pin)) is not None

    for pin in board.param("gpio.loop_pins") + board.param("gpio.locked_pins") + board.param("gpio.output_pins"):
        assert wired("gpio", pin), pin
    for generator in board.param("pwm.generators"):
        assert wired("pwm", generator["a"]) and wired("pwm", generator["b"]), generator
    assert wired("pwm", board.param("pwm.fault.pin"))
    assert wired("pwm", board.param("pwm.fault.dcmp.pin"), "wavegen")
    for instance in board.param("uart.instances"):
        assert wired("uart", instance["tx"]) and wired("uart", instance["rx"])
    flow = board.param("uart.flow_instance")
    assert wired("uart", flow["rts"]) and wired("uart", flow["cts"])
    for instance in board.param("spi.instances"):
        assert all(wired("spi", instance[key]) for key in ("clk", "cs", "mosi", "miso"))
    for instance in board.param("qei.instances"):
        assert all(wired("qei", instance[key]) for key in ("a", "b", "idx")), instance
    for instance in board.param("comparator.instances"):
        assert wired("comparator", instance["pos"], "wavegen")
        assert wired("comparator", instance["neg"], "wavegen")
        assert wired("comparator", instance["out"])
    for instance in board.param("comparator.c0_instances"):
        assert wired("comparator_c0", instance["c0"], "wavegen")
        assert wired("comparator_c0", instance["neg"], "wavegen")
        assert wired("comparator_c0", instance["out"])
    for pin in board.param("adc.inputs"):
        assert wired("adc", pin, "wavegen")
    can = board.wiring(["can"])
    assert can.dio(role="can_ad3_tx") is not None and can.dio(role="can_ad3_rx") is not None
    assert board.wiring(["gpio", "wdt"]).dio(role="wdt_pin") is not None


@pytest.mark.parametrize("name", BOARDS)
def test_matrices_load(name):
    board = load_board(name)
    for path in ("pwm.waveform", "pwm.outputs", "pwm.dead_time", "pwm.update", "pwm.irq", "pwm.adc_trigger", "pwm.fault.options"):
        assert board.matrix(path)
    for path in ("uart.transfer", "uart.flow", "uart.large", "spi.transfer", "spi.sessions", "adc.levels", "adc.sequencers"):
        assert board.matrix(path)
    for path in ("adc.timing", "adc.dcmp", "comparator.output", "comparator.ladder", "comparator.irq", "gpio.irq"):
        assert board.matrix(path)
    for path in ("qei.position", "qei.index", "qei.velocity", "qei.rollover", "can.frames", "watchdog.behaviour", "watchdog.period"):
        assert board.matrix(path)
    assert len(board.matrix("uart.transfer")["baud"]) == 12
    assert board.matrix("adc.sequencers")["seq"] == [0, 1, 2, 3]
    assert len(pairwise(board.matrix("pwm.waveform"))) < 100
    with pytest.raises(ConfigError):
        board.matrix("pwm.module")


def test_wiring_merge_and_optional_connections():
    board = load_board("ek_tm4c123gxl")
    wiring = board.wiring(["pwm", "adc"])
    assert wiring.dio(board.resolve_pin("m0pwm0")) == 0
    assert wiring.wavegen(board.resolve_pin("ain0")) == 1
    assert wiring.scope(board.resolve_pin("ain3")) == 2
    assert wiring.dio(role="pwm_fault") is None
    assert board.wiring(["pwm"], ["faultpin"]).dio(role="pwm_fault") == 8
    uart = board.wiring(["uart"])
    assert uart.dio(role="uart_rts") is None
    flow = board.wiring(["uart"], ["flow"])
    assert flow.dio(role="uart_rts") == 2
    assert flow.has("flow")
    assert "DIO0" in flow.describe()
    assert board.wiring(["gpio", "wdt"]).dio(role="wdt_pin") == 7


def test_conflicting_wiring_sets():
    board = load_board("ek_tm4c123gxl")
    with pytest.raises(ConfigError):
        board.wiring(["pwm", "gpio"])
    with pytest.raises(ConfigError):
        board.wiring(["comparator", "comparator_c0"])
    with pytest.raises(ConfigError):
        board.wiring(["nosuchset"])


def test_params_and_overrides():
    board = load_board("ek_tm4c123gxl")
    assert board.param("pwm.module") == 0
    assert board.param("pwm.missing", 5) == 5
    with pytest.raises(ConfigError):
        board.param("pwm.missing")
    board.apply_overrides(["pwm.waveform.freq=[20000]", "new.value=1.5"])
    assert board.matrix("pwm.waveform")["freq"] == [20000]
    assert board.param("new.value") == 1.5
    with pytest.raises(ConfigError):
        board.apply_overrides(["novalue"])
    assert board.aliases_of("PB6") == ["m0pwm0"]


def test_invalid_boards_rejected():
    raw = {"board": "x", "family": "tm4c123", "wiring_sets": {"bad": {"dio": {16: "PA0"}}}}
    with pytest.raises(ConfigError):
        parse_board(raw)
    raw = {"board": "x", "family": "tm4c123", "wiring_sets": {"bad": {"wavegen": {3: "PA0"}}}}
    with pytest.raises(ConfigError):
        parse_board(raw)
    with pytest.raises(ConfigError, match="generic"):
        parse_board({"board": "x", "family": "tm4c123", "pins": {"phasea": "PE3"}})
    with pytest.raises(ConfigError):
        parse_board({"board": "x", "family": "tm4c123", "pins": {"ain0": "PE3"}, "wiring_sets": {"s": {"dio": {0: "ain1"}}}})
