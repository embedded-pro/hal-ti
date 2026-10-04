Feature: ADC
  hal::tiva::Adc and SynchronousAdc read wavegen DC levels as raw 12-bit codes.
  Wiring set bundle1: W1/W2 on the two inputs of tests.adc.inputs (the first and the second input); the scope on the
  same pins (optional) measures the actual level, which then replaces the programmed one as reference. Asynchronous
  sequencers convert on a PWM generator trigger (the driver has no processor trigger), which each scenario opens
  itself: the trigger PWM generator is tests.adc.trigger, opened with an ADC trigger on its zero count, and it runs
  once its duty is set to 50 %; a sequencer "on the trigger" converts on it.
  A measurement covers tests.adc.samples runs of the sequence. A sequencer reads a level when the mean of a
  measurement is within tests.adc.tolerance_codes of the level's code at tests.adc.vref and its peak-to-peak within
  tests.adc.spread_codes (default twice the tolerance). The digital comparators compare against the window
  tests.adc.dcmp_window; tests.adc.dcmp_levels_v gives a level inside each band, and the level outside a band is the
  high one for the low band, the low one otherwise.

  @ad3
  Scenario: A DC level reads as its code
    Given the pin is driven to the level
    And the trigger PWM generator runs unless the sequencer is synchronous
    When sequencer 3 of the ADC opens on the pin, synchronous or on the trigger
    Then sequencer 3 of the ADC reads the level

  @ad3
  Scenario: Every sequencer reads its inputs with all its steps
    Every sequencer with all its steps (8/4/4/1), alternating the two driven inputs.
    Given the first input is driven to 0.8 V and the second to 2.4 V
    And the trigger PWM generator runs unless the sequencer is synchronous
    When the sequencer of the ADC opens with all its steps alternating the first and the second input, synchronous or on the trigger
    Then the sequencer of the ADC returns all steps of as many of the runs as fit in 64 values, each step reading the level of its input

  Scenario: A sequencer refuses more steps than its depth
    Then opening the sequencer of ADC 0 synchronously with one step more than its depth on the first input fails with "range"

  @ad3
  Scenario: A level reads as its code with any sample and hold time, averaging and sampling delay
    Given the first input is driven to 1.65 V
    And the trigger PWM generator runs unless the sequencer is synchronous
    When sequencer 3 of ADC 0 opens on the first input with the sample and hold time, the averaging and the delay, synchronous or on the trigger
    Then sequencer 3 of ADC 0 reads the level

  Scenario: An asynchronous sequencer needs a trigger and every sequencer needs pins
    Then opening sequencer 3 of ADC 0 on the first input without a trigger fails with "usage"
    And opening sequencer 3 of ADC 0 synchronously without pins fails with "usage"

  Scenario: A synchronous sequencer refuses the asynchronous options
    Then opening sequencer 1 of ADC 0 synchronously with two steps on the first input and the option fails with "unsupported"

  Scenario: An asynchronous sequencer converts only while its PWM generator runs
    An asynchronous sequencer whose PWM generator does not run never converts.
    Given the trigger PWM generator is open with no duty set
    And sequencer 3 of ADC 0 is open on the first input, on the trigger
    Then measuring 1 run of sequencer 3 of ADC 0 within 3 s fails with "timeout"
    When the trigger PWM generator runs at 50 % duty
    Then measuring 4 runs of sequencer 3 of ADC 0 within 3 s returns 4 values

  @ad3
  Scenario: A digital comparator step stays out of the FIFO and reports its band as a PWM fault
    A digital comparator step leaves the FIFO and, routed to the PWM fault inputs, reports its band.
    Given the first input is driven outside the band
    And the trigger PWM generator is open with no duty set
    And sequencer 1 of ADC 0 is open with two steps on the first input, on the trigger, the second routed to the comparator with the window, the band and the mode
    And the comparator is a fault input of the trigger PWM generator
    When the trigger PWM generator runs at 50 % duty
    Then measuring 4 runs of sequencer 1 of ADC 0 within 3 s returns only the step not routed to the comparator
    And after 50 ms the trigger PWM module has reported no fault
    When the first input is driven inside the band
    Then the trigger PWM module reports a fault of the comparator

  Scenario: Invalid digital comparator entries are refused
    Then opening sequencer 1 of ADC 0 with two steps on the first input, on the trigger, with the comparator entries fails with the reason

  @ad3
  Scenario: Two synchronous sequencers with priorities read their own inputs
    Two synchronous sequencers of the same ADC with explicit priorities convert their own inputs.
    Given the first input is driven to 0.6 V and the second to 2.7 V
    When sequencer 1 of ADC 0 opens synchronously on the first input with the first priority of the order
    And sequencer 2 of ADC 0 opens synchronously on the second input with the second priority of the order
    Then sequencer 1 of ADC 0 reads the level of the first input
    And sequencer 2 of ADC 0 reads the level of the second input

  @ad3
  Scenario: A level reads as its code with the internal reference
    Given the first input is driven to 2.0 V
    And the trigger PWM generator runs
    When sequencer 3 of ADC 0 opens on the first input, on the trigger, with reference "int"
    Then sequencer 3 of ADC 0 reads the level

  @ad3
  Scenario: With the external reference the codes follow VREFA+
    With ref=ext the codes follow VREFA+ (tests.adc.external_reference_v).
    Given the board file gives the VREFA+ voltage
    And the first input is driven to the lower of 2.0 V and 0.8 times the VREFA+ voltage
    And the trigger PWM generator runs
    When sequencer 3 of ADC 0 opens on the first input, on the trigger, with reference "ext"
    Then sequencer 3 of ADC 0 reads the level against the VREFA+ voltage
