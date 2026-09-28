#include "services/hil/commands/UnsupportedCommands.hpp"
#include "validation/firmware/EthernetGroup.hpp"
#include <array>

namespace validation
{
    namespace
    {
        constexpr std::array<const char*, 3> commandNames{ { "eth.open", "eth.status", "eth.close" } };
    }

    void CreateEthernetGroup(services::hil::Context& context)
    {
        static services::hil::UnsupportedCommands::WithMaxCommands<commandNames.size()> ethernet{ context, infra::MakeRange(commandNames) };
    }
}
