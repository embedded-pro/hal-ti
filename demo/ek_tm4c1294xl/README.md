# EK-TM4C1294XL demo

Board bring-up: LEDs, user switches and the trace UART of the EK-TM4C1294XL (TM4C1294NCPDT).

```bash
cmake --preset tm4c1294ncpdt
cmake --build --preset tm4c1294ncpdt-RelWithDebInfo --target demo_ti.ek_tm4c1294xl
```

| Part         | Pins                       | Behaviour                                                            |
|--------------|----------------------------|----------------------------------------------------------------------|
| LED D1       | PN1                        | heartbeat (`services::DebugLed`)                                     |
| LED D2       | PN0                        | SW1 turns it on, SW2 turns it off                                    |
| SW1, SW2     | PJ0, PJ1 (active low)      | `services::DebouncedButton`                                          |
| Trace UART0  | PA1 (TX), 115200 8N1       | banner with core clock and unique device ID, then one line per press |
