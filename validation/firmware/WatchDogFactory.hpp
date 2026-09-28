#ifndef VALIDATION_WATCH_DOG_FACTORY_HPP
#define VALIDATION_WATCH_DOG_FACTORY_HPP

#include "hal_tiva/tiva/WatchDog.hpp"
#include "services/hil/HilPinPool.hpp"
#include "services/hil/commands/HilWatchDogCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>

namespace validation
{
    class TivaWatchDogFactory
        : public services::HilWatchDogFactory
    {
    public:
        TivaWatchDogFactory(const services::HilPinNaming& naming, services::HilPinPool& pins);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> StartKeys() const override;
        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override;
        services::HilStatus Create(uint8_t index, infra::Duration timeout, const services::HilArguments& arguments, hal::Watchdog*& watchDog) override;

    private:
        class WarningToggle
            : public hal::Watchdog
        {
        public:
            WarningToggle(hal::Watchdog& watchDog, hal::GpioPin& pin);

            infra::Duration EarlyWarningPeriod() const override;
            void Start(const infra::Function<void()>& onEarlyWarning) override;
            void Refresh() override;

        private:
            hal::Watchdog& watchDog;
            hal::GpioPin& pin;
            infra::Function<void()> onEarlyWarning;
        };

        services::HilStatus Parse(const services::HilArguments& arguments, hal::tiva::WatchDog::Config& config, std::optional<HilPinId>& pin) const;

    private:
        const services::HilPinNaming& naming;
        services::HilPinOwner warningPins;
        std::optional<hal::tiva::WatchDog> watchDog;
        std::optional<WarningToggle> toggle;
    };
}

#endif
