# EK-TM4C123GXL + BOOSTXL-K350QVG-S1 demo

Draws test patterns on the 3.5" 320x240 SSD2119 panel of the BOOSTXL-K350QVG-S1 over SPI, through EMIL's `boards::BoostxlK350qvgS1Setup`.

```bash
cmake --preset tm4c123gh6pm
cmake --build --preset tm4c123gh6pm-RelWithDebInfo --target demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1
```

The BoosterPack is used in its default 4-wire SPI mode (SCS, SCL, SDI, SDC; PS0 = 0). The panel is written in 8-row strips from one 5 KB buffer, because a full RGB565 frame (150 KB) does not fit in the 32 KB of RAM. SW2 cycles through colour bars, a grey ramp and solid red, green and blue.

## Wiring

BoosterPack pins are from the BOOSTXL-K350QVG-S1 schematic, LaunchPad pins from table 2-3 to 2-6 of the EK-TM4C123GXL user's manual (SPMU296). The pin constants are in `BoardPins.hpp`.

| BoosterPack signal | BoosterPack pin | LaunchPad pin | Use                                                        |
|--------------------|-----------------|---------------|------------------------------------------------------------|
| LCD_SCL            | J1.7            | PB4           | SSI2 clock, `hal::tiva::SpiMaster`                         |
| LCD_SDI            | J2.15           | PB7           | SSI2 transmit                                              |
| LCD_SCS            | J2.13           | PA4           | chip select, driven as GPIO by `services::SpiMasterWithChipSelect` |
| LCD_SDC            | J1.8            | PA5           | data/command, GPIO                                         |
| LCD_RST            | J4.32           | PD7           | reset, GPIO (NMI-locked pin, unlocked by `hal::tiva::GpioPin`) |
| LED_PWM            | J4.40           | PF2           | backlight enable, shared with the blue LED                 |

The SPI runs in mode 0 at 4 MHz. SSD2119 has no read-back over SPI, so MISO (PB6) is not configured.

## Notes

1. LED_PWM feeds the shutdown pin of the backlight regulator, so the demo drives PF2 high; the blue LED lights with the backlight. Brightness control would need PWM on PF2 (M1PWM6), which is not done here.
2. R9 and R10 on the EK-TM4C123GXL join PB6/PD0 and PB7/PD1; PD0 and PD1 are left unconfigured, so this does not matter for the demo.
3. The resistive touch panel (TOUCH_XP J3.24, TOUCH_YP J3.23, TOUCH_XN J4.31, TOUCH_YN J2.11) is not used. TOUCH_XN is on PF4, which is also SW1, so the demo uses SW2.
4. Verified only by building; the SPI mode, clock rate and backlight polarity have not been tried on hardware.
