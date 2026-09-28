#ifndef VALIDATION_COMPARATOR_COMMANDS_HPP
#define VALIDATION_COMPARATOR_COMMANDS_HPP

#include "hal_tiva/synchronous_tiva/SynchronousAnalogComparator.hpp"
#include "hal_tiva/tiva/AnalogComparator.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <atomic>
#include <optional>
#include <variant>

namespace validation
{
    class ComparatorCommands
        : public services::TerminalCommands
    {
    public:
        explicit ComparatorCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Read(const Arguments& arguments);
        Status Interrupt(const Arguments& arguments);
        Status Count(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status Find(const Arguments& arguments);

    private:
        Context& context;
        std::optional<uint8_t> index;
        std::variant<std::monostate, hal::tiva::AnalogComparator, hal::tiva::SynchronousAnalogComparator> driver;
        std::atomic<uint32_t> count{ 0 };
        std::array<Command, 5> commands;
    };
}

#endif
