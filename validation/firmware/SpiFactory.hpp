#ifndef VALIDATION_SPI_FACTORY_HPP
#define VALIDATION_SPI_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousSpiMaster.hpp"
#include "hal_tiva/tiva/SpiMaster.hpp"
#include "services/hil/commands/HilSpiCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>
#include <variant>

namespace validation
{
    class TivaSpiFactory
        : public services::HilSpiFactory
    {
    public:
        explicit TivaSpiFactory(const services::HilPinNaming& naming);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override;
        services::HilStatus Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilSpiHandle& handle) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        struct Request
        {
            std::optional<HilPinId> clock;
            std::optional<HilPinId> mosi;
            std::optional<HilPinId> miso;
            std::optional<HilPinId> chipSelect;
            uint32_t baud = 100000;
            uint32_t mode = 0;
            bool synchronous = false;
        };

        services::HilStatus Parse(const services::HilArguments& arguments, Request& request) const;

    private:
        const services::HilPinNaming& naming;
        std::variant<std::monostate, hal::tiva::SpiMaster, hal::tiva::SynchronousSpiMaster> driver;
    };
}

#endif
