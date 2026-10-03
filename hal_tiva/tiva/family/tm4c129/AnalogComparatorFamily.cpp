#include DEVICE_HEADER
#include "hal/cortex_m/InterruptCortex.hpp"

namespace
{
    extern "C" void Comp2_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(COMP2_IRQn);
    }
}
