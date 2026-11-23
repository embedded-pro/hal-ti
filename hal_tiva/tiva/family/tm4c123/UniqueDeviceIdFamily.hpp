#pragma once

#include "infra/util/MemoryRange.hpp"

namespace hal::tiva::family
{
    inline infra::ConstByteRange GetUniqueDeviceId()
    {
        // TM4C123 devices have no per-device unique identifier register
        return infra::ConstByteRange();
    }
}
