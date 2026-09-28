# Validation terminal protocol

The validation firmware (`validation/firmware`) exposes every hal-ti peripheral through an EMIL `services::TerminalWithCommandsImpl` terminal.
The host package (`validation/host`) drives this terminal and a Digilent Analog Discovery 3 to validate the peripherals.

## Board profiles

The pinout and peripheral assignment follow the e-foc project (`targets/platform_implementations/ti/<board>/PinsAndPeripherals.hpp`), so a LaunchPad already modified for e-foc needs no rework.
The terminal is a `hal::tiva::UartWithDma` at 921600 8N1 without flow control, as in e-foc.

| Alias                    | EK-TM4C123GXL       | EK-TM4C1294XL       |
|--------------------------|---------------------|---------------------|
| terminal (tx / rx)       | UART0 PA1 / PA0     | UART2 PD5 / PD4     |
| `phasea` `phaseb` `phasec` | PE3 PE2 PE1       | PE3 PE2 PE1         |
| `vbus`                   | PE0                 | PB5                 |
| `itotal`                 | PE0                 | PB4                 |
| `halla` `hallb` `hallc`  | PA4 PA5 PA6         | PE4 PE5 PE6         |
| `enca` `encb` `encz`     | PD6 PD7 PD3 (QEI0)  | PL1 PL2 PL3 (QEI0)  |
| `pwm1a` `pwm1b`          | PB6 PB7 (M0 gen 0)  | PF2 PF3 (M0 gen 1)  |
| `pwm2a` `pwm2b`          | PB4 PB5 (M0 gen 1)  | PG0 PG1 (M0 gen 2)  |
| `pwm3a` `pwm3b`          | PE4 PE5 (M0 gen 2)  | PK4 PK5 (M0 gen 3)  |
| `canrx` `cantx`          | PF0 PF3 (CAN0)      | PA0 PA1 (CAN0)      |
| `ledop` `ledwarn` `ledfail` | PF1 PF1 PF1      | PN3 PN2 PP2         |
| `perf`                   | PA2                 | PN4                 |
| `id0` `id1` `id2`        | -                   | PK0 PK1 PK2 (pull-up) |
| `pwrstatus`              | -                   | PC6 (pull-up)       |

- Wherever a pin is expected, an alias from this table may be used instead of `P<port><index>`.
- Every `*.open` command without pin arguments uses the e-foc assignment above, and the documented defaults reproduce e-foc's configuration (PWM module 0 center-aligned, divisor 2, 1000 ns dead time, 20 kHz; phase-current ADC0 sequencer 0 triggered by the first phase generator, sample-and-hold 8, oversampling 2, sampling delay 4; supply ADC1 sequencer 0, sample-and-hold 256, oversampling 8; QEI0 quadrature on A and B with reset on max position; CAN0; watchdog 0).
- `board.pins` → `OK <alias>=<pin>,...` lists the table for the running board.

## Framing

- The host sends one command per line, terminated by `\r`. The terminal echoes characters and prints a prompt; the host ignores echo and prompt.
- Every command produces exactly one final line, either `OK` followed by optional `key=value` pairs, or `ERR <reason>` where `<reason>` is one token (`usage`, `pin`, `busy`, `notopen`, `unsupported`, `range`, `timeout`, `failed`).
- Asynchronous notifications are single lines starting with `EVT <peripheral>` followed by `key=value` pairs. They can appear at any time, including between a command and its final line.
- After reset the firmware prints `EVT boot board=<name> family=<tm4c123|tm4c129> sysclk=<hz> reset=<cause>` once.
- Numbers are decimal unless prefixed with `0x`. Binary payloads are hex strings without separators (`a55a0102`). Lists are comma separated without spaces.
- Pins are written as `P<port><index>`, for example `PF1`, `PJ0`, `PQ3`.
- Keys in arguments are `key=value`; positional arguments come first, in the order shown. Optional arguments are shown in brackets.
- Instance numbers are the hardware index (UART 0-7, SSI 0-3, ADC 0-1, sequencer 0-3, PWM module 0-1, QEI 0-1, CAN 0-1, comparator 0-2, watchdog 0-1).
- Opening an instance that is already open returns `ERR busy`; using one that is not open returns `ERR notopen`; `*.close` releases the driver and its pins so it can be reopened with different settings.
- The terminal UART and its pins are reserved and cannot be opened; any other pin, including the ones e-foc uses, can be reconfigured freely.

## General

- `ping` → `OK`
- `info` → `OK board=<name> family=<family> sysclk=<hz> reset=<cause> uid=<hex|none>`
- `reset` → no final line; the board resets and prints `EVT boot ...`
- `delay <ms>` → `OK` after the given time (lets the host synchronise with firmware timing)

## GPIO (`hal::tiva::GpioPin`)

- `gpio.cfg <pin> <in|out|od> [pull=none|up|down] [drive=2|4|8]` → `OK`
- `gpio.set <pin> <0|1>` → `OK`
- `gpio.get <pin>` → `OK value=<0|1>`
- `gpio.pulse <pin> <count> <periodMs>` → `OK` after `count` toggles of the output, one every `periodMs` (EMIL timer driven)
- `gpio.irq <pin> <rising|falling|both|off> [type=immediate|dispatched]` → `OK`; each edge increments a counter
- `gpio.count <pin> [clear=0|1]` → `OK count=<n>`
- `gpio.release <pin>` → `OK`

## PWM (`hal::tiva::Pwm`, `sync=1` selects `hal::tiva::SynchronousPwm`)

- `pwm.open <module> [gens=<g>[,<g>...]] [pins=<a>:<b>[,<a>:<b>...]] [freq=<hz>] [mode=edge|center] [div=1|2|4|8|16|32|64] [dead=<ns>|off] [inva=0|1] [invb=0|1] [update=local|global] [trigger=zero|load|none] [irq=zero|load|cmpau|cmpad|cmpbu|cmpbd] [sync=0|1]` → `OK pwmclk=<hz>`; without `gens`/`pins` it opens the three e-foc phase generators; `trigger` sets the ADC trigger of the first generator (e-foc: `load` on TM4C123, `zero` on TM4C129); `sync` defaults to the e-foc choice (1 on TM4C123, 0 on TM4C129)
- `pwm.fault <module> <on|off>` → `OK`; on TM4C129 enables e-foc's fault path (ADC digital comparators 0 and 1 into the fault inputs of every open generator); faults report `EVT pwm module=<m> fault=<bits>`; on TM4C123 returns `ERR unsupported`
- `pwm.duty <module> <duty1%> [duty2%] [duty3%]` → `OK`; one duty per opened generator in open order, starts the outputs (as e-foc's `Start(a, b, c)`); duty accepts decimals (`12.5`), `0` and `100`
- `pwm.freq <module> <hz>` → `OK`
- `pwm.stop <module>` → `OK`
- `pwm.count <module> <gen> [clear=0|1]` → `OK count=<n>` (generator interrupts)
- `pwm.close <module>` → `OK`

## UART (`hal::tiva::Uart`, `dma=1` selects `hal::tiva::UartWithDma`, `sync=1` selects `hal::tiva::SynchronousUart`)

- `uart.open <index> [tx=<pin>] [rx=<pin>] [rts=<pin>] [cts=<pin>] [baud=<bps>] [parity=none|even|odd] [stop=1|2] [flow=none|rts|cts|rtscts] [dma=0|1] [sync=0|1]` → `OK`; `baud` must be one of the driver's rates (600 ... 921600); default 115200 8N1; without pins it uses the board's first free UART pins (TM4C123: UART1 PB0/PB1; TM4C129: UART0 is taken by CAN, so pins are required)
- `uart.send <index> <hex>` → `OK` once the driver reports completion
- `uart.recv <index> [timeout=<ms>] [len=<n>]` → `OK data=<hex>` with everything received since the last `uart.recv` (waits up to `timeout` for `len` bytes when given)
- `uart.close <index>` → `OK`

## SPI master (`hal::tiva::SpiMaster`, `sync=1` selects `hal::tiva::SynchronousSpiMaster`)

- `spi.open <index> clk=<pin> mosi=<pin> miso=<pin> [cs=<pin>] [baud=<hz>] [mode=0|1|2|3] [sync=0|1]` → `OK`
- `spi.xfer <index> <txHex> [rx=<n>] [continue=0|1]` → `OK rx=<hex>`; with an empty `txHex` (`-`) it receives `rx` bytes
- `spi.close <index>` → `OK`

## ADC (`hal::tiva::Adc`, `sync=1` selects `hal::tiva::SynchronousAdc`)

- `adc.open <adc> <seq> [pins=<pin>[,<pin>...]] [sh=4|8|16|32|64|128|256] [avg=off|2|4|8|16|32|64] [delay=<n>|off] [trigger=pwm0|pwm1|pwm2|pwm3] [dcmp=<index>:<low>:<high>[,...]] [sync=0|1]` → `OK`; `adc.open 0 0` without pins is e-foc's phase-current sequencer (async, PWM triggered), `adc.open 1 0` is e-foc's supply sequencer (`sync=1`); `dcmp` appends steps routed to digital comparators (high band, always), as e-foc does for over-current/over-voltage on TM4C129
- `adc.measure <adc> <seq> [n=<samples>]` → `OK samples=<v>[,<v>...]` (raw 12-bit codes)
- `adc.close <adc> <seq>` → `OK`

## Analog comparator (`hal::tiva::AnalogComparator`, `sync=1` selects `hal::tiva::SynchronousAnalogComparator`)

- `comp.open <index> pos=<pin> neg=<pin> [out=<pin>] [src=pin|c0|ref] [ref=low|high,<step>] [invert=0|1] [sync=0|1]` → `OK`
- `comp.read <index>` → `OK out=<0|1>`
- `comp.irq <index> <rising|falling|both|off>` → `OK`; `comp.count <index> [clear=0|1]` → `OK count=<n>`
- `comp.close <index>` → `OK`

## Quadrature encoder (`hal::tiva::QuadratureEncoder`)

- `qei.open <index> [a=<pin>] [b=<pin>] [idx=<pin>] [res=<n>] [offset=<n>] [inva=0|1] [invb=0|1] [invi=0|1] [reset=max|index] [cap=a|ab] [sig=quad|clkdir] [vel=<us>]` → `OK`
- `qei.read <index>` → `OK pos=<n> dir=<fwd|rev> speed=<n> res=<n>`
- `qei.close <index>` → `OK`

## CAN (`hal::tiva::Can`)

- `can.open <index> [rx=<pin>] [tx=<pin>] [bitrate=<bps>] [filter=<id>,<mask>,<ext0|1>] [loopback=0|1] [recover=0|1]` → `OK`
- `can.send <index> <id> <hex> [ext=0|1]` → `OK` when transmission succeeded, `ERR failed` otherwise
- Received frames: `EVT can index=<i> id=<id> ext=<0|1> data=<hex>`; errors: `EVT can index=<i> error=<name>`
- `can.close <index>` → `OK`

## EEPROM (`hal::tiva::Eeprom`)

- `eeprom.write <address> <hex>` → `OK`
- `eeprom.read <address> <len>` → `OK data=<hex>`
- `eeprom.erase` → `OK`

## Watchdog (`hal::tiva::WatchDog`)

- `wdt.start <index> timeout=<ms> [reset=0|1] [feed=auto|manual]` → `OK`; with `feed=auto` the firmware refreshes on every early warning
- `wdt.feed <index>` → `OK`
- Early warning: `EVT wdt index=<i> warning=<n>`. After a watchdog reset the next `EVT boot` reports `reset=wdt0` or `reset=wdt1`.

## Ethernet (TM4C129 only, `hal::tiva::Ethernet`)

- `eth.open [phy=internal] [speed=auto|10|100]` → `OK`
- `eth.status` → `OK link=<up|down> speed=<10|100> duplex=<half|full> rx=<frames> tx=<frames>`
- `eth.close` → `OK`
- On TM4C123 every `eth.*` command returns `ERR unsupported`.
