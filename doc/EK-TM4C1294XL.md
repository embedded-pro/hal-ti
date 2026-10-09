# EK-TM4C1294XL

Board support package: `hal_tiva/instantiations/LaunchPadBspEkTm4c1294.hpp`, selected by `LaunchPadBsp.hpp` when `TARGET_MCU_FAMILY` is `TM4C129`.

## Board resources used by the BSP

1. Clock: 25 MHz main crystal, 120 MHz system clock via the PLL (240 MHz VCO)
2. LED D1: PN1 (`ui.led1`, returned by `LaunchPad::DebugLed()`)
3. LED D2: PN0 (`ui.led2`)
4. User switch SW1: PJ0 (`ui.sw1`), internal pull-up
5. User switch SW2: PJ1 (`ui.sw2`), internal pull-up
6. Trace output: UART0 TX on PA1 (`LaunchPadTerminalAndTracer`), 115200 baud; the `terminal_uart_with_dma` example also uses UART0 RX on PA0

## Debugging

1. Connect the USB connector marked DEBUG (on-board ICDI)
2. The ICDI provides JTAG/SWD debugging and a virtual COM port connected to UART0 (PA0/PA1)

## Building the examples

1. Configure: `cmake --preset tm4c1294ncpdt`
2. Build: `cmake --build --preset tm4c1294ncpdt-Debug`
3. Optionally add `-DHAL_TI_INCLUDE_LWIP=ON` at configure time to build the lwIP Ethernet instantiation (TM4C129 only)

## Demos

1. [`demo/ek_tm4c1294xl`](../demo/ek_tm4c1294xl/README.md): LEDs, user switches and trace UART
2. [`demo/tm4c1294xl_boost_drv8711`](../demo/tm4c1294xl_boost_drv8711/README.md): BOOST-DRV8711 stepper driver BoosterPack

Build with `cmake --preset tm4c1294ncpdt` and `cmake --build --preset tm4c1294ncpdt-RelWithDebInfo --target <target>`; see [demo/README.md](../demo/README.md) for the target names.
