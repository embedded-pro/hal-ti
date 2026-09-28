#ifndef VALIDATION_ADC_FACTORY_HPP
#define VALIDATION_ADC_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousAdc.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "infra/util/BoundedVector.hpp"
#include "services/hil/commands/AdcCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <optional>
#include <variant>

namespace validation
{
    class TivaAdcFactory
        : public services::hil::AdcFactory
    {
    public:
        static constexpr std::size_t sequencers = 2;

        explicit TivaAdcFactory(const services::hil::PinNaming& naming);

        std::size_t KeyPositionals() const override;
        services::hil::Status ParseKey(const services::hil::Arguments& arguments, uint16_t& key) const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::hil::Status Prepare(uint16_t key, const services::hil::Arguments& arguments) override;
        services::hil::Status Open(std::size_t slot, uint16_t key, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, services::hil::AdcHandle& handle) override;
        void Close(std::size_t slot, uint16_t key, const infra::Function<void()>& onClosed) override;

    private:
        static constexpr std::size_t maximumSteps = 8;

        struct Request
        {
            std::array<PinId, maximumSteps> pins{};
            std::size_t steps = 0;
            bool synchronous = false;
            uint8_t sampleAndHold = 1;
            uint8_t oversampling = 1;
            uint32_t delay = 4;
            bool delayEnabled = true;
            hal::tiva::Adc::Trigger trigger{};
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

        services::hil::Status Parse(uint16_t key, const services::hil::Arguments& arguments, Request& request) const;
        services::hil::Status ParseComparators(infra::BoundedConstString text, std::size_t steps, Sequencer& sequencer, std::size_t& comparatorSteps) const;
        void Construct(Sequencer& sequencer, const Request& request, std::size_t comparatorSteps);

    private:
        const services::hil::PinNaming& naming;
        std::array<std::optional<Sequencer>, sequencers> slots;
    };
}

#endif
