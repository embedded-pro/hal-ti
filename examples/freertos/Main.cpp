#include "hal/interfaces/Gpio.hpp"
#include "hal_tiva/instantiations/LaunchPadBsp.hpp"
#include "infra/timer/Timer.hpp"
#include "osal/Osal.hpp"
#include "osal/freertos/LowPowerStrategyFreeRtos.hpp"
#include "osal/freertos_system_time/TimerServiceFreeRtos.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

extern "C" void xPortPendSVHandler();
extern "C" void xPortSysTickHandler();
extern "C" void vPortSVCHandler();

extern "C" [[gnu::naked]] void SVC_Handler()
{
    asm("b vPortSVCHandler");
}

extern "C" [[gnu::naked]] void PendSV_Handler()
{
    asm("b xPortPendSVHandler");
}

extern "C" [[gnu::naked]] void SysTick_Handler()
{
    asm("b xPortSysTickHandler");
};

namespace
{
    hal::tiva::GpioPin& SecondLed(instantiations::LaunchPad& launchPad)
    {
#if defined(TM4C123)
        return launchPad.ui.ledRed;
#else
        return launchPad.ui.led2;
#endif
    }
}

int main()
{
    static instantiations::LaunchPad launchPad;

    static hal::TimerServiceFreeRtos timerService;
    static hal::LowPowerStrategyFreeRtos lowPowerStrategy;
    static infra::LowPowerEventDispatcher::WithSize<50> eventDispatcher(lowPowerStrategy);

    osal::Init();

    static std::thread t1([]()
        {
            hal::OutputPin pin(launchPad.DebugLed());

            while (true)
            {
                std::this_thread::sleep_for(700ms);
                pin.Set(true);
                std::this_thread::sleep_for(200ms);
                pin.Set(false);
            }
        });

    static std::thread t2([]()
        {
            hal::OutputPin pin(SecondLed(launchPad));

            infra::TimerRepeating toggle(500ms, [&]()
                {
                    pin.Set(!pin.GetOutputLatch());
                });

            eventDispatcher.Run();
        });

    osal::Run();
    __builtin_unreachable();
}
