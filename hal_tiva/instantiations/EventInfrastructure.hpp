#ifndef HAL_TI_EVENT_INFRASTRUCTURE_HPP
#define HAL_TI_EVENT_INFRASTRUCTURE_HPP

#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

extern "C" uint32_t SystemCoreClock;

namespace instantiations
{
    struct EventInfrastructure
    {
        explicit EventInfrastructure(infra::Duration tickDuration = std::chrono::milliseconds(1), uint32_t coreClockHz = SystemCoreClock);

        void Run();

        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;
        hal::cortex::SystemTickTimerService systemTick;
    };
}

#endif
