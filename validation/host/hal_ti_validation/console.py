"""`hal-ti-console`: the `ad3-bench-console` with the validation firmware defaults (921600 baud).

hal-ti-console --port /dev/ttyACM0
hal-ti-console --port /dev/ttyUSB0 -c info -c board.pins
"""

from __future__ import annotations

import sys
from pathlib import Path

from ad3_waveforms_bench import console


def main(argv: list[str] | None = None) -> int:
    return console.main(
        argv,
        prog="hal-ti-console",
        name="hal-ti validation console",
        prompt="hal-ti> ",
        baud=921600,
        history=Path.home() / ".hal_ti_validation_history",
    )


if __name__ == "__main__":
    sys.exit(main())
