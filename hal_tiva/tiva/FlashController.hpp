#ifndef HAL_TIVA_FLASH_CONTROLLER_HPP
#define HAL_TIVA_FLASH_CONTROLLER_HPP

#include "infra/util/ByteRange.hpp"
#include <cstdint>

namespace hal::tiva
{
    infra::ConstByteRange SectorAlignedRegion(infra::ConstByteRange flashMemory);

    void EraseFlashSector(uint32_t address);
    void ProgramFlash(infra::ConstByteRange buffer, uint32_t address);
}

#endif
