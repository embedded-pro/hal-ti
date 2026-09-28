#ifndef VALIDATION_COMPARATOR_FACTORY_HPP
#define VALIDATION_COMPARATOR_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousAnalogComparator.hpp"
#include "hal_tiva/tiva/AnalogComparator.hpp"
#include "services/hil/commands/HilComparatorCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>
#include <variant>

namespace validation
{
    class TivaComparatorFactory
        : public services::HilComparatorFactory
    {
    public:
        explicit TivaComparatorFactory(const services::HilPinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override;
        services::HilStatus Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilComparatorHandle& handle) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<HilPinId> positive;
            std::optional<HilPinId> negative;
            std::optional<HilPinId> output;
            hal::tiva::AnalogComparator::Config config;
            bool synchronous = false;
        };

        services::HilStatus Parse(const services::HilArguments& arguments, Request& request) const;

    private:
        const services::HilPinNaming& naming;
        std::variant<std::monostate, hal::tiva::AnalogComparator, hal::tiva::SynchronousAnalogComparator> driver;
    };
}

#endif
