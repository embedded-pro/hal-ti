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
def test_harness_wires_every_dio_once(name):
    """The fixed harness uses all 16 DIOs, each on its own pin; the AD3 is not on the CAN bus."""
    board = load_board(name)
    harness = board.wiring(["harness"])
    dios = [connection for connection in harness.connections if connection.kind == "dio"]
    assert sorted(connection.channel for connection in dios) == list(range(16))
    assert len({connection.pin for connection in dios}) == 16
    assert not board.wiring(["can"]).connections


@pytest.mark.parametrize("name", BOARDS)
def test_parameters_reference_wired_pins(name):
    """Pins listed in the test parameters are on the harness, or in the analog/extra set their tests name."""
    board = load_board(name)

    def wired(sets, pin, kind="dio"):
        return board.wiring(sets).channel(kind, board.resolve_pin(pin)) is not None

    harness = ["harness"]
    for pin in board.param("gpio.loop_pins") + board.param("gpio.output_pins"):
        assert wired(harness, pin), pin
    for pin in board.param("gpio.locked_pins"):
        assert wired(harness, pin) or wired(["locked"], pin), pin
    for generator in board.param("pwm.generators"):
        assert wired(harness, generator["a"]) and wired(harness, generator["b"]), generator
    assert wired(harness, board.param("pwm.fault.pin"))
    assert wired(["harness", "adc"], board.param("pwm.fault.dcmp.pin"), "wavegen")
    for instance in board.param("uart.instances"):
        assert wired(harness, instance["tx"]) and wired(harness, instance["rx"])
    flow = board.param("uart.flow_instance")
    assert all(wired(harness, flow[key]) for key in ("tx", "rx", "rts", "cts"))
    for instance in board.param("spi.instances"):
        assert all(wired(harness, instance[key]) for key in ("clk", "cs", "mosi", "miso"))
    for instance in board.param("qei.instances"):
        assert all(wired(harness, instance[key]) for key in ("a", "b", "idx")), instance
    assert wired(harness, board.param("watchdog.pin"))
    for set_name, key, instances in (("comparator", "pos", "comparator.instances"), ("comparator_c0", "c0", "comparator.c0_instances")):
        for instance in board.param(instances):
            assert wired([set_name], instance[key], "wavegen") and wired([set_name], instance["neg"], "wavegen")
            assert instance["out"] is None or wired([set_name], instance["out"]), instance
    for pin in board.param("adc.inputs"):
        assert wired(["harness", "adc"], pin, "wavegen")


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


def test_wiring_merge_and_conflicts():
    board = load_board("ek_tm4c123gxl")
    wiring = board.wiring(["harness", "adc"])
    assert wiring.dio(board.resolve_pin("m0pwm0")) == 0
    assert wiring.wavegen(board.resolve_pin("ain0")) == 1
    assert wiring.scope(board.resolve_pin("ain3")) == 2
    assert "DIO15" in wiring.describe()
    assert board.wiring(["harness", "comparator_c0"]).dio(board.resolve_pin("led0")) == 15
    assert board.wiring(["harness"], ["loopback"]).has("loopback")
    with pytest.raises(ConfigError):
        board.wiring(["comparator", "comparator_c0"])
    with pytest.raises(ConfigError):
        board.wiring(["nosuchset"])
    other = load_board("ek_tm4c1294xl")
    with pytest.raises(ConfigError):
        other.wiring(["harness", "comparator"])
    with pytest.raises(ConfigError):
        other.wiring(["harness", "locked"])


def test_optional_connections_and_roles():
    raw = {
        "board": "x",
        "family": "tm4c123",
        "pins": {"gpio0": "PA2"},
        "wiring_sets": {"s": {"dio": {0: "gpio0", 1: {"pin": "PB0", "role": "probe", "requires": "extra"}}}},
    }
    board = parse_board(raw)
    assert board.wiring(["s"]).dio(role="probe") is None
    assert board.wiring(["s"], ["extra"]).dio(role="probe") == 1


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
