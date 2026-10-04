@resets_board
Feature: Watchdog
  `hal::tiva::WatchDog`: early warnings, feeding, resets and the early-warning period.

  A started watchdog cannot be stopped, so every test resets the board afterwards. `tests.watchdog.pin` is a
  bundle1 pin: the `pin=` toggle output is on its DIO, so the logic analyzer measures the warning period. The
  observation periods, the period tolerance and the swept parameters are in tests.watchdog of the board file; the
  observation window is the observation periods of timeouts, at least 0.3 s.

  @slow
  Scenario: A fed watchdog warns once per timeout, a missed feed resets the board only with reset enabled
    `feed=auto` keeps the board alive with one warning per timeout; a missed feed resets only with
    `reset=1`, and warns without resetting otherwise.
    When the watchdog starts with the timeout, the reset setting and the feed mode
    Then a manually fed watchdog with reset warns within 2 timeouts plus 1 s, and the board then boots from its reset within 3 timeouts plus the boot timeout
    And any other watchdog warns at least once during the observation window, only under its own index, and the board does not reset
    And an automatically fed watchdog warns at least 0.5 and at most 1.5 times plus 1 as often as the observation window holds timeouts
    And the board answers ping unless the watchdog is fed manually with reset

  @ad3 @slow
  Scenario: The early-warning pin toggles once per timeout
    The `pin=` output toggles on every early warning: the toggle interval is the programmed timeout.
    Given the watchdog pin is wired
    And the logic analyzer is armed on its DIO for the observation periods plus 1.5 timeouts, at its clock or slower so the buffer holds them, triggering on either edge with 2 % pretrigger
    When the watchdog starts with the timeout and the reset setting, fed automatically and toggling the pin on every warning
    And the capture completes within its length plus one timeout plus 2 s
    Then it holds at least the observation periods of toggles
    And the median interval between toggles is the timeout within the period tolerance or 2 samples
    And the board has not reset although the watchdog was fed

  Scenario: A manually fed watchdog keeps the board alive and resets it once the feeding stops
    When the watchdog starts with a 500 ms timeout, resetting the board and fed manually
    And it is fed every quarter timeout for 2 s
    Then the board has not reset while it was fed
    And the board boots from a reset by the watchdog within 3 timeouts plus the boot timeout

  Scenario: Only one watchdog runs at a time
    Both watchdogs share one interrupt vector: a second start is refused.
    Given watchdog 0 is started with a 1000 ms timeout and without reset
    Then starting watchdog 1 with a 1000 ms timeout and without reset fails with "busy"

  Scenario: Invalid start options are refused and leave the watchdog stopped
    Then starting watchdog 0 with the options fails with the reason
    And feeding watchdog 0 fails with "notopen"
