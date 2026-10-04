Feature: System
  General commands, framing and error semantics (no AD3 needed). The boot timeout, the debug LED, the reserved
  UARTs and the command lines of missing instances are in tests.system of the board file.

  Scenario: The board answers ping
    Then the board answers ping

  Scenario: The board info matches the board file
    When the board info is read
    Then it names the board of the board file
    And it reports the family of the board file
    And it reports the system clock of the board file, if the board file gives one

  Scenario: The firmware and the board file have the same pin aliases
    `board.pins` and the board file's `pins` are the same alias table, in both directions.
    When the firmware lists its pin aliases
    Then every alias of the board file names the same pin in the firmware
    And the firmware has no alias the board file lacks

  Scenario: The board file uses generic aliases, including the terminal pins
    Then every alias of the board file is a generic alias
    And the board file has the aliases terminaltx and terminalrx

  Scenario: Every alias names its pin in commands
    Each alias names its pin in commands; the terminal pins stay reserved.
    Then every alias of the board file configures as a GPIO input, its pin reads and the alias releases, except that configuring an alias of a terminal pin fails with "busy"

  Scenario: The terminal pins are reserved
    Then configuring each terminal pin as a GPIO input fails with "busy"

  Scenario: The debug LED is reserved
    The firmware blinks its debug LED, so tests cannot claim that pin.
    Then configuring the debug LED pin as a GPIO input fails with "busy"

  Scenario: The terminal UART is reserved
    Then opening the reserved UART fails with "busy"

  Scenario: Unknown commands and keys are refused
    Then the command "no.such.command" fails with "usage" or "unrecognized"
    And the command "ping" with nosuchkey=1 fails with "usage"

  Scenario: A command on a missing instance is refused
    Then the command line of the missing instance fails with "range"

  Scenario: Malformed commands are refused
    Then the command with the arguments fails with "usage" or "range"

  Scenario: Closing what is not open and opening what is open are refused
    Then closing ADC 1 sequencer 3 fails with "notopen"
    When ADC 1 sequencer 3 is opened in synchronous mode on ain0
    Then opening ADC 1 sequencer 3 in synchronous mode on ain0 again fails with "busy"
    And ADC 1 sequencer 3 closes

  Scenario: A delay lasts at least the requested time
    When the firmware delays for the time, timed on the host
    Then it took at least 95 % of the time

  @resets_board
  Scenario: A reset reports a software reset
    When the board resets
    Then the boot message names the board and the family of the board file and the reset cause "sw"
    And the board answers ping
    And the board info reports the reset cause "sw"
