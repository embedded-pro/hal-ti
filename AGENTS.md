# hal-ti — Agent Rules

<!-- shared:begin 0.4.0 sha256:9aee198041110762 -->
Canonical rules for Claude, Copilot and sub-agents. Sections marked *(shared)* come from embedded-pro/embedded-agent-kit:
edit them there, not here. *(repo)* sections belong to this repository. *Exceptions* overrides a shared rule.

## Behaviour *(shared)*

- **Be as terse as possible.** No preamble, summary or narration. Report file paths and pass/fail.
- Everything you write — code, docs, agent files, PR text — uses the fewest words that stay unambiguous.
- Prose (docs, comments, agent files, commit and PR text) follows ASD-STE100 Simplified Technical English:
  one word, one meaning; active voice; one instruction per sentence; sentences ≤ 20 words (procedures) or ≤ 25 (descriptions).
- Ambiguous request: ask. Clear request: act.
- Do only what was asked. No unrelated refactors, no extra features.
- Batch reads, don't re-read, prefer targeted edits.
- Never change a shared section. Add a deviation to *Exceptions* only when the user approves it.

## Workflow *(shared)*

1. Multi-file or architectural change: update the design document first. Get approval before you write code.
2. Read *Domain* and *Build*. Read each `.github/instructions/*.instructions.md` whose `applyTo` matches a file you change.
3. Find the nearest existing pattern. Follow it.
4. Per behaviour: write a failing test, write the minimum code that passes it, refactor with all tests green.
5. Add each new file to its `CMakeLists.txt`.
6. Update the documentation for each behavioural change.
7. Run the *Build* commands until build and tests pass. Never disable, skip or weaken a test to get green.
8. Non-trivial change: run the `reviewer` agent, or ask the user to run it. Fix each CRITICAL and WARNING finding.
9. Report changed files, build pass/fail, test pass/fail.

## Memory *(shared)*

Scope: production code. Tests and host tools may use the heap.

- Forbidden: `new`/`delete`/`malloc`/`free`, `make_unique`/`make_shared`,
  `std::vector`/`string`/`deque`/`list`/`map`/`set`, recursion.
- Use: `infra::BoundedVector<T>::WithMaxSize<N>`, `infra::BoundedString::WithStorage<N>`,
  `infra::BoundedDeque`, `infra::BoundedList`/`IntrusiveList`, `std::array`, `std::optional`, `std::variant`.

## Interfaces & errors *(shared)*

- No virtual destructors: each adds a deleting destructor and thunks to flash.
  Interface = pure virtual methods + `protected: ~I() = default;` (non-virtual).
- Public `virtual ~Base() = default;` only on a base destroyed through a base pointer:
  `delete`/`std::unique_ptr<Base>`, `infra::PolymorphicVariant<Base, ...>`, `infra::OptionalForPolymorphicObjects<Base, ...>`.
  Put it on that base, not on every interface.
- Not needed for `std::variant`, `std::optional<Concrete>`, `infra::SharedPtr`: they destroy the concrete type.
- No exceptions. `std::optional`, `bool` or status enum; `really_assert` for preconditions. Never swallow an error.
- Constructor injection; no global state.

## Optimisation *(shared)*

- No `#pragma GCC optimize`, no `optimize` attribute: GCC won't inline across differing options.
  The consumer sets flags per translation unit.
- `OPTIMIZE_FOR_SPEED` only on hot functions defined in a header.
- Hot paths: no heap, no virtual dispatch, no blocking.

## Style *(shared)*

- `.clang-format` is authoritative. Allman, 4 spaces, `{}` init.
- PascalCase types/methods; camelCase members and enum values; lowercase namespaces.
  No `I` prefix; implementations end in `Impl`.
- `#pragma once` only.
- Fixed-width integers. `const`/`constexpr`-correct. Functions ≤ 30 lines (hard limit 50).
- Comments: non-obvious *why* only, one line. No `TODO`/`FIXME`, commented-out code, or restating the code.
  Fix or delete a comment the change makes stale.

## Testing *(shared)*

- `TEST_F` for fixtures, `TYPED_TEST` for types, plain `TEST()` for stateless checks.
- `StrictMock` only. `EXPECT_NEAR` with explicit tolerance. Fixture in an anonymous namespace, macros outside.
- Heap allowed in tests.

## Documentation *(shared)*

- Update docs before or with any behavioural change.
- One fact, one place: link, never restate.
- Visuals: Mermaid or ASCII.
<!-- shared:end -->

## Domain *(repo)*

- HAL for TI Tiva C Cortex-M MCUs (TM4C123, TM4C129). Implements the embedded-infra-lib (EMIL) HAL interfaces
  for real-time motor control. *Memory* applies to all driver code.
- `hal_tiva/tiva/` — peripheral drivers, namespace `hal::tiva`. `hal_tiva/synchronous_tiva/` — blocking variants.
  `hal_tiva/instantiations/` — board support (`LaunchPadBsp`, `EventInfrastructure`), namespace `instantiations`.
- `hal_tiva/bringup/` — `HardwareInitialization()` and the weak `Default_Handler_Forwarded()`.
- `tiva/CMSIS/Device/TI/` — device headers, startup vector tables, linker scripts.
- `integration_test/` — host GoogleTest (logic only). `examples/` — reference apps. `demo/` — one folder per board;
  each builds only for its own MCU. `doc/` — board documents.
- EMIL supplies `hal::cortex::*` (InterruptTable, InterruptHandler, SystemTick, DWT, Reset, EventDispatcher) and the
  runtime (`hal.cortex_m.runtime`). Do not add local copies.
- EMIL comes through `FetchContent`, pinned by `GIT_TAG`; `update-emil-git-tag.yml` bumps it.
- `hal_ti_target_bringup()` must list `$<TARGET_OBJECTS:hal.cortex_m.runtime>`: object files do not propagate
  through `hal.cortex_m`. A miss fails only at the final ARM link.

### Peripheral lifecycle

- `PeripheralPin` members: construct them in the initializer list.
- Constructor body, in this order:
  1. `EnableClock()`: set `SYSCTL->RCGCxxx`, then poll `SYSCTL->PRxxx` until ready. Never a NOP delay.
  2. Configure the registers.
  3. `NVIC_ClearPendingIRQ(irq)`, then register the EMIL handler last.
- Destructor, in this order:
  1. Release the EMIL handler (`reset()`/`Unregister()`). This disables the IRQ.
  2. Disable the peripheral, then `DisableClock()`. Reverse order faults a pending IRQ on unpowered registers.
- Handler: a `std::optional<hal::cortex::ImmediateInterruptHandler>` (ISR context) or
  `DispatchedInterruptHandler` (main) member, or derive from `hal::cortex::InterruptHandler`.
  Registration enables the IRQ: never call `NVIC_EnableIRQ`.

### Interrupts

- `Default_Handler` (both startup files) calls `Default_Handler_Forwarded()`, which invokes the EMIL
  `InterruptTable`. A new ISR needs no vector-table change. Keep a new named handler in both startup files.
- The table is `InterruptTable::WithStorage<155>` (IRQn + 16) in `Bringup.cpp`. An IRQ outside it, or one with no
  handler, hits `really_assert`.
- In an ISR: no allocation, blocking or locks. Clear the status bits before return.
- ISR-to-main data: `infra::QueueForOneReaderOneIrqWriter<T>` only; `T` must be `std::is_trivial` (POD, fixed
  arrays). Its storage needs `N + 1` slots. `infra::BoundedDeque` is not ISR-safe.
- Flags shared with an ISR: `volatile` or `std::atomic`.
- `infra::Function` holds `2 * sizeof(void*)`: capture `[this]` only.
- GPIO pin interrupts share a pool of `HAL_TI_GPIO_MAX_INTERRUPT_PINS` slots (default 8); one more hits
  `really_assert`. `DisableInterrupt` frees a slot.

### Registers and families

- Registers through CMSIS struct pointers (`UART0->CTL`); `#include DEVICE_HEADER`.
- Bit constants: `constexpr uint32_t` in an anonymous namespace. No magic numbers.
- No `#ifdef TM4C123`/`TM4C129` in C++. Family code goes in `hal_tiva/tiva/family/<family>/<Driver>Family.hpp`
  (same API per family); CMake adds only `family/<family>` to the include path and selects sources with generator
  expressions.
- CAN: the prescaler must divide the bit clocks exactly. `really_assert` each manual `BitTiming` field is non-zero.
  After reading `CAN_STS`, write it back with `TXOK`/`RXOK` = 0 and `LEC` = 7.
- New driver or board: update `doc/` with pinout, setup and limitations.

## Build *(repo)*

```bash
cmake --preset host && cmake --build --preset host-Debug
ctest --preset host
cmake --preset tm4c123gh6pm && cmake --build --preset tm4c123gh6pm-Debug
cmake --preset tm4c1294ncpdt && cmake --build --preset tm4c1294ncpdt-Debug
```

Options: `HAL_TI_BUILD_TESTS`, `HAL_TI_INCLUDE_BRINGUP`, `HAL_TI_BUILD_EXAMPLES`, `HAL_TI_BUILD_EXAMPLES_FREERTOS`,
`HAL_TI_INCLUDE_LWIP` (TM4C129 only), `HAL_TI_GPIO_MAX_INTERRUPT_PINS`. Tests: `emil_build_for(... HOST ...)` +
`emil_add_test`.

## Exceptions *(repo)*

- *Workflow* step 4: a register-sequence change that only hardware can verify needs no new unit test. There is no
  on-target test suite; verify on a LaunchPad with a logic analyser.
