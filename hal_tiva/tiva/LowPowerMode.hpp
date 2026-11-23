#pragma once

#include "hal/interfaces/LowPowerMode.hpp"

namespace hal::tiva
{
    class LowPowerMode
        : public hal::LowPowerMode
    {
    public:
        void Enter(PowerMode mode) override;
    };
}
