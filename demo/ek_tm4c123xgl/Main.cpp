#include "hal/interfaces/Gpio.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "hal_tiva/instantiations/LaunchPadBsp.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include "infra/stream/OutputStream.hpp"
#include "services/peripheral/DebouncedButton.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "services/peripheral/GpioPinInverted.hpp"
#include "services/tracer/Tracer.hpp"

extern "C" uint32_t SystemCoreClock;

namespace
{
    void Toggle(hal::OutputPin& led, const char* name, services::Tracer& tracer)
    {
        led.Set(!led.GetOutputLatch());
        tracer.Trace() << name << " LED " << (led.GetOutputLatch() ? "on" : "off");
    }
}

int main()
{
    static instantiations::LaunchPad launchPad;
    static instantiations::EventInfrastructure eventInfrastructure;
    static instantiations::LaunchPadTerminalAndTracer terminal;

    static services::DebugLed heartbeat{ launchPad.DebugLed(), std::chrono::milliseconds(100), std::chrono::milliseconds(1400) };

    static hal::OutputPin redLed{ launchPad.ui.ledRed };
    static hal::OutputPin blueLed{ launchPad.ui.ledBlue };

    static services::GpioPinInverted sw1Active{ launchPad.ui.sw1 };
    static services::GpioPinInverted sw2Active{ launchPad.ui.sw2 };

    static services::DebouncedButton sw1{ sw1Active, []()
        {
            Toggle(redLed, "Red", terminal.tracer);
        } };
    static services::DebouncedButton sw2{ sw2Active, []()
        {
            Toggle(blueLed, "Blue", terminal.tracer);
        } };

    terminal.tracer.Trace() << "EK-TM4C123GXL demo";
    terminal.tracer.Trace() << "Core clock " << SystemCoreClock << " Hz";
    terminal.tracer.Trace() << "Device ID " << infra::AsHex(hal::tiva::UniqueDeviceId());
    terminal.tracer.Trace() << "SW1 toggles the red LED, SW2 the blue LED";

    eventInfrastructure.Run();
    __builtin_unreachable();
}
