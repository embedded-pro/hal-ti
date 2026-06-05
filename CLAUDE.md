# hal-ti

Hardware Abstraction Layer for TI ARM Cortex-M microcontrollers (TM4C123 and TM4C129 families), implementing `embedded-infra-lib` HAL interfaces over TI Tiva C peripherals for real-time motor control and similar embedded applications.

## Detailed Guidelines

- Coding rules (applies to all `*.hpp`/`*.cpp`): [.github/instructions/hal-ti-cpp.instructions.md](.github/instructions/hal-ti-cpp.instructions.md)
- Full reference with patterns and common pitfalls: [.github/copilot-instructions.md](.github/copilot-instructions.md)

## Repository Structure

| Path                         | Contents                                                                    |
|------------------------------|-----------------------------------------------------------------------------|
| `hal_tiva/cortex/`           | ARM Cortex-M core (InterruptTable, SystemTick, EventDispatcher, Reset, DWT) |
| `hal_tiva/tiva/`             | TM4C peripheral drivers (Gpio, Uart, Can, Adc, SpiMaster, Dma, Clock)       |
| `hal_tiva/synchronous_tiva/` | Blocking/polling driver variants                                            |
| `hal_tiva/instantiations/`   | Board Support Packages (LaunchPadBsp, EventInfrastructure)                  |
| `hal_tiva/default_init/`     | Startup code, atomics shim, hardware init hooks                             |
| `tiva/CMSIS/Device/TI/`      | CMSIS device headers, startup vector tables, linker scripts                 |
| `integration_test/`          | Host-side integration tests (GoogleTest)                                    |
| `examples/`                  | Reference applications (blink, terminal, FreeRTOS)                          |
| `doc/`                       | Board-specific documentation                                                |

## Critical Constraints

### No Heap Allocation

Never use `new`, `delete`, `malloc`, `free`, `std::make_unique`, `std::make_shared`, `std::vector`, `std::string`, `std::deque`, or `std::list`. Use `infra::BoundedVector`, `infra::BoundedString`, `std::array`, and `infra::WithStorage`. No recursion — stack usage must be predictable.

### ISR Safety

- Never allocate, block, or lock a mutex inside an ISR
- ISR-to-main transfer: `QueueForOneReaderOneIrqWriter<T>` only — `T` must be `std::is_trivial` (plain POD struct with fixed-size array members; no `BoundedVector`, no user-declared constructors)
- `infra::BoundedDeque` is NOT ISR-safe across the ISR/main boundary
- Shared flags accessed from both ISR and main must be `volatile` (or `std::atomic`)
- Always `NVIC_ClearPendingIRQ` before `NVIC_EnableIRQ`
- Clear all interrupt status bits before returning from an ISR

### Peripheral Lifecycle (Order is Mandatory)

**Constructor body order:**
1. `EnableClock()` first — sets `SYSCTL->RCGCxxx |= bit`, then polls `SYSCTL->PRxxx` until the peripheral-ready bit is set. Do NOT use `__asm("nop")` delays.
2. Configure peripheral registers
3. `NVIC_ClearPendingIRQ` + `NVIC_EnableIRQ` last

`PeripheralPin` members are initialized in the C++ initializer list, before the constructor body runs.

**Destructor body order:**
1. `NVIC_DisableIRQ` first — always before clock disable
2. `DisableClock()` — clears `SYSCTL->RCGCxxx &= ~bit`
3. `PeripheralPin` / `ImmediateInterruptHandler` destructors run automatically

### Vector Table — Both Startup Files

Every new ISR handler requires all three steps:
1. `extern "C" void HandlerName()` in driver `.cpp` inside an anonymous namespace, calling `InterruptTable::Instance().Invoke(IRQn)`
2. Weak alias + vector table entry in `tiva/CMSIS/.../startup_TM4C123.c`
3. Weak alias + vector table entry in `tiva/CMSIS/.../startup_TM4C129.c`

Missing any step → interrupt silently falls through to `Default_Handler` (infinite loop) on hardware.

## Naming and Style

- Classes/Methods: `PascalCase`; member variables: `camelCase`
- Namespaces: `hal::tiva`, `hal::cortex`, `instantiations`
- Register bit constants: `constexpr uint32_t` in anonymous namespace
- Allman braces (opening brace on new line), 4-space indent, functions ≤ 50 lines
- `const` on all non-mutating methods; `constexpr` for compile-time constants
- Use fixed-size types: `uint8_t`, `uint32_t`, etc.

## MCU Family Conditionals

Use CMake generator expressions for family-specific sources — never `#ifdef` in C++:
```cmake
$<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C123>:ClockTm4c123.cpp>
$<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>:ClockTm4c129.cpp>
```

## Build Commands

hal-ti cannot be built standalone; it is consumed as a dependency by a larger project (e.g., e-foc):
```bash
cmake --preset host
cmake --build --preset host-Debug
ctest --preset host-Debug
```

## Known Code Inconsistencies

- `.github/agents/executor.agent.md` previously used a 3-NOP delay (`__asm("nop")`) after `SYSCTL->RCGCxxx |=`. Corrected to `SYSCTL->PRxxx` polling — consistent with the instructions file, planner/reviewer agents, and the actual driver implementations (`UartBase.cpp`, `Can.cpp`).
- `.github/agents/reviewer.agent.md` previously had `model: "GPT-5.4"`. Corrected to `claude-sonnet-4-6`.
- `.github/agents/planner.agent.md` previously had `model: "Claude Opus 4.6"` (non-existent model ID). Corrected to `claude-opus-4-8`.
