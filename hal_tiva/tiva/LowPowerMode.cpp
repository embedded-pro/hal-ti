#include "hal_tiva/tiva/LowPowerMode.hpp"
#include DEVICE_HEADER
#include "hal/cortex_m/LowPowerModeCortex.hpp"

namespace
{
    constexpr uint32_t sramPowerStandby = 0x1u;
    constexpr uint32_t flashPowerLow = 0x2u << 4;
}

namespace hal::tiva
{
    LowPowerMode::LowPowerMode(const Config& config)
    {
        ConfigureDeepSleepClock(config.deepSleepClock);
        ConfigureDeepSleepPower(config.lowPowerMemoriesInDeepSleep);
    }

    void LowPowerMode::Enter(PowerMode mode)
    {
        hal::cortex::WaitForInterrupt(mode == PowerMode::deepSleep);
    }

    void LowPowerMode::ConfigureDeepSleepPower(bool lowPowerMemories) const
    {
        SYSCTL->DSLPPWRCFG = lowPowerMemories ? (flashPowerLow | sramPowerStandby) : 0;
    }
}
