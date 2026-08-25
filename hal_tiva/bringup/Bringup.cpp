#include DEVICE_HEADER
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#if defined(TM4C123)
#include "hal_tiva/tiva/PinoutTableDefaultTm4c123.hpp"
#elif defined(TM4C129)
#include "hal_tiva/tiva/PinoutTableDefaultTm4c129.hpp"
#else
#error "MCU family not defined or invalid [TM4C123 | TM4C129]!"
#endif

extern "C"
{
    [[gnu::weak]] void Default_Handler_Forwarded()
    {
        hal::cortex::InterruptTable::Instance().Invoke(hal::cortex::ActiveInterrupt());
    }

    void HardwareInitialization()
    {
        static hal::cortex::InterruptTable::WithStorage<128> interruptTable;
        static hal::tiva::Gpio gpio{ hal::tiva::pinoutTableDefault, hal::tiva::analogTableDefault };
    }
}
