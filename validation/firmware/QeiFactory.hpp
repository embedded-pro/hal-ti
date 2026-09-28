#ifndef VALIDATION_QEI_FACTORY_HPP
#define VALIDATION_QEI_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousQuadratureEncoder.hpp"
#include "services/hil/commands/QeiCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>

namespace validation
{
    class TivaQeiFactory
        : public services::hil::QeiFactory
    {
    public:
        explicit TivaQeiFactory(const services::hil::PinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::hil::Status Prepare(uint8_t index, const services::hil::Arguments& arguments) override;
        services::hil::Status Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, hal::SynchronousQuadratureEncoder*& encoder) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<PinId> a;
            std::optional<PinId> b;
            std::optional<PinId> index;
            hal::tiva::QuadratureEncoder::Config config;
        };

        services::hil::Status Parse(uint8_t index, const services::hil::Arguments& arguments, Request& request) const;

    private:
        const services::hil::PinNaming& naming;
        std::optional<hal::tiva::QuadratureEncoder> encoder;
    };
}

#endif
