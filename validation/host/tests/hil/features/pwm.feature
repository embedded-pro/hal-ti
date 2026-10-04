Feature: PWM
  `hal::tiva::Pwm` / `SynchronousPwm`: waveforms, outputs, dead band, update modes, interrupts, ADC triggers and the
  fault path.

  Wiring set `bundle1`: the A/B outputs of `tests.pwm.generators` and the fault pin on DIOs, W1 on the ADC input of
  the digital comparator fault path. Interrupt and ADC-trigger tests need no wiring.

  @ad3
  Scenario: Both outputs run at the set frequency and duty
    Given the system clock of the board
    And the A and B outputs of the first generator are wired
    And the duty does not round to a static level at the system clock over the divisor, unless it is 0 or 100 % or the period does not fit
    And the generator is opened with the mode, divisor, frequency and sync and no dead band, which fails with "range" exactly when the period does not fit
    Then it reports the system clock over the divisor as PWM clock, if it opened
    When the duty is set and both outputs are recorded, triggered on A unless the duty is 0 or 100 %, if the generator opened
    Then both outputs run at the expected frequency and duty, if the generator opened

  @ad3
  Scenario: The selected outputs run in step while the unused ones stay low
    1-4 generators with both outputs, only A (`<pin>:-`) or only B (`-:<pin>`); the unused pin stays a
    GPIO driven low, so the driver must not take it over.
    Given both outputs of as many generators as the generator count are wired
    And the outputs the output option leaves out are configured as GPIO outputs
    And the generators are opened on the outputs of the output option at 10000 Hz in edge mode with the sync and no dead band
    When the generators get the duties 20, 40, 60 and 80 % in order and the outputs are recorded, triggered on the first used one
    Then every used output runs at the expected frequency with the duty of its generator
    And the unused outputs stay low without an edge
    And the rising edges of the used outputs line up

  @ad3
  Scenario: The dead band delays each output after the other one switches off
    Dead band: A off -> B on is the falling-edge delay, B off -> A on the rising-edge delay; inverted outputs
    are compared after undoing the inversion.
    Given the system clock of the board
    And the A and B outputs of the first generator are wired
    And the generator is opened at the dead-time frequency with the mode, dead band, inversion and sync, which fails with "range" exactly when a delay is beyond the dead-band limits
    When the duty is set to 40 % and both outputs are recorded, if the generator opened
    Then A and B with the inversion undone are never active together, if the generator opened
    And A off -> B on takes the falling delay and B off -> A on the rising delay, if the generator opened
    And the high times of A and B plus both delays add up to one period, if the generator opened

  Scenario: Dead bands beyond the limits are refused
    Given the system clock of the board
    And the first configured generator
    Then opening it with the sync and a dead band just over the nanosecond limit, a dead band just over the clock limit or no rising and a falling delay just over the clock limit fails with "range"
    And it opens with the sync at divisor 64 with the largest dead band

  @ad3
  Scenario: The generators run in lock step and take new duties with the update mode
    Generators run in lock step (common edges or centres) and take new duties with local and global update.
    Given the A outputs of as many generators as the generator count are wired
    And the generators are opened on their A outputs at 20000 Hz with the update mode, mode and sync and no dead band
    When the generators get the duties 20, 50, 80 and 35 % in order and are recorded 10 ms later, triggered on the first one
    Then every A output runs at the expected frequency with the duty of its generator
    And the A outputs line up on their rising edges in edge mode and on their pulse centres in center mode
    When the generators get the duties 60, 30, 45 and 70 % in order and are recorded 10 ms later, triggered on the first one
    Then every A output runs at the expected frequency with the duty of its generator
    And the A outputs line up on their rising edges in edge mode and on their pulse centres in center mode

  @ad3
  Scenario: The outputs follow frequency changes and stop with the PWM
    Given the A and B outputs of the first generator are wired
    And the generator is opened at the first of the frequency changes with the mode and sync and no dead band
    And the duty is 50 %
    Then A runs at the new frequency with 50 % duty after each of the frequency changes
    When the PWM stops
    And the outputs are recorded at the last of the frequency changes
    Then neither output has an edge
    And setting the frequency to 1 Hz fails with "range"

  Scenario: The interrupt counts the events of its source
    Given the first configured generator is opened at the frequency in the mode with the interrupt source, at the smallest divisor that holds the period
    And the duty is 50 %
    When the interrupts of the generator are counted over the interrupt window
    Then the count matches the events of the source per period at the frequency

  Scenario: Each generator counts the events of its own interrupt source
    `irq` with one source per generator: every generator counts its own events, `none` counts nothing.
    Given all configured generators are opened at 5000 Hz in the mode with the interrupt sources zero, none, load and cmpbd in order, at the smallest divisor that holds the period
    And the duty is 50 %
    Then the interrupts of each generator, counted over the interrupt window one after the other, match the events of its source per period

  Scenario: Interrupts need the asynchronous driver
    Given the first configured generator
    Then opening it synchronously with the zero interrupt fails with "unsupported"

  Scenario: The generator triggers the ADC on the events of its source
    Generator `gen` triggers an asynchronous ADC sequencer (`trigger=pwm<gen>`) once per `source` event.
    Given the generator gen is opened at the ADC trigger frequency in the mode with divisor 64 and the trigger source
    And the duty is 50 %
    Then an ADC sequencer triggered by the generator converts no faster than the trigger, or times out if the source has no events in the mode

  Scenario: Only generators with a trigger source trigger the ADC
    `trigger` with one source per generator: only generators with a source trigger the ADC.
    Given all configured generators are opened at the ADC trigger frequency in center mode with divisor 64 and the trigger sources zero, none, load and none in order
    And the duty is 50 %
    Then an ADC sequencer triggered by each generator in turn converts no faster than the trigger, or times out if its source has no events in that mode

  Scenario: The ADC trigger stops with the PWM
    Given the first configured generator is opened at the ADC trigger frequency with divisor 64 and the zero trigger
    And the duty is 50 %
    And an ADC sequencer triggered by the generator converts 2 samples
    When the PWM closes
    Then converting 1 sample on the sequencer fails with "timeout"

  @ad3
  Scenario: A high fault pin raises a fault naming its input and the generators set up for it
    A high level on the fault pin raises `EVT pwm` naming the pin input and the configured generators.
    Given the fault pin is wired and driven low
    And all configured generators are opened at 10000 Hz
    And the fault pin input is enabled for the generators the gens option selects, with the latch and minperiod options
    And the duty is 50 %
    Then no fault is reported after 50 ms while the input is low
    When the fault pin is driven high
    Then a fault event arrives naming a fault input, a selected generator and no other generator
    When the fault pin is driven low and the fault detection is switched off

  @ad3
  Scenario: A fault stops both outputs
    Given the A and B outputs of the first generator are wired
    And the fault pin is wired and driven low
    And the first configured generator is opened at 10000 Hz on both outputs
    And the fault pin input is enabled
    And the duty is 50 %
    When the outputs are recorded at 10000 Hz
    Then A switches
    When the fault pin is driven high
    And a fault event arrives
    And the outputs are recorded at 10000 Hz
    And the fault pin is driven low
    Then neither output switched during the fault

  @ad3
  Scenario: Repeated faults are reported once until the PWM stops
    Repeated fault pulses raise a single `EVT pwm`; `pwm.stop` re-arms the report for the next fault.
    Given the fault pin is wired and driven low
    And the first configured generator is opened at 10000 Hz on both outputs
    And the fault pin input is enabled
    And the duty is 50 %
    When the fault pin pulses high 5 times, each time 5 ms high and 5 ms low
    Then a fault event arrives
    And no further fault is reported after 50 ms
    When the PWM stops
    And the duty is 50 %
    And the fault pin is driven high
    Then a fault event arrives
    When the fault pin is driven low

  @ad3
  Scenario: A digital comparator above its window raises a fault
    An ADC digital comparator (`adc.open dcmp=`) above its window raises `EVT pwm` with its comparator bit.
    Given the digital comparator input is wired to a wavegen at the safe level
    And the first configured generator is opened at 10000 Hz with the zero ADC trigger
    And an ADC sequencer triggered by the generator samples the input twice with the digital comparator window
    And the digital comparator fault is enabled for the generator
    And the duty is 50 %
    Then no fault is reported after 50 ms while the input is inside the safe band
    When the input goes to the trip level
    Then a fault event arrives naming the comparator and the generator
    When the input returns to the safe level

  Scenario: Invalid fault settings are refused
    Then enabling the fault with input mask 1 fails with "notopen"
    When the first generator opens
    Then every invalid fault setting fails with its reason
    And switching the fault detection off with a latch fails with "usage"
    And the fault inputs with comparator mask 1 can be enabled and switched off
    When the PWM closes and the first generator opens synchronously
    Then enabling the fault with input mask 1 fails with "unsupported"

  Scenario: Invalid opens and duties are refused
    Then every invalid open setting fails with its reason
    And setting a duty of 50 % fails with "notopen"
    When the PWM opens with the A pin of the first generator only
    Then setting 2 duties of 50 % fails with "usage"
    And opening the first generator fails with "busy"
