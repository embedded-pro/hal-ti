Feature: GPIO
  `hal::tiva::GpioPin`: levels, pulls, open drain, interrupts and timer-driven pulses.

  Wiring set `bundle1`: the pins of `tests.gpio.loop_pins`/`output_pins` are pins of other peripherals, used here as
  plain GPIO; a locked pin that does not fit in bundle1 is in `bundle2` (EK-TM4C1294XL), and a board
  without a user LED on its headers has no `output_pins`.

  @ad3
  Scenario: The pin drives the DIO with every drive strength
    Given the pin is wired to a DIO
    Then the pin, configured as an output with the drive strength, drives the DIO to every level it is set to

  @ad3
  Scenario: The output pins drive the DIO
    Given the pin is wired to a DIO
    Then the pin, configured as an output, drives the DIO to every level it is set to

  @ad3
  Scenario: The pin reads the level the DIO drives with every pull
    Given the pin is wired to a DIO
    Then the pin, configured as an input with the pull, reads every level the DIO drives, and the DIO is released

  @ad3
  Scenario: A locked pin works as an output and as an input
    Given the pin is wired to a DIO
    Then the pin, configured as an output, drives the DIO to every level it is set to
    When the pin is released
    Then the pin, configured as an input with pull "up", reads every level the DIO drives, and the DIO is released

  @ad3
  Scenario: The pull sets the idle level
    Given the pin is wired to a DIO
    And the DIO is released
    When the pin is configured as an input with the pull
    Then the pin and the DIO read high if the pull is up and low otherwise

  @ad3
  Scenario: An open-drain pin pulls low and, released, follows the external level
    Given the pin is wired to a DIO
    And the DIO is released
    When the pin is configured as open drain
    And the pin is set to 0
    Then the DIO is pulled low
    When the pin is set to 1
    Then the released open-drain pin reads every level the DIO drives, and the DIO is released

  Scenario: Open drain refuses a pull
    Then configuring the first of the loop pins as open drain with pull "up" fails with "usage"

  @ad3
  Scenario: The interrupt counts the edges of the pulses and stops counting when turned off
    Given the pin is wired to a DIO
    And the DIO is released
    And the pin is configured as an input with pull "down"
    And the interrupt on the edge is enabled with the handler, unless that fails with "unsupported" for the port of the pin
    And the edge count of the pin is cleared
    When the AD3 sends the pulses on the DIO at the frequency
    And the firmware waits 10 ms
    Then the edge count is the number of pulses, twice that for both edges
    When the interrupt is turned off
    And the AD3 sends 3 pulses on the DIO at the frequency
    Then the edge count is still the number of the first pulses, twice that for both edges

  @ad3
  Scenario: Timer-driven pulses toggle the pin at the period
    Given the pin is wired to a DIO
    And the pulse count, tolerance and jitter of the board file, the jitter 0.5 ms unless set
    And the pin is configured as an output
    And the pin is set to 0
    When the pin pulses the pulse count of times at the period while the logic analyzer records the DIO from just before its first rising edge
    Then the DIO toggles the pulse count of times
    And the intervals between the toggles average the period within the tolerance and none is off by more than the jitter, if there are any
