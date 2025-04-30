#pragma once

#include "infra/util/BoundedVector.hpp"
#include <cstdint>
#include DEVICE_HEADER
#include "hal/interfaces/AdcMultiChannel.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include <optional>

namespace hal::tiva
{
    class Adc
        : public hal::AdcMultiChannel
        , private hal::ImmediateInterruptHandler
    {
    public:
        enum class Trigger : uint8_t
        {
            pwmGenerator0,
            pwmGenerator1,
            pwmGenerator2,
            pwmGenerator3,
        };

        enum class SampleAndHold : uint8_t
        {
            sampleAndHold4,
            sampleAndHold8,
            sampleAndHold16,
            sampleAndHold32,
            sampleAndHold64,
            sampleAndHold128,
            sampleAndHold256,
        };

        enum class Oversampling : uint8_t
        {
            oversampling2 = 1,
            oversampling4,
            oversampling8,
            oversampling16,
            oversampling32,
            oversampling64,
        };

        struct Config
        {
            bool externalReference;
            uint8_t priority;
            Trigger trigger;
            SampleAndHold sampleAndHold;
            std::optional<Oversampling> oversampling;
        };

        Adc(uint8_t adcIndex, uint8_t adcSequencer, infra::MemoryRange<AnalogPin> inputs, const Config& config);
        ~Adc();

        void Measure(const infra::Function<void(Samples)>& onDone) override;

    private:
        void EnableClock();
        void DisableClock();

    private:
        constexpr static uint32_t maxSamples = 23;

        uint8_t adcIndex;
        uint8_t adcSequencer;
        infra::Function<void(Samples)> callback;
        infra::BoundedVector<uint16_t>::WithMaxSize<maxSamples> buffer;
    };
}
