#ifndef VALIDATION_TIVA_PIN_FACTORY_HPP
#define VALIDATION_TIVA_PIN_FACTORY_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/EnumCast.hpp"
#include "services/hil/HilPinPool.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace validation
{
    // Several ADC sequencers and comparators may share an analog pin, but the driver reserves a pin once per analog user
    class ManagedPin
        : public hal::tiva::GpioPin
    {
    public:
        ManagedPin(HilPinId id, hal::tiva::Drive drive, hal::tiva::Current current);

        void ConfigAnalog() override;
        void ResetConfig() override;

    private:
        uint8_t analogUsers = 0;
    };

    constexpr uint16_t Function(hal::tiva::PinConfigPeripheral function)
    {
        return static_cast<uint16_t>(infra::enum_cast(function));
    }

    bool SupportsFunction(HilPinId id, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex);
    std::optional<HilPinId> FindFunctionPin(hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex);
    hal::tiva::GpioPin& PinOrDummy(hal::GpioPin* pin);

    class TivaPinFactory
        : public services::HilPinFactory
    {
    public:
        static constexpr std::size_t capacity = 32;

        bool IsValid(HilPinId pin) const override;
        bool SupportsFunction(HilPinId pin, uint16_t function, uint8_t instance) const override;
        bool SupportsAnalog(HilPinId pin) const override;
        bool SupportsInterrupt(HilPinId pin) const override;
        std::optional<uint8_t> ParseDrive(infra::BoundedConstString text) const override;

        hal::GpioPin& Construct(std::size_t slot, HilPinId pin, const services::HilPinOptions& options) override;
        void Destroy(std::size_t slot) override;

    private:
        std::array<std::optional<ManagedPin>, capacity> pins;
    };
}

#endif
