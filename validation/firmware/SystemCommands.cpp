#include "validation/firmware/SystemCommands.hpp"
#include "BoardProfile.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include DEVICE_HEADER

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        constexpr uint32_t rescExternal = 1u << 0;
        constexpr uint32_t rescPowerOn = 1u << 1;
        constexpr uint32_t rescBrownOut = 1u << 2;
        constexpr uint32_t rescWatchDog0 = 1u << 3;
        constexpr uint32_t rescSoftware = 1u << 4;
        constexpr uint32_t rescWatchDog1 = 1u << 5;
        constexpr uint32_t rescMainOscillatorFailure = 1u << 16;

        constexpr std::array<Choice<uint32_t>, 7> resetCauses{ {
            { "wdt0", rescWatchDog0 },
            { "wdt1", rescWatchDog1 },
            { "sw", rescSoftware },
            { "moscfail", rescMainOscillatorFailure },
            { "bor", rescBrownOut },
            { "por", rescPowerOn },
            { "ext", rescExternal },
        } };

        constexpr infra::Duration resetFlushTime = std::chrono::milliseconds(20);
        constexpr uint32_t maximumDelayMs = 600000;
    }

    const char* ReadAndClearResetCause()
    {
        const uint32_t cause = SYSCTL->RESC;
        SYSCTL->RESC = 0;

        for (const auto& entry : resetCauses)
            if ((cause & entry.value) != 0)
                return entry.name;

        return "unknown";
    }

    SystemCommands::SystemCommands(Context& context, const char* resetCause)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , resetCause(resetCause)
        , commands{ {
              Bind<SystemCommands, &SystemCommands::Ping>("ping", "", *this, context.response),
              Bind<SystemCommands, &SystemCommands::Info>("info", "", *this, context.response),
              Bind<SystemCommands, &SystemCommands::Reset>("reset", "", *this, context.response),
              Bind<SystemCommands, &SystemCommands::Delay>("delay", "<ms>", *this, context.response),
              Bind<SystemCommands, &SystemCommands::BoardPins>("board.pins", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> SystemCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    void SystemCommands::PrintBoot()
    {
        context.response.Event("boot") << " board=" << board::name << " family=" << board::family << " sysclk=" << SystemCoreClock << " reset=" << resetCause;
    }

    Status SystemCommands::Ping(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        context.response.Ok();
        return Status::done;
    }

    Status SystemCommands::Info(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        auto uid = hal::tiva::UniqueDeviceId();
        auto line = context.response.Ok();
        line << " board=" << board::name << " family=" << board::family << " sysclk=" << SystemCoreClock << " reset=" << resetCause << " uid=";

        if (uid.empty())
            line << "none";
        else
            line.Hex(uid);

        return Status::done;
    }

    Status SystemCommands::Reset(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        resetTimer.Start(resetFlushTime, []()
            {
                NVIC_SystemReset();
            });

        return Status::done;
    }

    Status SystemCommands::Delay(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        uint32_t milliseconds = 0;
        Status status = Status::done;
        arguments.NumberAt(0, milliseconds, 0, maximumDelayMs, status);
        if (status != Status::done)
            return status;

        if (timer.Armed())
            return Status::busy;

        timer.Start(std::chrono::milliseconds(milliseconds), [this]()
            {
                context.response.Ok();
            });

        return Status::done;
    }

    Status SystemCommands::BoardPins(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        auto line = context.response.Ok();
        const char* separator = " ";

        for (const auto& alias : board::aliases)
        {
            line << separator << alias.name << "=" << alias.pin;
            separator = ",";
        }

        return Status::done;
    }
}
