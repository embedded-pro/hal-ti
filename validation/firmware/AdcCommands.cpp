#include "validation/firmware/AdcCommands.hpp"
#include "BoardProfile.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/Tokenizer.hpp"
#include <algorithm>

namespace validation
{
    namespace
    {
        constexpr std::array<uint8_t, 4> sequencerDepths{ { 8, 4, 4, 1 } };
        constexpr uint8_t phaseCurrentAdc = 0;
        constexpr uint8_t supplyAdc = 1;
        constexpr uint8_t eFocSequencer = 0;
        constexpr uint8_t maximumDigitalComparator = 7;
        constexpr uint32_t maximumCode = 0x0fff;
        constexpr uint32_t maximumDelay = 15;
        constexpr infra::Duration measureTimeout = std::chrono::milliseconds(1000);

        constexpr std::array<Choice<uint8_t>, 7> sampleAndHolds{ {
            { "4", 0 },
            { "8", 1 },
            { "16", 2 },
            { "32", 3 },
            { "64", 4 },
            { "128", 5 },
            { "256", 6 },
        } };

        constexpr std::array<Choice<uint8_t>, 7> oversamplings{ {
            { "off", 0 },
            { "2", 1 },
            { "4", 2 },
            { "8", 3 },
            { "16", 4 },
            { "32", 5 },
            { "64", 6 },
        } };

        constexpr std::array<Choice<hal::tiva::Adc::Trigger>, 4> triggers{ {
            { "pwm0", hal::tiva::Adc::Trigger::pwmGenerator0 },
            { "pwm1", hal::tiva::Adc::Trigger::pwmGenerator1 },
            { "pwm2", hal::tiva::Adc::Trigger::pwmGenerator2 },
            { "pwm3", hal::tiva::Adc::Trigger::pwmGenerator3 },
        } };
    }

    AdcCommands::AdcCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<AdcCommands, &AdcCommands::Open>("adc.open", "<adc> <seq> [pins=] [sh=] [avg=] [delay=] [trigger=] [dcmp=] [sync=]", *this, context.response),
              Bind<AdcCommands, &AdcCommands::Measure>("adc.measure", "<adc> <seq> [n=]", *this, context.response),
              Bind<AdcCommands, &AdcCommands::Close>("adc.close", "<adc> <seq>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> AdcCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status AdcCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "pins", "sh", "avg", "delay", "trigger", "dcmp", "sync" }))
            return Status::usage;

        uint32_t adc = 0;
        uint32_t sequencer = 0;
        Status status = Status::done;
        arguments.NumberAt(0, adc, 0, board::adcs - 1, status);
        arguments.NumberAt(1, sequencer, 0, sequencerDepths.size() - 1, status);
        if (status != Status::done)
            return status;

        const bool supply = adc == supplyAdc && sequencer == eFocSequencer;
        const bool phaseCurrent = adc == phaseCurrentAdc && sequencer == eFocSequencer;
        bool synchronous = supply;
        uint8_t sampleAndHold = supply ? 6 : 1;
        uint8_t oversampling = supply ? 3 : 1;
        uint32_t delay = 4;
        bool delayEnabled = !supply;
        auto trigger = board::adcTrigger;

        arguments.Select("sh", sampleAndHold, sampleAndHolds, status);
        arguments.Select("avg", oversampling, oversamplings, status);
        arguments.Select("trigger", trigger, triggers, status);
        arguments.Flag("sync", synchronous, status);
        if (status == Status::done && arguments.Has("delay"))
        {
            delayEnabled = arguments.Key("delay") != "off";
            if (delayEnabled)
                arguments.Number("delay", delay, 0, maximumDelay, status);
        }
        if (status != Status::done)
            return status;

        if (synchronous && (arguments.Has("delay") || arguments.Has("trigger") || arguments.Has("dcmp")))
            return Status::unsupported;

        std::array<PinId, maximumSteps> pins{};
        std::size_t steps = 0;
        if (auto list = arguments.Key("pins"))
        {
            infra::Tokenizer tokens(*list, ',');
            steps = tokens.Size();
            if (steps == 0 || steps > maximumSteps)
                return Status::range;

            for (std::size_t i = 0; i != steps; ++i)
            {
                auto pin = ParsePin(tokens.Token(i));
                if (!pin)
                    return Status::pin;

                pins[i] = *pin;
            }
        }
        else if (phaseCurrent)
            steps = std::copy(board::phaseCurrentPins.begin(), board::phaseCurrentPins.end(), pins.begin()) - pins.begin();
        else if (supply)
            steps = std::copy(board::supplyPins.begin(), board::supplyPins.end(), pins.begin()) - pins.begin();
        else
            return Status::usage;

        if (steps > sequencerDepths[sequencer])
            return Status::range;

        for (const auto& slot : slots)
            if (slot && slot->adc == adc && slot->sequencer == sequencer)
                return Status::busy;

        std::optional<std::size_t> free;
        for (std::size_t i = 0; i != slots.size(); ++i)
            if (!slots[i] && !free)
                free = i;

        if (!free)
            return Status::busy;

        auto& opened = slots[*free].emplace();
        opened.adc = static_cast<uint8_t>(adc);
        opened.sequencer = static_cast<uint8_t>(sequencer);

        std::size_t comparatorSteps = 0;
        if (auto list = arguments.Key("dcmp"))
            status = ParseComparators(*list, steps, opened, comparatorSteps);

        const auto slotOwner = static_cast<Owner>(owner::adc + *free);
        for (std::size_t i = 0; i != steps && status == Status::done; ++i)
        {
            hal::tiva::GpioPin* pin = nullptr;
            status = context.pins.ClaimAdc(pins[i], slotOwner, pin);
            if (status == Status::done)
                opened.inputs.emplace_back(*pin);
        }

        if (status != Status::done)
        {
            opened.inputs.clear();
            slots[*free] = std::nullopt;
            context.pins.Release(slotOwner);
            return status;
        }

        opened.fifoSteps = steps - comparatorSteps;
        const auto oversamplingValue = oversampling != 0 ? std::make_optional(oversampling) : std::nullopt;

        if (synchronous)
        {
            opened.syncConfig.sampleAndHold = static_cast<hal::tiva::SynchronousAdc::SampleAndHold>(sampleAndHold);
            opened.syncConfig.priority = static_cast<hal::tiva::SynchronousAdc::Priority>(sequencer);
            opened.syncConfig.oversampling = oversamplingValue ? std::make_optional(static_cast<hal::tiva::SynchronousAdc::Oversampling>(*oversamplingValue)) : std::nullopt;
            opened.driver.emplace<hal::tiva::SynchronousAdc>(opened.adc, opened.sequencer, infra::MakeRange(opened.inputs), opened.syncConfig);
        }
        else
        {
            opened.asyncConfig.externalReference = false;
            opened.asyncConfig.priority = opened.sequencer;
            opened.asyncConfig.trigger = trigger;
            opened.asyncConfig.sampleAndHold = static_cast<hal::tiva::Adc::SampleAndHold>(sampleAndHold);
            opened.asyncConfig.oversampling = oversamplingValue ? std::make_optional(static_cast<hal::tiva::Adc::Oversampling>(*oversamplingValue)) : std::nullopt;
            opened.asyncConfig.samplingDelay = delayEnabled ? std::make_optional(hal::tiva::Adc::SamplingDelay(static_cast<uint8_t>(delay))) : std::nullopt;
            opened.asyncConfig.digitalComparators = comparatorSteps != 0 ? infra::MemoryRange<const hal::tiva::Adc::DigitalComparatorConfig>(opened.comparators.data(), opened.comparators.data() + steps) : infra::MemoryRange<const hal::tiva::Adc::DigitalComparatorConfig>();
            opened.asyncConfig.interruptPriority = hal::cortex::InterruptPriority::highest;
            opened.driver.emplace<hal::tiva::Adc>(opened.adc, opened.sequencer, infra::MakeRange(opened.inputs), opened.asyncConfig);
        }

        context.response.Ok();
        return Status::done;
    }

    Status AdcCommands::Measure(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "n" }))
            return Status::usage;

        std::size_t slot = 0;
        uint32_t runs = 1;
        Status status = Find(arguments, slot);
        arguments.Number("n", runs, 1, maximumValues, status);
        if (status != Status::done)
            return status;

        auto& opened = *slots[slot];
        if (runs * opened.fifoSteps > maximumValues)
            return Status::range;

        if (measuringSlot)
            return Status::busy;

        valueCount = 0;

        if (auto synchronous = std::get_if<hal::tiva::SynchronousAdc>(&opened.driver))
        {
            for (uint32_t run = 0; run != runs; ++run)
                for (auto sample : synchronous->Measure(opened.fifoSteps))
                    values[valueCount++] = sample;

            Report();
            return Status::done;
        }

        measuringSlot = slot;
        runsRemaining = runs;
        timer.Start(measureTimeout, [this]()
            {
                Timeout();
            });

        std::get<hal::tiva::Adc>(opened.driver).Measure([this](hal::AdcMultiChannel::Samples samples)
            {
                Collect(samples);
            });

        return Status::done;
    }

    Status AdcCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        std::size_t slot = 0;
        Status status = Find(arguments, slot);
        if (status != Status::done)
            return status;

        if (measuringSlot == slot)
        {
            std::get<hal::tiva::Adc>(slots[slot]->driver).Stop();
            runsRemaining = 0;
            measuringSlot = std::nullopt;
            timer.Cancel();
        }

        slots[slot]->driver.emplace<std::monostate>();
        slots[slot]->inputs.clear();
        slots[slot] = std::nullopt;
        context.pins.Release(static_cast<Owner>(owner::adc + slot));
        context.response.Ok();
        return Status::done;
    }

    Status AdcCommands::Find(const Arguments& arguments, std::size_t& slot)
    {
        uint32_t adc = 0;
        uint32_t sequencer = 0;
        Status status = Status::done;
        arguments.NumberAt(0, adc, 0, board::adcs - 1, status);
        arguments.NumberAt(1, sequencer, 0, sequencerDepths.size() - 1, status);
        if (status != Status::done)
            return status;

        for (std::size_t i = 0; i != slots.size(); ++i)
            if (slots[i] && slots[i]->adc == adc && slots[i]->sequencer == sequencer)
            {
                slot = i;
                return Status::done;
            }

        return Status::notOpen;
    }

    Status AdcCommands::ParseComparators(infra::BoundedConstString text, std::size_t steps, Sequencer& sequencer, std::size_t& comparatorSteps) const
    {
        infra::Tokenizer entries(text, ',');
        comparatorSteps = entries.Size();

        if (comparatorSteps == 0 || comparatorSteps >= steps)
            return Status::range;

        uint32_t used = 0;
        for (std::size_t i = 0; i != comparatorSteps; ++i)
        {
            infra::Tokenizer fields(entries.Token(i), ':');
            if (fields.Size() != 3)
                return Status::usage;

            auto comparator = ParseNumber(fields.Token(0));
            auto low = ParseNumber(fields.Token(1));
            auto high = ParseNumber(fields.Token(2));
            if (!comparator || !low || !high)
                return Status::usage;

            if (*comparator > maximumDigitalComparator || *high > maximumCode || *low > *high || (used & (1u << *comparator)) != 0)
                return Status::range;

            used |= 1u << *comparator;
            sequencer.comparators[steps - comparatorSteps + i] = hal::tiva::Adc::DigitalComparatorConfig{ static_cast<uint8_t>(*comparator), static_cast<uint16_t>(*low), static_cast<uint16_t>(*high),
                hal::tiva::Adc::ComparatorCondition::highBand, hal::tiva::Adc::ComparatorMode::always };
        }

        return Status::done;
    }

    void AdcCommands::Collect(hal::AdcMultiChannel::Samples samples)
    {
        if (runsRemaining == 0)
            return;

        for (auto sample : samples)
        {
            auto position = valueCount.load();
            if (position < values.size())
            {
                values[position] = sample;
                valueCount = position + 1;
            }
        }

        if (--runsRemaining == 0)
        {
            std::get<hal::tiva::Adc>(slots[*measuringSlot]->driver).Stop();
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Finish();
                });
        }
    }

    void AdcCommands::Finish()
    {
        if (!measuringSlot)
            return;

        measuringSlot = std::nullopt;
        timer.Cancel();
        Report();
    }

    void AdcCommands::Timeout()
    {
        if (!measuringSlot)
            return;

        runsRemaining = 0;
        std::get<hal::tiva::Adc>(slots[*measuringSlot]->driver).Stop();
        measuringSlot = std::nullopt;
        context.response.Error(Status::timeout);
    }

    void AdcCommands::Report()
    {
        auto line = context.response.Ok();
        line << " samples=";

        for (std::size_t i = 0; i != valueCount; ++i)
            line << (i == 0 ? "" : ",") << static_cast<uint32_t>(values[i]);
    }
}
