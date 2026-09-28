#include "validation/firmware/TivaBoardInfo.hpp"
#include "BoardProfile.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include "services/hil/Arguments.hpp"
#include DEVICE_HEADER

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        constexpr uint32_t rescExternal = 1u << 0;
        constexpr uint32_t rescPowerOn = 1u << 1;
        constexpr uint32_t rescBrownOut = 1u << 2;
        constexpr uint32_t rescWatchDog0 = 1u << 3;
        constexpr uint32_t rescSoftware = 1u << 4;
        constexpr uint32_t rescWatchDog1 = 1u << 5;
        constexpr uint32_t rescMainOscillatorFailure = 1u << 16;

        constexpr std::array<services::hil::Choice<uint32_t>, 7> resetCauses{ {
            { "wdt0", rescWatchDog0 },
            { "wdt1", rescWatchDog1 },
            { "sw", rescSoftware },
            { "moscfail", rescMainOscillatorFailure },
            { "bor", rescBrownOut },
            { "por", rescPowerOn },
            { "ext", rescExternal },
        } };
    }

    const char* ReadAndClearResetCause()
    {
        const uint32_t cause = SYSCTL->RESC;
        SYSCTL->RESC = 0;

        for (const auto& entry : resetCauses)
            if ((cause & entry.value) != 0)
                return entry.name;

        return "unknown";
    }

    TivaBoardInfo::TivaBoardInfo(const char* resetCause)
        : resetCause(resetCause)
    {}

    const char* TivaBoardInfo::Name() const
    {
        return board::name;
    }

    const char* TivaBoardInfo::Family() const
    {
        return board::family;
    }

    uint32_t TivaBoardInfo::SystemClock() const
    {
        return SystemCoreClock;
    }

    const char* TivaBoardInfo::ResetCause() const
    {
        return resetCause;
    }

    infra::ConstByteRange TivaBoardInfo::UniqueId() const
    {
        return hal::tiva::UniqueDeviceId();
    }
}
