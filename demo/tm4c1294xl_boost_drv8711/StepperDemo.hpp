#ifndef DEMO_TM4C1294XL_BOOST_DRV8711_STEPPER_DEMO_HPP
#define DEMO_TM4C1294XL_BOOST_DRV8711_STEPPER_DEMO_HPP

#include "drivers/motor_controller/StepDirStepperMotorDrv8711Decorator.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/synchronous_interfaces/SynchronousAdc.hpp"
#include "infra/timer/Timer.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace demo
{
    class StepperDemo
    {
    public:
        StepperDemo(drivers::StepDirStepperMotorDrv8711Decorator& stepper, hal::SynchronousAdc& potentiometer, hal::GpioPin& faultLed, services::Tracer& tracer);

        void ToggleRunning();
        void ToggleDirection();

    private:
        void Configured(bool verified);
        void Faulted();
        void TraceStatus(const drivers::StepperMotorControllerDrv8711::Status& status);
        void SamplePotentiometer();
        void RestartStepping();

    private:
        drivers::StepDirStepperMotorDrv8711Decorator& stepper;
        hal::SynchronousAdc& potentiometer;
        hal::OutputPin faultLed;
        services::Tracer& tracer;
        infra::TimerRepeating potentiometerTimer;
        infra::TimerRepeating stepTimer;
        uint32_t stepPeriodInMilliseconds;
        bool configured = false;
        bool running = false;
        bool forward = true;
        bool statusPending = false;
    };
}

#endif
