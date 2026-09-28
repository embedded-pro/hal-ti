#ifndef VALIDATION_WATCH_DOG_FACTORY_HPP
#define VALIDATION_WATCH_DOG_FACTORY_HPP

#include "hal_tiva/tiva/WatchDog.hpp"
#include "services/hil/commands/HilWatchDogCommands.hpp"
#include <optional>

namespace validation
{
    class TivaWatchDogFactory
        : public services::HilWatchDogFactory
    {
    public:
        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> StartKeys() const override;
        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override;
        services::HilStatus Create(uint8_t index, infra::Duration timeout, const services::HilArguments& arguments, hal::Watchdog*& watchDog) override;

    private:
        services::HilStatus Parse(const services::HilArguments& arguments, hal::tiva::WatchDog::Config& config) const;

    private:
        std::optional<hal::tiva::WatchDog> watchDog;
    };
}

#endif
