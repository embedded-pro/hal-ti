Feature: Ethernet
  `hal::tiva::Ethernet`, TM4C129 only. The link test needs a cable: `--with ethernet`.

  The speeds and the link timeout are in tests.ethernet of the board file.

  @family:tm4c123
  Scenario: Ethernet is unsupported on the TM4C123
    Then opening Ethernet, reading its status and closing it each fail with "unsupported"

  @family:tm4c129
  Scenario: Ethernet opens and reports its status
    When Ethernet is opened without options
    Then its status reports the link up or down and rx and tx counts of 0 or more

  @family:tm4c129 @requires_option:ethernet
  Scenario: The link comes up at the speed
    When Ethernet is opened on the internal PHY at the speed
    Then the status reports the link up within the link timeout, read again every 0.25 s while it is not
    And the status reports the speed, 10 or 100 Mbit/s when the speed is auto
    And the status reports half or full duplex
