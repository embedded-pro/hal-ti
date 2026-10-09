#include DEVICE_HEADER
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal_tiva/tiva/PinoutTableDefault.hpp"

extern "C"
{
    // Weak so that a timer service supplying its own tick overrides it; lwIP reads it through sys_now
    [[gnu::weak]] uint32_t HAL_GetTick()
    {
        if (hal::cortex::SystemTickTimerService::InstanceSet())
            return std::chrono::duration_cast<std::chrono::milliseconds>(hal::cortex::SystemTickTimerService::Instance().Now().time_since_epoch()).count();
        else
            return 0;
    }

    [[gnu::weak]] void Default_Handler_Forwarded()
    {
        hal::cortex::InterruptTable::Instance().Invoke(hal::cortex::ActiveInterrupt());
    }

    void HardwareInitialization()
    {
        static hal::cortex::InterruptTable::WithStorage<155> interruptTable;
        static hal::tiva::Gpio gpio{ hal::tiva::pinoutTableDefault, hal::tiva::analogTableDefault };
    }
}
