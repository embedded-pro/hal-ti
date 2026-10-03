"""Board description and AD3 wiring loaded from `boards/<board>.yaml`."""

from __future__ import annotations

import copy
from collections.abc import Iterable, Mapping
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Any, Literal

import yaml

from .protocol import is_alias, normalize_pin

BOARDS_DIR = Path(__file__).resolve().parent.parent / "boards"
ChannelKind = Literal["dio", "wavegen", "scope"]
_KINDS: tuple[ChannelKind, ...] = ("dio", "wavegen", "scope")
_MISSING = object()


class ConfigError(ValueError):
    pass


@dataclass(frozen=True)
class Connection:
    """One AD3 channel wired to one firmware pin."""

    kind: ChannelKind
    channel: int
    pin: str | None
    role: str | None = None
    note: str = ""
    requires: str | None = None
    # Pins tied to `pin` by a jumper: the channel reaches them as well.
    jumpered: tuple[str, ...] = ()

    @property
    def pins(self) -> tuple[str, ...]:
        return ((self.pin,) if self.pin else ()) + self.jumpered

    def describe(self) -> str:
        name = {"dio": "DIO", "wavegen": "W", "scope": "Scope "}[self.kind]
        parts = [f"{name}{self.channel}", "+".join(self.pins) or "-"]
        if self.role:
            parts.append(f"({self.role})")
        if self.requires:
            parts.append(f"[--with {self.requires}]")
        if self.note:
            parts.append(f"- {self.note}")
        return " ".join(parts)


@dataclass(frozen=True)
class WiringSet:
    name: str
    description: str
    connections: tuple[Connection, ...]
    options: Mapping[str, str] = field(default_factory=dict)
    jumpers: tuple[str, ...] = ()


@dataclass(frozen=True)
class Wiring:
    """The merged active wiring sets, restricted to the optional connections that are enabled."""

    sets: tuple[str, ...]
    connections: tuple[Connection, ...]
    enabled: frozenset[str]
    jumpers: tuple[str, ...] = ()

    def channel(self, kind: ChannelKind, pin: str | None = None, role: str | None = None) -> int | None:
        for connection in self.connections:
            if connection.kind != kind:
                continue
            if role is not None and connection.role != role:
                continue
            if pin is not None and pin not in connection.pins:
                continue
            return connection.channel
        return None

    def dio(self, pin: str | None = None, role: str | None = None) -> int | None:
        return self.channel("dio", pin, role)

    def wavegen(self, pin: str | None = None, role: str | None = None) -> int | None:
        return self.channel("wavegen", pin, role)

    def scope(self, pin: str | None = None, role: str | None = None) -> int | None:
        return self.channel("scope", pin, role)

    def has(self, tag: str) -> bool:
        return tag in self.enabled

    def describe(self) -> str:
        lines = [f"wiring sets: {', '.join(self.sets) or '(none)'}"]
        lines += [f"  {connection.describe()}" for connection in self.connections]
        lines += [f"  jumper: {jumper}" for jumper in self.jumpers]
        return "\n".join(lines)


@dataclass(frozen=True)
class TerminalConfig:
    baud: int = 921600
    interface: str = ""
    uart: int | None = None
    pins: tuple[str, ...] = ()
    max_command_length: int = 255
    command_timeout: float = 2.0
    boot_timeout: float = 5.0


@dataclass(frozen=True)
class Ad3Config:
    vplus: float | None = None
    vminus: float | None = None
    analog_min: float = 0.0
    analog_max: float = 3.3


@dataclass
class BoardConfig:
    name: str
    family: str
    description: str
    sysclk: int | None
    terminal: TerminalConfig
    pins: dict[str, str]
    ad3: Ad3Config
    wiring_sets: dict[str, WiringSet]
    tests: dict[str, Any]
    path: Path | None = None
    firmware_name: str | None = None

    def matches_firmware_name(self, reported: str) -> bool:
        """`info`/`EVT boot` report e.g. `EK-TM4C123GXL` for the `ek_tm4c123gxl` board file."""
        expected = self.firmware_name or self.name
        return reported.lower().replace("-", "_") == expected.lower().replace("-", "_")

    def resolve_pin(self, pin: str) -> str:
        return normalize_pin(pin, self.pins)

    def param(self, path: str, default: Any = _MISSING) -> Any:
        """`param("pwm.frequencies")` → `tests.pwm.frequencies`."""
        node: Any = self.tests
        for part in path.split("."):
            if isinstance(node, Mapping) and part in node:
                node = node[part]
            elif default is not _MISSING:
                return default
            else:
                raise ConfigError(f"{self.name}: tests.{path} is not configured")
        return node

    def matrix(self, path: str) -> dict[str, list[Any]]:
        """`tests.<path>` as an ordered mapping of dimension name to a non-empty list of values."""
        node = self.param(path)
        if not isinstance(node, Mapping) or not node:
            raise ConfigError(f"{self.name}: tests.{path} must be a mapping of dimension to values")
        result: dict[str, list[Any]] = {}
        for name, values in node.items():
            values = list(values) if isinstance(values, (list, tuple)) else [values]
            if not values:
                raise ConfigError(f"{self.name}: tests.{path}.{name} has no values")
            result[str(name)] = values
        return result

    def aliases_of(self, pin: str) -> list[str]:
        """Aliases of the board table that name `pin`."""
        resolved = self.resolve_pin(pin)
        return [alias for alias, target in self.pins.items() if target == resolved]

    def set_param(self, path: str, value: Any) -> None:
        parts = path.split(".")
        node = self.tests
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        node[parts[-1]] = value

    def apply_overrides(self, overrides: Iterable[str]) -> None:
        """`pwm.frequencies=[20000]` style overrides (values parsed as YAML)."""
        for override in overrides:
            path, sep, text = override.partition("=")
            if not sep:
                raise ConfigError(f"override must be path=value: {override!r}")
            self.set_param(path.strip(), yaml.safe_load(text))

    def wiring(self, names: Iterable[str], enabled: Iterable[str] = ()) -> Wiring:
        tags = frozenset(enabled)
        selected: list[Connection] = []
        jumpers: list[str] = []
        used: dict[tuple[str, int], str] = {}
        names = tuple(name for name in names if name)
        for name in names:
            if name not in self.wiring_sets:
                known = ", ".join(sorted(self.wiring_sets))
                raise ConfigError(f"{self.name}: unknown wiring set {name!r} (known: {known})")
            wiring_set = self.wiring_sets[name]
            jumpers += [jumper for jumper in wiring_set.jumpers if jumper not in jumpers]
            for connection in wiring_set.connections:
                if connection.requires and connection.requires not in tags:
                    continue
                key = (connection.kind, connection.channel)
                if key in used and used[key] != name:
                    position, other = next((i, c) for i, c in enumerate(selected) if (c.kind, c.channel) == key)
                    if other.pins != connection.pins:
                        raise ConfigError(f"{connection.kind}{connection.channel} is wired differently in sets {used[key]!r} and {name!r}")
                    if other.role is None and connection.role is not None:
                        selected[position] = replace(other, role=connection.role, note=other.note or connection.note)
                    continue
                used[key] = name
                selected.append(connection)
        return Wiring(names, tuple(selected), tags, tuple(jumpers))


def _connection(kind: ChannelKind, channel: Any, spec: Any, pins: Mapping[str, str]) -> Connection:
    if isinstance(spec, str) or spec is None:
        spec = {"pin": spec}
    if not isinstance(spec, Mapping):
        raise ConfigError(f"bad {kind}{channel} entry: {spec!r}")
    pin = spec.get("pin")
    jumpered = spec.get("jumpered") or ()
    if isinstance(jumpered, str) or not isinstance(jumpered, (list, tuple)):
        raise ConfigError(f"{kind}{channel}: jumpered must be a list of pins")
    return Connection(
        kind=kind,
        channel=int(channel),
        pin=None if pin is None else normalize_pin(str(pin), pins, strict=True),
        role=spec.get("role"),
        note=str(spec.get("note", "")),
        requires=spec.get("requires"),
        jumpered=tuple(normalize_pin(str(other), pins, strict=True) for other in jumpered),
    )


def _wiring_set(name: str, raw: Mapping[str, Any], pins: Mapping[str, str]) -> WiringSet:
    connections: list[Connection] = []
    for kind in _KINDS:
        for channel, spec in (raw.get(kind) or {}).items():
            connections.append(_connection(kind, channel, spec, pins))
    for connection in connections:
        if connection.kind == "dio" and not 0 <= connection.channel <= 15:
            raise ConfigError(f"{name}: DIO{connection.channel} does not exist on the AD3")
        if connection.kind != "dio" and connection.channel not in (1, 2):
            raise ConfigError(f"{name}: {connection.kind} channel must be 1 or 2")
    return WiringSet(
        name=name,
        description=str(raw.get("description", "")),
        connections=tuple(connections),
        options=dict(raw.get("options") or {}),
        jumpers=tuple(raw.get("jumpers") or ()),
    )


def parse_board(raw: Mapping[str, Any], path: Path | None = None) -> BoardConfig:
    try:
        pins = {str(alias).lower(): normalize_pin(str(pin)) for alias, pin in (raw.get("pins") or {}).items()}
        unknown = sorted(alias for alias in pins if not is_alias(alias))
        if unknown:
            raise ConfigError(f"aliases outside the generic naming scheme: {', '.join(unknown)}")
        terminal_raw = dict(raw.get("terminal") or {})
        terminal_raw["pins"] = tuple(normalize_pin(str(pin), pins, strict=True) for pin in terminal_raw.get("pins", ()))
        ad3_raw = dict(raw.get("ad3") or {})
        limits = ad3_raw.pop("analog_limits", None)
        if limits is not None:
            ad3_raw["analog_min"], ad3_raw["analog_max"] = (float(value) for value in limits)
        return BoardConfig(
            name=str(raw["board"]),
            family=str(raw["family"]),
            description=str(raw.get("description", "")),
            sysclk=raw.get("sysclk"),
            terminal=TerminalConfig(**terminal_raw),
            pins=pins,
            ad3=Ad3Config(**ad3_raw),
            wiring_sets={name: _wiring_set(name, spec or {}, pins) for name, spec in (raw.get("wiring_sets") or {}).items()},
            tests=copy.deepcopy(dict(raw.get("tests") or {})),
            path=path,
            firmware_name=raw.get("firmware_name"),
        )
    except (KeyError, TypeError, ValueError) as error:
        raise ConfigError(f"{path or 'board'}: {error}") from error


def board_path(name_or_path: str | Path, boards_dir: Path = BOARDS_DIR) -> Path:
    candidate = Path(name_or_path)
    if candidate.suffix in (".yaml", ".yml") and candidate.exists():
        return candidate
    named = boards_dir / f"{name_or_path}.yaml"
    if named.exists():
        return named
    known = ", ".join(sorted(path.stem for path in boards_dir.glob("*.yaml")))
    raise ConfigError(f"no board {name_or_path!r} (known: {known})")


def load_board(name_or_path: str | Path, boards_dir: Path = BOARDS_DIR) -> BoardConfig:
    path = board_path(name_or_path, boards_dir)
    with path.open(encoding="utf-8") as stream:
        raw = yaml.safe_load(stream)
    return parse_board(raw, path)


def available_boards(boards_dir: Path = BOARDS_DIR) -> list[str]:
    return sorted(path.stem for path in boards_dir.glob("*.yaml"))
