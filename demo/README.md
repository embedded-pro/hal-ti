# Demos

Board-specific firmware, one folder per board or board plus BoosterPack. Each demo builds only for its own MCU, so the configure preset decides which ones exist. Trace output is on UART0 (PA1 TX, 115200 8N1), which the on-board ICDI exposes as a virtual COM port.

| Folder                             | Hardware                           | Preset          | Target                                     |
|------------------------------------|------------------------------------|-----------------|--------------------------------------------|
| `ek_tm4c123xgl`                    | EK-TM4C123GXL                      | `tm4c123gh6pm`  | `demo_ti.ek_tm4c123xgl`                    |
| `ek_tm4c123xgl_boostxl_k350qvg_s1` | EK-TM4C123GXL + BOOSTXL-K350QVG-S1 | `tm4c123gh6pm`  | `demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1` |
| `ek_tm4c1294xl`                    | EK-TM4C1294XL                      | `tm4c1294ncpdt` | `demo_ti.ek_tm4c1294xl`                    |
| `tm4c1294xl_boost_drv8711`         | EK-TM4C1294XL + BOOST-DRV8711      | `tm4c1294ncpdt` | `demo_ti.tm4c1294xl_boost_drv8711`         |

Build one (the demos are part of `HAL_TI_BUILD_EXAMPLES`, which the presets enable):

```bash
cmake --preset tm4c123gh6pm
cmake --build --preset tm4c123gh6pm-RelWithDebInfo --target demo_ti.ek_tm4c123xgl
```

The `.elf`, `.bin` and `.hex` are written to `build/<preset>/demo/<folder>/`. See [EK-TM4C123GXL](../doc/EK-TM4C123GXL.md) and [EK-TM4C1294XL](../doc/EK-TM4C1294XL.md) for the boards.

The demos use the same board support as `examples/` (`LaunchPad`, `LaunchPadTerminalAndTracer`) and EMIL's `boards.boostxl_k350qvg_s1` and `drivers.motor_controller`. They have only been built, not run on hardware.
