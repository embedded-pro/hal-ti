#ifndef VALIDATION_ETHERNET_COMMANDS_HPP
#define VALIDATION_ETHERNET_COMMANDS_HPP

#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>

namespace validation
{
    class EthernetCommands
        : public services::TerminalCommands
    {
    public:
        explicit EthernetCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status LinkStatus(const Arguments& arguments);
        Status Close(const Arguments& arguments);

    private:
        Context& context;
        std::array<Command, 3> commands;
    };
}

#endif
