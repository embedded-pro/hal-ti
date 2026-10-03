#pragma once

#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/MemoryRange.hpp"

namespace hal::tiva
{
    extern const infra::MemoryRange<const infra::MemoryRange<const Gpio::PinoutTable>> pinoutTableDefault;
    extern const infra::MemoryRange<const Gpio::AnalogPinPosition> analogTableDefault;
}
