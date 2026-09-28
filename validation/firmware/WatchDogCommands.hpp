#ifndef VALIDATION_WATCH_DOG_COMMANDS_HPP
#define VALIDATION_WATCH_DOG_COMMANDS_HPP

#include "hal_tiva/tiva/WatchDog.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <atomic>
#include <optional>

namespace validation
{
    class WatchDogCommands
        : public services::TerminalCommands
    {
    public:
        explicit WatchDogCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Start(const Arguments& arguments);
        Status Feed(const Arguments& arguments);

        void EarlyWarning();
        void Report();

    private:
        Context& context;
        std::optional<uint8_t> index;
        std::optional<hal::tiva::WatchDog> watchDog;
        bool autoFeed = true;
        std::atomic<uint32_t> warnings{ 0 };
        std::atomic<bool> reportPending{ false };
        std::array<Command, 2> commands;
    };
}

#endif
