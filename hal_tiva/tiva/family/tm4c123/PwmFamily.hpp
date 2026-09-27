#ifndef HAL_PWM_FAMILY_TM4C123_HPP
#define HAL_PWM_FAMILY_TM4C123_HPP

#include DEVICE_HEADER
#include "infra/util/ReallyAssert.hpp"
#include <array>
#include <cstdint>

namespace hal::tiva::family
{
    constexpr std::size_t numberOfPwms = 2;

    struct PwmIrqInfo
    {
        std::array<int32_t, 4> generatorIrqs;
        int32_t faultIrq;
    };

    // clang-format off
    inline constexpr std::array<uint32_t, numberOfPwms> peripheralPwmArray = { {
        PWM0_BASE,
        PWM1_BASE,
    } };

    inline constexpr std::array<PwmIrqInfo, numberOfPwms> peripheralPwmIrqs = { {
        { { PWM0_0_IRQn, PWM0_1_IRQn, PWM0_2_IRQn, PWM0_3_IRQn }, PWM0_FAULT_IRQn },
        { { PWM1_0_IRQn, PWM1_1_IRQn, PWM1_2_IRQn, PWM1_3_IRQn }, PWM1_FAULT_IRQn },
    } };
    // clang-format on

    constexpr uint32_t SysctlRccUsePwmDiv = 0x00100000;
    constexpr uint32_t SysctlRccPwmDivM = 0x000E0000;
    constexpr uint32_t SysctlRccPwmDiv2 = 0x00000000;
    constexpr uint32_t SysctlRccPwmDiv4 = 0x00020000;
    constexpr uint32_t SysctlRccPwmDiv8 = 0x00040000;
    constexpr uint32_t SysctlRccPwmDiv16 = 0x00060000;
    constexpr uint32_t SysctlRccPwmDiv32 = 0x00080000;
    constexpr uint32_t SysctlRccPwmDiv64 = 0x000A0000;
    constexpr uint32_t SysctlDc1Pwm0 = 0x00100000;
    constexpr uint32_t SysctlDc1Pwm1 = 0x00200000;

    inline constexpr std::array<uint32_t, 7> clockDivisor = { {
        0,
        SysctlRccPwmDiv2 | SysctlRccUsePwmDiv,
        SysctlRccPwmDiv4 | SysctlRccUsePwmDiv,
        SysctlRccPwmDiv8 | SysctlRccUsePwmDiv,
        SysctlRccPwmDiv16 | SysctlRccUsePwmDiv,
        SysctlRccPwmDiv32 | SysctlRccUsePwmDiv,
        SysctlRccPwmDiv64 | SysctlRccUsePwmDiv,
    } };

    template<typename ClockDivisorEnum>
    inline void SetClockDivisor(PWM0_Type* const, ClockDivisorEnum divisor)
    {
        really_assert(SYSCTL->DC1 & (SysctlDc1Pwm0 | SysctlDc1Pwm1));
        SYSCTL->RCC = ((SYSCTL->RCC & ~(SysctlRccUsePwmDiv | SysctlRccPwmDivM)) | clockDivisor[static_cast<std::size_t>(divisor)]);
    }

    inline uint32_t GetClockDivisor(PWM0_Type* const)
    {
        auto result = (SYSCTL->RCC & SysctlRccPwmDivM) >> 17;
        if (!(SYSCTL->RCC & SysctlRccUsePwmDiv))
            return 1;
        else
            return 1U << ((result > 5U ? 5U : result) + 1);
    }
}

#endif
