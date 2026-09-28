#ifndef VALIDATION_ADC_FACTORY_HPP
#define VALIDATION_ADC_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousAdc.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "infra/util/BoundedVector.hpp"
#include "services/hil/commands/HilAdcCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <optional>
#include <variant>

namespace validation
{
    class TivaAdcFactory
        : public services::HilAdcFactory
    {
    public:
        static constexpr std::size_t sequencers = 2;

        explicit TivaAdcFactory(const services::HilPinNaming& naming);

        std::size_t KeyPositionals() const override;
        services::HilStatus ParseKey(const services::HilArguments& arguments, uint16_t& key) const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint16_t key, const services::HilArguments& arguments) override;
        services::HilStatus Open(std::size_t slot, uint16_t key, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilAdcHandle& handle) override;
        void Close(std::size_t slot, uint16_t key, const infra::Function<void()>& onClosed) override;

    private:
        static constexpr std::size_t maximumSteps = 8;

        struct Request
        {
            std::array<HilPinId, maximumSteps> pins{};
            std::size_t steps = 0;
            bool synchronous = false;
            uint8_t sampleAndHold = 0;
            uint8_t oversampling = 0;
            std::optional<uint32_t> delay;
            std::optional<hal::tiva::Adc::Trigger> trigger;
            bool externalReference = false;
            std::optional<uint32_t> priority;
            std::array<hal::tiva::Adc::DigitalComparatorConfig, maximumSteps> comparators{};
            std::size_t comparatorSteps = 0;
        };

        struct Sequencer
        {
            uint16_t key = 0;
            infra::BoundedVector<hal::tiva::AnalogPin>::WithMaxSize<maximumSteps> inputs;
            std::array<hal::tiva::Adc::DigitalComparatorConfig, maximumSteps> comparators{};
            hal::tiva::Adc::Config asyncConfig{};
            hal::tiva::SynchronousAdc::Config syncConfig{};
            std::variant<std::monostate, hal::tiva::Adc, hal::tiva::SynchronousAdc> driver;
        };

        services::HilStatus Parse(uint16_t key, const services::HilArguments& arguments, Request& request) const;
        static services::HilStatus ParseComparators(infra::BoundedConstString text, Request& request);
        void Construct(Sequencer& sequencer, const Request& request);

    private:
        const services::HilPinNaming& naming;
        std::array<std::optional<Sequencer>, sequencers> slots;
    };
}

#endif
