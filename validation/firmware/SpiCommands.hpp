#ifndef VALIDATION_SPI_COMMANDS_HPP
#define VALIDATION_SPI_COMMANDS_HPP

#include "hal_tiva/synchronous_tiva/SynchronousSpiMaster.hpp"
#include "hal_tiva/tiva/SpiMaster.hpp"
#include "infra/timer/Timer.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <optional>
#include <variant>

namespace validation
{
    class SpiCommands
        : public services::TerminalCommands
    {
    public:
        explicit SpiCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        static constexpr std::size_t capacity = 64;

        Status Open(const Arguments& arguments);
        Status Transfer(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status Find(const Arguments& arguments);
        void Done(uint32_t generation);
        void Timeout();
        void Report();

    private:
        Context& context;
        std::optional<uint8_t> index;
        std::variant<std::monostate, hal::tiva::SpiMaster, hal::tiva::SynchronousSpiMaster> driver;
        std::array<uint8_t, capacity> transmitBuffer{};
        std::array<uint8_t, capacity> receiveBuffer{};
        std::size_t reportSize = 0;
        uint32_t generation = 0;
        bool transferring = false;
        bool awaiting = false;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
