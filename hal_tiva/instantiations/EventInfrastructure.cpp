#include "hal_tiva/instantiations/EventInfrastructure.hpp"

namespace instantiations
{
    EventInfrastructure::EventInfrastructure(infra::Duration tickDuration, uint32_t coreClockHz)
        : systemTick(coreClockHz, tickDuration)
    {}

    void EventInfrastructure::Run()
    {
        eventDispatcher.Run();
    }

    LowPowerEventInfrastructure::LowPowerEventInfrastructure(infra::Duration tickDuration, uint32_t coreClockHz, const hal::tiva::LowPowerMode::Config& lowPowerModeConfig)
        : lowPowerMode(lowPowerModeConfig)
        , lowPowerStrategy(lowPowerMode, mainClock)
        , eventDispatcher(lowPowerStrategy)
        , systemTick(coreClockHz, tickDuration)
    {}

    void LowPowerEventInfrastructure::Run()
    {
        eventDispatcher.Run();
    }
}
