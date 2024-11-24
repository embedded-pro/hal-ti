#ifndef HAL_TI_EVENT_INFRASTRUCTURE_HPP
#define HAL_TI_EVENT_INFRASTRUCTURE_HPP

#include "hal_tiva/cortex/SystemTickTimerService.hpp"
#include "hal_tiva/tiva/Gpio.hpp"


namespace instantiations
{
    struct EventInfrastructure
    {
        EventInfrastructure(infra::Duration tickDuration = std::chrono::milliseconds(1));

        void Run();

        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;
        hal::cortex::SystemTickTimerService systemTick;
    };
}

#endif
