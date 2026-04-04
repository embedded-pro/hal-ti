# GitHub Copilot Instructions for hal-ti

## Project Overview

This is a Hardware Abstraction Layer (HAL) for TI ARM Cortex-M based microcontrollers (TM4C123 and TM4C129 families). It implements the `embedded-infra-lib` HAL interfaces over TI Tiva C peripherals, providing event-driven drivers for GPIO, UART, CAN, ADC, SPI, DMA, and more. The library is designed for strict realtime and memory constraints in BLDC/PMSM motor control and similar embedded applications.

## Repository Structure

- **hal_tiva/cortex/**: ARM Cortex-M core abstractions (InterruptTable, SystemTick, EventDispatcher, Reset, DWT)
- **hal_tiva/tiva/**: TM4C-specific peripheral drivers (Gpio, Uart, Can, Adc, SpiMaster, Dma, Clock)
- **hal_tiva/synchronous_tiva/**: Blocking/polling driver variants (SynchronousAdc, SynchronousUart)
- **hal_tiva/instantiations/**: Board Support Packages and infrastructure (LaunchPadBsp, EventInfrastructure)
- **hal_tiva/default_init/**: Startup code, atomic operations shim, hardware initialization hooks
- **tiva/CMSIS/Device/TI/**: CMSIS device headers, register structs, startup vector tables, linker scripts
- **integration_test/**: Host-side integration tests (GoogleTest)
- **examples/**: Reference applications (blink, terminal, FreeRTOS)
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
1. Save parameters (index, config) and register base class (`ImmediateInterruptHandler`) — all in the initializer list
2. `PeripheralPin` members constructed in the **initializer list** — RAII GPIO multiplexing completes before the constructor body runs
3. `EnableClock()` — **first call in the constructor body**: set `SYSCTL->RCGCxxx` bit, then poll `SYSCTL->PRxxx` until the peripheral-ready bit is set (e.g., `while ((SYSCTL->PRCAN & (1 << index)) == 0) {}`)
4. Configure hardware registers (baud rate, mode, control bits)
5. NVIC: `NVIC_ClearPendingIRQ` then `NVIC_EnableIRQ` (if not handled by the `ImmediateInterruptHandler` base)
6. Enable peripheral in control register

**Destructor (reverse order):**
1. Disable NVIC interrupt (`NVIC_DisableIRQ`) **before** disabling the peripheral clock. Disabling the clock first leaves the NVIC enabled for a peripheral whose registers are unpowered — any pending interrupt would fault.
2. `DisableClock()` — clear SYSCTL `RCGCxxx` bit
3. `~PeripheralPin` objects auto-restore GPIO configuration
4. `~ImmediateInterruptHandler` auto-unregisters from InterruptTable

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
1. A top-level `extern "C"` handler is defined per IRQ (e.g., `Can0_Handler`, `Adc0Sequence0_Handler`)
2. The handler calls `hal::InterruptTable::Instance().Invoke(IRQn)` to dispatch
3. `InterruptTable` routes to the registered `InterruptHandler` or `ImmediateInterruptHandler`
4. Peripheral driver inherits `ImmediateInterruptHandler` privately and processes in ISR context

**Adding a new interrupt handler:**
1. Define `extern "C" void NewPeripheral_Handler()` in the `.cpp` file inside an anonymous namespace
2. Add a weak alias declaration in **both** startup files (`startup_TM4C123.c` and `startup_TM4C129.c`):
   ```c
   void NewPeripheral_Handler() __attribute__((weak, alias("Default_Handler")));
   ```
3. Replace the corresponding `Default_Handler` entry in the vector table with `NewPeripheral_Handler`
4. If the weak alias or vector table entry is missing, the handler will **never execute** on hardware — interrupts silently go to `Default_Handler` (infinite loop)

**NVIC management:**
- Always call `NVIC_ClearPendingIRQ(irq)` before `NVIC_EnableIRQ(irq)` to prevent stale interrupts from firing immediately upon enable
- Set priority via `NVIC_SetPriority(irq, priority)` before enabling

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
- **Status register (`CAN_STS`)**: After reading, write back with `TXOK`, `RXOK`, and `LEC` bits cleared to acknowledge them. Failure to clear causes repeated spurious interrupts
- **Message objects**: TM4C CAN uses message objects 1–32. Assign fixed objects per direction (e.g., TX=1, RX=2) to avoid conflicts
- **RX data path**: Read arbitration/data registers in ISR into a trivial POD struct, enqueue via `QueueForOneReaderOneIrqWriter`, reconstruct high-level types (`Id`, `Message`) in the main-thread callback

### Register Access

- Use CMSIS-style volatile struct pointers (`CAN0`, `GPIOA`, `UART0`) from device headers
- Define register bit constants as `constexpr uint32_t` in anonymous namespaces
- Access: direct bit manipulation (`|=`, `&= ~`, shifts, masks) on volatile registers
- Include device header via macro: `#include DEVICE_HEADER` (resolves to `"TIVA.h"`, set by CMake)

## Namespace Conventions

- `hal::tiva` — All TM4C-specific drivers and types
- `hal` — Cross-platform abstractions (InterruptHandler, InterruptTable, SystemTick, interfaces)
- `hal::cortex` — Cortex-M core services (EventDispatcher, Reset)
- `instantiations` — Board support packages and ready-to-use application stacks

## Build System

### CMake Targets

- `hal_tiva.tiva` — Peripheral drivers
- `hal_tiva.cortex` — Cortex-M core
- `hal_tiva.synchronous_tiva` — Blocking drivers
- `hal_tiva.instantiations` — BSP
- `hal_tiva.default_init` — Startup (linked as object files, not static library)
- `ti.hal_driver` — CMSIS device headers and linker scripts

### MCU Family Conditionals

CMake uses generator expressions for MCU-specific sources:
```cmake
$<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C123>:ClockTm4c123.cpp>
$<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>:ClockTm4c129.cpp>
```

Compile definitions: `TM4C123` or `TM4C129`, plus device variant (e.g., `TM4C123GH6PM`).

### Build Commands

hal-ti cannot be built standalone; it must be part of a larger project (e.g., e-foc):
```bash
cmake --preset host
cmake --build --preset host-Debug
ctest --preset host-Debug
```

## Testing

- Unit tests run on host using GoogleTest
- Test target pattern: `add_executable` + `emil_build_for(... HOST All BOOL HAL_TI_BUILD_TESTS)` + `emil_add_test`
- Link against `gmock_main`
- Prefer small, deterministic tests that do not require hardware
- For platform-specific tests, provide host stubs/mocks

## Startup Vector Tables

Both `startup_TM4C123.c` and `startup_TM4C129.c` define the Cortex-M vector table using the weak alias pattern:

1. Forward-declare each handler with `__attribute__((weak, alias("Default_Handler")))`
2. Place the handler name in the vector table array
3. When a driver defines the strong symbol (`extern "C" void Handler_Name()`), the linker overrides the weak default

**Critical**: If a handler is declared in the driver `.cpp` but not added to the startup vector table, it will **never** be called. The NVIC dispatches to whatever address is in the vector table — if that's `Default_Handler`, the interrupt enters an infinite loop. Always update **both** family startup files when adding a new peripheral interrupt.

## Common Pitfalls

1. **Missing vector table entry** — Handler defined in code but never called because startup file still has `Default_Handler`
2. **Clock disabled before NVIC** — Destructor must disable NVIC interrupt before clearing the clock gate, or a pending interrupt faults on unpowered registers
3. **Non-trivial types in ISR queue** — `QueueForOneReaderOneIrqWriter` requires `std::is_trivial<T>`; use POD structs with fixed-size arrays, not `BoundedVector` members
4. **Inexact bit timing division** — CAN prescaler must divide bitClocks exactly; remainder produces wrong baud rate silently
5. **Stale pending interrupts** — Always `NVIC_ClearPendingIRQ` before `NVIC_EnableIRQ`
6. **infra::Function capture size** — Default capacity is `2 * sizeof(void*)` (8 bytes on ARM). Capturing `[this]` (1 pointer) fits; capturing more may exceed capacity silently
7. **Missing peripheral-ready poll after clock enable** — After `SYSCTL->RCGCxxx |= bit`, poll `SYSCTL->PRxxx` until the ready bit for that peripheral index is set before accessing any peripheral registers; the hardware does not guarantee immediate availability
