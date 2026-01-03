#ifndef HAL_SYNCHRONOUS_ADC_TIVA_HPP
#define HAL_SYNCHRONOUS_ADC_TIVA_HPP

#include "hal/synchronous_interfaces/SynchronousAdc.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/BoundedVector.hpp"
#include <cstdint>
#include <optional>

namespace hal::tiva
{
    class SynchronousAdc
        : public hal::SynchronousAdc
    {
    public:
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

        enum class Priority : uint8_t
        {
            priority0 = 0,
            priority1,
            priority2,
            priority3,
        };

        struct Config
        {
            SampleAndHold sampleAndHold;
            Priority priority;
            std::optional<Oversampling> oversampling;
        };

        SynchronousAdc(uint8_t adcIndex, uint8_t adcSequencer, infra::MemoryRange<AnalogPin> inputs, const Config& config);
        ~SynchronousAdc();

        Samples Measure(std::size_t numberOfSamples) override;

    private:
        void EnableClock() const;
        void DisableClock() const;

    private:
        constexpr static uint32_t maxSamples = 23;

        uint8_t adcIndex;
        uint8_t adcSequencer;
        std::size_t numberOfInputs;
        infra::BoundedVector<uint16_t>::WithMaxSize<maxSamples> buffer;
    };
}

#endif
