#ifndef HAL_WATCHDOG_TIVA_HPP
#define HAL_WATCHDOG_TIVA_HPP

#include DEVICE_HEADER
#include "hal/cortex_m/InterruptCortex.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>

namespace hal::tiva
{
    class WatchDog
        : private hal::cortex::ImmediateInterruptHandler
    {
    public:
        struct Config
        {
            constexpr Config()
            {}

            infra::Duration timeout{ std::chrono::milliseconds(50) };
            infra::Duration feedTimerInterval{ std::chrono::milliseconds(25) };
            infra::Duration expirationTimeout{ std::chrono::milliseconds(1500) };
            bool resetOnMissedInterrupt{ true };
            hal::cortex::InterruptPriority interruptPriority{ hal::cortex::InterruptPriority::normal };
        };

        WatchDog(uint8_t watchDogIndex, const infra::Function<void()>& onExpired, const Config& config = Config());
        ~WatchDog();

        void Refresh();

    private:
        WATCHDOG0_Type& Peripheral() const;
        uint32_t ClockFrequency() const;
        void EnablePeripheralClock() const;
        void DisablePeripheralClock() const;
        void Unlock() const;
        void WaitForWriteComplete() const;
        void Feed();
        void HandleInterrupt();

        uint8_t watchDogIndex;
        uint32_t reloadValue{ 0 };
        uint32_t expirationCount{ 1 };
        std::atomic<uint32_t> missedFeeds{ 0 };
        infra::Function<void()> onExpired;
        infra::TimerRepeating feedTimer;
    };
}

#endif
