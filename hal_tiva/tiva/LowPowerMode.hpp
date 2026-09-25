#ifndef HAL_LOW_POWER_MODE_TIVA_HPP
#define HAL_LOW_POWER_MODE_TIVA_HPP

#include "hal/interfaces/LowPowerMode.hpp"
#include <cstdint>

namespace hal::tiva
{
    class LowPowerMode
        : public hal::LowPowerMode
    {
    public:
        enum class DeepSleepClock : uint8_t
        {
            precisionInternalOscillator,
            lowFrequencyInternalOscillator,
        };

        struct Config
        {
            constexpr Config()
            {}

            DeepSleepClock deepSleepClock{ DeepSleepClock::precisionInternalOscillator };
            bool lowPowerMemoriesInDeepSleep{ true };
        };

        explicit LowPowerMode(const Config& config = Config());

        void Enter(PowerMode mode) override;

    private:
        void ConfigureDeepSleepClock(DeepSleepClock clock) const;
        void ConfigureDeepSleepPower(bool lowPowerMemories) const;
    };
}

#endif
