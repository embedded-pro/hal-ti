#ifndef HAL_QUADRATURE_ENCODER_FAMILY_TM4C123_HPP
#define HAL_QUADRATURE_ENCODER_FAMILY_TM4C123_HPP

#include DEVICE_HEADER
#include <array>
#include <cstdint>

namespace hal::tiva::family
{
    constexpr std::size_t numberOfQei = 2;

    inline constexpr std::array<uint32_t, numberOfQei> peripheralQeiArray = { {
        QEI0_BASE,
        QEI1_BASE,
    } };

    inline constexpr std::array<int32_t, numberOfQei> peripheralIrqQeiArray = { {
        QEI0_IRQn,
        QEI1_IRQn,
    } };
}

#endif
