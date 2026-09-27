#ifndef HAL_ANALOG_COMPARATOR_FAMILY_TM4C129_HPP
#define HAL_ANALOG_COMPARATOR_FAMILY_TM4C129_HPP

#include DEVICE_HEADER
#include "infra/util/ReallyAssert.hpp"
#include <array>
#include <cstdint>

namespace hal::tiva::family
{
    inline constexpr std::array<int32_t, 3> peripheralIrqComp = { {
        COMP0_IRQn,
        COMP1_IRQn,
        COMP2_IRQn,
    } };

    inline volatile uint32_t& GetAcctl(std::size_t index)
    {
        switch (index)
        {
            case 0:
                return COMP->ACCTL0;
            case 1:
                return COMP->ACCTL1;
            case 2:
                return COMP->ACCTL2;
            default:
                really_assert(false);
                return COMP->ACCTL0;
        }
    }

    inline volatile uint32_t& GetAcstat(std::size_t index)
    {
        switch (index)
        {
            case 0:
                return COMP->ACSTAT0;
            case 1:
                return COMP->ACSTAT1;
            case 2:
                return COMP->ACSTAT2;
            default:
                really_assert(false);
                return COMP->ACSTAT0;
        }
    }
}

#endif
