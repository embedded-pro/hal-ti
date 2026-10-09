#include "BoardPins.hpp"
#include "StepperDemo.hpp"
#include "drivers/motor_controller/StepDirStepperMotorDrv8711Decorator.hpp"
#include "hal/interfaces/AnalogToDigitalPin.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "hal_tiva/instantiations/LaunchPadBsp.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousAdc.hpp"
#include "hal_tiva/tiva/SpiMaster.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/peripheral/DebouncedButton.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "services/peripheral/GpioPinInverted.hpp"
#include "services/peripheral/SpiMasterWithChipSelect.hpp"

namespace
{
    constexpr uint32_t driverBaudRate = 1'000'000;

    hal::tiva::SpiMaster::Config DriverSpiConfig()
    {
        hal::tiva::SpiMaster::Config config;
        config.baudRate = driverBaudRate;
        return config;
    }

    hal::tiva::SynchronousAdc::Config PotentiometerAdcConfig()
    {
        return { hal::tiva::SynchronousAdc::SampleAndHold::sampleAndHold256, hal::tiva::SynchronousAdc::Priority::priority0, hal::tiva::SynchronousAdc::Oversampling::oversampling16 };
    }

    // The BOOST-DRV8711 does not route BEMF to its headers; the step/dir decorator only reads it through OnBemf, which this demo never calls
    class UnroutedBemf
        : public hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>
    {
    public:
        void Measure(SamplesRange, const infra::Function<void()>&) override
        {
            really_assert(false);
        }
    };

    hal::tiva::GpioPin MakePin(const demo::pins::Pin& pin, hal::tiva::Drive drive = hal::tiva::Drive::Default)
    {
        return hal::tiva::GpioPin{ pin.port, pin.index, drive };
    }
}

int main()
{
    static instantiations::LaunchPad launchPad;
    static instantiations::EventInfrastructure eventInfrastructure;
    static instantiations::LaunchPadTerminalAndTracer terminal;

    static services::DebugLed heartbeat{ launchPad.DebugLed(), std::chrono::milliseconds(100), std::chrono::milliseconds(1400) };

    static hal::tiva::GpioPin sclk = MakePin(demo::pins::sclk);
    static hal::tiva::GpioPin sdatI = MakePin(demo::pins::sdatI);
    static hal::tiva::GpioPin sdatO = MakePin(demo::pins::sdatO);
    static hal::tiva::GpioPin chipSelect = MakePin(demo::pins::chipSelect);
    static hal::tiva::SpiMaster spi{ demo::pins::driverSsi, sclk, sdatO, sdatI, DriverSpiConfig() };

    // The DRV8711 chip select is active high
    static services::GpioPinInverted chipSelectActiveHigh{ chipSelect };
    static services::SpiMasterWithChipSelect spiWithChipSelect{ spi, chipSelectActiveHigh };

    static hal::tiva::GpioPin nFault = MakePin(demo::pins::nFault, hal::tiva::Drive::Up);
    static hal::tiva::GpioPin nStall = MakePin(demo::pins::nStall, hal::tiva::Drive::Up);
    static hal::tiva::GpioPin nSleep = MakePin(demo::pins::nSleep);
    static hal::tiva::GpioPin reset = MakePin(demo::pins::reset);
    static hal::tiva::GpioPin stepAin1 = MakePin(demo::pins::stepAin1);
    static hal::tiva::GpioPin dirAin2 = MakePin(demo::pins::dirAin2);

    static hal::tiva::GpioPin bin1 = MakePin(demo::pins::bin1);
    static hal::tiva::GpioPin bin2 = MakePin(demo::pins::bin2);
    static hal::OutputPin bin1Low{ bin1 };
    static hal::OutputPin bin2Low{ bin2 };

    static UnroutedBemf bemf;
    static drivers::StepDirStepperMotorDrv8711Decorator stepper{ spiWithChipSelect, nFault, nStall, bemf, stepAin1, dirAin2, reset, nSleep };

    static hal::tiva::GpioPin potentiometerPin = MakePin(demo::pins::potentiometer);
    static infra::BoundedVector<hal::tiva::AnalogPin>::WithMaxSize<1> potentiometerInputs;
    potentiometerInputs.emplace_back(potentiometerPin);
    static hal::tiva::SynchronousAdc potentiometer{ 0, 0, infra::MakeRange(potentiometerInputs), PotentiometerAdcConfig() };

    static demo::StepperDemo stepperDemo{ stepper, potentiometer, launchPad.SecondLed(), terminal.tracer };

    static services::GpioPinInverted sw1Active{ launchPad.ui.sw1 };
    static services::GpioPinInverted sw2Active{ launchPad.ui.sw2 };
    static services::DebouncedButton sw1{ sw1Active, []()
        {
            stepperDemo.ToggleRunning();
        } };
    static services::DebouncedButton sw2{ sw2Active, []()
        {
            stepperDemo.ToggleDirection();
        } };

    terminal.tracer.Trace() << "EK-TM4C1294XL + BOOST-DRV8711 demo";
    terminal.tracer.Trace() << "SW1 starts and stops stepping, SW2 reverses, the potentiometer sets the speed";

    eventInfrastructure.Run();
    __builtin_unreachable();
}
