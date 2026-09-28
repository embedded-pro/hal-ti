"""Shared pytest plumbing: board/wiring options, firmware and AD3 fixtures, YAML-driven parametrisation."""

from __future__ import annotations

import os
from collections.abc import Iterator
from pathlib import Path
from typing import Any

import pytest

from hal_ti_validation.config import BoardConfig, ConfigError, Wiring, load_board
from hal_ti_validation.firmware import Firmware
from hal_ti_validation.terminal import FirmwareTerminal, TerminalError

HIL_DIR = Path(__file__).parent / "hil"
_BOARD_KEY = pytest.StashKey[BoardConfig]()


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("hal-ti validation")
    group.addoption("--board", default=os.environ.get("HAL_TI_BOARD", "ek_tm4c123gxl"), help="board YAML name or path")
    group.addoption("--port", default=os.environ.get("HAL_TI_PORT"), help="firmware terminal serial port; HIL tests skip without it")
    group.addoption("--baud", type=int, default=None, help="terminal baud rate (default from the board YAML)")
    group.addoption("--wiring-set", default=os.environ.get("HAL_TI_WIRING", ""), help="comma separated wiring sets from the board YAML")
    group.addoption("--with", dest="with_tags", action="append", default=[], help="enable an optional wiring tag (repeatable)")
    group.addoption("--ad3-serial", default=os.environ.get("HAL_TI_AD3_SERIAL"), help="AD3 serial number (default: first device)")
    group.addoption("--no-ad3", action="store_true", help="skip every test that needs the Analog Discovery 3")
    group.addoption("--fake", action="store_true", help="dry-run the HIL plumbing against the in-memory fakes (no hardware)")
    group.addoption("--set", dest="overrides", action="append", default=[], help="override a test parameter: pwm.frequencies_hz=[20000]")
    group.addoption("--quick", action="store_true", help="use only the first value of every YAML parameter list")


def board_config(config: pytest.Config) -> BoardConfig:
    if _BOARD_KEY not in config.stash:
        board = load_board(config.getoption("--board"))
        board.apply_overrides(config.getoption("overrides"))
        config.stash[_BOARD_KEY] = board
    return config.stash[_BOARD_KEY]


def _ids(value: Any) -> str:
    if isinstance(value, dict):
        return "-".join(f"{key}={value[key]}" for key in value if key != "name") if "name" not in value else str(value["name"])
    if isinstance(value, (list, tuple)):
        return ":".join(str(item) for item in value)
    return str(value)


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    """`@pytest.mark.board_params("freq", "pwm.frequencies_hz")` parametrises from the board YAML.

    Several markers produce the cartesian product. For several argnames, each list item must be a
    mapping (picked by name) or a sequence (positional).
    """
    markers = list(metafunc.definition.iter_markers("board_params"))
    if not markers:
        return
    try:
        board = board_config(metafunc.config)
    except ConfigError as error:
        raise pytest.UsageError(str(error)) from error
    quick = metafunc.config.getoption("--quick")
    for marker in markers:
        argnames, path = marker.args[:2]
        names = [name.strip() for name in argnames.split(",")]
        values = board.param(path, None)
        if values is None:
            metafunc.parametrize(
                argnames, [pytest.param(*([None] * len(names)), marks=pytest.mark.skip(reason=f"tests.{path} not configured"))]
            )
            continue
        if not isinstance(values, list):
            values = [values]
        if quick:
            values = values[:1]
        params = []
        for value in values:
            if len(names) == 1:
                params.append(pytest.param(value, id=_ids(value)))
            elif isinstance(value, dict):
                params.append(pytest.param(*[value.get(name) for name in names], id=_ids(value)))
            else:
                params.append(pytest.param(*value, id=_ids(value)))
        metafunc.parametrize(argnames, params)


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    port = config.getoption("--port")
    tags = set(config.getoption("with_tags"))
    try:
        family = board_config(config).family
    except ConfigError:
        family = None
    for item in items:
        if HIL_DIR in Path(str(item.path)).parents:
            item.add_marker(pytest.mark.hil)
        if item.get_closest_marker("hil") and not (port or config.getoption("--fake")):
            item.add_marker(pytest.mark.skip(reason="HIL test: pass --port"))
        marker = item.get_closest_marker("family")
        if marker and family and marker.args[0] != family:
            item.add_marker(pytest.mark.skip(reason=f"only for {marker.args[0]}"))
        marker = item.get_closest_marker("requires_option")
        if marker and marker.args[0] not in tags:
            item.add_marker(pytest.mark.skip(reason=f"enable with --with {marker.args[0]}"))
        if item.get_closest_marker("ad3") and config.getoption("--no-ad3"):
            item.add_marker(pytest.mark.skip(reason="--no-ad3"))


@pytest.fixture(scope="session")
def board_cfg(pytestconfig: pytest.Config) -> BoardConfig:
    return board_config(pytestconfig)


@pytest.fixture(scope="session")
def wiring(pytestconfig: pytest.Config, board_cfg: BoardConfig) -> Wiring:
    names = [name.strip() for name in pytestconfig.getoption("--wiring-set").split(",") if name.strip()]
    return board_cfg.wiring(names, pytestconfig.getoption("with_tags"))


class Need:
    """Wiring lookups that skip the test when the active wiring lacks the connection."""

    def __init__(self, wiring: Wiring, board: BoardConfig) -> None:
        self.wiring = wiring
        self.board = board

    def _found(self, value: int | None, what: str) -> int:
        if value is None:
            pytest.skip(f"{what} is not wired in wiring set(s) {', '.join(self.wiring.sets) or '(none)'}")
        return value

    def dio(self, pin: str | None = None, role: str | None = None) -> int:
        resolved = None if pin is None else self.board.resolve_pin(pin)
        return self._found(self.wiring.dio(resolved, role), f"DIO for {pin or role}")

    def wavegen(self, pin: str) -> int:
        return self._found(self.wiring.wavegen(self.board.resolve_pin(pin)), f"wavegen for {pin}")

    def scope(self, pin: str) -> int:
        return self._found(self.wiring.scope(self.board.resolve_pin(pin)), f"scope for {pin}")

    def optional_scope(self, pin: str) -> int | None:
        return self.wiring.scope(self.board.resolve_pin(pin))

    def tag(self, tag: str) -> None:
        if not self.wiring.has(tag):
            pytest.skip(f"enable with --with {tag}")


@pytest.fixture(scope="session")
def need(wiring: Wiring, board_cfg: BoardConfig) -> Need:
    return Need(wiring, board_cfg)


@pytest.fixture(scope="session")
def terminal(pytestconfig: pytest.Config, board_cfg: BoardConfig) -> Iterator[FirmwareTerminal]:
    port = pytestconfig.getoption("--port")
    serial = None
    if pytestconfig.getoption("--fake"):
        from hal_ti_validation.instruments.fake import FakeFirmware, FakeSerial

        name = board_cfg.firmware_name or board_cfg.name
        serial = FakeSerial(FakeFirmware(board=name, family=board_cfg.family, sysclk=board_cfg.sysclk or 0, pins=dict(board_cfg.pins)))
    elif not port:
        pytest.skip("pass --port")
    baud = pytestconfig.getoption("--baud") or board_cfg.terminal.baud
    with FirmwareTerminal(
        port,
        baud,
        timeout=board_cfg.terminal.command_timeout,
        serial=serial,
        max_command_length=board_cfg.terminal.max_command_length,
    ) as term:
        try:
            term.sync()
        except TerminalError as error:
            pytest.exit(f"firmware on {port} does not answer ping: {error}", returncode=3)
        yield term


@pytest.fixture(scope="session")
def fw(terminal: FirmwareTerminal, board_cfg: BoardConfig) -> Firmware:
    firmware = Firmware(terminal, board_cfg.pins)
    info = firmware.system.info()
    if not board_cfg.matches_firmware_name(info.board):
        pytest.exit(f"--board {board_cfg.name} but the firmware reports {info.board}", returncode=3)
    return firmware


@pytest.fixture(scope="session")
def ad3(pytestconfig: pytest.Config, board_cfg: BoardConfig) -> Iterator[Any]:
    if pytestconfig.getoption("--no-ad3"):
        pytest.skip("--no-ad3")
    from hal_ti_validation.instruments.ad3 import AnalogDiscovery3
    from hal_ti_validation.instruments.fake import FakeDwfApi

    device = AnalogDiscovery3(
        serial=pytestconfig.getoption("--ad3-serial"),
        analog_limits=(board_cfg.ad3.analog_min, board_cfg.ad3.analog_max),
        api_factory=FakeDwfApi if pytestconfig.getoption("--fake") else None,
    )
    try:
        device.open()
    except (OSError, RuntimeError) as error:
        pytest.skip(f"Analog Discovery 3 unavailable: {error}")
    if board_cfg.ad3.vplus is not None or board_cfg.ad3.vminus is not None:
        device.supplies.set(board_cfg.ad3.vplus, board_cfg.ad3.vminus)
    yield device
    device.close()


@pytest.fixture(autouse=True)
def _hil_isolation(request: pytest.FixtureRequest) -> Iterator[None]:
    """Clear stale events before a HIL test; afterwards close what it opened and reset the AD3 outputs."""
    if request.node.get_closest_marker("hil") is None:
        yield
        return
    firmware: Firmware = request.getfixturevalue("fw")
    firmware.terminal.drain_events()
    yield
    boots = firmware.terminal.drain_events("boot")
    failures = firmware.close_all()
    if "ad3" in request.fixturenames:
        request.getfixturevalue("ad3").reset_outputs()
    if boots and request.node.get_closest_marker("resets_board") is None:
        pytest.fail(f"unexpected reset during the test: {boots[-1].raw}")
    if failures:
        pytest.fail(f"cleanup failed: {failures}")
