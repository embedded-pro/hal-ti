#include "validation/firmware/GpioCommands.hpp"
#include "BoardProfile.hpp"
#include <concepts>

namespace validation
{
    namespace
    {
        template<class G>
        concept GpioWithInterruptType = requires(G& gpio, const infra::Function<void()>& action) {
            gpio.EnableInterrupt(hal::tiva::Port::A, uint8_t{ 0 }, action, hal::InterruptTrigger::risingEdge, hal::InterruptType::immediate);
        };

        constexpr uint8_t portsWithInterrupts = GpioWithInterruptType<hal::tiva::Gpio> ? 15 : 6;

        enum class Mode : uint8_t
        {
            input,
            output,
            openDrain,
        };

        enum class Edge : uint8_t
        {
            rising,
            falling,
            both,
            off,
        };

        constexpr std::array<Choice<Mode>, 3> modes{ {
            { "in", Mode::input },
            { "out", Mode::output },
            { "od", Mode::openDrain },
        } };

        constexpr std::array<Choice<hal::tiva::Drive>, 3> pulls{ {
            { "none", hal::tiva::Drive::None },
            { "up", hal::tiva::Drive::Up },
            { "down", hal::tiva::Drive::Down },
        } };

        constexpr std::array<Choice<hal::tiva::Current>, 3> currents{ {
            { "2", hal::tiva::Current::_2mA },
            { "4", hal::tiva::Current::_4mA },
            { "8", hal::tiva::Current::_8mA },
        } };

        constexpr std::array<Choice<Edge>, 4> edges{ {
            { "rising", Edge::rising },
            { "falling", Edge::falling },
            { "both", Edge::both },
            { "off", Edge::off },
        } };

        constexpr std::array<Choice<hal::InterruptType>, 2> interruptTypes{ {
            { "immediate", hal::InterruptType::immediate },
            { "dispatched", hal::InterruptType::dispatched },
        } };

        constexpr std::array<Choice<bool>, 2> levels{ {
            { "0", false },
            { "1", true },
        } };
    }

    GpioCommands::GpioCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<GpioCommands, &GpioCommands::Configure>("gpio.cfg", "<pin> <in|out|od> [pull=] [drive=]", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Set>("gpio.set", "<pin> <0|1>", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Get>("gpio.get", "<pin>", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Pulse>("gpio.pulse", "<pin> <count> <periodMs>", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Interrupt>("gpio.irq", "<pin> <rising|falling|both|off> [type=]", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Count>("gpio.count", "<pin> [clear=]", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Release>("gpio.release", "<pin>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> GpioCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status GpioCommands::Configure(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "pull", "drive" }))
            return Status::usage;

        PinId id{};
        auto pull = hal::tiva::Drive::None;
        auto mode = Mode::input;
        auto current = hal::tiva::Current::_2mA;
        Status status = Status::done;
        arguments.PinAt(0, id, status, &pull);
        arguments.SelectAt(1, mode, modes, status);
        arguments.Select("pull", pull, pulls, status);
        arguments.Select("drive", current, currents, status);
        if (status != Status::done)
            return status;

        if (mode == Mode::openDrain)
        {
            if (arguments.Has("pull") && pull != hal::tiva::Drive::None)
                return Status::usage;

            pull = hal::tiva::Drive::OpenDrain;
        }

        Entry* entry = nullptr;
        for (auto& candidate : entries)
            if (candidate.id == id)
                entry = &candidate;

        if (entry != nullptr)
        {
            if (entry == pulseEntry)
                return Status::busy;

            Free(*entry);
        }
        else
            for (auto& candidate : entries)
                if (!candidate.id && entry == nullptr)
                    entry = &candidate;

        if (entry == nullptr)
            return Status::busy;

        status = context.pins.Claim(id, owner::gpio, PinPool::Use::exclusive, entry->pin, pull, current);
        if (status != Status::done)
            return status;

        entry->id = id;
        entry->output = mode != Mode::input;
        entry->count = 0;

        if (mode == Mode::input)
            entry->pin->Config(hal::PinConfigType::input);
        else
            entry->pin->Config(hal::PinConfigType::output, mode == Mode::openDrain);

        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::Set(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        Entry* entry = nullptr;
        bool level = false;
        Status status = Find(arguments, entry);
        arguments.SelectAt(1, level, levels, status);
        if (status != Status::done)
            return status;

        entry->pin->Set(level);
        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::Get(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Entry* entry = nullptr;
        Status status = Find(arguments, entry);
        if (status != Status::done)
            return status;

        context.response.Ok() << " value=" << (entry->pin->Get() ? 1u : 0u);
        return Status::done;
    }

    Status GpioCommands::Pulse(const Arguments& arguments)
    {
        if (!arguments.Shape(3, 3, {}))
            return Status::usage;

        Entry* entry = nullptr;
        uint32_t count = 0;
        uint32_t period = 0;
        Status status = Find(arguments, entry);
        arguments.NumberAt(1, count, 1, 1000000, status);
        arguments.NumberAt(2, period, 1, 60000, status);
        if (status != Status::done)
            return status;

        if (!entry->output)
            return Status::usage;

        if (pulseEntry != nullptr)
            return Status::busy;

        pulseEntry = entry;
        pulsesRemaining = count;
        pulseTimer.Start(std::chrono::milliseconds(period), [this]()
            {
                Toggle();
            });

        return Status::done;
    }

    Status GpioCommands::Interrupt(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "type" }))
            return Status::usage;

        Entry* entry = nullptr;
        auto edge = Edge::off;
        auto type = hal::InterruptType::dispatched;
        Status status = Find(arguments, entry);
        arguments.SelectAt(1, edge, edges, status);
        arguments.Select("type", type, interruptTypes, status);
        if (status != Status::done)
            return status;

        if (static_cast<uint8_t>(entry->id->port) >= portsWithInterrupts)
            return Status::unsupported;

        if (entry->interruptEnabled)
        {
            entry->pin->DisableInterrupt();
            entry->interruptEnabled = false;
        }

        if (edge != Edge::off)
        {
            auto counter = &entry->count;
            entry->pin->EnableInterrupt([counter]()
                {
                    counter->fetch_add(1, std::memory_order_relaxed);
                },
                static_cast<hal::InterruptTrigger>(edge), type);
            entry->interruptEnabled = true;
        }

        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::Count(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "clear" }))
            return Status::usage;

        Entry* entry = nullptr;
        bool clear = false;
        Status status = Find(arguments, entry);
        arguments.Flag("clear", clear, status);
        if (status != Status::done)
            return status;

        uint32_t count = clear ? entry->count.exchange(0) : entry->count.load();
        context.response.Ok() << " count=" << count;
        return Status::done;
    }

    Status GpioCommands::Release(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Entry* entry = nullptr;
        Status status = Find(arguments, entry);
        if (status != Status::done)
            return status;

        if (entry == pulseEntry)
            return Status::busy;

        Free(*entry);
        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::Find(const Arguments& arguments, Entry*& entry)
    {
        PinId id{};
        Status status = Status::done;
        arguments.PinAt(0, id, status);
        if (status != Status::done)
            return status;

        for (auto& candidate : entries)
            if (candidate.id == id)
            {
                entry = &candidate;
                return Status::done;
            }

        return Status::notOpen;
    }

    void GpioCommands::Free(Entry& entry)
    {
        if (entry.interruptEnabled)
            entry.pin->DisableInterrupt();

        entry.pin->ResetConfig();
        context.pins.Release(*entry.id, owner::gpio);
        entry.id = std::nullopt;
        entry.pin = nullptr;
        entry.interruptEnabled = false;
    }

    void GpioCommands::Toggle()
    {
        pulseEntry->pin->Set(!pulseEntry->pin->GetOutputLatch());

        if (--pulsesRemaining == 0)
        {
            pulseTimer.Cancel();
            pulseEntry = nullptr;
            context.response.Ok();
        }
    }
}
