Feature: EEPROM
  hal::tiva::Eeprom erases, writes and reads back data at any address, refuses accesses past its end and keeps its
  content across a reset. The size, the chunk limits and the swept addresses and patterns are in tests.eeprom of
  the board file.

  @slow
  Scenario: Erasing clears every byte
    Given "0000000000000000" is written at address 0
    When the EEPROM is erased
    Then every byte reads ff

  Scenario: A pattern reads back without touching its neighbours
    Given the pattern fits at the address
    And the EEPROM is erased
    When the pattern is written at the address
    Then the pattern reads back at the address
    And the 4 bytes on each side of it still read ff

  Scenario: Writing without erasing replaces the old bytes
    Given "00112233" is written at address 16
    When "ffeeddcc" is written at address 16
    Then 4 bytes at address 16 read "ffeeddcc"

  Scenario: The largest write a command line can carry reads back
    When the largest pattern one command line can carry is written at address 0
    Then it reads back in chunks

  Scenario: Accesses past the end are refused
    When "a1b2c3d4" is written in the last 4 bytes
    Then the last 4 bytes read "a1b2c3d4"
    And writing 4 bytes 2 before the end fails with "range"
    And reading 4 bytes 2 before the end fails with "range"
    And reading 1 byte at the end fails with "range"

  @resets_board
  Scenario: The content persists across a reset
    Given "c0ffee00deadbeef" is written at address 64
    When the board resets
    Then 8 bytes at address 64 read "c0ffee00deadbeef"
