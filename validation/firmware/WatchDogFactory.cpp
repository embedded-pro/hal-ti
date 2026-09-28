#include "validation/firmware/WatchDogFactory.hpp"
#include "BoardProfile.hpp"

namespace validation
{
    namespace
    {
        using services::hil::Status;

        constexpr std::array<const char*, 3> startKeys{ { "timeout", "reset", "feed" } };
    }

    uint8_t TivaWatchDogFactory::Instances() const
    {
        return board::watchDogs;
    }

    infra::MemoryRange<const char* const> TivaWatchDogFactory::StartKeys() const
    {
        return infra::MakeRange(startKeys);
    }

    Status TivaWatchDogFactory::Prepare(uint8_t, const services::hil::Arguments& arguments)
    {
        hal::tiva::WatchDog::Config config;
        return Parse(arguments, config);
    }

    Status TivaWatchDogFactory::Create(uint8_t index, infra::Duration timeout, const services::hil::Arguments& arguments, hal::Watchdog*& created)
    {
        hal::tiva::WatchDog::Config config;
        Parse(arguments, config);
        config.timeout = timeout;
        created = &watchDog.emplace(index, config);
        return Status::done;
    }

    Status TivaWatchDogFactory::Parse(const services::hil::Arguments& arguments, hal::tiva::WatchDog::Config& config) const
    {
        Status status = Status::done;
        config.interruptPriority = hal::cortex::InterruptPriority::lowest;
        arguments.Flag("reset", config.resetOnMissedInterrupt, status);
        return status;
    }
}
