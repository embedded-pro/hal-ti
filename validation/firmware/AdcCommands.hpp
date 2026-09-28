#ifndef VALIDATION_ADC_COMMANDS_HPP
#define VALIDATION_ADC_COMMANDS_HPP

#include "hal_tiva/synchronous_tiva/SynchronousAdc.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/BoundedVector.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <atomic>
#include <optional>
#include <variant>

namespace validation
{
    class AdcCommands
        : public services::TerminalCommands
    {
    public:
        explicit AdcCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        static constexpr std::size_t maximumSteps = 8;
        static constexpr std::size_t maximumValues = 64;
        static constexpr std::size_t slotCount = 2;

        struct Sequencer
        {
            uint8_t adc = 0;
            uint8_t sequencer = 0;
            std::size_t fifoSteps = 0;
            infra::BoundedVector<hal::tiva::AnalogPin>::WithMaxSize<maximumSteps> inputs;
            std::array<hal::tiva::Adc::DigitalComparatorConfig, maximumSteps> comparators{};
            hal::tiva::Adc::Config asyncConfig{};
            hal::tiva::SynchronousAdc::Config syncConfig{};
            std::variant<std::monostate, hal::tiva::Adc, hal::tiva::SynchronousAdc> driver;
        };

        Status Open(const Arguments& arguments);
        Status Measure(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status Find(const Arguments& arguments, std::size_t& slot);
        Status ParseComparators(infra::BoundedConstString text, std::size_t steps, Sequencer& sequencer, std::size_t& comparatorSteps) const;
        void Collect(hal::AdcMultiChannel::Samples samples);
        void Finish();
        void Timeout();
        void Report();

    private:
        Context& context;
        std::array<std::optional<Sequencer>, slotCount> slots;
        std::array<uint16_t, maximumValues> values{};
        std::atomic<std::size_t> valueCount{ 0 };
        std::atomic<uint32_t> runsRemaining{ 0 };
        std::optional<std::size_t> measuringSlot;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
