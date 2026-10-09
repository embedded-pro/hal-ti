#include "BoardPins.hpp"
#include "DisplayDemo.hpp"
#include "boards/boostxl_k350qvg_s1/BoostxlK350qvgS1Setup.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "hal_tiva/instantiations/LaunchPadBsp.hpp"
#include "hal_tiva/tiva/SpiMaster.hpp"
#include "services/peripheral/DebouncedButton.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "services/peripheral/GpioPinInverted.hpp"
#include "services/peripheral/SpiMasterWithChipSelect.hpp"

namespace
{
    constexpr uint32_t lcdBaudRate = 4'000'000;

    hal::tiva::SpiMaster::Config LcdSpiConfig()
    {
        hal::tiva::SpiMaster::Config config;
        config.baudRate = lcdBaudRate;
        return config;
    }
}

int main()
{
    static instantiations::LaunchPad launchPad;
    static instantiations::EventInfrastructure eventInfrastructure;
    static instantiations::LaunchPadTerminalAndTracer terminal;

    static services::DebugLed heartbeat{ launchPad.DebugLed(), std::chrono::milliseconds(100), std::chrono::milliseconds(1400) };

    // LED_PWM of the BoosterPack drives the backlight regulator's shutdown pin and shares PF2 with the blue LED
    static hal::OutputPin backlight{ launchPad.ui.ledBlue, true };

    static hal::tiva::GpioPin lcdClock{ demo::pins::lcdClock.port, demo::pins::lcdClock.index };
    static hal::tiva::GpioPin lcdMosi{ demo::pins::lcdMosi.port, demo::pins::lcdMosi.index };
    static hal::tiva::GpioPin lcdChipSelect{ demo::pins::lcdChipSelect.port, demo::pins::lcdChipSelect.index };
    static hal::tiva::GpioPin lcdDataCommand{ demo::pins::lcdDataCommand.port, demo::pins::lcdDataCommand.index };
    static hal::tiva::GpioPin lcdReset{ demo::pins::lcdReset.port, demo::pins::lcdReset.index };

    static hal::tiva::SpiMaster spi{ demo::pins::lcdSsi, lcdClock, hal::tiva::dummyPin, lcdMosi, LcdSpiConfig() };
    static services::SpiMasterWithChipSelect spiWithChipSelect{ spi, lcdChipSelect };

    static demo::DisplayDemo displayDemo{ terminal.tracer };
    static boards::BoostxlK350qvgS1Setup display{ spiWithChipSelect, lcdDataCommand, lcdReset, []()
        {
            terminal.tracer.Trace() << "Display initialized";
            displayDemo.Start(display.Display());
        } };

    static services::GpioPinInverted sw2Active{ launchPad.ui.sw2 };
    static services::DebouncedButton sw2{ sw2Active, []()
        {
            displayDemo.NextPattern();
        } };

    terminal.tracer.Trace() << "EK-TM4C123GXL + BOOSTXL-K350QVG-S1 demo";
    terminal.tracer.Trace() << "SW2 selects the next pattern";

    eventInfrastructure.Run();
    __builtin_unreachable();
}
