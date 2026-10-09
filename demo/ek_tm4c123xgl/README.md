# EK-TM4C123GXL demo

Board bring-up: LEDs, user switches and the trace UART of the EK-TM4C123GXL (TM4C123GH6PM).

```bash
cmake --preset tm4c123gh6pm
cmake --build --preset tm4c123gh6pm-RelWithDebInfo --target demo_ti.ek_tm4c123xgl
```

| Part         | Pins                       | Behaviour                                                            |
|--------------|----------------------------|----------------------------------------------------------------------|
| Green LED    | PF3                        | heartbeat (`services::DebugLed`)                                     |
| Red LED      | PF1                        | toggled by SW1                                                       |
| Blue LED     | PF2                        | toggled by SW2                                                       |
| SW1, SW2     | PF4, PF0 (active low)      | `services::DebouncedButton`; PF0 is unlocked by `hal::tiva::GpioPin` |
| Trace UART0  | PA1 (TX), 115200 8N1       | banner with core clock and unique device ID, then one line per press |
