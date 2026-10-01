# Hardware-in-the-loop validation

This directory validates the hal-ti drivers on real hardware: an EK-TM4C123GXL or EK-TM4C1294XL LaunchPad runs a validation firmware, and a host PC drives that firmware and a Digilent Analog Discovery 3 (AD3) to stimulate and measure every peripheral.
Every driver is exercised generically, with its synchronous and asynchronous variant where it has both and with every option the firmware exposes.
Hardware baseline: the pin assignment follows LaunchPads modified for the e-foc project; nothing else of e-foc is assumed.

- `firmware/` - C++ firmware on hal-ti and EMIL. It exposes every hal-ti peripheral through a line-based terminal; the command set is specified in [PROTOCOL.md](PROTOCOL.md).
- `host/` - Python package `hal_ti_validation` and a pytest suite that talks to the firmware terminal over a serial port and to the AD3 through the WaveForms SDK.
- The generic bench code (AD3 wrapper over the WaveForms SDK, signal analysis, `OK`/`ERR`/`EVT` terminal client, console, pytest plugin and fakes) lives in the separate [ad3-waveforms-bench](https://github.com/embedded-pro/ad3-waveforms-bench) repository; `hal_ti_validation` only adds what is specific to hal-ti.
- hal-ti has no I2C driver, so there is no I2C validation.

## Why C++ and Python

- The firmware has to be C++: it is built from hal-ti and EMIL exactly like an application would use them, so what is validated is the real driver code with the real interrupt table, clocks and pin muxing.
- The host is Python because Digilent ships the WaveForms SDK with official Python bindings and samples, `pyserial` covers the terminal, and pytest brings parametrisation, fixtures, skips and JUnit/HTML reports for free.
- Python's latency does not matter: every timing-critical stimulus or measurement is done by the AD3 hardware (pattern generator, logic analyzer, wavegen, protocol engines) or by the firmware itself; the host only configures, triggers and evaluates.

## What the AD3 does

| Protocol or signal       | AD3 capability                                                                   | How the tests use it                                                                        |
|--------------------------|----------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
| UART                     | Full TX/RX through the SDK protocol UART (`FDwfDigitalUart*`)                    | Peer in both directions; the logic analyzer decodes the firmware TX line and its bit rate   |
| CAN                      | TX/RX at logic level (`FDwfDigitalCan*`); ACK behaviour unverified               | Not used: the bus peer is a CANable behind a transceiver (see [CAN bus](#can-bus))          |
| SPI                      | SPI master in the SDK; SPI slave reported in recent WaveForms, unverified in SDK | Firmware is the master: logic-analyzer decode, static MISO level or MOSI-MISO jumper        |
| I2C                      | Master and spy in the SDK; slave only through newer scripting                    | Not used (hal-ti has no I2C driver)                                                         |
| PWM, QEI, GPIO, watchdog | Pattern generator, logic analyzer with DIO-edge trigger, static DIO              | Encoder signals, pulse trains, fault inputs and CTS; frequency, duty, dead band and periods |
| ADC, analog comparator   | Wavegen W1/W2 (DC, square) and scope channels 1/2                                | Analog levels and edges on the inputs; the scope measures the level actually applied        |

- SPI: the AD3 SDK exposes an SPI master; recent WaveForms versions are reported to offer an SPI-slave mode, but its exposure in the AD3 SDK is unverified, so the tests do not rely on it.
- Pattern generator uses: exact pulse counts, quadrature with index, custom sequences. Logic analyzer uses: PWM waveforms, UART and SPI decode, GPIO pulse and watchdog warning periods.

## Hardware setup

- Connect AD3 GND to LaunchPad GND. All AD3 DIOs are 3.3 V LVCMOS, compatible with the TM4C pins.
- The LaunchPad is powered from its USB debug port. The AD3 V+/V- supplies stay off unless `ad3.vplus`/`ad3.vminus` are set in the board file.
- Wavegen outputs are refused outside `ad3.analog_limits` (0..3.3 V by default) so a wrong parameter cannot overdrive an analog input.
- Run the tests with nothing else connected to the pins of the selected wiring sets: the tests drive them directly.
- The AD3 has 16 DIOs, so the wiring is split into named wiring sets (tables below). Tests whose connections are missing from the selected sets are skipped with the reason.
- EK-TM4C123GXL: the terminal is UART0 on the ICDI virtual COM port (`/dev/ttyACM0`, `COMx`). Stock boards connect PB6-PD0 and PB7-PD1 through R9/R10; remove them (part of the baseline modification) or leave PD0/PD1 unconfigured.
- EK-TM4C1294XL: the terminal is UART2 on PD5 (TX) / PD4 (RX), because PA0/PA1 are the CAN0 pins. Use a 3.3 V USB-UART adapter on PD4/PD5 (`/dev/ttyUSB0`), or move JP4/JP5 to the UART2 position so the ICDI virtual COM port is routed to PD4/PD5 (check the EK-TM4C1294XL user's guide for the jumper positions). The adapter must handle 921600 baud.
- CAN: CAN0 goes through a 3.3 V transceiver to a CANable (wiring set `can`, see [CAN bus](#can-bus)); the AD3 is not on the bus.
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

This also installs `ad3-waveforms-bench` from `git+https://github.com/embedded-pro/ad3-waveforms-bench@v0.2.0` (see `host/pyproject.toml`), the first release that can use an AD3 on another machine.
This is the setup for one machine that has the AD3, the LaunchPad and Python together. When the AD3 and the LaunchPad are on a Windows PC and the build and the tests run in Docker, follow [Windows host and Docker (bridge mode)](#windows-host-and-docker-bridge-mode) instead.
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

Hardware tests need `--port`; without it they are skipped. Select the board file and the wiring sets that are actually connected, and start with the quick depth:

```bash
pytest validation/host/tests/hil --board ek_tm4c123gxl --port /dev/ttyACM0 --wiring-set pwm --depth quick
pytest validation/host/tests/hil --board ek_tm4c123gxl --port /dev/ttyACM0 --wiring-set pwm --depth full
pytest validation/host/tests/hil/test_uart.py --board ek_tm4c1294xl --port /dev/ttyUSB0 --wiring-set uart --with flow
pytest validation/host/tests/hil --board ek_tm4c123gxl --port /dev/ttyACM0 --no-ad3
```

Run each wiring set with `--depth quick` first; once it passes, run it again with `--depth full`, which takes much longer. Collect the reports with `--junitxml report-<board>-<set>-<depth>.xml`.

Options from `tests/conftest.py`:

- `--board` - board file name in `host/boards/` or a path to a YAML file (default `ek_tm4c123gxl`, or `HAL_TI_BOARD`).
- `--port`, `--baud` - firmware terminal serial port (or `HAL_TI_PORT`) and baud rate (default from the board file, 921600).
- `--wiring-set a,b` - active wiring sets; sets that use different AD3 channels, or the same channels on the same pins, can be combined (`--wiring-set gpio,wdt`).
- `--with <tag>` - enable optional wiring (`pwm4`, `faultpin`, `dcmp`, `qei1`, `flow`, `loopback`, `ethernet`), repeatable.
- `--depth quick|full` - `quick` (default, or `HAL_TI_DEPTH`) runs a pairwise subset of every parameter matrix: every pair of values of any two parameters appears in at least one test. `full` runs the complete cartesian products.
- `--set path=value` - override a test parameter, value parsed as YAML: `--set pwm.waveform.freq=[20000] --set uart.transfer.baud=[921600]`.
- `--can-mode loopback|bus|both`, `--can-peer`, `--can-peer-bitrate` - the CAN link and the adapter on the bus (or `HAL_TI_CAN_MODE`, `HAL_TI_CAN_PEER`, `HAL_TI_CAN_PEER_BITRATE`), see [CAN bus](#can-bus).

Options from the `ad3_waveforms_bench` pytest plugin (loaded automatically once the package is installed); `tests/conftest.py` feeds it the `ad3` section of the board file:

- `--ad3-serial` - pick an AD3 by serial number (or `AD3_SERIAL`); `--no-ad3` skips every test that needs it.
- `--fake` - run the HIL plumbing against the in-memory fakes (`FakeDwfApi` for the AD3, `hal_ti_validation.fake_firmware` for the terminal, a python-can virtual bus for the CANable); only useful when changing the test code.
  - The fake firmware validates arguments and models pins, instances, EEPROM, PWM interrupt counts, ADC triggers, CAN loopback and bus frames with filters, and the watchdog.
  - It models no measured signal and no CAN bit errors, so most tests that read the AD3 fail and the CAN error and bus-off tests skip.

Tests that need no AD3 (system, argument errors, PWM interrupts and ADC triggers, CAN loopback, EEPROM, watchdog, Ethernet) run with any wiring set.
Tests that reset the board on purpose (watchdog, EEPROM persistence) are marked `resets_board`; any other unexpected `EVT boot` fails the test that caused it.
Every instance a test opened is closed afterwards and the AD3 outputs are released, so tests are independent (the firmware keeps at most one PWM module, UART, SSI, comparator, QEI and CAN open at a time).
Where a known driver gap makes an assertion fail on hardware, the test is marked `xfail(strict=False)` with the reason, so the run reports it without failing.
Use `-k`, `-m "not slow"` and `--junitxml report.xml` as usual.

### CAN bus

The CAN frame, bit-timing and acceptance-filter tests run over the link `--can-mode` selects:

- `loopback` (default) - the controller's internal test mode (`loopback=1`); needs no wiring.
- `bus` - the controller sends through the transceiver to a CANable, which sends every frame back; the acceptance filter is checked on frames the CANable sends.
- `both` - each test runs once per link.

The `test_bus_*` tests need the CANable whatever the mode: an unacknowledged send (CANable listen-only), receive errors and bus off (CANable at `can.bus.wrong_bitrate`), and the automatic bus-off recovery with `recover=0/1`.

Wiring (set `can`): a 3.3 V CAN transceiver (SN65HVD230 or similar) with TXD on `can0tx` and RXD on `can0rx`, CANH/CANL/GND to the CANable, and 120 Ohm across CANH/CANL at each end (the CANable has a termination jumper).

`--can-peer` names the CANable:

| `--can-peer`                       | CANable firmware        | Bit rate per test | Listen-only | Bus-off tests |
|------------------------------------|-------------------------|-------------------|-------------|---------------|
| `slcan:/dev/ttyACM0`, `slcan:COM7` | slcan (factory default) | yes               | yes         | yes           |
| `gs_usb:0`, `candle:0`             | candleLight             | yes               | no          | yes           |
| `socketcan:can0`                   | candleLight (Linux)     | no, `ip link`     | no          | no            |
| `port-bridge:<host>:5001`          | any                     | no, port-bridge   | no          | no            |

- slcan gives every test; on Linux the CANable shows up as `/dev/ttyACM*`.
- `gs_usb` and `candle` need python-can's extras of the same name (`pip install "python-can[gs-usb]"`).
- With a fixed bit rate, pass it as `--can-peer-bitrate` (500000 is what the bus tests use); tests at other bit rates skip.
- `slcan:` accepts any pyserial URL, so `slcan:socket://<host>:<port>` reaches a CANable whose COM port another machine forwards (see the bridge mode below).

```bash
pytest validation/host/tests/hil/test_can.py --board ek_tm4c123gxl --port /dev/ttyACM0 --can-mode both --can-peer slcan:/dev/ttyACM1
```

## Windows host and Docker (bridge mode)

Use this when the build and the tests run in the hal-ti devcontainer (Docker) but the AD3, the LaunchPad's serial port, its ICDI debugger and the CANable are plugged into a Windows PC. Docker cannot reach those USB devices, so Windows shares them over TCP and the container uses them through `host.docker.internal`:

```text
 Docker container (devcontainer)                  Windows host
 ┌─────────────────────────────────┐  TCP 5025   ┌─────────────────────────────┐
 │ pytest --ad3-remote ...         │ ──────────▶ │ ad3-bench-server            │── WaveForms ── AD3 (USB)
 │ pytest --port socket://...:5000 │  TCP 5000   │ port-bridge  (serial)       │── COMx ─────── LaunchPad UART
 │ gdb-multiarch (flash, debug)    │  TCP 3333   │ port-bridge  (OpenOCD)      │── ICDI ─────── LaunchPad debug
 │ pytest --can-peer slcan:...5002 │  TCP 5002   │ port-bridge  (serial)       │── COMy ─────── CANable (slcan)
 └─────────────────────────────────┘             └─────────────────────────────┘
```

| Port | Windows service                                                                             | Used in the container                                                      |
|------|---------------------------------------------------------------------------------------------|----------------------------------------------------------------------------|
| 5025 | [`ad3-bench-server`](https://github.com/embedded-pro/ad3-waveforms-bench) (WaveForms, AD3)  | `--ad3-remote host.docker.internal:5025` or `AD3_REMOTE`                   |
| 5000 | [`port-bridge`](https://github.com/gabrielfrasantos/port-bridge) serial (firmware terminal) | `--port socket://host.docker.internal:5000` or `HAL_TI_PORT`               |
| 3333 | `port-bridge` OpenOCD GDB server (ICDI)                                                     | `gdb-multiarch ... -ex "target extended-remote host.docker.internal:3333"` |
| 5002 | second `port-bridge`, serial (the CANable's slcan COM port)                                 | `--can-peer slcan:socket://host.docker.internal:5002` or `HAL_TI_CAN_PEER` |

The container needs no USB access, no WaveForms runtime and no OpenOCD: the devcontainer already has `gdb-multiarch`, and its `runArgs` map `host.docker.internal` to the host (Docker Desktop defines the name anyway; the mapping makes it work with Docker Engine in WSL2 too).

### On Windows

1. Install [WaveForms](https://digilent.com/reference/software/waveforms/waveforms-3/start), plug in the AD3 and check that WaveForms sees it. Close WaveForms afterwards: only one program can own the AD3.
2. Install OpenOCD for Windows (for example the [xPack OpenOCD](https://xpack-dev-tools.github.io/openocd-xpack/) build) and put it on `PATH`. OpenOCD reaches the ICDI through libusb:
   - if it reports that it cannot open the ICDI, install the WinUSB driver for the "In-Circuit Debug Interface" with [Zadig](https://zadig.akeo.ie/);
   - this replaces TI's driver, so switch it back to use LM Flash Programmer or UniFlash;
   - this step has not been checked on every Windows and OpenOCD combination.
3. Install Python 3.10 or newer, then both bridges (neither is on PyPI; the [ad3-waveforms-bench releases](https://github.com/embedded-pro/ad3-waveforms-bench/releases) and [port-bridge releases](https://github.com/gabrielfrasantos/port-bridge/releases) also provide Windows installers with a GUI):

   ```powershell
   py -m venv $env:USERPROFILE\hil-bridge
   & $env:USERPROFILE\hil-bridge\Scripts\Activate.ps1
   pip install "ad3-waveforms-bench @ git+https://github.com/embedded-pro/ad3-waveforms-bench@v0.2.0"
   pip install "port-bridge @ git+https://github.com/gabrielfrasantos/port-bridge@v0.1.5"
   ```

4. Start both, in two terminals:

   ```powershell
   ad3-bench-server
   port-bridge --serial-port COM5 --serial-baudrate 921600 --probe openocd --openocd-board ek-tm4c1294xl
   ```

   - `COM5` is the COM port of the firmware terminal: the ICDI virtual COM port (on the EK-TM4C1294XL with JP4/JP5 in the UART2 position) or the USB-UART adapter on PD4/PD5.
   - 921600 is the terminal baud rate of the firmware. The serial bridge is a plain byte stream, so the baud rate is set here and `--baud` has no effect in the container.
   - Use `--openocd-board ek-tm4c123gxl` for the EK-TM4C123GXL.
   - `port-bridge` holds the COM port and the ICDI while it runs; stop it before flashing with UniFlash or opening the port in another tool.
   - `ad3-bench-server --fake` serves a simulated AD3 to try the setup without hardware.

5. For the CAN bus tests, forward the CANable's COM port with a second `port-bridge` (slcan firmware; the TTY baud rate does not matter over USB):

   ```powershell
   port-bridge --serial-port COM7 --serial-baudrate 115200 --serial-tcp-port 5002
   ```

   - The CANable's slcan commands go through the byte stream, so the tests set the bit rate and listen-only mode themselves.
   - Alternatively, add `--can-interface slcan --can-channel COM7 --can-bitrate 500000` (or `--can-interface candle --can-channel 0` for candleLight) to the first `port-bridge` and use `--can-peer port-bridge:host.docker.internal:5001 --can-peer-bitrate 500000`. That bridge runs the CANable at one fixed bit rate, so the tests at other bit rates, the listen-only test and the bus-off tests skip.

Both listen on `127.0.0.1` by default, which Docker Desktop reaches through `host.docker.internal`. If the container cannot connect (for example Docker Engine inside WSL2):

- listen on all interfaces with `ad3-bench-server --host 0.0.0.0 --token <secret>` (and `AD3_REMOTE_TOKEN=<secret>` in the container) and `port-bridge --bind 0.0.0.0`;
- keep ports 5025, 5000, 5002 and 3333 blocked from the network in the Windows firewall: whoever reaches them controls the AD3 and the board.

### In the container

Build, flash through the OpenOCD of the host, then run the tests against the host's AD3 and serial port:

```bash
cmake --preset tm4c1294ncpdt
cmake --build --preset tm4c1294ncpdt-Debug --target hal_ti.validation_firmware

gdb-multiarch build/tm4c1294ncpdt/validation/firmware/hal_ti.validation_firmware.elf -batch \
    -ex "target extended-remote host.docker.internal:3333" \
    -ex "monitor reset halt" -ex load -ex "monitor reset run" -ex detach

python3 -m venv .venv && . .venv/bin/activate
pip install -e "validation/host[ad3]"

export AD3_REMOTE=host.docker.internal:5025
export HAL_TI_PORT=socket://host.docker.internal:5000
pytest validation/host/tests/hil --board ek_tm4c1294xl --wiring-set pwm --depth quick

export HAL_TI_CAN_PEER=slcan:socket://host.docker.internal:5002
pytest validation/host/tests/hil/test_can.py --board ek_tm4c1294xl --can-mode both
```

- `AD3_REMOTE` and `HAL_TI_PORT` can be replaced by `--ad3-remote` and `--port` on the pytest command line.
- If `python3 -m venv` is not available in the image, use `pip install --user -e "validation/host[ad3]"`.
- To debug from VS Code inside the container, use the `ek-tm4c123gxl (OpenOCD on host)` or `ek-tm4c1294xl (OpenOCD on host)` launch configuration: it attaches `gdb-multiarch` to the OpenOCD of the host instead of starting a local debug server.
- The AD3 serves one client at a time; a second pytest run is refused until the first one disconnects. When a client disconnects, the server releases every AD3 output and switches the supplies off.

## Wiring sets

The tables are generated from the board files; `role` and `requires` entries of the YAML are shown as notes. Scope inputs are single ended: connect the `-` input of each used scope channel to GND.
The mandatory connections are the same physical pins as before the tests became generic; the only additions are optional (`--with pwm4`, `--with faultpin`, `--with dcmp` on the EK-TM4C123GXL, `--with qei1`) and the `comparator_c0` set, which replaces the `comparator` set when used.
The `wdt` set reuses the `gpio0` connection of the `gpio` set.

### EK-TM4C123GXL wiring

| Wiring set      | AD3      | Pin           | Note                                                                                             |
|-----------------|----------|---------------|--------------------------------------------------------------------------------------------------|
| `gpio`          | DIO0     | PB2           |                                                                                                  |
| `gpio`          | DIO1     | PB3           |                                                                                                  |
| `gpio`          | DIO2     | PA7           |                                                                                                  |
| `gpio`          | DIO3     | PC6           |                                                                                                  |
| `gpio`          | DIO4     | PF0 (can0rx)  | locked pin, also SW2 (open unless pressed)                                                       |
| `gpio`          | DIO5     | PD7 (qei0b)   | locked pin (NMI)                                                                                 |
| `gpio`          | DIO6     | PF1 (led0)    |                                                                                                  |
| `gpio`          | DIO7     | PA2 (gpio0)   |                                                                                                  |
| `wdt`           | DIO7     | PA2 (gpio0)   | wdt pin                                                                                          |
| `pwm`           | DIO0     | PB6 (m0pwm0)  |                                                                                                  |
| `pwm`           | DIO1     | PB7 (m0pwm1)  |                                                                                                  |
| `pwm`           | DIO2     | PB4 (m0pwm2)  |                                                                                                  |
| `pwm`           | DIO3     | PB5 (m0pwm3)  |                                                                                                  |
| `pwm`           | DIO4     | PE4 (m0pwm4)  |                                                                                                  |
| `pwm`           | DIO5     | PE5 (m0pwm5)  |                                                                                                  |
| `pwm`           | DIO6     | PC4           | M0PWM6, generator 3 A; only with `--with pwm4`                                                   |
| `pwm`           | DIO7     | PC5           | M0PWM7, generator 3 B; only with `--with pwm4`                                                   |
| `pwm`           | DIO8     | PD2           | M0FAULT0, driven by the AD3; only with `--with faultpin`                                         |
| `pwm`           | W1       | PE3 (ain0)    | ADC digital comparator input of the fault path; only with `--with dcmp`                          |
| `pwm`           | Scope 1+ | PE3 (ain0)    | only with `--with dcmp`                                                                          |
| `pwm`           | -        | -             | stock boards short PB6-PD0 and PB7-PD1 through R9/R10; remove them or leave PD0/PD1 unconfigured |
| `adc`           | W1       | PE3 (ain0)    |                                                                                                  |
| `adc`           | W2       | PE0 (ain3)    |                                                                                                  |
| `adc`           | Scope 1+ | PE3 (ain0)    |                                                                                                  |
| `adc`           | Scope 2+ | PE0 (ain3)    |                                                                                                  |
| `comparator`    | DIO0     | PF0 (can0rx)  | C0o output                                                                                       |
| `comparator`    | W1       | PC6           | C0+ input                                                                                        |
| `comparator`    | W2       | PC7           | C0- input                                                                                        |
| `comparator`    | Scope 1+ | PC6           |                                                                                                  |
| `comparator`    | Scope 2+ | PC7           |                                                                                                  |
| `comparator_c0` | DIO1     | PF1 (led0)    | C1o output                                                                                       |
| `comparator_c0` | W1       | PC6           | C0+ input                                                                                        |
| `comparator_c0` | W2       | PC4           | C1- input                                                                                        |
| `comparator_c0` | Scope 1+ | PC6           |                                                                                                  |
| `comparator_c0` | Scope 2+ | PC4           |                                                                                                  |
| `qei`           | DIO0     | PD6 (qei0a)   |                                                                                                  |
| `qei`           | DIO1     | PD7 (qei0b)   |                                                                                                  |
| `qei`           | DIO2     | PD3 (qei0idx) |                                                                                                  |
| `qei`           | DIO3     | PC5           | QEI1 phase A; only with `--with qei1`                                                            |
| `qei`           | DIO4     | PC6           | QEI1 phase B; only with `--with qei1`                                                            |
| `qei`           | DIO5     | PC4           | QEI1 index; only with `--with qei1`                                                              |
| `uart`          | DIO0     | PB1           | firmware TX, AD3 UART RX                                                                         |
| `uart`          | DIO1     | PB0           | firmware RX, AD3 UART TX                                                                         |
| `uart`          | DIO2     | PC4           | firmware RTS; only with `--with flow`                                                            |
| `uart`          | DIO3     | PC5           | firmware CTS, driven by the AD3; only with `--with flow`                                         |
| `spi`           | DIO0     | PA2 (gpio0)   | spi clk                                                                                          |
| `spi`           | DIO1     | PA3           | spi cs                                                                                           |
| `spi`           | DIO2     | PA5 (gpio2)   | spi mosi                                                                                         |
| `spi`           | DIO3     | PA4 (gpio1)   | AD3 drives the MISO level unless the loopback jumper is fitted                                   |
| `spi`           | -        | -             | `--with loopback`: jumper PA5 (MOSI) to PA4 (MISO); DIO3 then only listens                       |
| `can`           | -        | -             | 3.3 V CAN transceiver (SN65HVD230 or similar), VCC 3.3 V                                         |
| `can`           | -        | -             | transceiver TXD to PF3 (can0tx), RXD to PF0 (can0rx)                                             |
| `can`           | -        | -             | transceiver CANH/CANL/GND to the CANable CANH/CANL/GND                                           |
| `can`           | -        | -             | 120 Ohm across CANH/CANL at each end (CANable jumper on)                                         |

### EK-TM4C1294XL wiring

| Wiring set      | AD3      | Pin           | Note                                                                       |
|-----------------|----------|---------------|----------------------------------------------------------------------------|
| `gpio`          | DIO0     | PB2           |                                                                            |
| `gpio`          | DIO1     | PB3           |                                                                            |
| `gpio`          | DIO2     | PE4 (gpio1)   |                                                                            |
| `gpio`          | DIO3     | PE5 (gpio2)   |                                                                            |
| `gpio`          | DIO4     | PD7           | locked pin (NMI)                                                           |
| `gpio`          | DIO5     | PN3 (led0)    |                                                                            |
| `gpio`          | DIO6     | PN4 (gpio0)   |                                                                            |
| `wdt`           | DIO6     | PN4 (gpio0)   | wdt pin                                                                    |
| `pwm`           | DIO0     | PF2 (m0pwm2)  |                                                                            |
| `pwm`           | DIO1     | PF3 (m0pwm3)  |                                                                            |
| `pwm`           | DIO2     | PG0 (m0pwm4)  |                                                                            |
| `pwm`           | DIO3     | PG1 (m0pwm5)  |                                                                            |
| `pwm`           | DIO4     | PK4 (m0pwm6)  |                                                                            |
| `pwm`           | DIO5     | PK5 (m0pwm7)  |                                                                            |
| `pwm`           | DIO6     | PF0           | M0PWM0, generator 0 A; only with `--with pwm4`                             |
| `pwm`           | DIO7     | PF1           | M0PWM1, generator 0 B; only with `--with pwm4`                             |
| `pwm`           | DIO8     | PK6           | M0FAULT1, driven by the AD3; only with `--with faultpin`                   |
| `pwm`           | W1       | PB5 (ain11)   | ADC digital comparator input of the fault path                             |
| `pwm`           | W2       | PB4 (ain10)   |                                                                            |
| `pwm`           | Scope 1+ | PB5 (ain11)   |                                                                            |
| `adc`           | W1       | PE3 (ain0)    |                                                                            |
| `adc`           | W2       | PB5 (ain11)   |                                                                            |
| `adc`           | Scope 1+ | PE3 (ain0)    |                                                                            |
| `adc`           | Scope 2+ | PB5 (ain11)   |                                                                            |
| `comparator`    | DIO0     | PD1           | C1o output                                                                 |
| `comparator`    | W1       | PC5           | C1+ input                                                                  |
| `comparator`    | W2       | PC4           | C1- input                                                                  |
| `comparator`    | Scope 1+ | PC5           |                                                                            |
| `comparator`    | Scope 2+ | PC4           |                                                                            |
| `comparator_c0` | DIO0     | PD1           | C1o output                                                                 |
| `comparator_c0` | W1       | PC6 (gpio6)   | C0+ input                                                                  |
| `comparator_c0` | W2       | PC4           | C1- input                                                                  |
| `comparator_c0` | Scope 1+ | PC6 (gpio6)   |                                                                            |
| `comparator_c0` | Scope 2+ | PC4           |                                                                            |
| `qei`           | DIO0     | PL1 (qei0a)   |                                                                            |
| `qei`           | DIO1     | PL2 (qei0b)   |                                                                            |
| `qei`           | DIO2     | PL3 (qei0idx) |                                                                            |
| `uart`          | DIO0     | PA5           | firmware TX, AD3 UART RX                                                   |
| `uart`          | DIO1     | PA4           | firmware RX, AD3 UART TX                                                   |
| `uart`          | DIO2     | PP4           | firmware RTS; only with `--with flow`                                      |
| `uart`          | DIO3     | PP5           | firmware CTS, driven by the AD3; only with `--with flow`                   |
| `spi`           | DIO0     | PD3           | spi clk                                                                    |
| `spi`           | DIO1     | PD2           | spi cs                                                                     |
| `spi`           | DIO2     | PD0           | spi mosi                                                                   |
| `spi`           | DIO3     | PD1           | AD3 drives the MISO level unless the loopback jumper is fitted             |
| `spi`           | -        | -             | `--with loopback`: jumper PD0 (MOSI) to PD1 (MISO); DIO3 then only listens |
| `can`           | -        | -             | JP4/JP5 in the UART2 position so PA0/PA1 are not driven by the ICDI        |
| `can`           | -        | -             | 3.3 V CAN transceiver (SN65HVD230 or similar), VCC 3.3 V                   |
| `can`           | -        | -             | transceiver TXD to PA1 (can0tx), RXD to PA0 (can0rx)                       |
| `can`           | -        | -             | transceiver CANH/CANL/GND to the CANable CANH/CANL/GND                     |
| `can`           | -        | -             | 120 Ohm across CANH/CANL at each end (CANable jumper on)                   |
| `ethernet`      | -        | -             | `--with ethernet`: the Ethernet cable is plugged in                        |

## What is tested

- `test_system.py` - `ping`, `info`, and the `board.pins` alias table against the board file in both directions, every alias accepted as a pin, generic alias names, reserved terminal pins/UART, error reasons (`usage`, `busy`, `notopen`, `range`), `delay`, `reset` and the `EVT boot` cause.
- `test_gpio.py` - output levels with every drive strength, inputs following the AD3 with every pull, pull-only idle levels, open drain, locked pins (PF0/PD7), LED output, interrupt counts for edge x handler type x pulse count x frequency against exact AD3 pulse trains, `gpio.pulse` timing.
- `test_pwm.py` - for `Pwm` and `SynchronousPwm`:
  - mode x divisor x frequency x duty (0 and 100 % included), with `ERR range` exactly where LOAD does not fit;
  - 1-4 generators with A-and-B, A-only and B-only outputs;
  - separate rising/falling dead time x output inversion with a shoot-through check, and the dead-time limits;
  - local/global update with generator alignment and duty changes; frequency change and stop.
  - Asynchronous only: every interrupt source counted with `pwm.count` (up-count comparator events never occur in edge mode), and one source per generator.
  - Every ADC trigger source of every generator, checked through an asynchronous ADC sequencer triggered by it, and one trigger per generator.
  - The fault path with the AD3 driving a fault pin (latch, minimum period, generator subsets) and with a wavegen driving an ADC digital comparator input, both expecting `EVT pwm`.
  - Argument errors of `pwm.open` and `pwm.fault`. Forced outputs during a fault are `xfail` (driver does not program PWMFAULTVAL).
- `test_uart.py` - every baud rate x parity x stop bits x driver variant (interrupt, DMA, synchronous; synchronous is 8N1 only), both directions against the AD3 UART, frames decoded from the firmware TX line and the bit rate measured on it, large payloads, full-duplex streaming, RTS/CTS/RTS+CTS flow control for every variant, argument errors.
- `test_spi.py` - SPI modes 0-3 x baud rates up to sysclk/2 x asynchronous/synchronous driver x FSS or no chip select, decoded from the logic analyzer (MOSI data, MISO data, clock polarity, clock rate, FSS release) where the sample rate allows, received data with a static MISO level or the MOSI-MISO jumper, receive-only and largest transfers, continued sessions, argument errors.
- `test_adc.py` - for `Adc` and `SynchronousAdc`:
  - wavegen DC levels against raw 12-bit codes (checked against the scope when it is wired) on both ADCs;
  - every sequencer at full depth (8/4/4/1 steps) and the depth limits; sample-and-hold x averaging x sampling delay;
  - asynchronous sequencers triggered by a PWM generator the test opens, and their timeout without it;
  - digital comparator bands x modes reported through the PWM fault path, with the comparator steps kept out of the FIFO;
  - two sequencers with explicit priorities; internal reference. The external reference is `xfail` (the driver never writes the reference selection) and needs `tests.adc.external_reference_v`.
- `test_comparator.py` - output and output pin for inputs above/below each other with and without inversion for both drivers, the positive input taken from C0+ (`src=c0`), the internal reference ladder located by a wavegen search, every ADC trigger sense accepted (its effect is not observable), edge interrupt counts against a square wave, level interrupts and synchronous interrupts refused.
- `test_qei.py` - position counts for frequency x cycles x direction x capture mode x phase inversion, physically inverted phases restored by `inva`/`invb`, offset and rollover, reset on the index pulse with and without `invi`, speed against the velocity period, clock/direction mode, default pins; QEI1 on the EK-TM4C123GXL with `--with qei1`.
- `test_can.py`:
  - over loopback and/or the bus (`--can-mode`): bit rate x standard/extended id x DLC 0-8, explicit bit timing (on the bus, the CANable runs at the computed bit rate), standard and extended acceptance filters with and without id-type match;
  - on the bus only: `ackError` with a listen-only CANable, receive errors and bus off with the CANable at a wrong bit rate, `recover=0/1`;
  - argument errors.
- `test_eeprom.py` - erase, patterns at aligned/unaligned addresses without touching neighbours, overwrite, largest write, range errors at the end, persistence across reset.
- `test_watchdog.py` - both watchdogs with timeouts from 1 ms to 5 s x warning-only or reset x automatic or manual feeding (warnings, no reset while fed, `reset=wdt<n>` otherwise), the early-warning period measured on the `pin=` toggle, manual feeding, a single watchdog at a time, argument errors.
- `test_ethernet.py` - `ERR unsupported` on TM4C123; open/status on TM4C129 and, with a cable, link up at each speed.

## Customising

Each board file (`host/boards/<board>.yaml`) holds:

- `terminal`, `pins` (the PROTOCOL.md alias table, compared with `board.pins`; only the generic names of `hal_ti_validation.protocol.PIN_ALIASES` are accepted) and `ad3` (supplies, analog limits).
- `wiring_sets` - per set, `dio`, `wavegen` and `scope` maps from AD3 channel to pin or alias. An entry is either a pin or a mapping with `pin`, `role` (looked up by tests, e.g. `wdt_pin`), `note` and `requires` (only used with `--with <tag>`); `jumpers` and `options` document extra wiring.
- `tests` - the parameters of every test module: parameter matrices, pins and instances, levels and tolerances.
  - `@pytest.mark.matrix("pwm.waveform")` turns every key of that mapping into one test parameter of the same name; `@pytest.mark.board_params("argname", "section.key")` adds one parameter from a list.
  - All parameters of a test form one matrix: `--depth full` runs its product, `--depth quick` a pairwise subset (`hal_ti_validation.pairwise`); `@pytest.mark.constraint(valid=...)` removes combinations a driver cannot take (for example parity with the synchronous UART).
  - Extending a sweep or moving a peripheral to other pins is a YAML change.

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

- `firmware.py` - typed API with one group per PROTOCOL.md section (`fw.gpio`, `fw.pwm`, `fw.uart`, `fw.spi`, `fw.adc`, `fw.comp`, `fw.qei`, `fw.can`, `fw.eeprom`, `fw.wdt`, `fw.eth`, `fw.system`); keyword arguments map 1:1 to protocol options (`continue_` for `continue`), `Dcmp` builds `dcmp` entries, `PwmFault` parses `EVT pwm`.
- `protocol.py` - the hal-ti part of the protocol: error reasons, `P<port><index>` pins and the generic alias names (`normalize_pin`, `parse_pin_map`).
- `config.py` - board file loading, wiring-set merging, parameter matrices and overrides; `expect.py` - expected TM4C values (PWM quantisation, dead band, interrupt events, CAN bit timing, comparator reference, counter wrap).
- `pairwise.py` - the full product and the deterministic pairwise generator behind `--depth`.
- `fake_firmware.py` - `FakeFirmware`, an in-memory stand-in for the validation firmware used by the unit tests and `--fake`.
- `console.py` - the `hal-ti-console` entry point.

In [ad3-waveforms-bench](https://github.com/embedded-pro/ad3-waveforms-bench) (generic, imported as `ad3_waveforms_bench`):

- `protocol.py` - pure parsing and formatting: `Response`, `Event`, number/hex/list helpers, command formatting, configurable `Dialect`.
- `terminal.py` - `FirmwareTerminal`: sends `cmd\r`, strips echo, prompts and escape sequences, queues `EVT` lines, raises `FirmwareError(reason, command)` on `ERR`, `wait_event`, `wait_boot`, `sync`.
- `instruments/ad3.py`, `instruments/dwf.py` - `AnalogDiscovery3` over the WaveForms SDK: supplies, static DIO, pattern generator (pulses, clocks, custom sequences, quadrature with index), logic analyzer with DIO-edge trigger, wavegen, scope, UART/SPI/CAN protocol engines.
- `analysis.py` - pure signal analysis (frequency, duty, edges, dead time, phase, quadrature/SPI/UART decode, statistics).
- `instruments/fake.py`, `fake_terminal.py` - `FakeDwfApi` and the generic fake terminal device and serial port; `pytest_plugin.py` - AD3 options, marker and fixture.
