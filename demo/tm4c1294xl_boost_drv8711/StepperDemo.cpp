#include "StepperDemo.hpp"

namespace demo
{
    namespace
    {
        constexpr uint32_t adcFullScale = 4095;
        constexpr uint32_t slowestStepPeriodInMilliseconds = 50;
        constexpr uint32_t fastestStepPeriodInMilliseconds = 2;
        constexpr uint32_t periodHysteresisInMilliseconds = 1;
        constexpr infra::Duration potentiometerPeriod = std::chrono::milliseconds(100);

        drivers::StepperMotorControllerDrv8711::Configuration MotorConfiguration()
        {
            drivers::StepperMotorControllerDrv8711::Configuration configuration;
            configuration.isgain = drivers::StepperMotorControllerDrv8711::Isgain::gain40;
            configuration.mode = drivers::StepperMotorControllerDrv8711::Mode::step1Over4;
            configuration.torque = 0x80;
            return configuration;
        }

        const char* Flag(bool value)
        {
            return value ? "1" : "0";
        }
    }

    StepperDemo::StepperDemo(drivers::StepDirStepperMotorDrv8711Decorator& stepper, hal::SynchronousAdc& potentiometer, hal::GpioPin& faultLed, services::Tracer& tracer)
        : stepper(stepper)
        , potentiometer(potentiometer)
        , faultLed(faultLed)
        , tracer(tracer)
        , stepPeriodInMilliseconds(slowestStepPeriodInMilliseconds)
    {
        stepper.OnFault([this]()
            {
                Faulted();
            });
        stepper.OnStall([this]()
            {
                this->tracer.Trace() << "Stall detected";
            });

        stepper.SetDirection(forward);
        stepper.Configure(MotorConfiguration(), [this](bool verified)
            {
                Configured(verified);
            });

        potentiometerTimer.Start(potentiometerPeriod, [this]()
            {
                SamplePotentiometer();
            });
    }

    void StepperDemo::ToggleRunning()
    {
        if (!configured)
        {
            tracer.Trace() << "Driver not configured";
            return;
        }

        running = !running;

        if (running)
            RestartStepping();
        else
            stepTimer.Cancel();

        tracer.Trace() << (running ? "Stepping at " : "Stopped, step period ") << stepPeriodInMilliseconds << " ms";
    }

    void StepperDemo::ToggleDirection()
    {
        forward = !forward;
        stepper.SetDirection(forward);
        tracer.Trace() << "Direction " << (forward ? "forward" : "reverse");
    }

    void StepperDemo::Configured(bool verified)
    {
        configured = verified;
        tracer.Trace() << (verified ? "DRV8711 configured, registers verified" : "DRV8711 register readback mismatch");
    }

    void StepperDemo::Faulted()
    {
        faultLed.Set(true);
        stepTimer.Cancel();
        running = false;
        tracer.Trace() << "Fault, stepping stopped; reset the board to recover";

        if (statusPending)
            return;

        statusPending = true;
        stepper.ReadStatus([this](drivers::StepperMotorControllerDrv8711::Status status)
            {
                statusPending = false;
                TraceStatus(status);
            });
    }

    void StepperDemo::TraceStatus(const drivers::StepperMotorControllerDrv8711::Status& status)
    {
        tracer.Trace() << "Status OTS " << Flag(status.overTemperature)
                       << " AOCP " << Flag(status.channelAOverCurrent)
                       << " BOCP " << Flag(status.channelBOverCurrent)
                       << " APDF " << Flag(status.channelAPreDriverFault)
                       << " BPDF " << Flag(status.channelBPreDriverFault)
                       << " UVLO " << Flag(status.underVoltageLockout)
                       << " STD " << Flag(status.stallDetected)
                       << " STDLAT " << Flag(status.latchedStallDetect);
    }

    void StepperDemo::SamplePotentiometer()
    {
        const uint32_t raw = potentiometer.Measure(1).front();
        const uint32_t period = slowestStepPeriodInMilliseconds - raw * (slowestStepPeriodInMilliseconds - fastestStepPeriodInMilliseconds) / adcFullScale;
        const uint32_t difference = period > stepPeriodInMilliseconds ? period - stepPeriodInMilliseconds : stepPeriodInMilliseconds - period;

        if (difference <= periodHysteresisInMilliseconds)
            return;

        stepPeriodInMilliseconds = period;

        if (running)
            RestartStepping();
    }

    void StepperDemo::RestartStepping()
    {
        stepTimer.Start(std::chrono::milliseconds(stepPeriodInMilliseconds), [this]()
            {
                stepper.Step();
            });
    }
}
