#include "validation/firmware/WatchDogFactory.hpp"
#include "BoardProfile.hpp"

namespace validation
{
    namespace
    {
        using services::HilStatus;

        constexpr std::array<const char*, 4> startKeys{ { "timeout", "reset", "feed", "pin" } };
        constexpr services::HilOwner warningPinOwner = services::HilOwners::extension + 1;
    }

    TivaWatchDogFactory::WarningToggle::WarningToggle(hal::Watchdog& watchDog, hal::GpioPin& pin)
        : watchDog(watchDog)
        , pin(pin)
    {}

    infra::Duration TivaWatchDogFactory::WarningToggle::EarlyWarningPeriod() const
    {
        return watchDog.EarlyWarningPeriod();
    }

    void TivaWatchDogFactory::WarningToggle::Start(const infra::Function<void()>& onEarlyWarning)
    {
        this->onEarlyWarning = onEarlyWarning;
        watchDog.Start([this]()
            {
                pin.Set(!pin.GetOutputLatch());
                this->onEarlyWarning();
            });
    }

    void TivaWatchDogFactory::WarningToggle::Refresh()
    {
        watchDog.Refresh();
    }

    TivaWatchDogFactory::TivaWatchDogFactory(const services::HilPinNaming& naming, services::HilPinPool& pins)
        : naming(naming)
        , warningPins(pins, warningPinOwner)
    {}

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
        std::optional<HilPinId> pin;
        return Parse(arguments, config, pin);
    }

    HilStatus TivaWatchDogFactory::Create(uint8_t index, infra::Duration timeout, const services::HilArguments& arguments, hal::Watchdog*& created)
    {
        hal::tiva::WatchDog::Config config;
        std::optional<HilPinId> pin;
        Parse(arguments, config, pin);
        config.timeout = timeout;

        hal::GpioPin* gpio = nullptr;
        if (pin)
        {
            HilStatus status = warningPins.Claim(*pin, services::HilPinPool::Use::exclusive, gpio);
            if (status != HilStatus::done)
                return status;

            gpio->Config(hal::PinConfigType::output, false);
        }

        created = &watchDog.emplace(index, config);
        if (gpio != nullptr)
            created = &toggle.emplace(*watchDog, *gpio);

        return HilStatus::done;
    }

    HilStatus TivaWatchDogFactory::Parse(const services::HilArguments& arguments, hal::tiva::WatchDog::Config& config, std::optional<HilPinId>& pin) const
    {
        HilStatus status = HilStatus::done;
        config.interruptPriority = hal::cortex::InterruptPriority::lowest;
        arguments.Flag("reset", config.resetOnMissedInterrupt, status);
        arguments.Pin("pin", naming, pin, status);
        return status;
    }
}
