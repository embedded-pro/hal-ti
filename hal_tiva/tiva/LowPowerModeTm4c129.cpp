#include "hal_tiva/tiva/LowPowerMode.hpp"
#include DEVICE_HEADER

namespace
{
    constexpr uint32_t deepSleepOscillatorSourcePrecisionInternal = 0x0u << 20;
    constexpr uint32_t deepSleepOscillatorSourceLowFrequencyInternal = 0x2u << 20;
}

namespace hal::tiva
{
    void LowPowerMode::ConfigureDeepSleepClock(DeepSleepClock clock) const
    {
        SYSCTL->DSCLKCFG = clock == DeepSleepClock::precisionInternalOscillator ? deepSleepOscillatorSourcePrecisionInternal : deepSleepOscillatorSourceLowFrequencyInternal;
    }
}
