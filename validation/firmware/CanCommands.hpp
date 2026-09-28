#ifndef VALIDATION_CAN_COMMANDS_HPP
#define VALIDATION_CAN_COMMANDS_HPP

#include "hal_tiva/tiva/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <optional>

namespace validation
{
    class CanCommands
        : public services::TerminalCommands
    {
    public:
        explicit CanCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Send(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status Find(const Arguments& arguments);
        void Received(hal::Can::Id id, const hal::Can::Message& data);
        void Error(hal::tiva::Can::Error error);
        void SendDone(uint32_t generation, bool success);
        void SendTimeout();

    private:
        Context& context;
        std::optional<uint8_t> index;
        std::optional<hal::tiva::Can::WithMaxRxBuffer<8>> can;
        uint32_t generation = 0;
        bool transmitting = false;
        bool awaiting = false;
        bool closing = false;
        std::optional<hal::tiva::Can::Error> lastError;
        infra::TimePoint lastErrorTime;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
