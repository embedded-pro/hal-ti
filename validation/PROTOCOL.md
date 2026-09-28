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
- `board.pins` → `OK <alias>=<pin>,...` lists the table for the running board, plus `terminaltx` and `terminalrx`.

## Framing

- The host sends one command per line, terminated by `\r`. The terminal echoes characters and prints a prompt; the host ignores echo and prompt.
- Every command produces exactly one final line, either `OK` followed by optional `key=value` pairs, or `ERR <reason>` where `<reason>` is one token (`usage`, `pin`, `busy`, `notopen`, `unsupported`, `range`, `timeout`, `failed`).
- Asynchronous notifications are single lines starting with `EVT <peripheral>` followed by `key=value` pairs. They can appear at any time, including between a command and its final line.
- Lines produced outside the processing of a command line (deferred results such as `delay`, and every `EVT`) start with `\r\n`, so an empty line or a bare prompt `> ` may precede them.
- Command lines are at most 255 characters (EMIL terminal buffer). Unknown commands, unknown keys and a wrong number of positional arguments return `ERR usage`.
- After reset the firmware prints `EVT boot board=<name> family=<tm4c123|tm4c129> sysclk=<hz> reset=<cause>` once; `<cause>` is `wdt0`, `wdt1`, `sw`, `moscfail`, `bor`, `por`, `ext` or `unknown`.
- Numbers are decimal unless prefixed with `0x`. Binary payloads are hex strings without separators (`a55a0102`). Lists are comma separated without spaces.
- Pins are written as `P<port><index>`, for example `PF1`, `PJ0`, `PQ3`.
- Keys in arguments are `key=value`; positional arguments come first, in the order shown. Optional arguments are shown in brackets.
- Instance numbers are the hardware index (UART 0-7, SSI 0-3, ADC 0-1, sequencer 0-3, PWM module 0-1, QEI 0-1, CAN 0-1, comparator 0-2, watchdog 0-1); an index the running MCU lacks (PWM module 1 and QEI 1 on TM4C129, comparator 2 on TM4C123) returns `ERR range`.
- Opening an instance that is already open returns `ERR busy`; using one that is not open returns `ERR notopen`; `*.close` releases the driver and its pins so it can be reopened with different settings.
- The terminal UART and its pins are reserved and cannot be opened (`ERR busy`); any other pin, including the ones e-foc uses, can be reconfigured freely. A pin held by another open instance returns `ERR busy`; a pin the pinout table does not offer for the requested function and instance returns `ERR pin`.
- RAM limits how many instances are open at the same time: 1 PWM module, 1 UART besides the terminal, 1 SSI, 2 ADC sequencers, 1 comparator, 1 QEI, 1 CAN, 1 watchdog and 8 GPIO pins; one more returns `ERR busy`.

## General

- `ping` → `OK`
- `info` → `OK board=<name> family=<family> sysclk=<hz> reset=<cause> uid=<hex|none>`
- `reset` → no final line; the board resets and prints `EVT boot ...`
- `delay <ms>` → `OK` after the given time (lets the host synchronise with firmware timing)

## GPIO (`hal::tiva::GpioPin`)

- `gpio.cfg <pin> <in|out|od> [pull=none|up|down] [drive=2|4|8]` → `OK`; `out` starts low, `od` starts released and takes no pull; `pull` defaults to the alias's pull (`up` for `id0`-`id2` and `pwrstatus`), otherwise `none`
- `gpio.set <pin> <0|1>` → `OK`
- `gpio.get <pin>` → `OK value=<0|1>`
- `gpio.pulse <pin> <count> <periodMs>` → `OK` after `count` toggles of the output, one every `periodMs` (EMIL timer driven); `ERR usage` on an input
- `gpio.irq <pin> <rising|falling|both|off> [type=immediate|dispatched]` → `OK`; each edge increments a counter; with a hal-ti `Gpio` that only serves ports A-F, other ports return `ERR unsupported` and `type` has no effect (always dispatched)
- `gpio.count <pin> [clear=0|1]` → `OK count=<n>`
- `gpio.release <pin>` → `OK`

## PWM (`hal::tiva::Pwm`, `sync=1` selects `hal::tiva::SynchronousPwm`)

- `pwm.open <module> [gens=<g>[,<g>...]] [pins=<a>:<b>[,<a>:<b>...]] [freq=<hz>] [mode=edge|center] [div=1|2|4|8|16|32|64] [dead=<ns>|off] [inva=0|1] [invb=0|1] [update=local|global] [trigger=zero|load|none] [irq=zero|load|cmpau|cmpad|cmpbu|cmpbd] [sync=0|1]` → `OK pwmclk=<hz>`; without `gens`/`pins` it opens the three e-foc phase generators (module 0 only); a `pins` entry may use `-` for an unused channel and, without `gens`, the generator follows from the pin; `gens` without `pins` takes the first pins of the pinout table; defaults are e-foc's (`freq=20000 mode=center div=2 dead=1000 update=global`); `irq` needs `sync=0`; `trigger` sets the ADC trigger of the first generator (e-foc: `load` on TM4C123, `zero` on TM4C129); `sync` defaults to the e-foc choice (1 on TM4C123, 0 on TM4C129)
- `pwm.fault <module> <on|off>` → `OK`; on TM4C129 enables e-foc's fault path (ADC digital comparators 0 and 1 into the fault inputs of every open generator, latched); the driver takes this only at construction, so the module is rebuilt and its outputs stay stopped until the next `pwm.duty`; faults report `EVT pwm module=<m> fault=<bits>` (digital comparator inputs of all generators OR-ed); on TM4C123, and with `sync=1`, returns `ERR unsupported`
- `pwm.duty <module> <duty1%> [duty2%] [duty3%] [duty4%]` → `OK`; one duty per opened generator in open order, or a single duty for all of them, starts the outputs (as e-foc's `Start(a, b, c)`); duty accepts decimals (`12.5`, up to 4 digits), `0` and `100`
- `pwm.freq <module> <hz>` → `OK`
- `pwm.stop <module>` → `OK`
- `pwm.count <module> <gen> [clear=0|1]` → `OK count=<n>` (generator interrupts)
- A frequency whose period does not fit the 16-bit load register, or a dead time above 4095 PWM clocks, returns `ERR range`
- `pwm.close <module>` → `OK`

## UART (`hal::tiva::Uart`, `dma=1` selects `hal::tiva::UartWithDma`, `sync=1` selects `hal::tiva::SynchronousUart`)

- `uart.open <index> [tx=<pin>] [rx=<pin>] [rts=<pin>] [cts=<pin>] [baud=<bps>] [parity=none|even|odd] [stop=1|2] [flow=none|rts|cts|rtscts] [dma=0|1] [sync=0|1]` → `OK`; `baud` must be one of the driver's rates (600 ... 921600); default 115200 8N1; without pins it uses the board's first free UART pins (TM4C123: UART1 PB0/PB1; TM4C129: UART0 is taken by CAN, so pins are required); `flow` needs the matching `rts`/`cts` pins; `dma=1` with `sync=1` returns `ERR usage`; `sync=1` supports only `parity=none stop=1` (`ERR unsupported` otherwise)
- `uart.send <index> <hex>` → `OK` once the driver reports completion (up to 112 bytes; `ERR timeout` if the driver never completes)
- `uart.recv <index> [timeout=<ms>] [len=<n>]` → `OK data=<hex>` with everything received since the last `uart.recv`, at most 256 bytes (waits up to `timeout`, default 1000, at most 10000, for `len` bytes when given, and returns what arrived even if fewer)
- `uart.close <index>` → `OK`

## SPI master (`hal::tiva::SpiMaster`, `sync=1` selects `hal::tiva::SynchronousSpiMaster`)

- `spi.open <index> clk=<pin> mosi=<pin> miso=<pin> [cs=<pin>] [baud=<hz>] [mode=0|1|2|3] [sync=0|1]` → `OK`; defaults `baud=100000 mode=0`; `cs` is the SSI frame-select pin and needs `sync=0`; `baud` outside what the SSI prescalers reach (sysclk/65024 < baud <= sysclk/2) returns `ERR range`
- `spi.xfer <index> <txHex> [rx=<n>] [continue=0|1]` → `OK rx=<hex>`; with an empty `txHex` (`-`) it receives `rx` bytes; `rx` defaults to the length of `txHex`, the transfer lasts max(tx, `rx`) bytes with `txHex` zero-padded, and the first `rx` received bytes are returned (`rx=0` only transmits); at most 64 bytes
- `spi.close <index>` → `OK`

## ADC (`hal::tiva::Adc`, `sync=1` selects `hal::tiva::SynchronousAdc`)

- `adc.open <adc> <seq> [pins=<pin>[,<pin>...]] [sh=4|8|16|32|64|128|256] [avg=off|2|4|8|16|32|64] [delay=<n>|off] [trigger=pwm0|pwm1|pwm2|pwm3] [dcmp=<index>:<low>:<high>[,...]] [sync=0|1]` → `OK`; `adc.open 0 0` without pins is e-foc's phase-current sequencer (async, PWM triggered), `adc.open 1 0` is e-foc's supply sequencer (`sync=1`); `dcmp` routes the last N steps of the sequence (N = number of entries, fewer than the steps) to digital comparators (high band, always) instead of the FIFO, as e-foc does for over-current/over-voltage on TM4C129; without `pins` that sequence is e-foc's `phasea,phaseb,phasec,itotal,vbus`, so `adc.open 0 0 dcmp=0:0:<oc>,1:0:<ov>` reproduces e-foc; other sequencers need `pins` and default to the phase-current settings; `delay`, `trigger` and `dcmp` need `sync=0`
- `adc.measure <adc> <seq> [n=<samples>]` → `OK samples=<v>[,<v>...]` (raw 12-bit codes); `n` is the number of sequence runs (default 1), each contributing one value per FIFO step, at most 64 values; an asynchronous sequencer waits for its PWM trigger and returns `ERR timeout` after 1000 ms
- `adc.close <adc> <seq>` → `OK`

## Analog comparator (`hal::tiva::AnalogComparator`, `sync=1` selects `hal::tiva::SynchronousAnalogComparator`)

- `comp.open <index> pos=<pin> neg=<pin> [out=<pin>] [src=pin|c0|ref] [ref=low|high,<step>] [invert=0|1] [sync=0|1]` → `OK`; `pos` is only needed with `src=pin`; `ref` implies `src=ref`
- `comp.read <index>` → `OK out=<0|1>`
- `comp.irq <index> <rising|falling|both|off>` → `OK`; `comp.count <index> [clear=0|1]` → `OK count=<n>`; `comp.irq` needs `sync=0` (`ERR unsupported`)
- `comp.close <index>` → `OK`

## Quadrature encoder (`hal::tiva::QuadratureEncoder`)

- `qei.open <index> [a=<pin>] [b=<pin>] [idx=<pin>] [res=<n>] [offset=<n>] [inva=0|1] [invb=0|1] [invi=0|1] [reset=max|index] [cap=a|ab] [sig=quad|clkdir] [vel=<us>]` → `OK`; defaults `res=1024 offset=0 reset=max cap=ab sig=quad vel=1000`; the e-foc pins are the default for QEI0 only
- `qei.read <index>` → `OK pos=<n> dir=<fwd|rev> speed=<n> res=<n>` (`res` is the driver's `Resolution()`)
- `qei.close <index>` → `OK`

## CAN (`hal::tiva::Can`)

- `can.open <index> [rx=<pin>] [tx=<pin>] [bitrate=<bps>] [filter=<id>,<mask>,<ext0|1>] [loopback=0|1] [recover=0|1]` → `OK`; defaults `bitrate=500000 loopback=0 recover=1`, the e-foc pins for CAN0 only; a bit rate without a valid C_CAN timing at the running sysclk returns `ERR range`
- `can.send <index> <id> <hex> [ext=0|1]` → `OK` when transmission succeeded, `ERR failed` otherwise (`ERR timeout` after 1000 ms; later sends return `ERR busy` until the frame leaves or the controller is closed); up to 8 bytes, `-` for none
- Received frames: `EVT can index=<i> id=<id> ext=<0|1> data=<hex>`; errors: `EVT can index=<i> error=<name>` with `<name>` one of `stuffError formError ackError bit1Error bit0Error crcError busOff errorWarning errorPassive messageLost`; the same error repeated within 100 ms is reported once
- `can.close <index>` → `OK`

## EEPROM (`hal::tiva::Eeprom`)

- `eeprom.write <address> <hex>` → `OK`
- `eeprom.read <address> <len>` → `OK data=<hex>`
- At most 112 bytes per `eeprom.write` or `eeprom.read`
- `eeprom.erase` → `OK`

## Watchdog (`hal::tiva::WatchDog`)

- `wdt.start <index> timeout=<ms> [reset=0|1] [feed=auto|manual]` → `OK`; with `feed=auto` the firmware refreshes on every early warning; defaults `reset=1 feed=auto`, `timeout` 1-30000; both watchdogs share one interrupt vector, so only one can be started, and a started watchdog runs until reset
- `wdt.feed <index>` → `OK`
- Early warning: `EVT wdt index=<i> warning=<n>`. After a watchdog reset the next `EVT boot` reports `reset=wdt0` or `reset=wdt1`.

## Ethernet (TM4C129 only, `hal::tiva::Ethernet`)

- `eth.open [phy=internal] [speed=auto|10|100]` → `OK`; `auto` and `100` both advertise up to 100 Mbit/s full duplex (the driver takes a single `hal::LinkSpeed`)
- `eth.status` → `OK link=<up|down> speed=<10|100> duplex=<half|full> rx=<frames> tx=<frames>`; `speed`/`duplex` are those of the last link-up (`10`/`half` before the first); the firmware sends no frames
- `eth.close` → `OK`
- On TM4C123 every `eth.*` command returns `ERR unsupported`.
