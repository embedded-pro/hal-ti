---
description: "hal-ti C++ coding rules: no heap allocation, ISR safety with QueueForOneReaderOneIrqWriter (trivial types only), TI Tiva C peripheral register sequences, ARM Cortex-M NVIC and EMIL interrupt registration, peripheral lifecycle order (handler released before clock disable), embedded-infra-lib patterns, Allman brace style, PascalCase naming, SOLID principles, const correctness."
applyTo: "**/*.{hpp,cpp}"
---

# hal-ti C++ Rules

This project is a Hardware Abstraction Layer for TI ARM Cortex-M microcontrollers (TM4C123 and TM4C129), implementing `embedded-infra-lib` HAL interfaces over TI Tiva C peripherals. Follow these rules strictly.

## Memory — No Heap Allocation

Never use `new`, `delete`, `malloc`, `free`, `std::make_unique`, or `std::make_shared`.

Replace standard containers:
- `std::vector<T>` → `infra::BoundedVector<T>::WithMaxSize<N>`
- `std::string` → `infra::BoundedString::WithStorage<N>`
- Use `std::array<T, N>` for fixed-size buffers
- Use `infra::WithStorage<Base, StorageType>` to inject compile-time storage into drivers
- No recursion — stack usage must be predictable

## ISR Safety — CRITICAL

- Nothing inside an ISR allocates, blocks, or acquires a mutex
- ISR-to-main data transfer: `QueueForOneReaderOneIrqWriter<T>` **only**
  - `T` must satisfy `std::is_trivial` — POD struct with fixed-size array members; no `BoundedVector`, no user-declared constructors
  - `infra::BoundedDeque` is **NOT** ISR-safe across ISR/main boundary
- Shared flags written in ISR and read in main must be `volatile` (or `std::atomic`)
- Clear all interrupt status bits before returning from ISR
- Always: `NVIC_ClearPendingIRQ(irq)` **before** registering the EMIL interrupt handler (registration enables the IRQ) — never call `NVIC_EnableIRQ` directly
- `WithStorage` buffers passed to `QueueForOneReaderOneIrqWriter` need `N + 1` capacity (one sentinel slot)

## Peripheral Lifecycle — ORDER IS MANDATORY

**Constructor** (this order only):
1. `PeripheralPin` members are **class member variables** initialized in the C++ initializer list — they are constructed before the constructor body runs, so before `EnableClock()` is called
2. `EnableClock()` — **first call in the constructor body**: `SYSCTL->RCGCxxx |= bit`, then poll `SYSCTL->PRxxx` until the peripheral-ready bit is set (e.g., `while ((SYSCTL->PRCAN & (1 << index)) == 0) {}`) — no `__asm("nop")` pattern
3. Peripheral register configuration
4. `NVIC_ClearPendingIRQ`, then register the EMIL handler **last** — `handler.emplace(irq, priority, callback)` on a `std::optional<hal::cortex::ImmediateInterruptHandler>` / `DispatchedInterruptHandler` member, or `Register(irq, priority)` when deriving from `hal::cortex::InterruptHandler`

**Destructor** (this order only):
1. Release the EMIL handler **first** — `handler.reset()` / `Unregister()` (disables the IRQ)
2. Disable the peripheral, then `DisableClock()` — `SYSCTL->RCGCxxx &= ~bit`
3. `PeripheralPin` destructors run automatically

## Interrupt Dispatch — EMIL InterruptTable

- No vector-table entry or weak alias is needed for a new ISR: `Default_Handler` in both startup files calls `Default_Handler_Forwarded()` (`hal_tiva/bringup/Bringup.cpp`) → `hal::cortex::InterruptTable::Instance().Invoke(hal::cortex::ActiveInterrupt())`
- Registering an EMIL handler for the `IRQn` is what makes the interrupt reach the driver
- The table is `InterruptTable::WithStorage<155>` (indexed by IRQn + 16) — an out-of-range IRQ, or one that fires with no registered handler, hits `really_assert`
- Existing named `extern "C"` handlers (`Can0_Handler`, …) only call `Invoke(IRQn)`; if you add one, keep `startup_TM4C123.c` and `startup_TM4C129.c` in sync

## Register Access

- Register bit constants: `constexpr uint32_t` in anonymous namespace — no magic numbers inline
- Access via CMSIS volatile struct pointers: `UART0->CTL`, `SYSCTL->RCGCUART`, etc.
- Include device header via `#include DEVICE_HEADER` macro (not a hardcoded path)

## Naming

- Classes/Methods: `PascalCase` (e.g., `SpiMaster`, `EnableClock()`)
- Member variables: `camelCase` (e.g., `peripheralIndex`, `receiveBuffer`)
- Namespaces: `hal::tiva`, `hal::cortex`, `instantiations`
- Register constants: `constexpr uint32_t` `PascalCase` in anonymous namespace

## Style

- Allman braces (opening brace on new line), 4-space indent
- Functions ~30 lines max (hard limit ~50)
- Extract `EnableClock()`, `DisableClock()`, `ConfigureRegisters()` as named helpers
- `const` on all non-mutating methods, `constexpr` for all compile-time constants
- Fixed-size types: `uint8_t`, `uint32_t`, etc.

## Design

- One class = one peripheral / one concern (SRP)
- Implements abstract `embedded-infra-lib` interface exactly — no extra virtual methods
- Hardware dependencies injected via constructor pins and callbacks
- Reuse `PeripheralPin`, `ImmediateInterruptHandler`, `InterruptTable` — no raw GPIO/NVIC duplication
- MCU-family splits via CMake generator expressions — not `#ifdef` in C++

## Documentation — MANDATORY

For every new peripheral driver or board target, update `doc/` with pinout, setup instructions, and known limitations. Update `copilot-instructions.md` Common Pitfalls if a new failure mode is discovered.

Full details: [copilot-instructions.md](../../.github/copilot-instructions.md)
