# EK-TM4C1294XL + BOOST-DRV8711 demo

Drives a bipolar stepper through the DRV8711 BoosterPack with EMIL's `drivers::StepDirStepperMotorDrv8711Decorator`: SPI configuration, then step pulses on STEP and a level on DIR.

```bash
cmake --preset tm4c1294ncpdt
cmake --build --preset tm4c1294ncpdt-RelWithDebInfo --target demo_ti.tm4c1294xl_boost_drv8711
```

> The BoosterPack runs from a motor supply of 8 to 52 V at up to 4.5 A. Check the wiring table against your own board before connecting the motor supply, and start with a motor you can afford to lose.

## Behaviour

1. At boot the driver is configured over SPI (1/4 microstep, ISGAIN 40, TORQUE 0x80) and the registers are read back; the trace says whether they match.
2. SW1 (PJ0) starts and stops stepping, SW2 (PJ1) reverses direction.
3. The potentiometer on the BoosterPack sets the step period from 50 ms (20 steps/s) to 2 ms (500 steps/s). Step pulses come from a 1 ms-resolution `infra::TimerRepeating`.
4. nFAULT (falling edge) stops stepping, lights LED D2 and traces the status register. Reset the board to recover. nSTALL is traced; stall detection itself is not configured.
5. LED D1 is the heartbeat.

With TORQUE 0x80 and ISGAIN 40 the full-scale current is about 0.7 A per the DRV8711 datasheet formula (2.75 V x TORQUE / (256 x ISGAIN x 0.05 ohm)); adjust `MotorConfiguration()` in `StepperDemo.cpp` for the motor.

## Wiring

BoosterPack pins are from the BOOST-DRV8711 schematic and reference firmware (SLVC575B), LaunchPad pins from the EK-TM4C1294XL schematic (SPMR241, BoosterPack 1 interface, connectors X8 and X9). The pin constants are in `BoardPins.hpp`.

| DRV8711 signal | BoosterPack pin | LaunchPad pin | Connector | Use                                                       |
|----------------|-----------------|---------------|-----------|-----------------------------------------------------------|
| POT            | J1.2            | PE4 (AIN9)    | X8-3      | speed, `hal::tiva::SynchronousAdc`                        |
| nSLEEP         | J1.6            | PE5           | X8-11     | output, high = awake                                      |
| SCLK           | J1.7            | PD3           | X8-13     | SSI2 clock                                                |
| RESET          | J1.8            | PC7           | X8-15     | output, high = reset                                      |
| STEP / AIN1    | J1.9            | PB2           | X8-17     | step pulse                                                |
| DIR / AIN2     | J1.10           | PB3           | X8-19     | direction                                                 |
| SCS            | J2.11           | PP2           | X9-20     | chip select, active high (`services::GpioPinInverted`)    |
| BIN2           | J2.12           | PN3           | X9-18     | held low (unused with the indexer)                        |
| BIN1           | J2.13           | PN2           | X9-16     | held low (unused with the indexer)                        |
| SDATO (MISO)   | J2.14           | PD0           | X9-14     | SSI2 receive                                              |
| SDATI (MOSI)   | J2.15           | PD1           | X9-12     | SSI2 transmit                                             |
| nFAULT         | J2.18           | PH2           | X9-6      | input with pull-up, interrupt on falling edge             |
| nSTALL         | J2.19           | PM3           | X9-4      | input with pull-up, interrupt on falling edge             |

The BoosterPack is a 20-pin board (J1 and J2 only). Pin numbers use the BoosterPack standard, so pins 11 to 19 of J2 are pins 10 to 2 of J2 on the BOOST-DRV8711 schematic. SPI is mode 0 at 1 MHz: the reference firmware sets UCCKPH and clears UCCKPL on its MSP430 USCI, which is clock idle low with data captured on the first edge, and idles SCS low.

## Notes

1. The DRV8711 BEMF output is not routed to the BoosterPack headers, so `drivers::StepperMotorControllerDrv8711` gets an `UnroutedBemf` pin that asserts if it is ever measured; the step/dir decorator only reads it through `OnBemf`, which the demo does not call.
2. The decorator pulses STEP from software, so the step rate is limited by the 1 ms timer tick. Direct PWM mode (AIN1/AIN2/BIN1/BIN2 driven by `hal::tiva::Pwm`) is not implemented here.
3. The DRV8711 logic is supplied from the motor supply (VM), so with VM off it does not answer on SPI and the boot trace reports a register readback mismatch.
4. Verified only by building; nothing has been run on hardware. The 1 MHz SPI clock is a conservative choice, not a datasheet maximum.
