Feature: Analog comparator
  `hal::tiva::AnalogComparator` / `SynchronousAnalogComparator`.

  Wiring set `bundle2`: W1 on the positive input, W2 on the negative input (a jumper ties the second comparator's
  input to the same channel), the output pin on a DIO when the instance has one; this covers the comparators whose
  positive input is C0+ (`src=c0`) too.
  The output is high while VIN- < VIN+ (inverted with `invert=1`).

  The negative level is tests.comparator.neg_level_v; the inputs and the output settle for 5 ms.

  @ad3
  Scenario: The output follows the comparison of the inputs
    Given the positive level is at least 0.1 V away from the negative level
    And the positive input is driven to the positive level
    And the negative input is driven to the negative level
    And the inputs settle
    When the instance opens on its inputs and output with source "pin", the inversion and the sync
    And the output settles
    Then the comparator reads whether the positive level is above the negative level, inverted with the inversion, and so does the output pin if the instance has one

  @ad3
  Scenario: With C0+ as positive input the output follows the comparison against C0+
    `src=c0`: the comparator compares its negative input against C0+.
    Given the positive level is at least 0.1 V away from the negative level
    And C0+ is driven to the positive level
    And the negative input is driven to the negative level
    And the inputs settle
    When the instance opens on its negative input and output with source "c0", the inversion and the sync
    And the output settles
    Then the comparator reads whether the positive level is above the negative level, inverted with the inversion, and so does the output pin if the instance has one

  @ad3
  Scenario: The internal reference ladder sets the threshold
    Given the expected reference of the range and step lies more than 0.05 V inside the wavegen range
    And the negative input is driven to the bottom of the wavegen range
    When the instance opens on its negative input with source "ref", the range and step and the sync
    Then the lowest negative input voltage at which the output drops, found by a binary search of the wavegen range down to the search step, is the expected reference within the reference tolerance

  @ad3
  Scenario: Every ADC trigger sense is accepted and leaves the comparison intact
    Every ADC trigger sense opens and leaves the comparison intact; the trigger itself is not observed
    because the driver gives it no observable effect.
    Given the negative input is driven to the negative level
    When the instance opens on its inputs and output with the trigger sense and the sync
    Then with the positive input driven 0.4 V below and then as far above the negative level, the comparator reads, once the inputs settle, whether the positive level is above the negative level, not inverted, and so does the output pin if the instance has one

  @ad3
  Scenario: The interrupt counts the output edges and stops counting when turned off
    Given the positive input is wired to a wavegen
    And the negative input is driven to the negative level
    And the positive input is driven halfway between the interrupt low and high levels
    And the inputs settle
    And the instance is open on its inputs and output with source "pin", the inversion and the asynchronous driver
    And the interrupt on the edge is enabled
    And the interrupt count is cleared
    When the positive input runs the cycles of a square wave between the interrupt low and high levels at the interrupt frequency
    And the firmware waits 10 ms
    Then the interrupt count is the number of cycles, twice that for both edges
    When the interrupt is turned off
    And the positive input runs 3 cycles of the square wave
    Then the interrupt count is still the number of the first cycles, twice that for both edges

  Scenario: Only the asynchronous driver has interrupts, and only on edges
    Only edge interrupts exist, and only on the asynchronous driver.
    Given the first instance is open on its inputs with the asynchronous driver
    Then enabling a "high" or a "low" level interrupt on it fails with "usage"
    When the first instance is closed and opened again on its inputs with the synchronous driver
    Then enabling a "rising" edge interrupt on it fails with "unsupported"

  Scenario: Invalid option combinations are refused with their reason
    Then opening the first instance with each of these options fails with the reason
      | pos     | neg     | src | ref    | trigger   | reason |
      | its pin |         |     |        |           | usage  |
      |         | its pin | pin |        |           | usage  |
      |         | its pin | pin | low,4  |           | usage  |
      |         | its pin |     | low,16 |           | range  |
      | its pin | its pin |     |        | sometimes | usage  |
