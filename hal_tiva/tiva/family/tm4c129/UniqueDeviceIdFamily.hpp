#ifndef HAL_UNIQUE_DEVICE_ID_FAMILY_TM4C129_HPP
#define HAL_UNIQUE_DEVICE_ID_FAMILY_TM4C129_HPP

#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace hal::tiva::family
{
    inline infra::ConstByteRange GetUniqueDeviceId()
    {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(0x400FEF20);
        return infra::ConstByteRange(base, base + 16);
    }
}

#endif
