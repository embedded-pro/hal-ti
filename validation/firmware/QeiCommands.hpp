#ifndef VALIDATION_QEI_COMMANDS_HPP
#define VALIDATION_QEI_COMMANDS_HPP

#include "hal_tiva/synchronous_tiva/SynchronousQuadratureEncoder.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <optional>

namespace validation
{
    class QeiCommands
        : public services::TerminalCommands
    {
    public:
        explicit QeiCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Read(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status Find(const Arguments& arguments);

    private:
        Context& context;
        std::optional<uint8_t> index;
        std::optional<hal::tiva::QuadratureEncoder> encoder;
        std::array<Command, 3> commands;
    };
}

#endif
