"""The HIL scenarios are complete: pytest-bdd reports an unbound scenario or a step without a definition only when it
runs the scenario, and without hardware almost every HIL scenario skips before its first step."""

import subprocess
import sys
from pathlib import Path

HOST = Path(__file__).parents[2]
FEATURES = HOST / "tests" / "hil" / "features"


def test_every_scenario_is_bound_and_every_step_defined():
    result = subprocess.run(
        [sys.executable, "-m", "pytest", "--generate-missing", "--feature", str(FEATURES), "tests/hil", "-p", "no:cacheprovider"],
        cwd=HOST,
        capture_output=True,
        text=True,
        check=False,
    )
    output = result.stdout + result.stderr
    assert result.returncode == 0, output
    missing = [line for line in output.splitlines() if "is not bound" in line or "is not defined" in line]
    assert not missing, "\n".join(missing)


def test_every_hil_module_has_its_feature():
    modules = {path.stem.removeprefix("test_") for path in (HOST / "tests" / "hil").glob("test_*.py")}
    features = {path.stem for path in FEATURES.glob("*.feature")}
    assert modules == features
