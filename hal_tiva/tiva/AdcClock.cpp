#include "hal_tiva/tiva/AdcClock.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>
#include DEVICE_HEADER

namespace hal::tiva
{
    namespace
    {
        std::array<uint8_t, 2> adcClockUsers{};

        void WaitForAdcReady(uint8_t adcIndex)
        {
            while ((SYSCTL->PRADC & (1 << adcIndex)) == 0)
            {
            }
        }
    }

    void AcquireAdcClock(uint8_t adcIndex)
    {
        really_assert(adcIndex < adcClockUsers.size());

        if (adcClockUsers[adcIndex]++ != 0)
            return;

        SYSCTL->RCGCADC |= 1 << adcIndex;
        WaitForAdcReady(adcIndex);

        // The first two conversions after the clock is enabled may be wrong (TM4C123 erratum ADC#14); the reset also
        // clears whatever an earlier user left in the registers
        SYSCTL->SRADC |= 1 << adcIndex;
        SYSCTL->SRADC &= ~(1 << adcIndex);
        WaitForAdcReady(adcIndex);
    }

    void ReleaseAdcClock(uint8_t adcIndex)
    {
        really_assert(adcIndex < adcClockUsers.size() && adcClockUsers[adcIndex] != 0);

        if (--adcClockUsers[adcIndex] == 0)
            SYSCTL->RCGCADC &= ~(1 << adcIndex);
    }
}
