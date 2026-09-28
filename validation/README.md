# Hardware-in-the-loop validation

This directory validates the hal-ti drivers on real hardware: an EK-TM4C123GXL or EK-TM4C1294XL LaunchPad (already modified for e-foc) runs a validation firmware, and a host PC drives that firmware and a Digilent Analog Discovery 3 (AD3) to stimulate and measure every peripheral.

- `firmware/` - C++ firmware on hal-ti and EMIL. It exposes every hal-ti peripheral through a line-based terminal; the command set is specified in [PROTOCOL.md](PROTOCOL.md).
- `host/` - Python package `hal_ti_validation` and a pytest suite that talks to the firmware terminal over a serial port and to the AD3 through the WaveForms SDK.
- The generic bench code (AD3 wrapper over the WaveForms SDK, signal analysis, `OK`/`ERR`/`EVT` terminal client, console, pytest plugin and fakes) lives in the separate [ad3-waveforms-bench](https://github.com/embedded-pro/ad3-waveforms-bench) repository; `hal_ti_validation` only adds what is specific to hal-ti.

## Why C++ and Python

- The firmware has to be C++: it is built from hal-ti and EMIL exactly like an application (e-foc) would use them, so what is validated is the real driver code with the real interrupt table, clocks and pin muxing.
- The host is Python because Digilent ships the WaveForms SDK with official Python bindings and samples, `pyserial` covers the terminal, and pytest brings parametrisation, fixtures, skips and JUnit/HTML reports for free.
- Python's latency does not matter: every timing-critical stimulus or measurement is done by the AD3 hardware (pattern generator, logic analyzer, wavegen, protocol engines) or by the firmware itself; the host only configures, triggers and evaluates.

## Hardware setup

- Connect AD3 GND to LaunchPad GND. All AD3 DIOs are 3.3 V LVCMOS, compatible with the TM4C pins.
- The LaunchPad is powered from its USB debug port. The AD3 V+/V- supplies stay off unless `ad3.vplus`/`ad3.vminus` are set in the board file.
- Wavegen outputs are refused outside `ad3.analog_limits` (0..3.3 V by default) so a wrong parameter cannot overdrive an analog input.
- Run the tests with the LaunchPad detached from the e-foc power stage: the tests drive the phase, supply, encoder and CAN pins directly.
- The AD3 has 16 DIOs, so the wiring is split into named wiring sets (tables below). Tests whose connections are missing from the selected sets are skipped with the reason.
- EK-TM4C123GXL: the terminal is UART0 on the ICDI virtual COM port (`/dev/ttyACM0`, `COMx`). Stock boards connect PB6-PD0 and PB7-PD1 through R9/R10; the e-foc modification removes them.
- EK-TM4C1294XL: the terminal is UART2 on PD5 (TX) / PD4 (RX), not UART0, because e-foc uses PA0/PA1 for CAN0. Use a 3.3 V USB-UART adapter on PD4/PD5 (`/dev/ttyUSB0`), or move JP4/JP5 to the UART2 position so the ICDI virtual COM port is routed to PD4/PD5 (check the EK-TM4C1294XL user's guide for the jumper positions). The adapter must handle 921600 baud.
- CAN: neither LaunchPad has a transceiver, so the `can` wiring set builds a wired-AND bus at logic level.
  - The bus node joins the controller RX pin, the AD3 RX DIO and a 1 kOhm pull-up to 3.3 V.
  - The controller TX pin and the AD3 TX DIO each pull the bus low through a Schottky diode (anode on the bus).
  - Both nodes then see every bit, including their own, as with a transceiver. The loopback tests need no wiring at all.
  - The AD3 CAN receiver is assumed not to acknowledge frames (`can.ad3_acknowledges: false`), so a firmware-to-AD3 transfer may end with `ERR failed` or `ERR timeout` although the AD3 received it.
- Ethernet (EK-TM4C1294XL): plug a cable into a switch or PC and pass `--with ethernet`.

## Build and flash the firmware

The target is `hal_ti.validation_firmware`, built with the regular presets (`HAL_TI_BUILD_EXAMPLES` is on in them):

```bash
cmake --preset tm4c123gh6pm
cmake --build --preset tm4c123gh6pm-Debug --target hal_ti.validation_firmware
cmake --preset tm4c1294ncpdt
cmake --build --preset tm4c1294ncpdt-Debug --target hal_ti.validation_firmware
```

The artifacts are `build/<preset>/validation/firmware/hal_ti.validation_firmware.{elf,bin,hex}`. Flash through the on-board ICDI with [lm4flash](https://github.com/utzig/lm4tools) or TI UniFlash:

```bash
lm4flash build/tm4c123gh6pm/validation/firmware/hal_ti.validation_firmware.bin
```

After reset the firmware prints `EVT boot board=... family=... sysclk=... reset=...`.

## Install the host package

1. Install the Digilent WaveForms application (it contains the WaveForms runtime `dwf` and the SDK) from the Digilent website; on Linux also install the Adept 2 runtime it depends on. `ad3-waveforms-bench` loads `libdwf.so` / `dwf.dll` / `dwf.framework` from the default location; set `DWF_LIBRARY` to override it.
2. Create a virtual environment and install the package (Python 3.10 or newer):

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install -e "validation/host[ad3]"
```

This also installs `ad3-waveforms-bench` from `git+https://github.com/embedded-pro/ad3-waveforms-bench@main` (see `host/pyproject.toml`; the reference can be pinned to a tag once the repository has releases).
To work on both at the same time, install a local checkout first and then this package without dependencies:

```bash
pip install -e ../ad3-waveforms-bench
pip install -e validation/host --no-deps
```

The `ad3` extra adds no Python dependency: the SDK is reached through `ctypes` (the reasons are in the [ad3-waveforms-bench README](https://github.com/embedded-pro/ad3-waveforms-bench#why-ctypes-instead-of-pydwf)), so only the WaveForms runtime must be installed. Without it, everything except the AD3 still works and AD3 tests are skipped.

## Run the tests

Unit tests run without hardware:

```bash
pytest validation/host/tests/unit
```

Hardware tests need `--port`; without it they are skipped. Select the board file and the wiring sets that are actually connected:

```bash
pytest validation/host/tests/hil --board ek_tm4c123gxl --port /dev/ttyACM0 --wiring-set pwm
pytest validation/host/tests/hil/test_uart.py --board ek_tm4c1294xl --port /dev/ttyUSB0 --wiring-set uart --with flow
pytest validation/host/tests/hil --board ek_tm4c123gxl --port /dev/ttyACM0 --no-ad3
```

Options from `tests/conftest.py`:

- `--board` - board file name in `host/boards/` or a path to a YAML file (default `ek_tm4c123gxl`, or `HAL_TI_BOARD`).
- `--port`, `--baud` - firmware terminal serial port (or `HAL_TI_PORT`) and baud rate (default from the board file, 921600).
- `--wiring-set a,b` - active wiring sets; sets that use different AD3 channels can be combined (`--wiring-set pwm,adc`).
- `--with <tag>` - enable optional wiring (`flow`, `loopback`, `ethernet`), repeatable.
- `--set path=value` - override a test parameter, value parsed as YAML: `--set pwm.frequencies_hz=[20000] --set uart.bauds=[921600]`.
- `--quick` - use only the first value of every parameter list.

Options from the `ad3_waveforms_bench` pytest plugin (loaded automatically once the package is installed); `tests/conftest.py` feeds it the `ad3` section of the board file:

- `--ad3-serial` - pick an AD3 by serial number (or `AD3_SERIAL`); `--no-ad3` skips every test that needs it.
- `--fake` - run the HIL plumbing against the in-memory fakes (`FakeDwfApi` for the AD3, `hal_ti_validation.fake_firmware` for the terminal); only useful when changing the test code (most peripheral tests fail because the fakes do not emulate peripherals).

Tests that need no AD3 (system, CAN loopback, EEPROM, watchdog, Ethernet) run with any wiring set.
Tests that reset the board on purpose (watchdog, EEPROM persistence) are marked `resets_board`; any other unexpected `EVT boot` fails the test that caused it.
Every instance a test opened is closed afterwards and the AD3 outputs are released, so tests are independent (the firmware keeps at most one PWM module, UART, SSI, comparator, QEI and CAN open at a time).
Use `-k`, `-m "not slow"` and `--junitxml report.xml` as usual.

## Wiring sets

The tables are generated from the board files; `role` and `requires` entries of the YAML are shown as notes. Scope inputs are single ended: connect the `-` input of each used scope channel to GND.

### EK-TM4C123GXL wiring

| Wiring set   | AD3      | Pin               | Note                                                                                                                           |
|--------------|----------|-------------------|--------------------------------------------------------------------------------------------------------------------------------|
| `gpio`       | DIO0     | PB2               |                                                                                                                                |
| `gpio`       | DIO1     | PB3               |                                                                                                                                |
| `gpio`       | DIO2     | PA7               |                                                                                                                                |
| `gpio`       | DIO3     | PC6               |                                                                                                                                |
| `gpio`       | DIO4     | PF0 (canrx)       | locked pin, also SW2 (open unless pressed)                                                                                     |
| `gpio`       | DIO5     | PD7 (encb)        | locked pin (NMI)                                                                                                               |
| `gpio`       | DIO6     | PF1 (ledop)       |                                                                                                                                |
| `gpio`       | DIO7     | PA2 (perf)        |                                                                                                                                |
| `pwm`        | DIO0     | PB6 (pwm1a)       |                                                                                                                                |
| `pwm`        | DIO1     | PB7 (pwm1b)       |                                                                                                                                |
| `pwm`        | DIO2     | PB4 (pwm2a)       |                                                                                                                                |
| `pwm`        | DIO3     | PB5 (pwm2b)       |                                                                                                                                |
| `pwm`        | DIO4     | PE4 (pwm3a)       |                                                                                                                                |
| `pwm`        | DIO5     | PE5 (pwm3b)       |                                                                                                                                |
| `pwm`        | -        | -                 | stock boards short PB6-PD0 and PB7-PD1 through R9/R10; remove them (the e-foc modification does) or leave PD0/PD1 unconfigured |
| `adc`        | W1       | PE3 (phasea)      |                                                                                                                                |
| `adc`        | W2       | PE0 (vbus/itotal) |                                                                                                                                |
| `adc`        | Scope 1+ | PE3 (phasea)      |                                                                                                                                |
| `adc`        | Scope 2+ | PE0 (vbus/itotal) |                                                                                                                                |
| `comparator` | DIO0     | PF0 (canrx)       | C0o output                                                                                                                     |
| `comparator` | W1       | PC6               | C0+ input                                                                                                                      |
| `comparator` | W2       | PC7               | C0- input                                                                                                                      |
| `comparator` | Scope 1+ | PC6               |                                                                                                                                |
| `comparator` | Scope 2+ | PC7               |                                                                                                                                |
| `qei`        | DIO0     | PD6 (enca)        |                                                                                                                                |
| `qei`        | DIO1     | PD7 (encb)        |                                                                                                                                |
| `qei`        | DIO2     | PD3 (encz)        |                                                                                                                                |
| `uart`       | DIO0     | PB1               | firmware TX, AD3 UART RX                                                                                                       |
| `uart`       | DIO1     | PB0               | firmware RX, AD3 UART TX                                                                                                       |
| `uart`       | DIO2     | PC4               | firmware RTS; only with `--with flow`                                                                                          |
| `uart`       | DIO3     | PC5               | firmware CTS, driven by the AD3; only with `--with flow`                                                                       |
| `spi`        | DIO0     | PA2 (perf)        | spi clk                                                                                                                        |
| `spi`        | DIO1     | PA3               | spi cs                                                                                                                         |
| `spi`        | DIO2     | PA5 (hallb)       | spi mosi                                                                                                                       |
| `spi`        | DIO3     | PA4 (halla)       | AD3 drives the MISO level unless the loopback jumper is fitted                                                                 |
| `spi`        | -        | -                 | `--with loopback`: jumper PA5 (MOSI) to PA4 (MISO); DIO3 then only listens                                                     |
| `can`        | DIO0     | PF0 (canrx)       | AD3 CAN TX through a diode onto the bus                                                                                        |
| `can`        | DIO1     | PF0 (canrx)       | AD3 CAN RX on the bus                                                                                                          |
| `can`        | -        | -                 | bus node = PF0 (canrx) + DIO1 + 1 kOhm pull-up to 3.3 V                                                                        |
| `can`        | -        | -                 | Schottky diode, anode on the bus, cathode on PF3 (cantx)                                                                       |
| `can`        | -        | -                 | Schottky diode, anode on the bus, cathode on DIO0                                                                              |

### EK-TM4C1294XL wiring

| Wiring set   | AD3      | Pin          | Note                                                                       |
|--------------|----------|--------------|----------------------------------------------------------------------------|
| `gpio`       | DIO0     | PB2          |                                                                            |
| `gpio`       | DIO1     | PB3          |                                                                            |
| `gpio`       | DIO2     | PE4 (halla)  |                                                                            |
| `gpio`       | DIO3     | PE5 (hallb)  |                                                                            |
| `gpio`       | DIO4     | PD7          | locked pin (NMI)                                                           |
| `gpio`       | DIO5     | PN3 (ledop)  |                                                                            |
| `gpio`       | DIO6     | PN4 (perf)   |                                                                            |
| `pwm`        | DIO0     | PF2 (pwm1a)  |                                                                            |
| `pwm`        | DIO1     | PF3 (pwm1b)  |                                                                            |
| `pwm`        | DIO2     | PG0 (pwm2a)  |                                                                            |
| `pwm`        | DIO3     | PG1 (pwm2b)  |                                                                            |
| `pwm`        | DIO4     | PK4 (pwm3a)  |                                                                            |
| `pwm`        | DIO5     | PK5 (pwm3b)  |                                                                            |
| `pwm`        | W1       | PB5 (vbus)   |                                                                            |
| `pwm`        | W2       | PB4 (itotal) |                                                                            |
| `pwm`        | Scope 1+ | PB5 (vbus)   |                                                                            |
| `adc`        | W1       | PE3 (phasea) |                                                                            |
| `adc`        | W2       | PB5 (vbus)   |                                                                            |
| `adc`        | Scope 1+ | PE3 (phasea) |                                                                            |
| `adc`        | Scope 2+ | PB5 (vbus)   |                                                                            |
| `comparator` | DIO0     | PD1          | C1o output                                                                 |
| `comparator` | W1       | PC5          | C1+ input                                                                  |
| `comparator` | W2       | PC4          | C1- input                                                                  |
| `comparator` | Scope 1+ | PC5          |                                                                            |
| `comparator` | Scope 2+ | PC4          |                                                                            |
| `qei`        | DIO0     | PL1 (enca)   |                                                                            |
| `qei`        | DIO1     | PL2 (encb)   |                                                                            |
| `qei`        | DIO2     | PL3 (encz)   |                                                                            |
| `uart`       | DIO0     | PA5          | firmware TX, AD3 UART RX                                                   |
| `uart`       | DIO1     | PA4          | firmware RX, AD3 UART TX                                                   |
| `uart`       | DIO2     | PP4          | firmware RTS; only with `--with flow`                                      |
| `uart`       | DIO3     | PP5          | firmware CTS, driven by the AD3; only with `--with flow`                   |
| `spi`        | DIO0     | PD3          | spi clk                                                                    |
| `spi`        | DIO1     | PD2          | spi cs                                                                     |
| `spi`        | DIO2     | PD0          | spi mosi                                                                   |
| `spi`        | DIO3     | PD1          | AD3 drives the MISO level unless the loopback jumper is fitted             |
| `spi`        | -        | -            | `--with loopback`: jumper PD0 (MOSI) to PD1 (MISO); DIO3 then only listens |
| `can`        | DIO0     | PA0 (canrx)  | AD3 CAN TX through a diode onto the bus                                    |
| `can`        | DIO1     | PA0 (canrx)  | AD3 CAN RX on the bus                                                      |
| `can`        | -        | -            | JP4/JP5 in the UART2 position so PA0/PA1 are not driven by the ICDI        |
| `can`        | -        | -            | bus node = PA0 (canrx) + DIO1 + 1 kOhm pull-up to 3.3 V                    |
| `can`        | -        | -            | Schottky diode, anode on the bus, cathode on PA1 (cantx)                   |
| `can`        | -        | -            | Schottky diode, anode on the bus, cathode on DIO0                          |
| `ethernet`   | -        | -            | `--with ethernet`: the Ethernet cable is plugged in                        |

## What is tested

- `test_system.py` - `ping`, `info` and `board.pins` against the board file, reserved terminal pins/UART, error reasons (`usage`, `busy`, `notopen`, `range`), `delay`, `reset` and the `EVT boot` cause.
- `test_gpio.py` - output levels with every drive strength, inputs following the AD3 with every pull, pull-only idle levels, open drain, locked pins (PF0/PD7), LED/perf outputs, interrupt counts for every edge and handler type against exact AD3 pulse trains, `gpio.pulse` timing.
- `test_pwm.py` - frequency and duty sweep in edge and center mode for `Pwm` and `SynchronousPwm`, divisors and the reported PWM clock, dead time between A and B with a shoot-through check, output inversion.
  Also the e-foc default (20 kHz, center aligned, 1000 ns dead time, three generators), generator synchronisation with local/global update, duty 0/100, frequency change and stop, generator interrupt counts.
  On TM4C129 the fault path: wavegen above the digital comparator threshold, `EVT pwm fault`, outputs stopped.
- `test_uart.py` - every baud rate x parity x stop bits x driver variant (interrupt, DMA, synchronous), both directions against the AD3 UART, large payloads, RTS/CTS flow control.
- `test_spi.py` - SPI modes 0-3 x baud rates x asynchronous/synchronous driver, decoded from the logic analyzer (MOSI data, MISO data, clock polarity, bit rate), receive-only and continued transfers.
- `test_adc.py` - wavegen DC levels against raw 12-bit codes (checked against the scope when it is wired), sample-and-hold and oversampling settings, sampling delay, multi-pin sequences, e-foc's PWM-triggered phase-current sequencer (and its timeout without trigger) and supply sequencer.
- `test_comparator.py` - output and output pin for inputs above/below each other with and without inversion, the internal reference ladder located by a wavegen search, interrupt counts against a square wave.
- `test_qei.py` - position counts for frequencies x cycle counts x directions x capture modes, phase inversion, offset and rollover, reset on the index pulse, speed against the velocity period, clock/direction mode.
- `test_can.py` - internal loopback for every bit rate and frame type, the acceptance filter, and frames in both directions with the AD3 on the logic-level bus.
- `test_eeprom.py` - erase, patterns at aligned/unaligned addresses without touching neighbours, overwrite, largest write, range errors at the end, persistence across reset.
- `test_watchdog.py` - automatic feeding keeps the board alive and warns every period, a missing feed resets with `reset=wdt<n>`, manual feeding, warning-only mode.
- `test_ethernet.py` - `ERR unsupported` on TM4C123; open/status on TM4C129 and, with a cable, link up at each speed.

## Customising

Each board file (`host/boards/<board>.yaml`) holds:

- `terminal`, `pins` (the PROTOCOL.md board profile, compared with `board.pins`) and `ad3` (supplies, analog limits).
- `wiring_sets` - per set, `dio`, `wavegen` and `scope` maps from AD3 channel to pin or alias. An entry is either a pin or a mapping with `pin`, `role` (looked up by tests, e.g. `can_ad3_tx`), `note` and `requires` (only used with `--with <tag>`); `jumpers` and `options` document extra wiring.
- `tests` - the parameters of every test module: frequencies, duties, baud rates, levels, tolerances, pins and instances. Most tests are parametrised directly from these lists with `@pytest.mark.board_params("argname", "section.key")`, so extending a sweep or moving a peripheral to other pins is a YAML change.

To validate another board, copy a board file, adapt the pins, wiring sets and parameters, and pass `--board path/to/board.yaml`.

## Interactive console

```bash
hal-ti-console --port /dev/ttyACM0
hal-ti-console --port /dev/ttyACM0 -c info -c board.pins
```

`hal-ti-console` is `ad3-bench-console` from ad3-waveforms-bench with the firmware's 921600 baud and its own history file (`ad3-bench-console --port /dev/ttyACM0` works as well).
The console forwards commands, prints final lines and events, and keeps a history in `~/.hal_ti_validation_history`. `:wait <s>` listens for events, `:raw` also shows non-protocol output, `:quit` leaves.

## Package layout

In `hal_ti_validation` (hal-ti specific):

- `firmware.py` - typed API with one group per PROTOCOL.md section (`fw.gpio`, `fw.pwm`, `fw.uart`, `fw.spi`, `fw.adc`, `fw.comp`, `fw.qei`, `fw.can`, `fw.eeprom`, `fw.wdt`, `fw.eth`, `fw.system`); keyword arguments map 1:1 to protocol options (`continue_` for `continue`).
- `protocol.py` - the hal-ti part of the protocol: error reasons, `P<port><index>` pins and board aliases (`normalize_pin`, `parse_pin_map`).
- `config.py` - board file loading, wiring-set merging and parameter overrides; `expect.py` - expected TM4C values (PWM quantisation, comparator reference, counter wrap).
- `fake_firmware.py` - `FakeFirmware`, an in-memory stand-in for the validation firmware used by the unit tests and `--fake`.
- `console.py` - the `hal-ti-console` entry point.

In [ad3-waveforms-bench](https://github.com/embedded-pro/ad3-waveforms-bench) (generic, imported as `ad3_waveforms_bench`):

- `protocol.py` - pure parsing and formatting: `Response`, `Event`, number/hex/list helpers, command formatting, configurable `Dialect`.
- `terminal.py` - `FirmwareTerminal`: sends `cmd\r`, strips echo, prompts and escape sequences, queues `EVT` lines, raises `FirmwareError(reason, command)` on `ERR`, `wait_event`, `wait_boot`, `sync`.
- `instruments/ad3.py`, `instruments/dwf.py` - `AnalogDiscovery3` over the WaveForms SDK: supplies, static DIO, pattern generator (pulses, clocks, custom sequences, quadrature with index), logic analyzer with DIO-edge trigger, wavegen, scope, UART/SPI/CAN protocol engines.
- `analysis.py` - pure signal analysis (frequency, duty, edges, dead time, phase, quadrature/SPI/UART decode, statistics).
- `instruments/fake.py`, `fake_terminal.py` - `FakeDwfApi` and the generic fake terminal device and serial port; `pytest_plugin.py` - AD3 options, marker and fixture.
