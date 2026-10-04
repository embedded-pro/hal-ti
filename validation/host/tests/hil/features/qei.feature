Feature: Quadrature encoder
  `hal::tiva::QuadratureEncoder` driven by the AD3 pattern generator.

  Wiring set `bundle1`: A, B and index of each `tests.qei.instances` entry on DIOs. The pattern generator produces
  an exact number of 4-state cycles (A leads B for `fwd`). The default resolution, the counts per cycle of each
  capture mode and per clock pulse, the tolerances and the swept parameters are in tests.qei of the board file.

  @ad3
  Scenario: The position moves by the counts of the capture mode, reversed by inverting exactly one phase
    Counts per cycle follow the capture mode; inverting exactly one phase reverses the direction.
    Given the A, B and index pins of the instance are wired
    And the instance is open on its pins at the default resolution with the capture mode, the phase inversions and reset at the maximum
    And the position is read
    When the pattern generator runs the cycles at the frequency in the direction
    And the encoder is read
    Then the position moved by the counts per cycle of the capture mode times the cycles, modulo the default resolution, forwards for fwd and backwards for rev, the other way round when exactly one phase is inverted
    And the direction reads fwd when the position moved forwards, rev otherwise

  @ad3
  Scenario: Inverted phases with the matching inversion count like the plain signal
    Physically inverted phases with the matching `inva`/`invb` count like the plain signal.
    Given the A, B and index pins of the instance are wired
    And the instance is open on its pins at the default resolution with the phase inversions
    And the position is read
    When the pattern generator runs 25 fwd cycles at 1000 Hz on the phases inverted as the instance inverts them, ending with both phases low
    And the encoder is read
    Then the position moved forwards by the counts per cycle of ab capture times 25, modulo the default resolution
    And the direction reads fwd

  @ad3
  Scenario: The position starts at the offset and rolls over at the resolution
    Given the A, B and index pins of the instance are wired
    And the instance is open on its pins at the resolution with the offset and reset at the maximum
    Then the position reads the offset and the encoder reports the resolution or one less
    When the pattern generator runs 30 fwd cycles at 1000 Hz
    Then the position reads the offset plus 4 counts per cycle run, modulo the resolution

  @ad3
  Scenario: An index pulse restarts the position
    With `reset=index` the position restarts on every index pulse (`invi` moves the reset to the pulse's
    other edge, within the tolerance).
    Given the A, B and index pins of the instance are wired
    And the instance is open on its pins at the default resolution with the index inversion and reset on the index pulse
    When the pattern generator runs 3 index intervals plus 5 cycles at 1000 Hz in the direction, with an index pulse at the start of every index interval
    Then the position reads 4 counts per cycle since the last index pulse, negative for rev, within the index tolerance modulo the default resolution

  @ad3
  Scenario: The speed is the count rate over the velocity period
    Given the A, B and index pins of the instance are wired
    And the instance is open on its pins at the default resolution with the velocity period and ab capture
    When the pattern generator runs at the frequency until stopped
    And 3 velocity periods plus 0.05 s pass
    And the speed is read
    And the pattern generator stops
    Then the speed is the counts per cycle of ab capture times the frequency times the velocity period, within the speed tolerance or 1

  @ad3
  Scenario: In clock and direction mode the B level sets the counting direction
    Given the A, B and index pins of the instance are wired
    And the instance is open on its pins at the default resolution in clock and direction mode
    When the position change modulo the default resolution over 50 pulses at 1000 Hz on A is measured with the AD3 driving B low, then high
    And the AD3 releases B
    Then the two position changes are opposite
    And each position change is 50 times the counts per clock pulse in size

  Scenario: QEI0 opens on its default pins, other instances need A and B
    QEI0 opens on its default pins without arguments; other instances need `a` and `b`.
    When QEI 0 is opened without pins and closed
    Then opening QEI 0 at resolution 100 with offset 100 fails with "range"
    And opening each instance other than QEI 0 without pins, then on its A pin only, fails with "usage"
