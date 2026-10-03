#include DEVICE_HEADER
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal_tiva/tiva/PinoutTableDefault.hpp"

extern "C"
{
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
