#ifndef HAL_LAUNCH_PAD_FAMILY_TM4C123_HPP
#define HAL_LAUNCH_PAD_FAMILY_TM4C123_HPP

#include "hal_tiva/instantiations/LaunchPadBspEkTm4c123g.hpp"
#include "hal_tiva/tiva/ClockTm4c123.hpp"
#include "hal_tiva/tiva/PinoutTableDefaultTm4c123.hpp"

namespace instantiations
{
    struct LaunchPad
    {
        LaunchPad()
        {
            hal::tiva::ConfigureClock(clock.crystal, clock.oscSource, clock.systemClockDivisor, clock.usesPll);
        }

        hal::tiva::GpioPin& DebugLed()
        {
            return ui.ledGreen;
        }

        hal::tiva::GpioPin& SecondLed()
        {
            return ui.ledRed;
        }

        LaunchPadClock clock;
        LaunchPadUi ui;
    };
}

#endif
