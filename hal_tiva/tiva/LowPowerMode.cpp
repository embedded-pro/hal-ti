#include "hal_tiva/tiva/LowPowerMode.hpp"
#include DEVICE_HEADER

namespace hal::tiva
{
    void LowPowerMode::Enter(PowerMode mode)
    {
        if (mode == PowerMode::deepSleep)
            SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

        __DSB();
        __WFI();

        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    }
}
