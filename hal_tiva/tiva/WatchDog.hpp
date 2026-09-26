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
    class WatchDog
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

        explicit WatchDog(uint8_t watchDogIndex, const Config& config = Config());
        ~WatchDog();

        infra::Duration EarlyWarningPeriod() const override;
        void Start(const infra::Function<void()>& onEarlyWarning) override;
        void Refresh() override;

    private:
        WATCHDOG0_Type& Peripheral() const;
        uint32_t ClockFrequency() const;
        void EnablePeripheralClock() const;
        void DisablePeripheralClock() const;
        void Unlock() const;
        void WaitForWriteComplete() const;
        void HandleInterrupt();

        uint8_t watchDogIndex;
        infra::Duration timeout;
        uint32_t reloadValue{ 0 };
        infra::Function<void()> onEarlyWarning;
    };
}

#endif
