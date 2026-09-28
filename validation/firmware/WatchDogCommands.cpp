#include "validation/firmware/WatchDogCommands.hpp"
#include "BoardProfile.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace validation
{
    namespace
    {
        constexpr uint32_t maximumTimeoutMs = 30000;

        constexpr std::array<Choice<bool>, 2> feedModes{ {
            { "auto", true },
            { "manual", false },
        } };
    }

    WatchDogCommands::WatchDogCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<WatchDogCommands, &WatchDogCommands::Start>("wdt.start", "<index> timeout= [reset=] [feed=]", *this, context.response),
              Bind<WatchDogCommands, &WatchDogCommands::Feed>("wdt.feed", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> WatchDogCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status WatchDogCommands::Start(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "timeout", "reset", "feed" }))
            return Status::usage;

        uint32_t requested = 0;
        uint32_t timeout = 0;
        bool feed = true;
        hal::tiva::WatchDog::Config config;
        config.interruptPriority = hal::cortex::InterruptPriority::lowest;

        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::watchDogs - 1, status);
        arguments.Number("timeout", timeout, 1, maximumTimeoutMs, status);
        arguments.Flag("reset", config.resetOnMissedInterrupt, status);
        arguments.Select("feed", feed, feedModes, status);
        if (status != Status::done)
            return status;

        if (!arguments.Has("timeout"))
            return Status::usage;

        if (watchDog)
            return Status::busy;

        config.timeout = std::chrono::milliseconds(timeout);
        autoFeed = feed;
        warnings = 0;
        index = static_cast<uint8_t>(requested);
        watchDog.emplace(*index, config);
        watchDog->Start([this]()
            {
                EarlyWarning();
            });

        context.response.Ok();
        return Status::done;
    }

    Status WatchDogCommands::Feed(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        uint32_t requested = 0;
        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::watchDogs - 1, status);
        if (status != Status::done)
            return status;

        if (index != requested)
            return Status::notOpen;

        watchDog->Refresh();
        context.response.Ok();
        return Status::done;
    }

    void WatchDogCommands::EarlyWarning()
    {
        warnings.fetch_add(1, std::memory_order_relaxed);

        if (!reportPending.exchange(true))
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Report();
                });
    }

    void WatchDogCommands::Report()
    {
        reportPending = false;

        if (autoFeed)
            watchDog->Refresh();

        context.response.Event("wdt") << " index=" << *index << " warning=" << warnings.load();
    }
}
