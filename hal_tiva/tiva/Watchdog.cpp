#include "hal_tiva/tiva/Watchdog.hpp"
#include "infra/util/ReallyAssert.hpp"
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
    constexpr uint32_t numberOfWatchdogs = 2u;
    constexpr uint32_t precisionInternalOscillatorFrequency = 16000000u;

    uint32_t ToTicks(uint32_t clockFrequency, infra::Duration duration)
    {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        really_assert(microseconds > 0);
        auto ticks = (static_cast<uint64_t>(clockFrequency) * static_cast<uint64_t>(microseconds)) / 1000000u;
        really_assert(ticks > 0 && ticks <= std::numeric_limits<uint32_t>::max());
        return static_cast<uint32_t>(ticks);
    }
}

namespace hal::tiva
{
    Watchdog::Watchdog(uint8_t watchdogIndex, const Config& config)
        : ImmediateInterruptHandler(WATCHDOG0_IRQn, config.interruptPriority, [this]()
              {
                  HandleInterrupt();
              })
        , watchdogIndex(watchdogIndex)
        , timeout(config.timeout)
    {
        really_assert(watchdogIndex < numberOfWatchdogs);

        EnablePeripheralClock();

        auto& watchdog = Peripheral();

        Unlock();

        watchdog.LOAD = ToTicks(ClockFrequency(), config.timeout);
        WaitForWriteComplete();

        watchdog.ICR = 0;
        WaitForWriteComplete();

        // The destructor only gates the clock, so CTL keeps its previous contents and has to be written in full rather than or-ed into
        watchdog.CTL = config.resetOnMissedInterrupt ? ctlResetEnable : 0;
        WaitForWriteComplete();
    }

    Watchdog::~Watchdog()
    {
        NVIC_DisableIRQ(WATCHDOG0_IRQn);
        NVIC_ClearPendingIRQ(WATCHDOG0_IRQn);

        DisablePeripheralClock();
    }

    void Watchdog::Refresh()
    {
        Peripheral().ICR = 0;
        WaitForWriteComplete();
    }

    infra::Duration Watchdog::EarlyWarningPeriod() const
    {
        return timeout;
    }

    void Watchdog::Start(const infra::Function<void()>& onEarlyWarning)
    {
        this->onEarlyWarning = onEarlyWarning;

        // Setting INTEN starts the counter, and it can only be cleared again by a reset
        Peripheral().CTL |= ctlIntEnable;
        WaitForWriteComplete();
    }

    WATCHDOG0_Type& Watchdog::Peripheral() const
    {
        return watchdogIndex == 0 ? *WATCHDOG0 : *WATCHDOG1;
    }

    uint32_t Watchdog::ClockFrequency() const
    {
        return watchdogIndex == 0 ? SystemCoreClock : precisionInternalOscillatorFrequency;
    }

    void Watchdog::EnablePeripheralClock() const
    {
        SYSCTL->RCGCWD |= 1u << watchdogIndex;

        while ((SYSCTL->PRWD & (1u << watchdogIndex)) == 0)
        {
            // Wait for peripheral clock to be ready
        }
    }

    void Watchdog::DisablePeripheralClock() const
    {
        SYSCTL->RCGCWD &= ~(1u << watchdogIndex);
    }

    void Watchdog::Unlock() const
    {
        // Registers are left unlocked because the interrupt handler has to write ICR on every timeout
        Peripheral().LOCK = lockUnlockKey;
        WaitForWriteComplete();
    }

    void Watchdog::WaitForWriteComplete() const
    {
        if (watchdogIndex == 0)
            return;

        while ((Peripheral().CTL & ctlWriteComplete) == 0)
        {
            // Watchdog 1 is in a separate clock domain, writes only complete after WRC is set
        }
    }

    void Watchdog::HandleInterrupt()
    {
        // Watchdog 0 and 1 share one vector, so an interrupt raised by the other unit is not ours to report
        if ((Peripheral().MIS & misTimeout) == 0)
            return;

        onEarlyWarning();
    }
}
