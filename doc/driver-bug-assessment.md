# Driver bug assessment

Scope: all of `hal_tiva/` (tiva, synchronous_tiva, instantiations, bringup), `tiva/` (startup, system, ldscripts, CMake), `examples/`, `integration_test/`.

Method: line-by-line review per subsystem, each candidate finding re-checked against the source, the CMSIS headers in `tiva/CMSIS`, TivaWare reference behaviour and the TM4C123GH6PM / TM4C1294NCPDT datasheets. Findings that did not survive re-checking are listed at the end so they are not re-raised.

Build status at `580a896`:

| Build | Result |
|---|---|
| `host-single-Debug` + ctest | pass (1/1) — but `integration_test/test/Test.cpp` is an empty placeholder; no driver code is compiled or tested on host |
| `tm4c123gh6pm` Debug | pass |
| `tm4c1294ncpdt` Debug | pass |
| `hal_tiva/` with `-Wall -Wextra -Wlogical-op -Wduplicated-cond …` | 4 unused-variable warnings (`Ethernet.cpp:776,783,792`, `Gpio.cpp:242`), 2 harmless `-Wparentheses` |

## Resolution status

All Critical, High and Medium findings and L1–L18, L20–L21 are fixed on this branch. Not changed:

- **L19** `SystemCoreClockUpdate()` on TM4C129 — the MOSC crystal frequency cannot be read back from hardware, so it cannot be implemented generically; `ConfigureClock` keeps assigning `SystemCoreClock` directly.
- **L22** host tests — the affected logic lives inside driver translation units that include the device header; extracting it for host testing is a refactor outside this fix set.

Found while fixing and also addressed: `ClockTm4c129` forced `MOSCCTL` (powering MOSC) even when running from internal oscillators; the FreeRTOS example had no `FREERTOS_CONFIG_FILE_DIRECTORY`, a 65 KB heap (> TM4C123 RAM) and `configPRIO_BITS 4` (Tiva has 3 — the CM4F port asserts on this at scheduler start).

Severity: **Critical** = driver unusable or memory corruption in normal use. **High** = wrong behaviour in a common configuration. **Medium** = wrong behaviour in a specific configuration / robustness. **Low** = latent, cosmetic, or rule violation without current impact. "Plausible" = reasoning is sound but needs a datasheet/hardware confirmation.

---

## Critical

| # | Location | Defect | Consequence |
|---|---|---|---|
| C1 | `hal_tiva/tiva/SpiMaster.cpp:220` | ISR reads `SSI->SR` but tests it with `SSI_RIS_*` masks. `SSI_RIS_TXRIS` (0x08) is `SR.RFF` (RX FIFO full); `SSI_RIS_RORRIS` (0x01) is `SR.TFE`. | TX branch never runs, TXIM (level, EOT set) re-fires forever → interrupt storm on the first `SendAndReceive`. Fix: read `MIS`. |
| C2 | `hal_tiva/tiva/SpiMaster.cpp:265` | TXIM is disabled after the first byte and never re-enabled anywhere (RX branch included). RTIM is also never enabled, and RXIM fires only at FIFO half-full. | Even with C1 fixed, any transfer > 1 byte stalls; the done callback never fires. The async SPI master needs a rework of its ISR state machine. |
| C3 | `hal_tiva/tiva/UartWithDma.cpp:162` | `Invoke()` only handles `DMATXRIS`/`DMARXRIS`/`RTRIS`. `OEIM` is enabled by `UartBase::Initialization` (`UartBase.cpp:203`) but overrun is never cleared here. | Any RX overrun → UART IRQ stays asserted → CPU locked in ISR. |
| C4 | `hal_tiva/tiva/UartWithDma.cpp:162` (Plausible — TM4C123 only) | UART `RIS` bits 16/17 (`DMARXRIS`/`DMATXRIS`) exist only on TM4C129. On TM4C123 uDMA completion is signalled on the peripheral vector and acknowledged via `UDMA->CHIS`, which this driver never reads/clears. | On TM4C123: TX completion callback never runs, RX ping-pong halves never re-armed, and the DMA-done interrupt is never acknowledged → IRQ storm. `UartWithDma` is compiled for both families. |
| C5 | `hal_tiva/tiva/Ethernet.cpp:1064` | `GetEthernetMacConfiguration` swaps its outputs: `config = DMAOPMODE`, `mode = CFG`. `SetEthernetMacConfiguration` writes `CFG ← config`, `DMAOPMODE ← mode` (TivaWare `EMACConfigGet` does the opposite). | On every PHY speed/ANC interrupt (`ProcessPhyInterrupt`, line 1159) CFG is overwritten with DMA-mode bits and DMAOPMODE with MAC-config bits (ST/SR lost) → Ethernet stops after link-up. |
| C6 | `hal_tiva/tiva/Ethernet.cpp:815`, `:840`, `:807`, `:827` | `IsEMACReady()`/`IsEPHYReady()` read `RCGCEMAC`/`RCGCEPHY` (just written to 1) instead of `PREMAC`/`PREPHY`. The reset pulse is a non-volatile 16-iteration empty loop, removable by the optimiser. | No wait for peripheral ready after clock-enable/reset (violates the repo's PRxxx rule); EMAC/EPHY registers can be accessed while still in reset. |

## High

| # | Location | Defect | Consequence |
|---|---|---|---|
| H1 | `hal_tiva/tiva/Uart.cpp:43` | `really_assert(!(RIS & OERIS))` — overrun (OEIM enabled unconditionally) halts the system; the flag is never cleared. | A single RX overrun (e.g. long higher-priority ISR) is fatal. Clear `OEIC`, report via error path. |
| H2 | `hal_tiva/tiva/Uart.cpp:49-54` | RX loop clears `RXIC` then reads one byte; `RXRIS` is then 0 so the loop exits. FIFO trigger is 7/8 (`UartBase.cpp:202`) and `RTIM` is not enabled for `Uart`. | Each interrupt delivers one byte; 13 bytes stay stranded in the FIFO until more data arrives. Interactive/short messages are delivered late or never. Drain while `!(FR & RXFE)` and enable RTIM. |
| H3 | `hal_tiva/synchronous_tiva/SynchronousUart.cpp:244-273` | `SynchronousUart` is a stub: no clock enable, no baud/LCRH, no IRQ registration; `SendData()` and `Invoke()` are empty. | `SendData` silently drops data; `ReceiveData` always times out. |
| H4 | `hal_tiva/synchronous_tiva/SynchronousSpiMaster.cpp:170` | Loop bound is `sendData.size()`; receive-only calls (allowed by the assert on line 168) loop 0 times. | `SendAndReceive({}, rx, …)` returns without clocking or receiving anything. Use `max(send, receive)` sizes. |
| H5 | `hal_tiva/synchronous_tiva/SynchronousPwm.cpp:353` | TM4C123 divisor `1U << ((result >> 1) + 1)`; `Pwm.cpp:223` correctly uses `1U << (result + 1)`. | With the default `divisor64` the computed period is 8× too small → PWM frequency 8× too high on TM4C123. |
| H6 | `hal_tiva/tiva/Adc.cpp:294` | `ConfigureSequencerStepDc` uses `bitShift = step` for `SSOPn`, but `SSOPn` bits are 4 apart (`SnDCOP` at bit `4·n`; `SequenceStepConfigure` gets this right). | Step 0 works; step 1 sets reserved bit 1; step 4 routes *step 1* to the comparator instead of the FIFO → wrong/missing samples when digital comparators are used on steps ≥ 1. |
| H7 | `hal_tiva/synchronous_tiva/SynchronousQuadratureEncoder.cpp:111` | `VELEN` enabled but `QEI->LOAD` (velocity timer period) is never written (reset value 0). | `Speed()` is meaningless (always ~0). Add a velocity-period config and write `LOAD` before `VELEN`. |
| H8 | `hal_tiva/tiva/Gpio.cpp:228-231` | `GpioPin::SetAsInput()` writes `DIR = 1`, which is *output*. | Pin driven instead of released. |
| H9 | `hal_tiva/tiva/ClockTm4c123.cpp:341` + `:480` | `crystalLookupTable[20]` duplicates 13.56 MHz, so entries 20-25 are shifted; the `+1` in `OscillatorFrequency` only compensates for 16/18/20 MHz. 24/25 MHz resolve to 25 MHz / 0. | Only 16, 18 and 20 MHz crystals report the correct frequency; others give wrong `SystemCoreClock` or 0. Fix the table (index 20 = 14.31818 MHz, 25 = 24 MHz) and drop `+1`. |
| H10 | `hal_tiva/tiva/ClockTm4c123.cpp:547` | `VerifyFrequency()` hard-asserts `SystemCoreClock == 80000000`. | Any `ConfigureClock` call with a different divider, oscillator or `usesPll=false` aborts. |
| H11 | `hal_tiva/tiva/UniqueDeviceId.cpp:7` | Reads 8 bytes at `0x400FE000` = `SYSCTL->DID0/DID1` (part/revision ID). | Every chip of the same part returns the same "unique" ID. TM4C129 has `UNIQUEID0-3` at `0x400FEF20`; TM4C123 has no unique ID register (needs another source or an explicit unsupported path). |
| H12 | `hal_tiva/tiva/PinoutTableDefaultTm4c123.cpp:267-270` | `pwmChannel0` entry `{0, B, 4, 4}` — PB4/AF4 is M0PWM2. M0PWM0 is PB6/AF4. | Requesting PWM channel 0 on module 0 muxes the generator-1 output to PB4. |
| H13 | `hal_tiva/tiva/PinoutTableDefaultTm4c123.cpp:260-265` (Plausible) | Fault table `{F0,AF4}`, `{B4,AF4}`, `{E4,AF4}` are not fault inputs (PF0 has no AF4; PB4/PE4 AF4 are PWM outputs). M0FAULT0 is PD2/PD6/PF2 (AF4); M1FAULT0 is PF4 (AF5, the only correct entry). | PWM fault input configured on a pin that cannot carry it. |
| H14 | `hal_tiva/tiva/PinoutTableDefaultTm4c129.cpp:164`, `:167` | I2C2 SCL listed as PL0 and I2C3 SCL as PK5 — both identical to their SDA entries. Correct: I2C2SCL = PL1, I2C3SCL = PK4. | SCL and SDA mapped to the same pin; bus unusable on those instances. |
| H15 | `hal_tiva/tiva/PinoutTableDefaultTm4c129.cpp:283-288` (Plausible) | PWM fault entry `{0, F, 0, 6}` is M0PWM0 (also present in `pwmChannel0`). M0FAULT0 is PF4/AF6. | Fault input mapped to a PWM output pin. |

## Medium

| # | Location | Defect | Consequence |
|---|---|---|---|
| M1 | `hal_tiva/tiva/Ethernet.cpp:1186` (Plausible) | RX descriptors are created with `OWN=1`, `Desc1` buffer size 0 and `Desc2` never set; buffers are attached later from an `EventDispatcher` job while `SR` is already set in `Initialize()`. | DMA owns descriptors with no buffer between start-up and the first `RequestReceiveBuffers()`. Initialise `Desc0 = 0` and set OWN only in `RequestReceiveBuffer()`. |
| M2 | `hal_tiva/tiva/Ethernet.cpp:1159` | `EPHY_MISR1_SPEED \| EPHY_MISR1_SPEED \| EPHY_MISR1_ANC` — `DUPLEXM` intended. | Duplex-only changes do not update the MAC duplex bit. |
| M3 | `hal_tiva/tiva/Ethernet.cpp:775-776` (Plausible) | LPI interrupt path reads `PMTCTLSTAT` instead of `LPICTLSTAT` (value unused). | LPI status never acknowledged if EEE is active. Use `(void)EMAC0->LPICTLSTAT`. |
| M4 | `hal_tiva/tiva/Ethernet.cpp:681-686` | Destructor issues DMA soft-reset while the EMAC IRQ is still enabled (the `interrupt` member is destroyed after the body). | ISR can run against a resetting MAC. Disable the IRQ first. |
| M5 | `hal_tiva/tiva/UartWithDma.cpp:142-147` + `Dma.cpp:331` | `ProcessRxTimeout` queries which half is active and its remaining count *before* `StopTransfer()`; `RemainingTransfers()` returns `XFERSIZE+1`, i.e. 1 on a completed (STOP) descriptor. | If the active half completes in that window, bytes are miscounted/dropped. Stop the channel first, and return 0 for a STOP-mode descriptor. |
| M6 | `hal_tiva/tiva/UartWithDma.cpp:61-66` | Destructor body leaves OEIM enabled, then members `dmaRx`/`dmaTx` are destroyed before `UartBase::~UartBase()` calls `Unregister()`. | An overrun IRQ in that window runs `Invoke()` on destroyed `DmaChannel`s. Call `Unregister()` / clear `IM` first. |
| M7 | `hal_tiva/tiva/Gpio.cpp:559`, `:582` | `handlers` is sized `8*6` but indexed by pin number only; `ExtiInterrupt` dispatches `handlers[line]` for whichever port fired. | Two ports can't both use the same pin number for interrupts (assert); table is 40 entries larger than used. Index by `port*8 + pin`. |
| M8 | `hal_tiva/tiva/Gpio.cpp:204-205` (Plausible) | Commit-unlock only handles PF0 and PD7 (TM4C123 set). TM4C1294 also locks PE7 (NMI). | `AFSEL/DEN/PUR` writes to PE7 on TM4C129 are ignored. |
| M9 | `hal_tiva/tiva/ClockTm4c129.cpp:305` | `crystalIndex >= idx(_10_MHz) - idx(_5_MHz)` evaluates to `>= 7`, true for every crystal. | `MOSCCTL.OSCRNG` (high range) set for 5-8 MHz crystals too. Compare against `idx(_10_MHz)`. |
| M10 | `hal_tiva/tiva/Adc.cpp:187`, `SynchronousAdc.cpp` | No check that `inputs.size()` fits the sequencer depth (8/4/4/1). | Extra steps write past the sequencer's `SSMUX/SSCTL` nibbles / into the next register block. Add `really_assert`. |
| M11 | `hal_tiva/tiva/Eeprom.cpp:57-67` | Init skips the datasheet/TivaWare step "poll `EEDONE.WORKING` and check `EESUPP` *before* `SREEPROM`". | Reset may be issued while EEPROM is still recovering from an interrupted write. |
| M12 | `hal_tiva/tiva/WatchDog.cpp:137-144` | ISR does not clear `ICR`; clearing relies on `Refresh()` being called synchronously inside `onEarlyWarning`. | If the application defers `Refresh()` via the event dispatcher, the level interrupt re-enters continuously until the second timeout resets the MCU. Document the contract or clear in the ISR. |
| M13 | `hal_tiva/tiva/Can.cpp:602-606` | `EWarn`/`EPass` are level flags; with `SIE` enabled every TxOk/RxOk status interrupt re-reports them. | Error callback flooded while the node stays in warning/passive state. Report on transitions only. |
| M14 | `hal_tiva/instantiations/LaunchPadBspEkTm4c123g.hpp:25-26`, `LaunchPadBspEkTm4c1294.hpp:25-26` | Buttons (PF4/PF0, PJ0/PJ1) created with `Drive::Default` (= no pull). Both LaunchPads have no external pull-ups. | Buttons float when released. Use `Drive::Up`. |
| M15 | `tiva/ldscripts/sections.ld:82-86` | `.ARM.exidx` placed `> REGION_RAM` without `AT(...)`. | Content is not in flash (LMA=VMA in RAM); a non-empty exidx also makes `objcopy -O binary` span 0x0…0x2000xxxx. Place it in `REGION_TEXT`. |
| M16 | `hal_tiva/tiva/UartBase.hpp:25-30` + `UartBase.cpp:106-111`; `SynchronousUart.hpp:73-78` + `SynchronousUart.cpp` table | Baud-rate enum/table use 38600, 56700, 921000 instead of the standard 38400, 57600, 921600. | 56700 is −1.6 % from a 57600 peer (38600: +0.5 %), eating most of the UART timing budget → framing errors against standard equipment. |
| M17 | `examples/freertos/Main.cpp:1-3` | Includes `generated/tiva/PinoutTableDefault.hpp`, `NucleoUi.hpp`, `StmEventInfrastructure.hpp` and calls STM32/Nucleo APIs — an unported STM32 example. | Build fails with `HAL_TI_BUILD_EXAMPLES_FREERTOS=ON`. |

## Low

| # | Location | Defect |
|---|---|---|
| L1 | `hal_tiva/tiva/SpiMaster.cpp:226` | `really_assert(status & SSI_RIS_RORRIS)` inside `if (status & SSI_RIS_RORRIS)` — tautology; overrun is silently accepted. |
| L2 | `hal_tiva/tiva/SpiMaster.cpp:159` | Phase/polarity RMW clears `SCR_M` instead of `SPH|SPO`; old mode bits survive a re-init without power cycle. |
| L3 | `hal_tiva/synchronous_tiva/SynchronousUart.cpp:349` | `CTL &= ~UART_CTL_UARTEN \| TXE \| RXE` — precedence; only UARTEN is cleared (TXE/RXE reset to 1 anyway). |
| L4 | `hal_tiva/synchronous_tiva/SynchronousUart.cpp:187-204` | `baudRateTiva` sized 13 with 12 values; `parityTiva` 4/3; `stopBitsTiva` 3/2 → zero-filled tail (div-by-zero if the enum grows). |
| L5 | `hal_tiva/synchronous_tiva/SynchronousUart.cpp:289-295` | `SynchronousUartSendOnly` ignores `uartRts` / `flowControl`. |
| L6 | `hal_tiva/tiva/Uart.cpp:51,59`; `UartBase.cpp:201`; `Gpio.cpp:562` | RMW on write-only `ICR` (`\|=`); writes to read-only `FR` and `GPIORIS` (no-ops). Use `ICR =`; clear errors via `ECR`. |
| L7 | `hal_tiva/tiva/Dma.cpp:151-160` | `|=` on write-only `*CLR` registers — use plain `=`. |
| L8 | `hal_tiva/tiva/Ethernet.cpp:626` | CRC not stripped (`ACS`/`CST` not set) → reported `frameSize` includes 4 FCS bytes (lwIP tolerates it via IP length). |
| L9 | `hal_tiva/tiva/Ethernet.cpp:1250-1253` | `#if 0` around `RXPOLLD` — RX DMA suspended on "no buffer" only resumes on the next incoming frame. |
| L10 | `hal_tiva/tiva/Ethernet.cpp:1282` | `Desc1 = data.size()` not masked to the 13-bit TBS1 field. |
| L11 | `hal_tiva/tiva/Ethernet.cpp:1310-1322` | `SentFrame()` inspects `sendDescriptorIndex-1`; correct only because EMIL keeps one frame in flight. |
| L12 | `hal_tiva/synchronous_tiva/SynchronousQuadratureEncoder.cpp:122-123` | `POS` written after `ENABLE`; write it first. |
| L13 | `hal_tiva/synchronous_tiva/SynchronousAdc.cpp:136` | `Measure(std::size_t)` ignores the requested sample count. |
| L14 | `hal_tiva/synchronous_tiva/SynchronousPwm.cpp:467` | Missing `load > 0` assert (present in `Pwm.cpp`). |
| L15 | `hal_tiva/tiva/AnalogComparator.hpp:91`, `SynchronousAnalogComparator.hpp:36` | Stores `const Config&`; only used inside the constructor today, but dangles for temporaries. Store by value. |
| L16 | `hal_tiva/tiva/Adc.cpp:372`, `Can.cpp:538-546`, `Pwm.cpp:323-336`, `Eeprom.cpp:93-99` | Destructors touch hardware / gate clocks before the IRQ is disabled (base-class or member handler destroyed later). |
| L17 | `hal_tiva/tiva/ClockTm4c123.cpp:572`; `Ethernet.cpp:807,827` | Non-volatile busy-wait loops (`Delay`, reset pulses) can be optimised away. |
| L18 | `tiva/CMSIS/.../startup_TM4C123.c:339` (same in 129) | `cpsie i` before C++ static init and `HardwareInitialization()`; an IRQ enabled by a static ctor would dispatch through an unconstructed `InterruptTable`. |
| L19 | `tiva/CMSIS/.../system_TM4C129.c:455` | `SystemCoreClockUpdate()` body compiled out (`CLOCK_SETUP 0`); harmless because `ConfigureClock` assigns `SystemCoreClock`, but a trap for callers. |
| L20 | `tiva/ldscripts/sections.ld:56` | No `ALIGN(4)` before `_edata`. |
| L21 | `hal_tiva/tiva/Ethernet.cpp:776,783,792`, `Gpio.cpp:242` | Unused variables (warnings). |
| L22 | `integration_test/test/Test.cpp` | Placeholder test only; none of the pure logic (CAN bit-timing search, PWM period/duty math, ADC step encoding, clock tables, UART divisors) is host-tested, although it could be without hardware. |

---

## Reviewed and rejected (do not re-raise)

- ADC `TSTSH` `(config & 0xf00000) >> 20` — correct; `TSHn` uses even encodings 0x0/0x2/…/0xC (matches TivaWare).
- ADC `SequenceStepConfigure` `SSOP &= ~(1 << step*4)` — correct; `SnDCOP` bits are 4 apart. (The bug is in `ConfigureSequencerStepDc`, H6.)
- ADC `SSEMUXn`/`SSTSHn` for sequencers 1-3 — exist on TM4C129 at the 0x20 stride.
- PWM `Control::Value()` `LOADUPD/CMPxUPD = 0` for `UpdateMode::locally` — 0 *is* locally-synchronised.
- TM4C129 `PLLFREQ1 |= PLL_Q_TO_REG(divisor)` — table column `[1]` is N only; matches TivaWare 2.2 PSYSDIV-erratum workaround.
- Ethernet MII clock table (`64 MHz → CR_35_60`) — identical to TivaWare.
- uDMA control table alignment — `.bss…controlTable` section is 1024-aligned in both builds.
- UART `dataReceived` called from ISR — EMIL `SerialCommunication` contract.
- Vector tables (123 and 129) — spot-checked IRQ positions are correct; linker memory sizes correct.
