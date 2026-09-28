"""hal-ti specifics of the validation terminal protocol (see validation/PROTOCOL.md): error reasons, pins and
aliases. Framing, parsing and formatting come from `ad3_waveforms_bench.protocol`."""

from __future__ import annotations

import re
from collections.abc import Mapping

from ad3_waveforms_bench.protocol import ProtocolError, Response

ERROR_REASONS = frozenset({"usage", "pin", "busy", "notopen", "unsupported", "range", "timeout", "failed"})

PIN_ALIASES = frozenset(
    {
        "phasea",
        "phaseb",
        "phasec",
        "vbus",
        "itotal",
        "halla",
        "hallb",
        "hallc",
        "enca",
        "encb",
        "encz",
        "pwm1a",
        "pwm1b",
        "pwm2a",
        "pwm2b",
        "pwm3a",
        "pwm3b",
        "canrx",
        "cantx",
        "ledop",
        "ledwarn",
        "ledfail",
        "perf",
        "id0",
        "id1",
        "id2",
        "pwrstatus",
    }
)

_PIN_RE = re.compile(r"^P([A-T])([0-7])$", re.IGNORECASE)


def is_pin(text: str) -> bool:
    return bool(_PIN_RE.match(text))


def normalize_pin(pin: str, aliases: Mapping[str, str] | None = None) -> str:
    """Canonical `P<port><index>` for a pin, or the lower-case alias when no alias map resolves it."""
    match = _PIN_RE.match(pin)
    if match:
        return f"P{match.group(1).upper()}{match.group(2)}"
    name = pin.lower()
    if aliases is not None and name in aliases:
        return normalize_pin(aliases[name])
    if name in PIN_ALIASES:
        return name
    raise ProtocolError(f"not a pin or alias: {pin!r}")


def parse_pin_map(response: Response) -> dict[str, str]:
    """`board.pins` → `OK alias=pin,alias=pin,...` (space separated pairs are accepted too)."""
    body = response.raw.split(maxsplit=1)[1] if " " in response.raw else ""
    result: dict[str, str] = {}
    for item in re.split(r"[\s,]+", body):
        key, sep, value = item.partition("=")
        if sep and key:
            result[key.lower()] = value.upper()
    return result
