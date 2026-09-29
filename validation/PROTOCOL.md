# Validation terminal protocol

The validation firmware (`validation/firmware`) exposes every hal-ti peripheral through EMIL's hardware-in-the-loop terminal (`services::HilTerminal` and the command groups of `services.hil.commands`); hal-ti supplies the board profiles, the Tiva pin factory and one factory per peripheral.
The host package (`validation/host`) drives this terminal and a Digilent Analog Discovery 3 to validate the peripherals.

## Board profiles

Hardware prerequisite: the pin wiring follows LaunchPads modified for the e-foc motor-control project (on the EK-TM4C123GXL, R9/R10 are removed so that PB6/PB7 are not shorted to PD0/PD1); the firmware has no other e-foc behaviour.
The terminal is a `hal::tiva::UartWithDma` at 921600 8N1 without flow control.

Aliases name the pins by peripheral function:

| Alias                     | EK-TM4C123GXL | EK-TM4C1294XL | Function                       |
|---------------------------|---------------|---------------|--------------------------------|
| `terminaltx` `terminalrx` | PA1 PA0       | PD5 PD4       | terminal UART (UART0 / UART2)  |
| `ain0` `ain1` `ain2`      | PE3 PE2 PE1   | PE3 PE2 PE1   | ADC inputs AIN0-AIN2           |
| `ain3`                    | PE0           | -             | ADC input AIN3                 |
| `ain10` `ain11`           | -             | PB4 PB5       | ADC inputs AIN10-AIN11         |
| `m0pwm0` `m0pwm1`         | PB6 PB7       | -             | PWM module 0 generator 0 A / B |
| `m0pwm2` `m0pwm3`         | PB4 PB5       | PF2 PF3       | PWM module 0 generator 1 A / B |
| `m0pwm4` `m0pwm5`         | PE4 PE5       | PG0 PG1       | PWM module 0 generator 2 A / B |
| `m0pwm6` `m0pwm7`         | -             | PK4 PK5       | PWM module 0 generator 3 A / B |
| `qei0a` `qei0b` `qei0idx` | PD6 PD7 PD3   | PL1 PL2 PL3   | QEI0 phase A, phase B, index   |
| `can0rx` `can0tx`         | PF0 PF3       | PA0 PA1       | CAN0 receive / transmit        |
| `led0`                    | PF1           | PN3           | LED output                     |
| `led1` `led2`             | -             | PN2 PP2       | LED outputs                    |
| `gpio0`                   | PA2           | PN4           | general-purpose test pin       |
| `gpio1` `gpio2`           | PA4 PA5       | PE4 PE5       | general-purpose test pins      |
| `gpio3`                   | PA6           | PK0           | general-purpose test pin       |
| `gpio4` `gpio5` `gpio6`   | -             | PK1 PK2 PC6   | general-purpose test pins      |

- Wherever a pin is expected, an alias from this table may be used instead of `P<port><index>`; no alias carries a default pull.
- Per-instance default pins apply only when a command gets no pins at all: UART1 PB1/PB0 (TM4C123 only), QEI0 `qei0a`/`qei0b`/`qei0idx`, CAN0 `can0rx`/`can0tx`. Other UART, QEI and CAN instances need their pins; ADC, SPI and the comparator always do, PWM needs `gens` or `pins`.
- `board.pins` → `OK <alias>=<pin>,...` lists the table for the running board.

## Framing

The generic framing (`OK`/`ERR`/`EVT` lines, reasons, the deferred `\r\n` prefix, number, hex and list syntax, open/close semantics) is specified in EMIL's [hardware-in-the-loop terminal documentation](https://github.com/embedded-pro/embedded-infra-lib/blob/main/docs/Hil.md). hal-ti adds:

- After reset the firmware prints `EVT boot board=<name> family=<tm4c123|tm4c129> sysclk=<hz> reset=<cause>` once; `<cause>` is `wdt0`, `wdt1`, `sw`, `moscfail`, `bor`, `por`, `ext` or `unknown`.
- Pins are written as `P<port><index>`, for example `PF1`, `PJ0`, `PQ3`: ports A-F on TM4C123 and A-H, J-N, P, Q on TM4C129, index 0-7.
- Instance numbers are the hardware index (UART 0-7, SSI 0-3, ADC 0-1, sequencer 0-3, PWM module 0-1, QEI 0-1, CAN 0-1, comparator 0-2, watchdog 0-1); an index the running MCU lacks (PWM module 1 and QEI 1 on TM4C129, comparator 2 on TM4C123) returns `ERR range`.
- The terminal UART and its pins are reserved and cannot be opened (`ERR busy`); any other pin, aliased or not, can be reconfigured freely. A pin held by another open instance returns `ERR busy`; a pin the pinout table does not offer for the requested function and instance returns `ERR pin`.
- RAM limits how many instances are open at the same time: 1 PWM module, 1 UART besides the terminal, 1 SSI, 2 ADC sequencers, 1 comparator, 1 QEI, 1 CAN, 1 watchdog and 8 GPIO pins; one more returns `ERR busy`.
- Argument errors (`usage`, `range`, `pin`, `unsupported`) are reported before `ERR busy`.

## General

- `ping` → `OK`
- `info` → `OK board=<name> family=<family> sysclk=<hz> reset=<cause> uid=<hex|none>`
- `reset` → no final line; the board resets and prints `EVT boot ...`
- `delay <ms>` → `OK` after the given time (lets the host synchronise with firmware timing)

## GPIO (`hal::tiva::GpioPin`)

- `gpio.cfg <pin> <in|out|od> [pull=none|up|down] [drive=2|4|8]` → `OK`; `out` starts low, `od` starts released and takes no pull; `pull` defaults to `none`
- `gpio.set <pin> <0|1>` → `OK`
- `gpio.get <pin>` → `OK value=<0|1>`
- `gpio.pulse <pin> <count> <periodMs>` → `OK` after `count` toggles of the output, one every `periodMs` (EMIL timer driven); `ERR usage` on an input
- `gpio.irq <pin> <rising|falling|both|off> [type=immediate|dispatched]` → `OK`; each edge increments a counter; with a hal-ti `Gpio` that only serves ports A-F, other ports return `ERR unsupported` and `type` has no effect (always dispatched)
- `gpio.count <pin> [clear=0|1]` → `OK count=<n>`
- `gpio.release <pin>` → `OK`

## PWM (`hal::tiva::Pwm`, `sync=1` selects `hal::tiva::SynchronousPwm`)

- `pwm.open <module> [gens=<g>[,<g>...]] [pins=<a>:<b>[,<a>:<b>...]] [freq=<hz>] [mode=edge|center] [div=1|2|4|8|16|32|64] [dead=<ns>|<riseNs>,<fallNs>|off] [inva=0|1] [invb=0|1] [update=local|global] [trigger=<src>[,<src>...]] [irq=<src>[,<src>...]] [sync=0|1]` → `OK pwmclk=<hz>`
  - `gens` or `pins` is required (`ERR usage`); 1 to 4 generators, each at most once
  - a `pins` entry may use `-` for an unused channel (`PB6:-` is A-only, `-:PB7` B-only); without `gens` the generator follows from the pin
  - `gens` without `pins` takes the first A and B pins of the pinout table for each generator
  - defaults: `freq=10000 mode=edge div=1 dead=off inva=0 invb=0 update=local trigger=none irq=none sync=0`
  - `dead=<ns>` sets equal rising- and falling-edge delays, `dead=<riseNs>,<fallNs>` sets them separately (each at most 1000000 ns)
  - `<src>` is `none`, `zero`, `load`, `cmpau`, `cmpad`, `cmpbu` or `cmpbd`; a single value applies to every generator, a list gives one per generator in open order (`ERR usage` for another count)
  - `trigger` selects each generator's ADC trigger event; ADC `trigger=pwm<g>` samples on generator `g` of module 0
  - `irq` enables one interrupt source per generator, counted by `pwm.count`; it needs `sync=0` (`ERR unsupported`)
  - each open resets the module, so no setting of a previous open survives
- `pwm.fault <module> on [gens=<g>[,<g>...]] [comparators=<mask>] [inputs=<mask>] [pin=<pin>] [latch=0|1] [minperiod=<clocks>]` → `OK`
  - configures the driver's `FaultConfig` for each generator in `gens` (default: every open generator; one that is not open returns `ERR usage`)
  - `comparators` enables ADC digital comparators 0-7 as fault sources (bitmask 0-255); `inputs` enables fault pins FAULT0-FAULT3 (bitmask 0-15); at least one must be nonzero (`ERR usage`)
  - `latch` (default 0) sets the driver's latch flag, so the fault stays asserted after its source clears; `minperiod` (default 0, off) is the minimum fault period in PWM clocks, at most 65535
  - `pin` muxes a fault pin of the module (`ERR pin` for another pin): TM4C123 PD2, PD6, PF2 (M0FAULT0) and PF4 (M1FAULT0); TM4C129 PF4 (M0FAULT0), PK6 (M0FAULT1), PK7 (M0FAULT2), PL0 (M0FAULT3); a high level is a fault
  - the pin is released by `pwm.fault <module> off`, the next accepted `pwm.fault` or `pwm.close`
  - the pin is claimed before the module is rebuilt: a `pin=` that answers `ERR pin` or `ERR busy` leaves the previous fault configuration, its pin and the running outputs untouched; repeating the same `pin=` keeps it
  - the driver takes the fault configuration only at construction, so the module is rebuilt and its outputs stay stopped until the next `pwm.duty`; the driver does not force the outputs on a fault, it only reports it
  - faults report `EVT pwm module=<m> gens=<mask> comparators=<mask> inputs=<mask>` (generator fault status, digital comparator and pin fault inputs of all generators OR-ed)
  - with `sync=1` returns `ERR unsupported`
- `pwm.fault <module> off` → `OK`; removes the fault configuration (rebuilds the module)
- `pwm.duty <module> <duty1%> [duty2%] [duty3%] [duty4%]` → `OK`; one duty per opened generator in open order, or a single duty for all of them, starts the outputs; duty accepts decimals (`12.5`, up to 4 digits), `0` and `100`
- `pwm.freq <module> <hz>` → `OK`
- `pwm.stop <module>` → `OK`
- `pwm.count <module> <gen> [clear=0|1]` → `OK count=<n>` (generator interrupts)
- A frequency whose period does not fit the 16-bit load register, or a dead time above 4095 PWM clocks, returns `ERR range`
- `pwm.close <module>` → `OK`

## UART (`hal::tiva::Uart`, `dma=1` selects `hal::tiva::UartWithDma`, `sync=1` selects `hal::tiva::SynchronousUart`)

- `uart.open <index> [tx=<pin>] [rx=<pin>] [rts=<pin>] [cts=<pin>] [baud=<bps>] [parity=none|even|odd] [stop=1|2] [flow=none|rts|cts|rtscts] [dma=0|1] [sync=0|1]` → `OK`
  - `baud` must be one of the driver's rates (600 ... 921600)
  - default 115200 8N1
  - without pins UART1 uses PB1/PB0 on TM4C123; every other UART, and every UART on TM4C129, needs `tx` and `rx`
  - `flow` needs the matching `rts`/`cts` pins
  - `dma=1` with `sync=1` returns `ERR usage`
  - `sync=1` supports only `parity=none stop=1` (`ERR unsupported` otherwise)
- `uart.send <index> <hex>` → `OK` once the driver reports completion (up to 112 bytes; `ERR timeout` if the driver never completes)
- `uart.recv <index> [timeout=<ms>] [len=<n>]` → `OK data=<hex>` with everything received since the last `uart.recv`, at most 256 bytes (waits up to `timeout`, default 1000, at most 10000, for `len` bytes when given, and returns what arrived even if fewer)
- `uart.close <index>` → `OK`

## SPI master (`hal::tiva::SpiMaster`, `sync=1` selects `hal::tiva::SynchronousSpiMaster`)

- `spi.open <index> clk=<pin> mosi=<pin> miso=<pin> [cs=<pin>] [baud=<hz>] [mode=0|1|2|3] [sync=0|1]` → `OK`; defaults `baud=100000 mode=0`; `cs` is the SSI frame-select pin and needs `sync=0`; `baud` outside what the SSI prescalers reach (sysclk/65024 < baud <= sysclk/2) returns `ERR range`
- `spi.xfer <index> <txHex> [rx=<n>] [continue=0|1]` → `OK rx=<hex>`; with an empty `txHex` (`-`) it receives `rx` bytes; `rx` defaults to the length of `txHex`, the transfer lasts max(tx, `rx`) bytes with `txHex` zero-padded, and the first `rx` received bytes are returned (`rx=0` only transmits); at most 64 bytes
- `spi.close <index>` → `OK`

## ADC (`hal::tiva::Adc`, `sync=1` selects `hal::tiva::SynchronousAdc`)

- `adc.open <adc> <seq> pins=<pin>[,<pin>...] [sh=4|8|16|32|64|128|256] [avg=off|2|4|8|16|32|64] [delay=<0-15>|off] [trigger=pwm0|pwm1|pwm2|pwm3] [dcmp=<entry>[,<entry>...]] [ref=int|ext] [prio=<0-3>] [sync=0|1]` → `OK`
  - `pins` is required (`ERR usage`), one step per pin, at most the sequencer depth (8, 4, 4 and 1 steps for sequencers 0-3; `ERR range` beyond)
  - defaults: `sh=4 avg=off delay=off ref=int sync=0`, `prio` equal to the sequencer number
  - `sync=0` requires `trigger` (`ERR usage`) because the driver has no processor trigger; `trigger=pwm<g>` is generator `g` of PWM module 0, which must be open with a `trigger` for that generator
  - `sync=1` returns `ERR unsupported` with `delay`, `trigger`, `dcmp` or `ref=ext`
  - `avg` and `delay` apply to the whole ADC; opening a sequencer while no other sequencer of the same ADC is open resets that ADC first
  - a `dcmp` entry is `<index>:<low>:<high>[:<band>][:<mode>]` with `band` one of `low`, `mid`, `high` (default `high`) and `mode` one of `always`, `once`, `hyst`, `hystonce` (default `always`)
  - `dcmp` routes the last N steps of the sequence (N = number of entries) to digital comparators instead of the FIFO; their trigger output feeds the PWM fault inputs (`pwm.fault comparators=`)
  - `ERR range` when N is not fewer than the steps, an index is above 7 or repeated, or not `low <= high <= 4095`
  - `ref` selects the driver's `externalReference` setting
- `adc.measure <adc> <seq> [n=<samples>]` → `OK samples=<v>[,<v>...]` (raw 12-bit codes); `n` is the number of sequence runs (default 1), each contributing one value per FIFO step, at most 64 values; an asynchronous sequencer waits for its PWM trigger and returns `ERR timeout` after 1000 ms
- `adc.close <adc> <seq>` → `OK`

## Analog comparator (`hal::tiva::AnalogComparator`, `sync=1` selects `hal::tiva::SynchronousAnalogComparator`)

- `comp.open <index> pos=<pin> neg=<pin> [out=<pin>] [src=pin|c0|ref] [ref=low|high,<step>] [invert=0|1] [trigger=off|rising|falling|both|high|low] [sync=0|1]` → `OK`
  - `pos` is only needed with `src=pin`; `ref` implies `src=ref`
  - `trigger` sets the ADC trigger sense (edges, or level `high`/`low`); default `off`
- `comp.read <index>` → `OK out=<0|1>`
- `comp.irq <index> <rising|falling|both|off>` → `OK`; `comp.count <index> [clear=0|1]` → `OK count=<n>`; `comp.irq` needs `sync=0` (`ERR unsupported`); only edge interrupts, because the driver derives the interrupt sense from `hal::InterruptTrigger`
- `comp.close <index>` → `OK`

## Quadrature encoder (`hal::tiva::QuadratureEncoder`)

- `qei.open <index> [a=<pin>] [b=<pin>] [idx=<pin>] [res=<n>] [offset=<n>] [inva=0|1] [invb=0|1] [invi=0|1] [reset=max|index] [cap=a|ab] [sig=quad|clkdir] [vel=<us>]` → `OK`; defaults `res=1024 offset=0 inva=0 invb=0 invi=0 reset=max cap=ab sig=quad vel=1000`; without pins QEI0 uses `qei0a`, `qei0b` and `qei0idx`, other instances need `a` and `b`
- `qei.read <index>` → `OK pos=<n> dir=<fwd|rev> speed=<n> res=<n>` (`res` is the driver's `Resolution()`)
- `qei.close <index>` → `OK`

## CAN (`hal::tiva::Can`)

- `can.open <index> [rx=<pin>] [tx=<pin>] [bitrate=<bps>] [timing=<tseg1>,<tseg2>,<sjw>,<brp>] [filter=<id>,<mask>,<ext0|1>[,<match0|1>]] [loopback=0|1] [recover=0|1]` → `OK`
  - defaults `bitrate=500000 loopback=0 recover=1`, no filter; without pins CAN0 uses `can0rx` and `can0tx`, other instances need both pins
  - a bit rate without a valid C_CAN timing at the running sysclk returns `ERR range`
  - `timing` sets the driver's explicit `BitTiming` instead of `bitrate` (both together: `ERR usage`): `tseg1` 1-16 (propagation plus phase segment 1, in quanta), `tseg2` 1-8, `sjw` 1-4, `brp` 1-1024 (`ERR range` outside); the bit rate is sysclk / (`brp` × (1 + `tseg1` + `tseg2`))
  - `filter` accepts ids matching `<id>` under `<mask>` as standard (`ext0`) or extended (`ext1`) ids; `match` (default 1) also requires the id type to match, `match0` lets frames of the other type through the mask
- `can.send <index> <id> <hex> [ext=0|1]` → `OK` when transmission succeeded, `ERR failed` otherwise (`ERR timeout` after 1000 ms; later sends return `ERR busy` until the frame leaves or the controller is closed); up to 8 bytes, `-` for none
- Received frames: `EVT can index=<i> id=<id> ext=<0|1> data=<hex>`; errors: `EVT can index=<i> error=<name>` with `<name>` one of `stuffError formError ackError bit1Error bit0Error crcError busOff errorWarning errorPassive messageLost`; the same error repeated within 100 ms is reported once
- `can.close <index>` → `OK`

## EEPROM (`hal::tiva::Eeprom`)

- `eeprom.write <address> <hex>` → `OK`
- `eeprom.read <address> <len>` → `OK data=<hex>`
- Every `eeprom.*` answers from the driver's completion, so its final line arrives as a deferred line (leading `\r\n`); `ERR timeout` after 5 s
- At most 112 bytes per `eeprom.write` or `eeprom.read`
- `eeprom.erase` → `OK`

## Watchdog (`hal::tiva::WatchDog`)

- `wdt.start <index> timeout=<ms> [reset=0|1] [feed=auto|manual] [pin=<pin>]` → `OK`
  - with `feed=auto` the firmware refreshes on every early warning; defaults `reset=1 feed=auto`, `timeout` 1-30000
  - `pin` is driven low and toggled in the early-warning interrupt, so its period can be measured; it stays claimed until reset
  - both watchdogs share one interrupt vector, so only one can be started, and a started watchdog cannot be stopped: it runs until reset
- `wdt.feed <index>` → `OK`
- Early warning: `EVT wdt index=<i> warning=<n>`. After a watchdog reset the next `EVT boot` reports `reset=wdt0` or `reset=wdt1`.

## Ethernet (TM4C129 only, `hal::tiva::Ethernet`)

- `eth.open [phy=internal] [speed=auto|10|100]` → `OK`; `auto` and `100` both advertise up to 100 Mbit/s full duplex (the driver takes a single `hal::LinkSpeed`)
- `eth.status` → `OK link=<up|down> speed=<10|100> duplex=<half|full> rx=<frames> tx=<frames>`; `speed`/`duplex` are those of the last link-up (`10`/`half` before the first); the firmware sends no frames
- `eth.close` → `OK`
- On TM4C123 every `eth.*` command returns `ERR unsupported`.
