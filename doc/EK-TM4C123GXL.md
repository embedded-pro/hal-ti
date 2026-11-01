# EK-TM4C123GXL

![alt text](ek-tm4c123gxl.png)

## Debugging with JLink SWD

1. Solder a strip header to DBG
2. Move switch to position DEVICE (Avoid conflicts to TI-ICDI)
3. Connect pin EXTDBG to ground (disable TI-ICDI)
4. Connect +3V3 to jtag pin 1
5. Connect GND to jtag pin 4
6. Connect TCK to jtag pin 9 (SWCLK)
7. Connect TMS to jtag pin 7 (SWDIO)
8. Connect RESET to jtag pin 15 (nRST)

## Internal flash

`hal::tiva::FlashInternal` (`hal::Flash`, completions through the event dispatcher) and `hal::tiva::SynchronousFlashInternal` (`hal::SynchronousFlash`, for boot loaders) drive a region of on-chip flash. The region must start and end on a 1 KB sector boundary; sector indices and addresses are relative to it.

1. Erase works per 1 KB sector; programming goes through the controller's 32-word write buffer, so any byte range can be written and bytes around it in a partly written word are left unchanged
2. A program or erase stalls every flash access until it completes: the CPU and every interrupt whose code runs from flash wait for it
3. The driver does not coordinate with `hal::tiva::Eeprom`; keep flash and EEPROM writes apart
4. Write and erase errors (access violation, voltage, verify) fail a `really_assert`

## Extra changes

1. Remove resistors R9 and R10.
2. Solder resistors R25 and R29 (0 ohm)

## Demos

1. [`demo/ek_tm4c123xgl`](../demo/ek_tm4c123xgl/README.md): LEDs, user switches and trace UART
2. [`demo/ek_tm4c123xgl_boostxl_k350qvg_s1`](../demo/ek_tm4c123xgl_boostxl_k350qvg_s1/README.md): BOOSTXL-K350QVG-S1 display over SPI

Build with `cmake --preset tm4c123gh6pm` and `cmake --build --preset tm4c123gh6pm-RelWithDebInfo --target <target>`; see [demo/README.md](../demo/README.md) for the target names.
