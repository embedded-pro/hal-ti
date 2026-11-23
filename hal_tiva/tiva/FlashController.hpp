#pragma once

#include "infra/util/ByteRange.hpp"
#include <cstdint>

namespace hal::tiva
{
    infra::ConstByteRange SectorAlignedRegion(infra::ConstByteRange flashMemory);

    void EraseFlashSector(uint32_t address);
    void ProgramFlash(infra::ConstByteRange buffer, uint32_t address);
}
