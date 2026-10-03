#include "validation/firmware/TivaPinFactory.hpp"
#include "BoardProfile.hpp"
#include "services/hil/HilArguments.hpp"
#include <concepts>

namespace validation
{
    namespace
    {
        template<class G>
        concept GpioWithInterruptType = requires(G& gpio, const infra::Function<void()>& action) {
            gpio.EnableInterrupt(hal::tiva::Port::A, uint8_t{ 0 }, action, hal::InterruptTrigger::risingEdge, hal::InterruptType::immediate);
        };

        constexpr uint8_t portsWithInterrupts = GpioWithInterruptType<hal::tiva::Gpio> ? 15 : 6;

        constexpr std::array<services::HilChoice<hal::tiva::Current>, 3> currents{ {
            { "2", hal::tiva::Current::_2mA },
            { "4", hal::tiva::Current::_4mA },
            { "8", hal::tiva::Current::_8mA },
        } };

        hal::tiva::Drive ToDrive(const services::HilPinOptions& options)
        {
            if (options.openDrain)
                return hal::tiva::Drive::OpenDrain;

            switch (options.pull)
            {
                case services::HilPull::up:
                    return hal::tiva::Drive::Up;
                case services::HilPull::down:
                    return hal::tiva::Drive::Down;
                default:
                    return hal::tiva::Drive::None;
            }
        }
    }

    ManagedPin::ManagedPin(HilPinId id, hal::tiva::Drive drive, hal::tiva::Current current)
        : hal::tiva::GpioPin(PortOf(id), id.index, drive, current)
    {}

    void ManagedPin::ConfigAnalog()
    {
        if (analogUsers++ == 0)
            hal::tiva::GpioPin::ConfigAnalog();
    }

    void ManagedPin::ResetConfig()
    {
        if (analogUsers > 1)
        {
            --analogUsers;
            return;
        }

        analogUsers = 0;
        hal::tiva::GpioPin::ResetConfig();
    }

    bool SupportsFunction(HilPinId id, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex)
    {
        for (const auto& subTable : hal::tiva::pinoutTableDefault)
            for (const auto& table : subTable)
                if (table.pinConfigType == function)
                    for (const auto& position : table.pinPositions)
                        if (position.peripheralIndex == peripheralIndex && position.port == PortOf(id) && position.pin == id.index)
                            return true;

        return false;
    }

    std::optional<HilPinId> FindFunctionPin(hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex)
    {
        for (const auto& subTable : hal::tiva::pinoutTableDefault)
            for (const auto& table : subTable)
                if (table.pinConfigType == function)
                    for (const auto& position : table.pinPositions)
                        if (position.peripheralIndex == peripheralIndex)
                            return Pin(position.port, position.pin);

        return std::nullopt;
    }

    hal::tiva::GpioPin& PinOrDummy(hal::GpioPin* pin)
    {
        if (pin != nullptr)
            return static_cast<hal::tiva::GpioPin&>(*pin);

        return hal::tiva::dummyPin;
    }

    bool TivaPinFactory::IsValid(HilPinId pin) const
    {
        return pin.index <= board::maximumPinIndex && pin.port < infra::BoundedConstString(board::portLetters).size();
    }

    bool TivaPinFactory::SupportsFunction(HilPinId pin, uint16_t function, uint8_t instance) const
    {
        return validation::SupportsFunction(pin, static_cast<hal::tiva::PinConfigPeripheral>(function), instance);
    }

    bool TivaPinFactory::SupportsAnalog(HilPinId pin) const
    {
        for (const auto& position : hal::tiva::analogTableDefault)
            if (position.type == hal::tiva::Type::adc && position.port == PortOf(pin) && position.pin == pin.index)
                return true;

        return false;
    }

    bool TivaPinFactory::SupportsInterrupt(HilPinId pin) const
    {
        return pin.port < portsWithInterrupts;
    }

    std::optional<uint8_t> TivaPinFactory::ParseDrive(infra::BoundedConstString text) const
    {
        if (auto current = services::HilArguments::ParseChoice(text, currents))
            return infra::enum_cast(*current);

        return std::nullopt;
    }

    hal::GpioPin& TivaPinFactory::Construct(std::size_t slot, HilPinId pin, const services::HilPinOptions& options)
    {
        return pins[slot].emplace(pin, ToDrive(options), static_cast<hal::tiva::Current>(options.drive));
    }

    void TivaPinFactory::Destroy(std::size_t slot)
    {
        pins[slot].reset();
    }
}
