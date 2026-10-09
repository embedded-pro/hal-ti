# GitHub Copilot Instructions for hal-ti

## Project Overview

This is a Hardware Abstraction Layer (HAL) for TI ARM Cortex-M based microcontrollers (TM4C123 and TM4C129 families). It implements the `embedded-infra-lib` HAL interfaces over TI Tiva C peripherals, providing event-driven drivers for GPIO, UART, CAN, ADC, SPI, DMA, and more. The library is designed for strict realtime and memory constraints in BLDC/PMSM motor control and similar embedded applications.

## Repository Structure

- **hal::cortex::***: Reset, SystemTick, SystemTickTimerService, InterruptTable/InterruptHandler, DataWatchpointAndTrace, EventDispatcher — all from EMIL's `hal/cortex_m/`, not this repo
- **hal_tiva/tiva/**: TM4C-specific peripheral drivers (Gpio, Uart, Can, Adc, SpiMaster, Dma, Clock)
- **hal_tiva/synchronous_tiva/**: Blocking/polling driver variants (SynchronousAdc, SynchronousUart)
- **hal_tiva/instantiations/**: Board Support Packages and infrastructure (LaunchPadBsp, EventInfrastructure)
- **hal_tiva/bringup/**: Startup glue (`HardwareInitialization()`, weak `Default_Handler_Forwarded()`) — generic runtime (atomics shim, syscall stubs, `abort`/`__assert_func`) comes from EMIL's `hal.cortex_m.runtime`
- **InterruptTable/InterruptHandler/DataWatchpointAndTrace/EventDispatcher**: `hal::cortex::*` from EMIL (`embedded-infra-lib`), not this repo
- **tiva/CMSIS/Device/TI/**: CMSIS device headers, register structs, startup vector tables, linker scripts
- **integration_test/**: Host-side integration tests (GoogleTest)
- **examples/**: Reference applications (blink, terminal, FreeRTOS)
- **demo/**: Board-specific firmware per board or board + BoosterPack (EK-TM4C123GXL, EK-TM4C1294XL, BOOSTXL-K350QVG-S1, BOOST-DRV8711)
- **doc/**: Board-specific documentation

## Critical Constraints

### Memory Management

- **NO HEAP**: Avoid `new`, `delete`, `malloc`, `free`, `std::make_unique`, `std::make_shared`
- **NO DYNAMIC CONTAINERS**: Use `infra::BoundedVector`, `infra::BoundedString`, `infra::BoundedDeque` instead of `std::vector`, `std::string`, `std::deque`
- **STATIC ALLOCATION**: All memory must be allocated at compile-time or on the stack
- **AVOID RECURSION**: Stack usage must be predictable and minimal

### Performance Requirements

- **REAL-TIME CONSTRAINTS**: Code must execute deterministically within strict timing requirements
- **AVOID VIRTUAL CALLS IN ISR**: Virtual function calls add overhead; avoid in interrupt service routines and hot paths
- **INLINE CRITICAL CODE**: Use `inline` for small, frequently-called functions
- **CONST CORRECTNESS**: Mark all non-mutating methods as `const`
- **PREFER CONSTEXPR**: Use `constexpr` for compile-time calculations
- **USE FIXED-SIZE TYPES**: Prefer `uint8_t`, `int32_t`, etc., over `int` for predictable sizing

### ISR Safety

- **NEVER allocate, lock, or block inside an ISR**
- **Use `infra::QueueForOneReaderOneIrqWriter<T>` for ISR-to-main data transfer** — it is the only lock-free queue safe for this pattern. The type `T` **must be `std::is_trivial`**; types containing `BoundedVector`, `BoundedString`, or any user-declared special members are NOT trivial. Use plain POD structs with fixed-size arrays instead.
- **`infra::BoundedDeque` is NOT ISR-safe** — reading and writing from different contexts (main + ISR) is a data race. Only use it when both reader and writer execute in the same context.
- **Keep ISR handlers minimal**: read registers, enqueue data, clear interrupt flags, return.
- **Mark shared flags `volatile`** when accessed by both ISR and main thread without atomics.

## Peripheral Driver Patterns

### Constructor / Destructor Lifecycle

Every peripheral driver follows this initialization order:

**Constructor:**
1. Save parameters (index, config) in the initializer list
2. `PeripheralPin` members constructed in the **initializer list** — RAII GPIO multiplexing completes before the constructor body runs
3. `EnableClock()` — **first call in the constructor body**: set `SYSCTL->RCGCxxx` bit, then poll `SYSCTL->PRxxx` until the peripheral-ready bit is set (e.g., `while ((SYSCTL->PRCAN & (1 << index)) == 0) {}`)
4. Configure hardware registers (baud rate, mode, control bits) and enable the peripheral
5. `NVIC_ClearPendingIRQ(irq)`, then register the EMIL handler **last** (e.g., `handler.emplace(irq, priority, [this]() { HandleInterrupt(); })` on a `std::optional<hal::cortex::ImmediateInterruptHandler>` member, or `Register(irq, priority)` when deriving from `hal::cortex::InterruptHandler`). Registration sets the priority and enables the IRQ — never call `NVIC_EnableIRQ` directly

**Destructor (reverse order):**
1. Release the EMIL handler **first** (`handler.reset()` / `Unregister()`), which disables the IRQ in the NVIC and removes it from the `InterruptTable`. Disabling the clock first would leave the IRQ enabled for a peripheral whose registers are unpowered — any pending interrupt would fault.
2. Disable the peripheral, then `DisableClock()` — clear SYSCTL `RCGCxxx` bit
3. `~PeripheralPin` objects auto-restore GPIO configuration

### Clock Gating

```cpp
void EnableClock() const
{
    SYSCTL->RCGCxxx |= (1 << peripheralIndex);
    while ((SYSCTL->PRxxx & (1 << peripheralIndex)) == 0)
    {
        // Wait until peripheral is ready
    }
}

void DisableClock() const
{
    SYSCTL->RCGCxxx &= ~(1 << peripheralIndex);
}
```

Always poll the peripheral-ready bit after enabling the clock gate. Use the corresponding `SYSCTL->PRxxx` register (e.g., `PRCAN`, `PRUART`, `PRSSI`) and wait until the bit for the peripheral index is set — do not rely on a fixed NOP delay.

### Interrupt Handling

**Architecture:**
1. Every vector not bound to a named handler in the startup files points to `Default_Handler`, which calls `Default_Handler_Forwarded()` (`hal_tiva/bringup/Bringup.cpp`)
2. `Default_Handler_Forwarded()` calls `hal::cortex::InterruptTable::Instance().Invoke(hal::cortex::ActiveInterrupt())`; the few named `extern "C"` handlers (e.g., `Can0_Handler`, `Adc0Sequence0_Handler`) call `hal::cortex::InterruptTable::Instance().Invoke(IRQn)` directly
3. `InterruptTable` routes to the registered `hal::cortex::InterruptHandler` (`ImmediateInterruptHandler` runs the callback in ISR context, `DispatchedInterruptHandler` defers it to the event dispatcher)
4. Peripheral drivers hold a `std::optional<hal::cortex::ImmediateInterruptHandler>` (or `DispatchedInterruptHandler`) member, or derive from `hal::cortex::InterruptHandler` and call `Register()`

**Adding a new interrupt handler:**
1. No vector-table entry or weak alias is required — `Default_Handler` already forwards every IRQ to the `InterruptTable`
2. Register an EMIL handler for the `IRQn` from the CMSIS device header (see lifecycle above)
3. The table built in `HardwareInitialization()` is `hal::cortex::InterruptTable::WithStorage<155>` (indexed by IRQn + 16); an IRQ outside it, or an IRQ that fires with no registered handler, hits `really_assert`

**NVIC management:**
- Always call `NVIC_ClearPendingIRQ(irq)` before registering the handler to prevent stale interrupts from firing immediately upon enable
- Pass the priority to the handler constructor / `Register(irq, priority)` — registration sets it before enabling the IRQ

### GPIO and Pin Configuration

- `GpioPin` — Represents a physical GPIO pin with port, number, drive mode
- `PeripheralPin` — RAII wrapper that configures GPIO for peripheral alternate function on construction and restores on destruction
- `AnalogPin` — Wraps GpioPin for ADC use; provides `AdcChannel()` method
- Pin lookup tables (`pinoutTableTm4c123`, `pinoutTableTm4c129`) map `PinConfigPeripheral` enums to hardware multiplexing values

### WithStorage Pattern

Use `infra::WithStorage<Base, StorageType>` to inject compile-time-sized storage:

```cpp
template<std::size_t N>
using WithMaxRxBuffer = infra::WithStorage<Can, std::array<CanRxEntry, N + 1>>;
```

The `+1` is required by `QueueForOneReaderOneIrqWriter` which uses one slot as a sentinel. The default constructor of `WithStorage` passes the storage reference to the base class constructor automatically.

### CAN Bus Specifics (Bosch C_CAN)

- **Bit timing**: When computing prescaler and time quanta, verify **exact division** (`bitClocks % prescaler == 0`) — inexact division silently produces the wrong baud rate
- **Manual `BitTiming`**: Validate all fields with `really_assert` — zero values in `phaseSegment1`, `phaseSegment2`, `synchronizationJumpWidth`, or `baudratePrescaler` cause division-by-zero or silent malfunction
- **Status register (`CAN_STS`)**: After reading, write back with `TXOK` and `RXOK` cleared to 0 and `LEC` set to 7 (the "no change" value, so the next error is detected). Failure to acknowledge them causes repeated spurious interrupts
- **Message objects**: TM4C CAN uses message objects 1–32. Assign fixed objects per direction (e.g., TX=1, RX=2) to avoid conflicts
- **RX data path**: Read arbitration/data registers in ISR into a trivial POD struct, enqueue via `QueueForOneReaderOneIrqWriter`, reconstruct high-level types (`Id`, `Message`) in the main-thread callback

### Register Access

- Use CMSIS-style volatile struct pointers (`CAN0`, `GPIOA`, `UART0`) from device headers
- Define register bit constants as `constexpr uint32_t` in anonymous namespaces
- Access: direct bit manipulation (`|=`, `&= ~`, shifts, masks) on volatile registers
- Include device header via macro: `#include DEVICE_HEADER` (resolves to `"TIVA.h"`, set by CMake)

## Namespace Conventions

- `hal::tiva` — All TM4C-specific drivers and types
- `hal` — Cross-platform EMIL interfaces (e.g., `SerialCommunication`, `SpiMaster`, `TimeKeeper`)
- `hal::cortex` — Cortex-M core services from EMIL (InterruptTable, InterruptHandler, SystemTick, EventDispatcher, Reset)
- `instantiations` — Board support packages and ready-to-use application stacks

## Build System

### CMake Targets

- `hal_tiva.tiva` — Peripheral drivers
- `hal.cortex_m` — Cortex-M core (from EMIL)
- `hal_tiva.synchronous_tiva` — Blocking drivers
- `hal_tiva.instantiations` — BSP
- `hal_tiva.bringup` — Startup (linked as object files, not static library)
- `ti.hal_driver` — CMSIS device headers and linker scripts

### MCU Family Conditionals

CMake uses generator expressions for MCU-specific sources:
```cmake
$<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C123>:ClockTm4c123.cpp>
$<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>:ClockTm4c129.cpp>
```

Family-specific constants, types and small register-access helpers live in `hal_tiva/tiva/family/<family>/<Driver>Family.hpp` (BSP: `hal_tiva/instantiations/family/<family>/LaunchPadFamily.hpp`) — one file per family with the same API, included as `#include "<Driver>Family.hpp"`. CMake puts only `family/<family>` (`tm4c123` or `tm4c129`) on the include path, so drivers contain no family `#ifdef`s.

Compile definitions: `TM4C123` or `TM4C129`, plus device variant (e.g., `TM4C123GH6PM`).

### Build Commands

hal-ti cannot be built standalone; it must be part of a larger project (e.g., e-foc):
```bash
cmake --preset host
cmake --build --preset host-Debug
ctest --preset host
```

## Testing

- Unit tests run on host using GoogleTest
- Test target pattern: `add_executable` + `emil_build_for(... HOST All BOOL HAL_TI_BUILD_TESTS)` + `emil_add_test`
- Link against `gmock_main`
- Prefer small, deterministic tests that do not require hardware
- For platform-specific tests, provide host stubs/mocks

## Startup Vector Tables

Both `startup_TM4C123.c` and `startup_TM4C129.c` define the Cortex-M vector table. Most slots point to `Default_Handler`; a few point to weak aliases of `Default_Handler` (e.g., `Can0_Handler`, `Uart0_Handler`) that a driver may override with a strong `extern "C"` symbol.

`Default_Handler` is not an infinite loop: it calls the weak `Default_Handler_Forwarded()` (`hal_tiva/bringup/Bringup.cpp`), which invokes `hal::cortex::InterruptTable::Instance().Invoke(hal::cortex::ActiveInterrupt())`. A new driver needs no startup-file change — registering an EMIL interrupt handler is sufficient. If you add a named handler, keep **both** family startup files in sync.

## Common Pitfalls

1. **Unregistered or out-of-range IRQ** — An IRQ that fires with no registered EMIL handler, or whose IRQn + 16 is not below the `InterruptTable` size (155 in `Bringup.cpp`), hits `really_assert`
2. **Clock disabled before the handler is released** — Destructor must release the EMIL handler (which disables the IRQ) before clearing the clock gate, or a pending interrupt faults on unpowered registers
3. **Non-trivial types in ISR queue** — `QueueForOneReaderOneIrqWriter` requires `std::is_trivial<T>`; use POD structs with fixed-size arrays, not `BoundedVector` members
4. **Inexact bit timing division** — CAN prescaler must divide bitClocks exactly; remainder produces wrong baud rate silently
5. **Stale pending interrupts** — Always `NVIC_ClearPendingIRQ` before registering the EMIL handler (registration enables the IRQ)
6. **infra::Function capture size** — Default capacity is `2 * sizeof(void*)` (8 bytes on ARM). Capturing `[this]` (1 pointer) fits; capturing more may exceed capacity silently
7. **Missing peripheral-ready poll after clock enable** — After `SYSCTL->RCGCxxx |= bit`, poll `SYSCTL->PRxxx` until the ready bit for that peripheral index is set before accessing any peripheral registers; the hardware does not guarantee immediate availability
