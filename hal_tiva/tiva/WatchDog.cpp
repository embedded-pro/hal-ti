#include "hal_tiva/tiva/WatchDog.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <limits>

extern "C" uint32_t SystemCoreClock;

extern "C" void WatchDog_Handler()
{
    hal::cortex::InterruptTable::Instance().Invoke(WATCHDOG0_IRQn);
}

namespace
{
    constexpr uint32_t misTimeout = 1u << 0;
    constexpr uint32_t ctlIntEnable = 1u << 0;
    constexpr uint32_t ctlResetEnable = 1u << 1;
    constexpr uint32_t ctlWriteComplete = 1u << 31;
    constexpr uint32_t lockUnlockKey = 0x1ACCE551u;
    constexpr uint32_t numberOfWatchDogs = 2u;
    constexpr uint32_t precisionInternalOscillatorFrequency = 16000000u;

    uint64_t ToMicroseconds(infra::Duration duration)
    {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        really_assert(microseconds > 0);
        return static_cast<uint64_t>(microseconds);
    }

    uint32_t ToTicks(uint32_t clockFrequency, infra::Duration duration)
    {
        auto ticks = (static_cast<uint64_t>(clockFrequency) * ToMicroseconds(duration)) / 1000000u;
        really_assert(ticks > 0 && ticks <= std::numeric_limits<uint32_t>::max());
        return static_cast<uint32_t>(ticks);
    }

    uint32_t ToNumberOfTimeouts(infra::Duration expirationTimeout, infra::Duration timeout)
    {
        auto timeoutMicroseconds = ToMicroseconds(timeout);
        auto count = (ToMicroseconds(expirationTimeout) + timeoutMicroseconds - 1) / timeoutMicroseconds;
        return static_cast<uint32_t>(std::max<uint64_t>(count, 1));
    }
}

namespace hal::tiva
{
    WatchDog::WatchDog(uint8_t watchDogIndex, const infra::Function<void()>& onExpired, const Config& config)
        : ImmediateInterruptHandler(WATCHDOG0_IRQn, config.interruptPriority, [this]()
              {
                  HandleInterrupt();
              })
        , watchDogIndex(watchDogIndex)
        , reloadValue(ToTicks(ClockFrequency(), config.timeout))
        , expirationCount(ToNumberOfTimeouts(config.expirationTimeout, config.timeout))
        , onExpired(onExpired)
    {
        really_assert(watchDogIndex < numberOfWatchDogs);
        really_assert(config.feedTimerInterval > infra::Duration::zero());

        EnablePeripheralClock();

        auto& watchDog = Peripheral();

        Unlock();

        watchDog.LOAD = reloadValue;
        WaitForWriteComplete();

        watchDog.ICR = 0;
        WaitForWriteComplete();

        // The destructor only gates the clock, so CTL keeps its previous contents and has to be written in full rather than or-ed into
        watchDog.CTL = config.resetOnMissedInterrupt ? ctlResetEnable : 0;
        WaitForWriteComplete();

        watchDog.CTL |= ctlIntEnable;
        WaitForWriteComplete();

        feedTimer.Start(config.feedTimerInterval, [this]()
            {
                Feed();
            });
    }

    WatchDog::~WatchDog()
    {
        feedTimer.Cancel();

        NVIC_DisableIRQ(WATCHDOG0_IRQn);
        NVIC_ClearPendingIRQ(WATCHDOG0_IRQn);

        DisablePeripheralClock();
    }

    void WatchDog::Refresh()
    {
        Peripheral().ICR = 0;
        WaitForWriteComplete();
    }

    WATCHDOG0_Type& WatchDog::Peripheral() const
    {
        return watchDogIndex == 0 ? *WATCHDOG0 : *WATCHDOG1;
    }

    uint32_t WatchDog::ClockFrequency() const
    {
        return watchDogIndex == 0 ? SystemCoreClock : precisionInternalOscillatorFrequency;
    }

    void WatchDog::EnablePeripheralClock() const
    {
        SYSCTL->RCGCWD |= 1u << watchDogIndex;

        while ((SYSCTL->PRWD & (1u << watchDogIndex)) == 0)
        {
            // Wait for peripheral clock to be ready
        }
    }

    void WatchDog::DisablePeripheralClock() const
    {
        SYSCTL->RCGCWD &= ~(1u << watchDogIndex);
    }

    void WatchDog::Unlock() const
    {
        // Registers are left unlocked because the interrupt handler has to write ICR on every timeout
        Peripheral().LOCK = lockUnlockKey;
        WaitForWriteComplete();
    }

    void WatchDog::WaitForWriteComplete() const
    {
        if (watchDogIndex == 0)
            return;

        while ((Peripheral().CTL & ctlWriteComplete) == 0)
        {
            // Watchdog 1 is in a separate clock domain, writes only complete after WRC is set
        }
    }

    void WatchDog::Feed()
    {
        missedFeeds = 0;
    }

    void WatchDog::HandleInterrupt()
    {
        // Watchdog 0 and 1 share one vector, so an interrupt raised by the other unit is not ours to count or clear
        if ((Peripheral().MIS & misTimeout) == 0)
            return;

        Refresh();

        if (++missedFeeds == expirationCount)
            onExpired();
    }
}
