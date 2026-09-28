#ifndef VALIDATION_EEPROM_COMMANDS_HPP
#define VALIDATION_EEPROM_COMMANDS_HPP

#include "hal_tiva/tiva/Eeprom.hpp"
#include "infra/timer/Timer.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <optional>

namespace validation
{
    class EepromCommands
        : public services::TerminalCommands
    {
    public:
        explicit EepromCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        static constexpr std::size_t capacity = 112;

        Status Write(const Arguments& arguments);
        Status Read(const Arguments& arguments);
        Status Erase(const Arguments& arguments);

        hal::tiva::Eeprom& Instance();
        void Start();
        void Done();
        void Timeout();

    private:
        Context& context;
        std::optional<hal::tiva::Eeprom> eeprom;
        std::array<uint8_t, capacity> buffer{};
        bool operating = false;
        bool awaiting = false;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
