#ifndef VALIDATION_CAN_FACTORY_HPP
#define VALIDATION_CAN_FACTORY_HPP

#include "hal_tiva/tiva/Can.hpp"
#include "services/hil/commands/CanCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>

namespace validation
{
    class TivaCanFactory
        : public services::hil::CanFactory
    {
    public:
        explicit TivaCanFactory(const services::hil::PinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::hil::Status Prepare(uint8_t index, const services::hil::Arguments& arguments) override;
        services::hil::Status Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& can) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<PinId> rx;
            std::optional<PinId> tx;
            uint32_t bitRate = 500000;
            hal::tiva::Can::Config config;
        };

        services::hil::Status Parse(uint8_t index, const services::hil::Arguments& arguments, Request& request) const;

    private:
        const services::hil::PinNaming& naming;
        std::optional<hal::tiva::Can::WithMaxRxBuffer<8>> can;
        infra::Function<void(const char* error)> onError;
        infra::Function<void()> onClosed;
    };
}

#endif
