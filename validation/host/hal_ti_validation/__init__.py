"""Host side of the hal-ti hardware-in-the-loop validation app (instrument, terminal and analysis code comes
from `ad3_waveforms_bench`)."""

from ad3_waveforms_bench.protocol import Event, Response
from ad3_waveforms_bench.terminal import FirmwareError, FirmwareTerminal, TerminalTimeout

from .config import BoardConfig, Wiring, load_board
from .firmware import Dcmp, Firmware, PwmFault
from .pairwise import combinations, full_product, pairwise

__all__ = [
    "BoardConfig",
    "Dcmp",
    "Event",
    "Firmware",
    "PwmFault",
    "FirmwareError",
    "FirmwareTerminal",
    "Response",
    "TerminalTimeout",
    "Wiring",
    "combinations",
    "full_product",
    "load_board",
    "pairwise",
]
