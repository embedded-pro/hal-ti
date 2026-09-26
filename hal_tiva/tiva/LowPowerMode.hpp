#ifndef HAL_LOW_POWER_MODE_TIVA_HPP
#define HAL_LOW_POWER_MODE_TIVA_HPP

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

#endif
