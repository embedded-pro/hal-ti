#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include <cstdint>

namespace hal::tiva
{
    infra::ConstByteRange UniqueDeviceId()
    {
#if defined(TM4C129)
        const uint8_t* base = reinterpret_cast<const uint8_t*>(0x400FEF20);
        return infra::ConstByteRange(base, base + 16);
#else
        // TM4C123 devices have no per-device unique identifier register
        return infra::ConstByteRange();
#endif
    }
}
