#ifndef HAL_LAUNCH_PAD_FAMILY_TM4C129_HPP
#define HAL_LAUNCH_PAD_FAMILY_TM4C129_HPP

#include "hal_tiva/instantiations/LaunchPadBspEkTm4c1294.hpp"
#include "hal_tiva/tiva/ClockTm4c129.hpp"
#include "hal_tiva/tiva/PinoutTableDefaultTm4c129.hpp"

namespace instantiations
{
    struct LaunchPad
    {
        LaunchPad()
        {
            hal::tiva::ConfigureClock(clock.frequency, clock.hseValue, clock.oscSource, clock.systemClockVco, clock.usesPll);
        }

        hal::tiva::GpioPin& DebugLed()
        {
            return ui.led1;
        }

        hal::tiva::GpioPin& SecondLed()
        {
            return ui.led2;
        }

        LaunchPadClock clock;
        LaunchPadUi ui;
    };
}

#endif
