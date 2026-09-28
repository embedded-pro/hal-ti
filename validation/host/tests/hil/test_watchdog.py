"""Watchdog (`hal::tiva::WatchDog`): early warnings, feeding and resets.

A started watchdog cannot be stopped, so every test resets the board afterwards.
"""

import time

import pytest

pytestmark = pytest.mark.resets_board


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


@pytest.mark.slow
@pytest.mark.board_params("index", "watchdog.indices")
@pytest.mark.board_params("timeout_ms", "watchdog.timeouts_ms")
def test_auto_feed_keeps_the_board_alive(fw, board_cfg, index, timeout_ms):
    keep_alive = board_cfg.param("watchdog.keep_alive_s")
    fw.wdt.start(index, timeout=timeout_ms, reset=True, feed="auto")
    warnings = fw.terminal.collect_events("wdt", keep_alive)
    assert not fw.terminal.events("boot"), "the board reset although the watchdog was fed"
    expected = keep_alive * 1000 / timeout_ms
    assert expected * 0.5 <= len(warnings) <= expected * 1.5 + 1, f"{len(warnings)} warnings in {keep_alive} s"
    assert all(event.as_int("index") == index for event in warnings)
    fw.system.ping()


@pytest.mark.board_params("index", "watchdog.indices")
@pytest.mark.board_params("timeout_ms", "watchdog.timeouts_ms")
def test_missing_feed_resets(fw, board_cfg, index, timeout_ms):
    fw.wdt.start(index, timeout=timeout_ms, reset=True, feed="manual")
    warning = fw.wdt.wait_warning(index, timeout=timeout_ms / 1000 * 2 + 1)
    assert warning.as_int("index") == index
    boot = fw.system.wait_boot(timeout=timeout_ms / 1000 * 3 + board_cfg.param("system.boot_timeout", 5.0))
    assert boot.reset == f"wdt{index}"


@pytest.mark.board_params("index", "watchdog.indices")
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


@pytest.mark.board_params("index", "watchdog.indices")
def test_warning_only_mode(fw, index):
    timeout_ms = 100
    fw.wdt.start(index, timeout=timeout_ms, reset=False, feed="manual")
    warnings = fw.terminal.collect_events("wdt", timeout_ms / 1000 * 5)
    assert warnings, "no early warning"
    assert not fw.terminal.events("boot"), "reset although reset=0"
