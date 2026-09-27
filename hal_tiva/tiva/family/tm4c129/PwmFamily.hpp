#ifndef HAL_PWM_FAMILY_TM4C129_HPP
#define HAL_PWM_FAMILY_TM4C129_HPP

#include DEVICE_HEADER
#include <array>
#include <cstdint>

namespace hal::tiva::family
{
    constexpr std::size_t numberOfPwms = 1;

    struct PwmIrqInfo
    {
        std::array<int32_t, 4> generatorIrqs;
        int32_t faultIrq;
    };

    // clang-format off
    inline constexpr std::array<uint32_t, numberOfPwms> peripheralPwmArray = { {
        PWM0_BASE,
    } };

    inline constexpr std::array<PwmIrqInfo, numberOfPwms> peripheralPwmIrqs = { {
        { { PWM0_0_IRQn, PWM0_1_IRQn, PWM0_2_IRQn, PWM0_3_IRQn }, PWM0_FAULT_IRQn },
    } };
    // clang-format on

    constexpr uint32_t PwmCcUsePwmDiv = 0x00000100;
    constexpr uint32_t PwmCcPwmDivM = 0x00000007;
    constexpr uint32_t PwmCcPwmDiv2 = 0x00000000;
    constexpr uint32_t PwmCcPwmDiv4 = 0x00000001;
    constexpr uint32_t PwmCcPwmDiv8 = 0x00000002;
    constexpr uint32_t PwmCcPwmDiv16 = 0x00000003;
    constexpr uint32_t PwmCcPwmDiv32 = 0x00000004;
    constexpr uint32_t PwmCcPwmDiv64 = 0x00000005;

    inline constexpr std::array<uint32_t, 7> clockDivisor = { {
        0,
        PwmCcPwmDiv2 | PwmCcUsePwmDiv,
        PwmCcPwmDiv4 | PwmCcUsePwmDiv,
        PwmCcPwmDiv8 | PwmCcUsePwmDiv,
        PwmCcPwmDiv16 | PwmCcUsePwmDiv,
        PwmCcPwmDiv32 | PwmCcUsePwmDiv,
        PwmCcPwmDiv64 | PwmCcUsePwmDiv,
    } };

    template<typename ClockDivisorEnum>
    inline void SetClockDivisor(PWM0_Type* const pwmBase, ClockDivisorEnum divisor)
    {
        pwmBase->CC = ((pwmBase->CC & ~(PwmCcUsePwmDiv | PwmCcPwmDivM)) | clockDivisor[static_cast<std::size_t>(divisor)]);
    }

    inline uint32_t GetClockDivisor(PWM0_Type* const pwmBase)
    {
        auto result = pwmBase->CC & PwmCcPwmDivM;
        if (!(pwmBase->CC & PwmCcUsePwmDiv))
            return 1;
        else
            return 1U << ((result > 5U ? 5U : result) + 1);
    }
}

#endif
