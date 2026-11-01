#include "hal_tiva/synchronous_tiva/SynchronousFlashInternal.hpp"
#include "FlashFamily.hpp"
#include "hal_tiva/tiva/FlashController.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal::tiva
{
    SynchronousFlashInternal::SynchronousFlashInternal(infra::ConstByteRange flashMemory)
        : hal::SynchronousFlashHomogeneous(flashMemory.size() / family::flashSectorSize, family::flashSectorSize)
        , flashMemory(SectorAlignedRegion(flashMemory))
    {}

    void SynchronousFlashInternal::WriteBuffer(infra::ConstByteRange buffer, uint32_t address)
    {
        really_assert(address <= flashMemory.size() && buffer.size() <= flashMemory.size() - address);

        ProgramFlash(buffer, AbsoluteAddress(address));
    }

    void SynchronousFlashInternal::ReadBuffer(infra::ByteRange buffer, uint32_t address)
    {
        really_assert(address <= flashMemory.size() && buffer.size() <= flashMemory.size() - address);

        std::copy(flashMemory.begin() + address, flashMemory.begin() + address + buffer.size(), buffer.begin());
    }

    void SynchronousFlashInternal::EraseSectors(uint32_t beginIndex, uint32_t endIndex)
    {
        really_assert(beginIndex <= endIndex && endIndex <= NumberOfSectors());

        for (auto index = beginIndex; index != endIndex; ++index)
            EraseFlashSector(AbsoluteAddress(AddressOfSector(index)));
    }

    uint32_t SynchronousFlashInternal::AbsoluteAddress(uint32_t address) const
    {
        return reinterpret_cast<uintptr_t>(flashMemory.begin()) + address;
    }
}
