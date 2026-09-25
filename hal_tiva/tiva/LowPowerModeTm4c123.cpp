#include "hal_tiva/tiva/LowPowerMode.hpp"
#include DEVICE_HEADER

namespace
{
    constexpr uint32_t deepSleepOscillatorSourcePrecisionInternal = 0x1u << 4;
    constexpr uint32_t deepSleepOscillatorSourceLowFrequencyInternal = 0x3u << 4;
}

namespace hal::tiva
{
    void LowPowerMode::ConfigureDeepSleepClock(DeepSleepClock clock) const
    {
        SYSCTL->DSLPCLKCFG = clock == DeepSleepClock::precisionInternalOscillator ? deepSleepOscillatorSourcePrecisionInternal : deepSleepOscillatorSourceLowFrequencyInternal;
    }
}
