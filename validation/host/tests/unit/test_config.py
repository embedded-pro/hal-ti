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
def test_bundles_wire_each_dio_once(name):
    """Each bundle uses an AD3 channel at most once, each DIO on its own pin; bundle1 uses all 16 DIOs."""
    board = load_board(name)
    assert {"bundle1", "bundle2"} <= set(board.wiring_sets)
    for bundle in ("bundle1", "bundle2"):
        dios = [connection for connection in board.wiring([bundle]).connections if connection.kind == "dio"]
        assert len({connection.channel for connection in dios}) == len(dios)
        assert len({connection.pin for connection in dios}) == len(dios)
    assert len([c for c in board.wiring(["bundle1"]).connections if c.kind == "dio"]) == 16
    assert not board.wiring(["can"]).connections


@pytest.mark.parametrize("name", BOARDS)
def test_parameters_reference_wired_pins(name):
    """Pins in the test parameters are wired in bundle1, or in bundle2 for the comparators (and, where bundle1
    has no room, UART flow control and the locked pin)."""
    board = load_board(name)

    def wired(bundle, pin, kind="dio"):
        return board.wiring([bundle]).channel(kind, board.resolve_pin(pin)) is not None

    for pin in board.param("gpio.loop_pins") + board.param("gpio.output_pins", []):
        assert wired("bundle1", pin), pin
    for pin in board.param("gpio.locked_pins"):
        assert wired("bundle1", pin) or wired("bundle2", pin), pin
    for generator in board.param("pwm.generators"):
        assert wired("bundle1", generator["a"]) and wired("bundle1", generator["b"]), generator
    assert wired("bundle1", board.param("pwm.fault.pin"))
    assert wired("bundle1", board.param("pwm.fault.dcmp.pin"), "wavegen")
    for instance in board.param("uart.instances"):
        assert wired("bundle1", instance["tx"]) and wired("bundle1", instance["rx"])
    flow = board.param("uart.flow_instance")
    assert any(all(wired(bundle, flow[key]) for key in ("tx", "rx", "rts", "cts")) for bundle in ("bundle1", "bundle2"))
    for instance in board.param("spi.instances"):
        assert all(wired("bundle1", instance[key]) for key in ("clk", "cs", "mosi", "miso"))
    for instance in board.param("qei.instances"):
        assert all(wired("bundle1", instance[key]) for key in ("a", "b", "idx")), instance
    assert wired("bundle1", board.param("watchdog.pin"))
    for key, instances in (("pos", "comparator.instances"), ("c0", "comparator.c0_instances")):
        for instance in board.param(instances):
            assert wired("bundle2", instance[key], "wavegen") and wired("bundle2", instance["neg"], "wavegen"), instance
            assert instance["out"] is None or wired("bundle2", instance["out"]), instance
    for pin in board.param("adc.inputs"):
        assert wired("bundle1", pin, "wavegen")


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


def test_wiring_lookup_and_conflicts():
    board = load_board("ek_tm4c123gxl")
    wiring = board.wiring(["bundle1"], ["loopback"])
    assert wiring.dio(board.resolve_pin("m0pwm0")) == 0
    assert wiring.wavegen(board.resolve_pin("ain0")) == 1
    assert wiring.scope(board.resolve_pin("ain3")) == 2
    assert wiring.has("loopback")
    assert "DIO15" in wiring.describe()
    bundle2 = board.wiring(["bundle2"])
    assert bundle2.wavegen("PC7") == bundle2.wavegen("PC4") == 2
    assert "W2 PC7+PC4" in bundle2.describe()
    with pytest.raises(ConfigError):
        board.wiring(["bundle1", "bundle2"])
    with pytest.raises(ConfigError):
        board.wiring(["nosuchset"])
    other = load_board("ek_tm4c1294xl")
    assert other.wiring(["bundle2"]).wavegen("PC6") == 1
    assert other.wiring(["bundle2"]).dio("PD1") == 15
    with pytest.raises(ConfigError):
        other.wiring(["bundle1", "bundle2"])


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
    raw["wiring_sets"]["j"] = {"wavegen": {1: {"pin": "PE3", "jumpered": ["PE2"]}}}
    assert parse_board(raw).wiring(["j"]).wavegen("PE2") == 1
    raw["wiring_sets"]["j"] = {"wavegen": {1: {"pin": "PE3", "jumpered": "PE2"}}}
    with pytest.raises(ConfigError):
        parse_board(raw)


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
