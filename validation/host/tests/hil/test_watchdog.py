"""Watchdog (`hal::tiva::WatchDog`): early warnings, feeding, resets and the early-warning period.

A started watchdog cannot be stopped, so every test resets the board afterwards. `tests.watchdog.pin` is a
bundle1 pin: the `pin=` toggle output is on its DIO, so the logic analyzer measures the warning period.

Scenarios: features/watchdog.feature.
"""

from __future__ import annotations

import statistics
import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when


@pytest.fixture
def wdt_cfg(board_cfg):
    return board_cfg.param("watchdog")


@pytest.fixture
def warning_capture():
    return {}


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


def resets_unfed(feed, reset):
    return feed == "manual" and reset


@pytest.mark.matrix("watchdog.behaviour")
@scenario("watchdog.feature", "A fed watchdog warns once per timeout, a missed feed resets the board only with reset enabled")
def test_behaviour(index, timeout_ms, reset, feed):
    pass


@pytest.mark.matrix("watchdog.period")
@scenario("watchdog.feature", "The early-warning pin toggles once per timeout")
def test_warning_period(index, timeout_ms, reset):
    pass


@pytest.mark.board_params("index", values=[0, 1])
@scenario("watchdog.feature", "A manually fed watchdog keeps the board alive and resets it once the feeding stops")
def test_manual_feed(index):
    pass


@scenario("watchdog.feature", "Only one watchdog runs at a time")
def test_only_one_watchdog():
    pass


@pytest.mark.board_params(
    "options,reason",
    values=[
        [{"timeout": 0}, "range"],
        [{"timeout": 30001}, "range"],
        [{"timeout": 100, "feed": "sometimes"}, "usage"],
        [{"timeout": 100, "pin": "terminaltx"}, "busy"],
    ],
)
@scenario("watchdog.feature", "Invalid start options are refused and leave the watchdog stopped")
def test_start_errors(options, reason):
    pass


@given("the watchdog pin is wired", target_fixture="dio")
def pin_wired(need, wdt_cfg):
    return need.dio(wdt_cfg["pin"])


@given(
    parsers.parse(
        "the logic analyzer is armed on its DIO for the observation periods plus {spare:g} timeouts, at its clock or slower "
        "so the buffer holds them, triggering on either edge with {pretrigger:d} % pretrigger"
    )
)
def arm_on_pin(ad3, wdt_cfg, timeout_ms, dio, warning_capture, spare, pretrigger):
    periods = wdt_cfg["observe_periods"]
    duration = (periods + spare) * timeout_ms / 1000
    rate = min(ad3.logic.clock_hz, ad3.logic.buffer_size / duration)
    warning_capture["duration"] = duration
    warning_capture["pending"] = ad3.logic.arm(rate, int(duration * rate), trigger=(dio, "either"), pretrigger=pretrigger / 100)


@given(parsers.parse("watchdog {unit:d} is started with a {period_ms:d} ms timeout and without reset"))
def start_without_reset(fw, unit, period_ms):
    fw.wdt.start(unit, timeout=period_ms, reset=False)


@when("the watchdog starts with the timeout, the reset setting and the feed mode")
def start_with_feed(fw, index, timeout_ms, reset, feed):
    fw.wdt.start(index, timeout=timeout_ms, reset=reset, feed=feed)


@when("the watchdog starts with the timeout and the reset setting, fed automatically and toggling the pin on every warning")
def start_toggling(fw, wdt_cfg, index, timeout_ms, reset):
    fw.wdt.start(index, timeout=timeout_ms, reset=reset, feed="auto", pin=wdt_cfg["pin"])


@when(parsers.parse("the capture completes within its length plus one timeout plus {margin:g} s"))
def capture_completes(timeout_ms, warning_capture, margin):
    # The capture triggers on the first toggle, one timeout after the start
    warning_capture["result"] = warning_capture["pending"].wait(timeout=warning_capture["duration"] + timeout_ms / 1000 + margin)


@when(
    parsers.parse("the watchdog starts with a {period_ms:d} ms timeout, resetting the board and fed manually"),
    target_fixture="fed_timeout_ms",
)
def start_manual(fw, index, period_ms):
    fw.wdt.start(index, timeout=period_ms, reset=True, feed="manual")
    return period_ms


@when(parsers.parse("it is fed every quarter timeout for {seconds:g} s"))
def feed_regularly(fw, index, fed_timeout_ms, seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        fw.wdt.feed(index)
        time.sleep(fed_timeout_ms / 1000 / 4)


@then(
    parsers.parse(
        "a manually fed watchdog with reset warns under its index within {warn_timeouts:d} timeouts plus {warn_margin:d} s, "
        "and the board then boots from its reset within {boot_timeouts:d} timeouts plus the boot timeout"
    )
)
def missed_feed_resets(fw, board_cfg, index, timeout_ms, reset, feed, warn_timeouts, warn_margin, boot_timeouts):
    if not resets_unfed(feed, reset):
        return
    boot_timeout = board_cfg.param("system.boot_timeout", 5.0)
    warning = fw.wdt.wait_warning(index, timeout=timeout_ms / 1000 * warn_timeouts + warn_margin)
    assert warning.as_int("index") == index
    boot = fw.system.wait_boot(timeout=timeout_ms / 1000 * boot_timeouts + boot_timeout)
    assert boot.reset == f"wdt{index}"


@then(
    "any other watchdog warns at least once during the observation window, only under its own index, and the board does not reset",
    target_fixture="warnings",
)
def warns_without_reset(fw, wdt_cfg, index, timeout_ms, reset, feed):
    if resets_unfed(feed, reset):
        return None
    window = observation(wdt_cfg, timeout_ms)
    warnings = fw.terminal.collect_events("wdt", window)
    assert not fw.terminal.events("boot"), "the board reset although it was fed or reset=0"
    assert warnings, "no early warning"
    assert all(event.as_int("index") == index for event in warnings)
    return warnings


@then(
    parsers.parse(
        "an automatically fed watchdog warns at least {low:g} and at most {high:g} times plus {plus:d} as often as the "
        "observation window holds timeouts"
    )
)
def one_warning_per_timeout(wdt_cfg, timeout_ms, feed, warnings, low, high, plus):
    if feed == "auto":
        window = observation(wdt_cfg, timeout_ms)
        expected = window * 1000 / timeout_ms
        assert expected * low <= len(warnings) <= expected * high + plus, f"{len(warnings)} warnings in {window} s"


@then("the board answers ping unless the watchdog is fed manually with reset")
def answers_ping(fw, reset, feed):
    if not resets_unfed(feed, reset):
        fw.system.ping()


@then("it holds at least the observation periods of toggles")
def enough_toggles(wdt_cfg, dio, warning_capture):
    periods = wdt_cfg["observe_periods"]
    duration = warning_capture["duration"]
    edges = [edge.index for edge in analysis.edges(warning_capture["result"].channel(dio))]
    warning_capture["edges"] = edges
    assert len(edges) >= periods, f"{len(edges)} toggles in {duration:.3f} s"


@then(parsers.parse("the median interval between toggles is the timeout within the period tolerance or {samples:d} samples"))
def toggle_interval(wdt_cfg, timeout_ms, warning_capture, samples):
    result = warning_capture["result"]
    edges = warning_capture["edges"]
    intervals = [(b - a) / result.rate for a, b in zip(edges, edges[1:])]
    measured = statistics.median(intervals)
    assert measured == pytest.approx(timeout_ms / 1000, rel=wdt_cfg["period_tolerance"], abs=samples / result.rate)


@then("the board has not reset although the watchdog was fed")
def not_reset_with_auto_feed(fw):
    assert not fw.terminal.events("boot"), "the board reset although it was fed"


@then("the board has not reset while it was fed")
def not_reset_while_fed(fw):
    assert not fw.terminal.events("boot"), "reset although fed"


@then(parsers.parse("the board boots from a reset by the watchdog within {boot_timeouts:d} timeouts plus the boot timeout"))
def boots_from_watchdog(fw, board_cfg, index, fed_timeout_ms, boot_timeouts):
    boot = fw.system.wait_boot(timeout=fed_timeout_ms / 1000 * boot_timeouts + board_cfg.param("system.boot_timeout", 5.0))
    assert boot.reset == f"wdt{index}"


@then(parsers.parse('starting watchdog {unit:d} with a {period_ms:d} ms timeout and without reset fails with "{refusal}"'))
def second_start_refused(fw, unit, period_ms, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.wdt.start(unit, timeout=period_ms, reset=False)
    assert error.value.reason == refusal


@then(parsers.parse("starting watchdog {unit:d} with the options fails with the reason"))
def start_refused(fw, options, reason, unit):
    with pytest.raises(FirmwareError) as error:
        fw.wdt.start(unit, **options)
    assert error.value.reason == reason


@then(parsers.parse('feeding watchdog {unit:d} fails with "{refusal}"'))
def feed_refused(fw, unit, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.wdt.feed(unit)
    assert error.value.reason == refusal
