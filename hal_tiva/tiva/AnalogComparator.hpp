#pragma once

#include "hal/interfaces/AnalogComparator.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>
#include <optional>
#include DEVICE_HEADER

namespace hal::tiva
{
    class AnalogComparator
        : public hal::AnalogComparator
    {
    public:
        enum class PositiveInputSource : uint8_t
        {
            externalPin = 0,
            sharedC0PlusPin = 1,
            internalReference = 2,
        };

        enum class TriggerSense : uint8_t
        {
            level = 0,
            falling = 1,
            rising = 2,
            both = 3,
        };

        enum class InterruptSense : uint8_t
        {
            level = 0,
            falling = 1,
            rising = 2,
            both = 3,
        };

        enum class ReferenceRange : uint8_t
        {
            low = 0,
            high = 1,
        };

        struct InternalReference
        {
            ReferenceRange range = ReferenceRange::low;
            uint8_t step  = 0;
        };

        struct PwmFaultRouting
        {
            uint8_t pwmIndex;
        };

        struct Config
        {
            PositiveInputSource positiveSource = PositiveInputSource::externalPin;
            std::optional<InternalReference> internalReference;
            bool invertOutput = false;
            bool outputToPin = false;
            bool triggerEnabled = false;
            TriggerSense triggerSense = TriggerSense::rising;
            bool triggerLevelHigh = true;
            std::optional<PwmFaultRouting> routeToPwmFault;
            InterruptSense interruptSense = InterruptSense::both;
            bool interruptLevelHigh = true;
            InterruptPriority priority = InterruptPriority::Normal;
        };

        AnalogComparator(uint8_t index, GpioPin& vinPositive, GpioPin& vinNegative, GpioPin& outputPin, const Config& config);
        ~AnalogComparator();

        void Enable(const infra::Function<void(bool output)>& onOutputChanged, InterruptTrigger trigger) override;
        void Disable() override;
        bool GetOutput() const override;

        static void EnableAcmpClock();
        static void DisableAcmpClockIfLastInstance();

    private:
        void ConfigureReference() const;
        void ConfigureControl(std::optional<InterruptSense> overrideSense) const;
        uint32_t ComputeAcctl(InterruptSense isense, bool islval) const;
        volatile uint32_t& Acctl() const;
        volatile uint32_t& Acstat() const;
        void HandleIrq();

        uint8_t index;
        const Config& config;
        std::optional<AnalogPin> vinPositivePin;
        std::optional<AnalogPin> vinNegativePin;
        std::optional<PeripheralPin> outputPeripheralPin;
        std::optional<hal::ImmediateInterruptHandler> irqHandler;
        infra::Function<void(bool)> onOutputChanged;
    };
}
