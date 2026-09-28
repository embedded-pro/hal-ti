#ifndef VALIDATION_WATCH_DOG_FACTORY_HPP
#define VALIDATION_WATCH_DOG_FACTORY_HPP

#include "hal_tiva/tiva/WatchDog.hpp"
#include "services/hil/commands/WatchDogCommands.hpp"
#include <optional>

namespace validation
{
    class TivaWatchDogFactory
        : public services::hil::WatchDogFactory
    {
    public:
        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> StartKeys() const override;
        services::hil::Status Prepare(uint8_t index, const services::hil::Arguments& arguments) override;
        services::hil::Status Create(uint8_t index, infra::Duration timeout, const services::hil::Arguments& arguments, hal::Watchdog*& watchDog) override;

    private:
        services::hil::Status Parse(const services::hil::Arguments& arguments, hal::tiva::WatchDog::Config& config) const;

    private:
        std::optional<hal::tiva::WatchDog> watchDog;
    };
}

#endif
