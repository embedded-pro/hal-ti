#include "validation/firmware/WatchDogFactory.hpp"
#include "BoardProfile.hpp"

namespace validation
{
    namespace
    {
        using services::HilStatus;

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

    HilStatus TivaWatchDogFactory::Prepare(uint8_t, const services::HilArguments& arguments)
    {
        hal::tiva::WatchDog::Config config;
        return Parse(arguments, config);
    }

    HilStatus TivaWatchDogFactory::Create(uint8_t index, infra::Duration timeout, const services::HilArguments& arguments, hal::Watchdog*& created)
    {
        hal::tiva::WatchDog::Config config;
        Parse(arguments, config);
        config.timeout = timeout;
        created = &watchDog.emplace(index, config);
        return HilStatus::done;
    }

    HilStatus TivaWatchDogFactory::Parse(const services::HilArguments& arguments, hal::tiva::WatchDog::Config& config) const
    {
        HilStatus status = HilStatus::done;
        config.interruptPriority = hal::cortex::InterruptPriority::lowest;
        arguments.Flag("reset", config.resetOnMissedInterrupt, status);
        return status;
    }
}
