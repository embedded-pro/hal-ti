"""Host side of the hal-ti hardware-in-the-loop validation app."""

from .config import BoardConfig, Wiring, load_board
from .firmware import Firmware
from .protocol import Event, Response
from .terminal import FirmwareError, FirmwareTerminal, TerminalTimeout

__all__ = [
    "BoardConfig",
    "Event",
    "Firmware",
    "FirmwareError",
    "FirmwareTerminal",
    "Response",
    "TerminalTimeout",
    "Wiring",
    "load_board",
]
