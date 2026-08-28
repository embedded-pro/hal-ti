#include "hal_tiva/instantiations/EventInfrastructure.hpp"

namespace instantiations
{
    EventInfrastructure::EventInfrastructure(infra::Duration tickDuration, uint32_t coreClockHz)
        : systemTick(coreClockHz, tickDuration)
    {
        systemTick.Start();
    }

    void EventInfrastructure::Run()
    {
        eventDispatcher.Run();
    }
}
