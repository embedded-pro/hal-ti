#include "services/hil/commands/HilUnsupportedCommands.hpp"
#include "validation/firmware/EthernetGroup.hpp"
#include <array>

namespace validation
{
    namespace
    {
        constexpr std::array<const char*, 3> commandNames{ { "eth.open", "eth.status", "eth.close" } };
    }

    void CreateEthernetGroup(services::HilContext& context)
    {
        static services::HilUnsupportedCommands::WithMaxCommands<commandNames.size()> ethernet{ context, infra::MakeRange(commandNames) };
    }
}
