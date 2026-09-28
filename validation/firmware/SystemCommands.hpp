#ifndef VALIDATION_SYSTEM_COMMANDS_HPP
#define VALIDATION_SYSTEM_COMMANDS_HPP

#include "infra/timer/Timer.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>

namespace validation
{
    const char* ReadAndClearResetCause();

    class SystemCommands
        : public services::TerminalCommands
    {
    public:
        SystemCommands(Context& context, const char* resetCause);

        infra::MemoryRange<const Command> Commands() override;

        void PrintBoot();

    private:
        Status Ping(const Arguments& arguments);
        Status Info(const Arguments& arguments);
        Status Reset(const Arguments& arguments);
        Status Delay(const Arguments& arguments);
        Status BoardPins(const Arguments& arguments);

    private:
        Context& context;
        const char* resetCause;
        infra::TimerSingleShot timer;
        infra::TimerSingleShot resetTimer;
        std::array<Command, 5> commands;
    };
}

#endif
