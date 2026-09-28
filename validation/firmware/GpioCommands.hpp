#ifndef VALIDATION_GPIO_COMMANDS_HPP
#define VALIDATION_GPIO_COMMANDS_HPP

#include "infra/timer/Timer.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <atomic>
#include <optional>

namespace validation
{
    class GpioCommands
        : public services::TerminalCommands
    {
    public:
        explicit GpioCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        struct Entry
        {
            std::optional<PinId> id;
            hal::tiva::GpioPin* pin = nullptr;
            bool output = false;
            bool interruptEnabled = false;
            std::atomic<uint32_t> count{ 0 };
        };

        Status Configure(const Arguments& arguments);
        Status Set(const Arguments& arguments);
        Status Get(const Arguments& arguments);
        Status Pulse(const Arguments& arguments);
        Status Interrupt(const Arguments& arguments);
        Status Count(const Arguments& arguments);
        Status Release(const Arguments& arguments);

        Status Find(const Arguments& arguments, Entry*& entry);
        void Free(Entry& entry);
        void Toggle();

    private:
        Context& context;
        std::array<Entry, 8> entries;
        infra::TimerRepeating pulseTimer;
        Entry* pulseEntry = nullptr;
        uint32_t pulsesRemaining = 0;
        std::array<Command, 7> commands;
    };
}

#endif
