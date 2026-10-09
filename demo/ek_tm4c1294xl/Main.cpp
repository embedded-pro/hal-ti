#include "hal/interfaces/Gpio.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "hal_tiva/instantiations/LaunchPadBsp.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include "infra/stream/OutputStream.hpp"
#include "services/peripheral/DebouncedButton.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "services/peripheral/GpioPinInverted.hpp"

extern "C" uint32_t SystemCoreClock;

int main()
{
    static instantiations::LaunchPad launchPad;
    static instantiations::EventInfrastructure eventInfrastructure;
    static instantiations::LaunchPadTerminalAndTracer terminal;

    static services::DebugLed heartbeat{ launchPad.DebugLed(), std::chrono::milliseconds(100), std::chrono::milliseconds(1400) };

    static hal::OutputPin secondLed{ launchPad.SecondLed() };

    static services::GpioPinInverted sw1Active{ launchPad.ui.sw1 };
    static services::GpioPinInverted sw2Active{ launchPad.ui.sw2 };

    static services::DebouncedButton sw1{ sw1Active, []()
        {
            secondLed.Set(true);
            terminal.tracer.Trace() << "SW1 pressed, LED D2 on";
        } };
    static services::DebouncedButton sw2{ sw2Active, []()
        {
            secondLed.Set(false);
            terminal.tracer.Trace() << "SW2 pressed, LED D2 off";
        } };

    terminal.tracer.Trace() << "EK-TM4C1294XL demo";
    terminal.tracer.Trace() << "Core clock " << SystemCoreClock << " Hz";
    terminal.tracer.Trace() << "Device ID " << infra::AsHex(hal::tiva::UniqueDeviceId());
    terminal.tracer.Trace() << "SW1 turns LED D2 on, SW2 turns it off";

    eventInfrastructure.Run();
    __builtin_unreachable();
}
