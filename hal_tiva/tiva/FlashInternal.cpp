#include "hal_tiva/tiva/FlashInternal.hpp"
#include "FlashFamily.hpp"
#include "hal_tiva/tiva/FlashController.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal::tiva
{
    FlashInternal::FlashInternal(infra::ConstByteRange flashMemory)
        : hal::FlashHomogeneous(flashMemory.size() / family::flashSectorSize, family::flashSectorSize)
        , flashMemory(SectorAlignedRegion(flashMemory))
    {}

    void FlashInternal::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address <= flashMemory.size() && buffer.size() <= flashMemory.size() - address);

        ProgramFlash(buffer, AbsoluteAddress(address));
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void FlashInternal::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address <= flashMemory.size() && buffer.size() <= flashMemory.size() - address);

        std::copy(flashMemory.begin() + address, flashMemory.begin() + address + buffer.size(), buffer.begin());
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void FlashInternal::EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone)
    {
        really_assert(beginIndex <= endIndex && endIndex <= NumberOfSectors());

        for (auto index = beginIndex; index != endIndex; ++index)
            EraseFlashSector(AbsoluteAddress(AddressOfSector(index)));

        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    uint32_t FlashInternal::AbsoluteAddress(uint32_t address) const
    {
        return reinterpret_cast<uintptr_t>(flashMemory.begin()) + address;
    }
}
