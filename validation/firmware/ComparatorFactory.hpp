#ifndef VALIDATION_COMPARATOR_FACTORY_HPP
#define VALIDATION_COMPARATOR_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousAnalogComparator.hpp"
#include "hal_tiva/tiva/AnalogComparator.hpp"
#include "services/hil/commands/ComparatorCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>
#include <variant>

namespace validation
{
    class TivaComparatorFactory
        : public services::hil::ComparatorFactory
    {
    public:
        explicit TivaComparatorFactory(const services::hil::PinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::hil::Status Prepare(uint8_t index, const services::hil::Arguments& arguments) override;
        services::hil::Status Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, services::hil::ComparatorHandle& handle) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<PinId> positive;
            std::optional<PinId> negative;
            std::optional<PinId> output;
            hal::tiva::AnalogComparator::Config config;
            bool synchronous = false;
        };

        services::hil::Status Parse(const services::hil::Arguments& arguments, Request& request) const;

    private:
        const services::hil::PinNaming& naming;
        std::variant<std::monostate, hal::tiva::AnalogComparator, hal::tiva::SynchronousAnalogComparator> driver;
    };
}

#endif
