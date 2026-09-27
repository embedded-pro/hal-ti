#include DEVICE_HEADER
#include "hal/cortex_m/InterruptCortex.hpp"

namespace
{
    extern "C" void Pwm1Generator0_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM1_0_IRQn);
    }

    extern "C" void Pwm1Generator1_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM1_1_IRQn);
    }

    extern "C" void Pwm1Generator2_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM1_2_IRQn);
    }

    extern "C" void Pwm1Generator3_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM1_3_IRQn);
    }

    extern "C" void Pwm1Fault_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM1_FAULT_IRQn);
    }
}
