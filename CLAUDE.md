# hal-ti — Claude Instructions

Canonical rules: [AGENTS.md](AGENTS.md) (shared with Copilot and sub-agents). C++ coding detail: [.github/instructions/hal-ti-cpp.instructions.md](.github/instructions/hal-ti-cpp.instructions.md). Full pattern reference: [.github/copilot-instructions.md](.github/copilot-instructions.md). Copilot agents: [.github/agents/](.github/agents/). Build presets: [CMakePresets.json](CMakePresets.json).

Essentials (full detail in AGENTS.md):

- No heap — bounded containers / `std::array` / `std::optional`; no recursion in driver code. Applies repo-wide (this is an MCU HAL library for realtime motor control).
- TI Tiva C — `SYSCTL->RCGCxxx` clock-enable then poll `SYSCTL->PRxxx` (never a NOP delay), `NVIC_ClearPendingIRQ` before `NVIC_EnableIRQ`, `hal::cortex::ImmediateInterruptHandler`/`DispatchedInterruptHandler` (from EMIL, not local) for ISRs, both `startup_TM4C123.c` and `startup_TM4C129.c` updated for every new vector.
- EMIL (`embedded-infra-lib`) provides `hal::cortex::*` core services and the generic runtime shim — don't reintroduce local copies of what EMIL already provides.
- Style — Allman braces, 4-space, PascalCase types/methods, camelCase members. No comments except non-obvious why.
- Testing — `integration_test/` GoogleTest runs on the host build; no on-target test suite. Don't add unit tests for driver changes that only real hardware can verify.
- No exceptions — `std::optional`/status enums; interfaces `virtual ~I() = default`.
- Be terse — minimal prose; report file paths + build pass/fail.
