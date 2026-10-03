#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include "UniqueDeviceIdFamily.hpp"

namespace hal::tiva
{
    namespace family = hal::tiva::family;

    infra::ConstByteRange UniqueDeviceId()
    {
        return family::GetUniqueDeviceId();
    }
}
