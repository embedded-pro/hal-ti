#include "validation/firmware/PinPool.hpp"
#include "BoardProfile.hpp"

namespace validation
{
    ManagedPin::ManagedPin(PinId id, hal::tiva::Drive drive, hal::tiva::Current current)
        : hal::tiva::GpioPin(id.port, id.index, drive, current)
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

    bool SupportsFunction(PinId id, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex)
    {
        for (const auto& subTable : hal::tiva::pinoutTableDefault)
            for (const auto& table : subTable)
                if (table.pinConfigType == function)
                    for (const auto& position : table.pinPositions)
                        if (position.peripheralIndex == peripheralIndex && position.port == id.port && position.pin == id.index)
                            return true;

        return false;
    }

    bool SupportsAdc(PinId id)
    {
        for (const auto& position : hal::tiva::analogTableDefault)
            if (position.type == hal::tiva::Type::adc && position.port == id.port && position.pin == id.index)
                return true;

        return false;
    }

    std::optional<PinId> FindFunctionPin(hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex)
    {
        for (const auto& subTable : hal::tiva::pinoutTableDefault)
            for (const auto& table : subTable)
                if (table.pinConfigType == function)
                    for (const auto& position : table.pinPositions)
                        if (position.peripheralIndex == peripheralIndex)
                            return PinId{ position.port, position.pin };

        return std::nullopt;
    }

    hal::tiva::GpioPin& PinOrDummy(hal::tiva::GpioPin* pin)
    {
        if (pin != nullptr)
            return *pin;

        return hal::tiva::dummyPin;
    }

    Status PinPool::Claim(PinId id, Owner owner, Use use, hal::tiva::GpioPin*& pin, hal::tiva::Drive drive, hal::tiva::Current current)
    {
        if (id.index > 7 || (board::availablePorts & (1u << static_cast<uint8_t>(id.port))) == 0)
            return Status::pin;

        if (id == board::terminal.tx || id == board::terminal.rx)
            return Status::busy;

        const auto bit = static_cast<uint16_t>(1u << owner);
        Slot* freeSlot = nullptr;

        for (auto& slot : slots)
        {
            if (slot.owners != 0 && slot.id == id)
            {
                if (use != Use::analog || slot.use != Use::analog)
                    return Status::busy;

                slot.owners |= bit;
                pin = &*slot.pin;
                return Status::done;
            }

            if (slot.owners == 0 && freeSlot == nullptr)
                freeSlot = &slot;
        }

        if (freeSlot == nullptr)
            return Status::busy;

        freeSlot->id = id;
        freeSlot->owners = bit;
        freeSlot->use = use;
        freeSlot->pin.emplace(id, drive, current);
        pin = &*freeSlot->pin;
        return Status::done;
    }

    Status PinPool::ClaimFunction(PinId id, Owner owner, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex, hal::tiva::GpioPin*& pin)
    {
        if (!SupportsFunction(id, function, peripheralIndex))
            return Status::pin;

        return Claim(id, owner, Use::exclusive, pin);
    }

    Status PinPool::ClaimFunction(const std::optional<PinId>& id, Owner owner, hal::tiva::PinConfigPeripheral function, uint8_t peripheralIndex, hal::tiva::GpioPin*& pin)
    {
        pin = nullptr;

        if (!id)
            return Status::done;

        return ClaimFunction(*id, owner, function, peripheralIndex, pin);
    }

    Status PinPool::ClaimAdc(PinId id, Owner owner, hal::tiva::GpioPin*& pin)
    {
        if (!SupportsAdc(id))
            return Status::pin;

        return Claim(id, owner, Use::analog, pin);
    }

    void PinPool::Release(Owner owner)
    {
        const auto bit = static_cast<uint16_t>(1u << owner);

        for (auto& slot : slots)
            if ((slot.owners & bit) != 0)
            {
                slot.owners &= static_cast<uint16_t>(~bit);
                if (slot.owners == 0)
                    slot.pin.reset();
            }
    }

    void PinPool::Release(PinId id, Owner owner)
    {
        const auto bit = static_cast<uint16_t>(1u << owner);

        for (auto& slot : slots)
            if ((slot.owners & bit) != 0 && slot.id == id)
            {
                slot.owners &= static_cast<uint16_t>(~bit);
                if (slot.owners == 0)
                    slot.pin.reset();
            }
    }
}
