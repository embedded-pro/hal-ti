---
description: "Start an orchestrated plan-execute-review workflow for a hal-ti development task. Routes through planning (Claude Opus 4.6), implementation, and code review stages with handoff buttons between each step."
agent: "orchestrator"
argument-hint: "Describe the peripheral driver, ISR, board support, or HAL change you want to implement"
model: "Claude Sonnet 4.6"
---

Analyze the following task for the **hal-ti** project — a Hardware Abstraction Layer for TI ARM Cortex-M microcontrollers (TM4C123 and TM4C129). Gather relevant context from the codebase — identify the affected layer (`hal_tiva/tiva/`, `hal_tiva/synchronous_tiva/`, `hal_tiva/cortex/`, `hal_tiva/instantiations/`, `tiva/CMSIS/`), the MCU family (TM4C123 / TM4C129 / both), whether ISR handling is involved (vector table hygiene, ISR-safe data transfer), and any documentation requirements. Then provide a brief scope summary and use the handoff buttons to route to the appropriate specialist:

- **Plan Implementation**: For new peripheral drivers, new interrupt handlers, new BSP targets, or multi-file changes needing careful register-sequence design
- **Execute Directly**: For straightforward bug fixes, register corrections, or small changes with a clear path
- **Review Code**: For reviewing existing or recently changed code against project standards

Task to orchestrate:
