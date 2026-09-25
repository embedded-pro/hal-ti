#ifndef HAL_TI_EVENT_INFRASTRUCTURE_HPP
#define HAL_TI_EVENT_INFRASTRUCTURE_HPP

#include "hal/cortex_m/LowPowerStrategyWithModes.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal_tiva/tiva/LowPowerMode.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "infra/event/LowPowerEventDispatcher.hpp"

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

    struct LowPowerEventInfrastructure
    {
        explicit LowPowerEventInfrastructure(infra::Duration tickDuration = std::chrono::milliseconds(1), uint32_t coreClockHz = SystemCoreClock, const hal::tiva::LowPowerMode::Config& lowPowerModeConfig = hal::tiva::LowPowerMode::Config());

        void Run();

        infra::MainClockReference mainClock;
        hal::tiva::LowPowerMode lowPowerMode;
        hal::cortex::LowPowerStrategyWithModes lowPowerStrategy;
        infra::LowPowerEventDispatcher::WithSize<50> eventDispatcher;
        hal::cortex::SystemTickTimerService systemTick;
    };
}

#endif
