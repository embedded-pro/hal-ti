#ifndef HAL_UNIQUE_DEVICE_ID_FAMILY_TM4C123_HPP
#define HAL_UNIQUE_DEVICE_ID_FAMILY_TM4C123_HPP

#include "infra/util/MemoryRange.hpp"

namespace hal::tiva::family
{
    inline infra::ConstByteRange GetUniqueDeviceId()
    {
        // TM4C123 devices have no per-device unique identifier register
        return infra::ConstByteRange();
    }
}

#endif
