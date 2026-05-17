#pragma once

#include "hal/synchronous_interfaces/SynchronousAnalogComparator.hpp"
#include "hal_tiva/tiva/AnalogComparator.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include <cstdint>
#include <optional>

namespace hal::tiva
{
    class SynchronousAnalogComparator
        : public hal::SynchronousAnalogComparator
    {
    public:
        using Config = AnalogComparator::Config;
        using PositiveInputSource = AnalogComparator::PositiveInputSource;
        using TriggerSense = AnalogComparator::TriggerSense;
        using InterruptSense = AnalogComparator::InterruptSense;
        using InternalReference = AnalogComparator::InternalReference;
        using ReferenceRange = AnalogComparator::ReferenceRange;
        using PwmFaultRouting = AnalogComparator::PwmFaultRouting;

        SynchronousAnalogComparator(uint8_t index, GpioPin& vinPositive, GpioPin& vinNegative, GpioPin& outputPin, const Config& config);
        ~SynchronousAnalogComparator();

        bool GetOutput() const override;

    private:
        void EnableClock();
        static void DisableClockIfLastInstance();
        void ConfigureReference() const;
        void ConfigureControl() const;
        uint32_t ComputeAcctl() const;
        volatile uint32_t& Acctl() const;
        volatile uint32_t& Acstat() const;

        uint8_t index;
        const Config& config;
        std::optional<AnalogPin> vinPositivePin;
        std::optional<AnalogPin> vinNegativePin;
        std::optional<PeripheralPin> outputPeripheralPin;
    };
}
