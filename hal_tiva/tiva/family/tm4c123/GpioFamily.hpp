#ifndef HAL_GPIO_FAMILY_TM4C123_HPP
#define HAL_GPIO_FAMILY_TM4C123_HPP

#include DEVICE_HEADER
#include <array>
#include <cstdint>

namespace hal::tiva::family
{
    struct GpioPortEntry
    {
        GPIOA_Type* address;
        uint32_t rcgc;
        int32_t irq;
        bool perPin;
    };

    // clang-format off
    inline const std::array<GpioPortEntry, 15> portAndRcgc = { {
        { GPIOA, 0x00000001, GPIOA_IRQn, false },
        { GPIOB, 0x00000002, GPIOB_IRQn, false },
        { GPIOC, 0x00000004, GPIOC_IRQn, false },
        { GPIOD, 0x00000008, GPIOD_IRQn, false },
        { GPIOE, 0x00000010, GPIOE_IRQn, false },
        { GPIOF, 0x00000020, GPIOF_IRQn, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
        { nullptr, 0, -1, false },
    } };
    // clang-format on

    inline constexpr std::array<int32_t, 16> perPinIrqs = { {
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
    } };

    inline bool IsLockProtected(GPIOA_Type* gpio, uint8_t index)
    {
        return ((gpio == GPIOF) && (index == 0)) ||
               ((gpio == GPIOD) && (index == 7));
    }
}

#endif
