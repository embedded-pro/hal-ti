#include "validation/firmware/EthernetCommands.hpp"

namespace validation
{
    EthernetCommands::EthernetCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<EthernetCommands, &EthernetCommands::Open>("eth.open", "unsupported", *this, context.response),
              Bind<EthernetCommands, &EthernetCommands::LinkStatus>("eth.status", "unsupported", *this, context.response),
              Bind<EthernetCommands, &EthernetCommands::Close>("eth.close", "unsupported", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> EthernetCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status EthernetCommands::Open(const Arguments&)
    {
        return Status::unsupported;
    }

    Status EthernetCommands::LinkStatus(const Arguments&)
    {
        return Status::unsupported;
    }

    Status EthernetCommands::Close(const Arguments&)
    {
        return Status::unsupported;
    }
}
