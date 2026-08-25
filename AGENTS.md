# hal-ti — Agent Rules (canonical)

Single source of truth for **Claude, Copilot, and sub-agents**. `CLAUDE.md` points here. Detailed C++ coding rules: `.github/instructions/hal-ti-cpp.instructions.md` (binding for all `*.hpp/*.cpp` changes). Full pattern reference with common pitfalls: `.github/copilot-instructions.md`. Copilot custom agents: `.github/agents/`. Build presets: `CMakePresets.json`.

hal-ti is a Hardware Abstraction Layer for TI ARM Cortex-M microcontrollers (TM4C123 and TM4C129 families), implementing [embedded-infra-lib](https://github.com/embedded-pro/embedded-infra-lib) (EMIL) HAL interfaces over TI Tiva C peripherals, for strict realtime/memory-constrained applications (BLDC/PMSM motor control and similar).

## Architecture

- `hal_tiva/{Reset,SystemTick,SystemTickTimerService,TimeKeeper}.{hpp,cpp}` — generic ARM Cortex-M core services, kept local only until EMIL hosts family-agnostic equivalents under `hal/cortex_m/` (tracked upstream; `hal::cortex::InterruptTable`/`InterruptHandler`/`DataWatchpointAndTrace`/`EventDispatcher` already come from EMIL directly, not from this repo)
- `hal_tiva/tiva/` — TM4C peripheral drivers (Gpio, Uart, Can, Adc, SpiMaster, Pwm, Dma, Eeprom, Ethernet, AnalogComparator, Clock), namespace `hal::tiva`
- `hal_tiva/synchronous_tiva/` — Blocking/polling driver variants (`SynchronousUart`, `SynchronousQuadratureEncoder`, …)
- `hal_tiva/instantiations/` — Board support packages and event infrastructure (`LaunchPadBsp`, `EventInfrastructure`, `TracingReset`)
- `hal_tiva/bringup/` — Startup glue: `HardwareInitialization()` (constructs the interrupt table + default GPIO pinout) and the weak `Default_Handler_Forwarded()`. Generic runtime (atomics shim, `abort`/`__assert_func`, libc syscall stubs) comes from EMIL's `hal.cortex_m.runtime`, not from this repo.
- `tiva/CMSIS/Device/TI/` — CMSIS device headers, startup vector tables (`startup_TM4C123.c`, `startup_TM4C129.c`), linker scripts
- `integration_test/` — Host-side integration tests (GoogleTest)
- `examples/` — Reference applications (`blink`, `terminal_and_peripherals`, `terminal_uart_with_dma`, `freertos`)
- `doc/` — Board-specific documentation

## Memory — no heap

This is a driver library for constrained MCUs running realtime motor control. Forbidden everywhere: `new`/`delete`/`malloc`/`free`, `make_unique`/`make_shared`, `std::vector`/`string`/`deque`/`list`/`map`/`set`. No recursion in driver code — stack depth must be statically bounded.

Use: `infra::BoundedVector<T>`, `infra::BoundedString`, `infra::WithStorage<Base, StorageType>`, `std::array<T,N>`, `std::optional<T>`.

## ISR safety — critical

- Nothing inside an ISR allocates, blocks, or locks a mutex
- ISR-to-main data transfer: `infra::QueueForOneReaderOneIrqWriter<T>` only — `T` must satisfy `std::is_trivial` (plain POD struct with fixed-size array members; no `BoundedVector`, no user-declared constructors)
- `infra::BoundedDeque` is **not** ISR-safe across the ISR/main boundary
- Shared flags written in ISR and read in main must be `volatile` (or `std::atomic`)
- Always `NVIC_ClearPendingIRQ` before `NVIC_EnableIRQ`; clear interrupt status bits before returning from an ISR

## Peripheral driver conventions

Full detail lives in `.github/instructions/hal-ti-cpp.instructions.md` and `.github/copilot-instructions.md` — read them before touching driver code. Key points:

- Constructor body: `EnableClock()` first (`SYSCTL->RCGCxxx |= bit`, then poll `SYSCTL->PRxxx` until ready — never a fixed NOP delay), then register configuration, then `NVIC_ClearPendingIRQ` + `NVIC_EnableIRQ` last
- Destructor body (reverse order): `NVIC_DisableIRQ` before `DisableClock()`
- `PeripheralPin` members are constructed in the initializer list, before the constructor body runs
- Interrupt handlers: inherit `hal::cortex::ImmediateInterruptHandler` (single-vector, ISR-context processing) or `hal::cortex::DispatchedInterruptHandler` (deferred to main); never call `NVIC_EnableIRQ` directly — use `Register()`
- Vector table hygiene: every new ISR handler needs an `extern "C"` handler in the driver `.cpp`, a weak alias in **both** `startup_TM4C123.c` and `startup_TM4C129.c`, and the corresponding vector table slot updated in both files — missing any step means the interrupt silently falls through to `Default_Handler` on real hardware
- MCU family conditionals: use CMake generator expressions (`$<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C123>:...>`), never `#ifdef TM4C123`/`#ifdef TM4C129` in C++

## Style

- Allman braces, 4-space indent, `.clang-format` authoritative
- PascalCase types/methods, camelCase members/locals; `const`-correct on all observer/query methods; `constexpr` for compile-time constants
- Fixed-size types (`uint8_t`, `uint32_t`, …) over `int`
- **No comments** except non-obvious *why*. No `TODO`/`FIXME`/`HACK`, no commented-out code
- No C-style casts — `static_cast<>`; `reinterpret_cast<>` only where raw register/void-pointer access requires it

## Interfaces & errors

- Interfaces = pure virtual; `virtual ~I() = default` — never `= 0` destructors
- No exceptions. `std::optional<T>` or status enums for fallible operations
- No global mutable state — driver state lives in class members

## Testing

`integration_test/` runs GoogleTest on the host build (`HAL_TI_BUILD_TESTS`) — this is host-side interface/logic testing, not hardware-in-the-loop. There is no on-target test suite; hardware validation is manual (LaunchPad boards, logic analyser/scope). Don't add new unit tests for driver register-sequence changes that can only be verified on real hardware.

## Build

hal-ti cannot be built standalone as a deployable target; it's consumed as a dependency by a larger project (e.g. a motor-control application), but the host preset builds and tests it directly:

```bash
cmake --preset host
cmake --build --preset host-Debug
ctest --preset host
```

## Dependency: EMIL (embedded-infra-lib)

Pulled via `FetchContent` in the top-level `CMakeLists.txt`, pinned to a specific commit (`GIT_TAG`), auto-bumped by `.github/workflows/update-emil-git-tag.yml`. `hal::cortex::*` (InterruptTable, InterruptHandler, DataWatchpointAndTrace, FaultTracer) and the generic runtime (`hal.cortex_m.runtime`: atomics shim, syscall stubs, `abort`/`__assert_func`) come from EMIL, not from this repo — don't reintroduce local copies of these.

## Assistant behavior — be terse

- Minimal prose. No preamble/postamble, no restating the plan, no summaries unless asked
- Report results as file paths + build pass/fail
- Don't re-read files already read; batch reads; prefer targeted edits
