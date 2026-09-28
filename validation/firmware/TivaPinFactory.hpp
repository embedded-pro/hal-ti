#ifndef VALIDATION_TIVA_PIN_FACTORY_HPP
#define VALIDATION_TIVA_PIN_FACTORY_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/EnumCast.hpp"
#include "services/hil/PinPool.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace validation
{
    // Several ADC sequencers may sample the same pin, as e-foc does with the supply voltage, but the driver reserves a pin once per analog user
    class ManagedPin
        : public hal::tiva::GpioPin
    {
    public:
        ManagedPin(PinId id, hal::tiva::Drive drive, hal::tiva::Current current);

        void ConfigAnalog() override;
        void ResetConfig() override;

    private:
        uint8_t analogUsers = 0;
    };

    constexpr uint16_t Function(hal::tiva::PinConfigPeripheral function)
    {
        return static_cast<uint16_t>(infra::enum_cast(function));
    }

    bool SupportsFunction(PinId id, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex);
    std::optional<PinId> FindFunctionPin(hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex);
    hal::tiva::GpioPin& PinOrDummy(hal::GpioPin* pin);

    class TivaPinFactory
        : public services::hil::PinFactory
    {
    public:
        static constexpr std::size_t capacity = 32;

        bool IsValid(PinId pin) const override;
        bool SupportsFunction(PinId pin, uint16_t function, uint8_t instance) const override;
        bool SupportsAnalog(PinId pin) const override;
        bool SupportsInterrupt(PinId pin) const override;
        std::optional<uint8_t> ParseDrive(infra::BoundedConstString text) const override;

        hal::GpioPin& Construct(std::size_t slot, PinId pin, const services::hil::PinOptions& options) override;
        void Destroy(std::size_t slot) override;

    private:
        std::array<std::optional<ManagedPin>, capacity> pins;
    };
}

#endif
