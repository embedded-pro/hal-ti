#ifndef VALIDATION_PIN_POOL_HPP
#define VALIDATION_PIN_POOL_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include "validation/firmware/Response.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace validation
{
    using Owner = uint8_t;

    namespace owner
    {
        constexpr Owner gpio = 0;
        constexpr Owner pwm = 1;
        constexpr Owner uart = 2;
        constexpr Owner spi = 3;
        constexpr Owner adc = 4;
        constexpr Owner comparator = 6;
        constexpr Owner qei = 7;
        constexpr Owner can = 8;
    }

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

    bool SupportsFunction(PinId id, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex);
    bool SupportsAdc(PinId id);
    std::optional<PinId> FindFunctionPin(hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex);
    hal::tiva::GpioPin& PinOrDummy(hal::tiva::GpioPin* pin);

    class PinPool
    {
    public:
        enum class Use : uint8_t
        {
            exclusive,
            analog,
        };

        Status Claim(PinId id, Owner owner, Use use, hal::tiva::GpioPin*& pin, hal::tiva::Drive drive = hal::tiva::Drive::None, hal::tiva::Current current = hal::tiva::Current::_2mA);
        Status ClaimFunction(PinId id, Owner owner, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex, hal::tiva::GpioPin*& pin);
        Status ClaimFunction(const std::optional<PinId>& id, Owner owner, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex, hal::tiva::GpioPin*& pin);
        Status ClaimAdc(PinId id, Owner owner, hal::tiva::GpioPin*& pin);
        void Release(Owner owner);
        void Release(PinId id, Owner owner);

    private:
        struct Slot
        {
            std::optional<ManagedPin> pin;
            PinId id{ hal::tiva::Port::A, 0 };
            uint16_t owners = 0;
            Use use = Use::exclusive;
        };

        std::array<Slot, 32> slots;
    };
}

#endif
