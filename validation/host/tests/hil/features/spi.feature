Feature: SPI
  SPI master (`hal::tiva::SpiMaster` / `SynchronousSpiMaster`), decoded from a logic-analyzer capture.

  Wiring set `bundle1`: CLK, FSS, MOSI and MISO on DIOs (the SSI pins are also PWM outputs). The AD3 SDK has no
  verified SPI-slave mode, so the firmware master is observed with the logic analyzer: MISO is driven to a static
  level by the AD3, or, with `--with loopback` and a MOSI-MISO jumper, only monitored (the firmware must then read
  back what it sent).

  The instance opens on its CLK, MOSI and MISO pins, and on its FSS pin where a step says so. The MISO data of a
  payload is the payload itself with the loopback jumper, else as many bytes ff or 00 as the payload has, for MISO
  level 1 or 0. The payloads, the session baud rate, the largest transfer size and the baud tolerance are in tests.spi
  of the board file.

  @ad3
  Scenario: The master exchanges payloads in the mode at the baud rate
    Given the CLK, FSS, MOSI and MISO pins of the instance are wired
    And the AD3 drives MISO to the MISO level, or with the loopback jumper MISO follows MOSI, which needs MISO level 0
    And the instance is open in the mode at the baud rate, synchronous or not, on its FSS pin if the chip select is fss
    Then each of the payloads sent with spi.xfer returns its MISO data; where the logic analyzer can record it at 4 or more samples per clock period, triggered on FSS falling or else on any clock edge, MOSI decodes to the payload and MISO to its MISO data, the clock idles at the CPOL of the mode and runs at the baud rate within the baud tolerance, and FSS ends high

  @ad3
  Scenario: A continued session sends the bytes of both transfers in order
    `continue=1` keeps the session open for the next `spi.xfer`; the bytes of both arrive in order.
    Given the CLK, FSS, MOSI and MISO pins of the instance are wired
    And the AD3 drives MISO low
    And the instance is open in the mode at the session baud rate, synchronous or not, on its FSS pin unless synchronous
    And the logic analyzer records 0.2 s, triggered on FSS falling or, when synchronous, on any clock edge, if it can sample at 4 times the session baud rate
    When "1234" is sent with continue=1 and then "56" with continue=0
    Then the recording decodes on MOSI to "123456"

  @ad3
  Scenario: Receive-only, largest and uneven transfers return the MISO data
    Given the CLK, FSS, MOSI and MISO pins of the instance are wired
    And the AD3 drives MISO high, or with the loopback jumper MISO follows MOSI
    And the instance is open at 1000000 baud, synchronous or not, on its FSS pin unless synchronous
    Then receiving 4 bytes without sending returns 00 bytes with the loopback jumper, else ff bytes
    And a payload of the largest transfer size returns itself with the loopback jumper, else ff bytes
    And sending its first 8 bytes while receiving 0 returns nothing
    And sending its first 2 bytes while receiving 6 returns them followed by 00 bytes with the loopback jumper, else ff bytes

  Scenario: Invalid settings are refused and half the system clock opens
    Given the system clock the firmware reports
    Then opening the instance on its CLK, MOSI and MISO pins at the system clock over 2 plus 1 baud fails with "range"
    And opening the instance on its CLK, MOSI and MISO pins at the system clock over 65024 baud fails with "range"
    And opening the instance on its CLK, MOSI and MISO pins in mode 4 fails with "range"
    And opening the instance on its CLK and MOSI pins fails with "usage"
    And opening the instance synchronously on its CLK, MOSI, MISO and FSS pins fails with "unsupported"
    And the instance opens on its CLK, MOSI and MISO pins at the system clock over 2 baud
