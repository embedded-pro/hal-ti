Feature: UART
  `hal::tiva::Uart`, `UartWithDma` and `SynchronousUart` against the AD3 protocol UART.

  Wiring set `bundle1`: firmware TX, RX, RTS and CTS of `tests.uart` on DIOs (`bundle2` for RTS/CTS on the
  EK-TM4C1294XL). The logic analyzer also records the firmware TX line to decode the frames and measure the bit rate.

  An instance is open against the AD3 when the firmware has opened it on its TX and RX pins with the baud rate, the
  parity and stop bits (no parity and 1 stop bit unless a step names them) and the driver of the variant (interrupt
  unless a step names it, see tests.uart.variants), the AD3 protocol UART runs with the same settings on the DIOs of
  these pins and both receivers have been emptied. A payload goes from the firmware to the AD3 when, sent with
  uart.send, it arrives unchanged at the AD3 without parity errors; it goes from the AD3 to the firmware when, written
  by the AD3, uart.recv returns it. The payloads, the large payload sizes, the stream rounds and size and the bit rate
  tolerance are in tests.uart of the board file.

  @ad3
  Scenario: Payloads cross both ways and the TX line runs at the baud rate
    Given the instance is open against the AD3 at the baud rate with the parity, stop bits and variant
    Then each of the payloads goes from the firmware to the AD3 and then from the AD3 to the firmware
    And 16 bytes 55 sent by the firmware decode from its TX line without parity or framing errors, at the baud rate within the bit rate tolerance, unless the logic analyzer cannot record them at 16 samples per bit

  @ad3
  Scenario: The largest payloads cross both ways
    Given the instance is open against the AD3 at the baud rate with the variant
    Then a payload counting up from 00, as long as one uart.send command line can carry and at most the large payload size, goes from the firmware to the AD3
    And a payload of the large payload size to the firmware, counting in steps of 7, goes from the AD3 to the firmware

  @ad3
  Scenario: Both directions stream at once without loss or reordering
    Both directions at once, round after round: nothing may be lost or reordered.
    Given the instance is open against the AD3 at the baud rate with the variant
    Then in each of the stream rounds, while the firmware sends a block of the stream size the AD3 sends another one, uart.send succeeds and each block arrives unchanged at the other end

  @ad3
  Scenario: CTS holds the transmitter and RTS is asserted while the firmware can receive
    CTS deasserted (high) holds the firmware's transmitter; RTS is asserted (low) while it can receive.
    Given the RTS and CTS pins of the flow instance that the flow control uses are wired
    And the AD3 drives CTS high if the flow control uses it
    And the flow instance is open against the AD3 at 115200 baud with the variant and the flow control on the pins it uses
    Then RTS reads low and "0123456789abcdef" goes from the AD3 to the firmware, if the flow control uses RTS
    And "0123456789abcdef" sent by the firmware does not reach the AD3 within 0.2 s, reaches it within 1 s once the AD3 drives CTS low, and uart.send succeeds, if the flow control uses CTS
    And "0123456789abcdef" goes from the firmware to the AD3, if the flow control does not use CTS

  @ad3
  Scenario: The instance reopens with other settings
    Then with each of these settings in turn, the instance is opened against the AD3, "5aa5" goes from the firmware to the AD3, the instance is closed and 10 ms pass
      | baud   | parity | stop bits |
      | 9600   | even   | 2         |
      | 460800 | none   | 1         |
      | 921600 | odd    | 1         |

  Scenario: Invalid options are refused with their reason
    Then opening the instance on its TX and RX pins with the options fails with the reason

  Scenario: An instance without default pins needs TX and RX
    Only UART1 of the TM4C123 has default pins; every other instance needs tx and rx.
    Given the first of UART 1 to 7 that is neither the instance nor the terminal UART
    Then opening it without pins fails with "usage"
    And opening it on the TX pin of the instance only fails with "usage"
