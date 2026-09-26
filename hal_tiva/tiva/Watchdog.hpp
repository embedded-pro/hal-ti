#ifndef HAL_WATCHDOG_TIVA_HPP
#define HAL_WATCHDOG_TIVA_HPP

#include DEVICE_HEADER
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/interfaces/Watchdog.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>

namespace hal::tiva
{
    class Watchdog
        : public hal::Watchdog
        , private hal::cortex::ImmediateInterruptHandler
    {
    public:
        struct Config
        {
            constexpr Config()
            {}

            infra::Duration timeout{ std::chrono::milliseconds(50) };
            bool resetOnMissedInterrupt{ true };
            hal::cortex::InterruptPriority interruptPriority{ hal::cortex::InterruptPriority::normal };
        };

        explicit Watchdog(uint8_t watchdogIndex, const Config& config = Config());
        ~Watchdog();

        void Refresh() override;
        infra::Duration EarlyWarningPeriod() const override;
        void Start(const infra::Function<void()>& onEarlyWarning) override;

    private:
        WATCHDOG0_Type& Peripheral() const;
        uint32_t ClockFrequency() const;
        void EnablePeripheralClock() const;
        void DisablePeripheralClock() const;
        void Unlock() const;
        void WaitForWriteComplete() const;
        void HandleInterrupt();

        uint8_t watchdogIndex;
        infra::Duration timeout;
        infra::Function<void()> onEarlyWarning;
    };
}

#endif
