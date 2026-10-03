#ifndef VALIDATION_CAN_FACTORY_HPP
#define VALIDATION_CAN_FACTORY_HPP

#include "hal_tiva/tiva/Can.hpp"
#include "services/hil/commands/HilCanCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>

namespace validation
{
    class TivaCanFactory
        : public services::HilCanFactory
    {
    public:
        explicit TivaCanFactory(const services::HilPinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override;
        services::HilStatus Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& can) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<HilPinId> rx;
            std::optional<HilPinId> tx;
            uint32_t bitRate = 500000;
            hal::tiva::Can::Config config;
        };

        services::HilStatus Parse(uint8_t index, const services::HilArguments& arguments, Request& request) const;

    private:
        const services::HilPinNaming& naming;
        std::optional<hal::tiva::Can::WithMaxRxBuffer<8>> can;
        infra::Function<void(const char* error)> onError;
        infra::Function<void()> onClosed;
    };
}

#endif
