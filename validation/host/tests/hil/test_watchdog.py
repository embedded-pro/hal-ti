"""Watchdog (`hal::tiva::WatchDog`): early warnings, feeding, resets and the early-warning period.

A started watchdog cannot be stopped, so every test resets the board afterwards. `tests.watchdog.pin` is a
bundle1 pin: the `pin=` toggle output is on its DIO, so the logic analyzer measures the warning period.
"""

from __future__ import annotations

import statistics
import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError

pytestmark = pytest.mark.resets_board


@pytest.fixture
def wdt_cfg(board_cfg):
    return board_cfg.param("watchdog")


@pytest.fixture(autouse=True)
def reset_afterwards(fw, board_cfg):
    yield
    boot_timeout = board_cfg.param("system.boot_timeout", 5.0)
    fw.terminal.drain_events()
    try:
        fw.system.ping()
        fw.system.reset(timeout=boot_timeout)
    except Exception:  # noqa: BLE001 - the board may be rebooting right now
        fw.system.wait_boot(timeout=boot_timeout)
    fw.terminal.drain_events()


def observation(wdt_cfg, timeout_ms):
    return max(0.3, wdt_cfg["observe_periods"] * timeout_ms / 1000)


@pytest.mark.slow
@pytest.mark.matrix("watchdog.behaviour")
def test_behaviour(fw, board_cfg, wdt_cfg, index, timeout_ms, reset, feed):
    """`feed=auto` keeps the board alive with one warning per timeout; a missed feed resets only with
    `reset=1`, and warns without resetting otherwise."""
    boot_timeout = board_cfg.param("system.boot_timeout", 5.0)
    fw.wdt.start(index, timeout=timeout_ms, reset=reset, feed=feed)
    if feed == "manual" and reset:
        warning = fw.wdt.wait_warning(index, timeout=timeout_ms / 1000 * 2 + 1)
        assert warning.as_int("index") == index
        boot = fw.system.wait_boot(timeout=timeout_ms / 1000 * 3 + boot_timeout)
        assert boot.reset == f"wdt{index}"
        return
    window = observation(wdt_cfg, timeout_ms)
    warnings = fw.terminal.collect_events("wdt", window)
    assert not fw.terminal.events("boot"), "the board reset although it was fed or reset=0"
    assert warnings, "no early warning"
    assert all(event.as_int("index") == index for event in warnings)
    if feed == "auto":
        expected = window * 1000 / timeout_ms
        assert expected * 0.5 <= len(warnings) <= expected * 1.5 + 1, f"{len(warnings)} warnings in {window} s"
    fw.system.ping()


@pytest.mark.ad3
@pytest.mark.slow
@pytest.mark.matrix("watchdog.period")
def test_warning_period(fw, ad3, need, wdt_cfg, index, timeout_ms, reset):
    """The `pin=` output toggles on every early warning: the toggle interval is the programmed timeout."""
    pin = wdt_cfg["pin"]
    dio = need.dio(pin)
    periods = wdt_cfg["observe_periods"]
    duration = (periods + 1.5) * timeout_ms / 1000
    rate = min(ad3.logic.clock_hz, ad3.logic.buffer_size / duration)
    capture = ad3.logic.arm(rate, int(duration * rate), trigger=(dio, "either"), pretrigger=0.02)
    fw.wdt.start(index, timeout=timeout_ms, reset=reset, feed="auto", pin=pin)
    # The capture triggers on the first toggle, one timeout after the start
    result = capture.wait(timeout=duration + timeout_ms / 1000 + 2.0)
    edges = [edge.index for edge in analysis.edges(result.channel(dio))]
    assert len(edges) >= periods, f"{len(edges)} toggles in {duration:.3f} s"
    intervals = [(b - a) / result.rate for a, b in zip(edges, edges[1:])]
    measured = statistics.median(intervals)
    assert measured == pytest.approx(timeout_ms / 1000, rel=wdt_cfg["period_tolerance"], abs=2 / result.rate)
    assert not fw.terminal.events("boot"), "the board reset although it was fed"


@pytest.mark.board_params("index", values=[0, 1])
def test_manual_feed(fw, board_cfg, index):
    timeout_ms = 500
    fw.wdt.start(index, timeout=timeout_ms, reset=True, feed="manual")
    end = time.monotonic() + 2.0
    while time.monotonic() < end:
        fw.wdt.feed(index)
        time.sleep(timeout_ms / 1000 / 4)
    assert not fw.terminal.events("boot"), "reset although fed"
    boot = fw.system.wait_boot(timeout=timeout_ms / 1000 * 3 + board_cfg.param("system.boot_timeout", 5.0))
    assert boot.reset == f"wdt{index}"


def test_only_one_watchdog(fw):
    """Both watchdogs share one interrupt vector: a second start is refused."""
    fw.wdt.start(0, timeout=1000, reset=False)
    with pytest.raises(FirmwareError) as error:
        fw.wdt.start(1, timeout=1000, reset=False)
    assert error.value.reason == "busy"


@pytest.mark.board_params(
    "options,reason",
    values=[
        [{"timeout": 0}, "range"],
        [{"timeout": 30001}, "range"],
        [{"timeout": 100, "feed": "sometimes"}, "usage"],
        [{"timeout": 100, "pin": "terminaltx"}, "busy"],
    ],
)
def test_start_errors(fw, options, reason):
    with pytest.raises(FirmwareError) as error:
        fw.wdt.start(0, **options)
    assert error.value.reason == reason
    with pytest.raises(FirmwareError) as error:
        fw.wdt.feed(0)
    assert error.value.reason == "notopen"
