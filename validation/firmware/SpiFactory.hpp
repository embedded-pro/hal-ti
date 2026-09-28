#ifndef VALIDATION_SPI_FACTORY_HPP
#define VALIDATION_SPI_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousSpiMaster.hpp"
#include "hal_tiva/tiva/SpiMaster.hpp"
#include "services/hil/commands/SpiCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>
#include <variant>

namespace validation
{
    class TivaSpiFactory
        : public services::hil::SpiFactory
    {
    public:
        explicit TivaSpiFactory(const services::hil::PinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::hil::Status Prepare(uint8_t index, const services::hil::Arguments& arguments) override;
        services::hil::Status Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, services::hil::SpiHandle& handle) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<PinId> clock;
            std::optional<PinId> mosi;
            std::optional<PinId> miso;
            std::optional<PinId> chipSelect;
            uint32_t baud = 100000;
            uint32_t mode = 0;
            bool synchronous = false;
        };

        services::hil::Status Parse(const services::hil::Arguments& arguments, Request& request) const;

    private:
        const services::hil::PinNaming& naming;
        std::variant<std::monostate, hal::tiva::SpiMaster, hal::tiva::SynchronousSpiMaster> driver;
    };
}

#endif
