#ifndef VALIDATION_QEI_FACTORY_HPP
#define VALIDATION_QEI_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousQuadratureEncoder.hpp"
#include "services/hil/commands/HilQeiCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>

namespace validation
{
    class TivaQeiFactory
        : public services::HilQeiFactory
    {
    public:
        explicit TivaQeiFactory(const services::HilPinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override;
        services::HilStatus Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, hal::SynchronousQuadratureEncoder*& encoder) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<HilPinId> a;
            std::optional<HilPinId> b;
            std::optional<HilPinId> index;
            hal::tiva::QuadratureEncoder::Config config;
        };

        services::HilStatus Parse(uint8_t index, const services::HilArguments& arguments, Request& request) const;

    private:
        const services::HilPinNaming& naming;
        std::optional<hal::tiva::QuadratureEncoder> encoder;
    };
}

#endif
