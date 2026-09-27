#ifndef HAL_GPIO_FAMILY_TM4C129_HPP
#define HAL_GPIO_FAMILY_TM4C129_HPP

#include DEVICE_HEADER
#define GPIOA_Type GPIOA_AHB_Type
#define GPIOA GPIOA_AHB
#define GPIOB GPIOB_AHB
#define GPIOC GPIOC_AHB
#define GPIOD GPIOD_AHB
#define GPIOE GPIOE_AHB
#define GPIOF GPIOF_AHB
#define GPIOG GPIOG_AHB
#define GPIOH GPIOH_AHB
#define GPIOJ GPIOJ_AHB
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
        { GPIOG, 0x00000040, GPIOG_IRQn, false },
        { GPIOH, 0x00000080, GPIOH_IRQn, false },
        { GPIOJ, 0x00000100, GPIOJ_IRQn, false },
        { GPIOK, 0x00000200, GPIOK_IRQn, false },
        { GPIOL, 0x00000400, GPIOL_IRQn, false },
        { GPIOM, 0x00000800, GPIOM_IRQn, false },
        { GPION, 0x00001000, GPION_IRQn, false },
        { GPIOP, 0x00002000, -1, true },
        { GPIOQ, 0x00004000, -1, true },
    } };
    // clang-format on

    inline constexpr std::array<int32_t, 16> perPinIrqs = { {
        GPIOP0_IRQn,
        GPIOP1_IRQn,
        GPIOP2_IRQn,
        GPIOP3_IRQn,
        GPIOP4_IRQn,
        GPIOP5_IRQn,
        GPIOP6_IRQn,
        GPIOP7_IRQn,
        GPIOQ0_IRQn,
        GPIOQ1_IRQn,
        GPIOQ2_IRQn,
        GPIOQ3_IRQn,
        GPIOQ4_IRQn,
        GPIOQ5_IRQn,
        GPIOQ6_IRQn,
        GPIOQ7_IRQn,
    } };

    inline bool IsLockProtected(GPIOA_Type* gpio, uint8_t index)
    {
        return ((gpio == GPIOD) && (index == 7)) ||
               ((gpio == GPIOE) && (index == 7));
    }
}

#endif
