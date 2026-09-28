#include "validation/firmware/AdcFactory.hpp"
#include "BoardProfile.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "infra/util/Tokenizer.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include <algorithm>

namespace validation
{
    namespace
    {
        using services::HilChoice;
        using services::HilStatus;

        constexpr std::array<uint8_t, 4> sequencerDepths{ { 8, 4, 4, 1 } };
        constexpr uint8_t phaseCurrentAdc = 0;
        constexpr uint8_t supplyAdc = 1;
        constexpr uint8_t eFocSequencer = 0;
        constexpr uint8_t maximumDigitalComparator = 7;
        constexpr uint32_t maximumCode = 0x0fff;
        constexpr uint32_t maximumDelay = 15;

        constexpr std::array<const char*, 7> openKeys{ { "pins", "sh", "avg", "delay", "trigger", "dcmp", "sync" } };

        constexpr std::array<HilChoice<uint8_t>, 7> sampleAndHolds{ {
            { "4", 0 },
            { "8", 1 },
            { "16", 2 },
            { "32", 3 },
            { "64", 4 },
            { "128", 5 },
            { "256", 6 },
        } };

        constexpr std::array<HilChoice<uint8_t>, 7> oversamplings{ {
            { "off", 0 },
            { "2", 1 },
            { "4", 2 },
            { "8", 3 },
            { "16", 4 },
            { "32", 5 },
            { "64", 6 },
        } };

        constexpr std::array<HilChoice<hal::tiva::Adc::Trigger>, 4> triggers{ {
            { "pwm0", hal::tiva::Adc::Trigger::pwmGenerator0 },
            { "pwm1", hal::tiva::Adc::Trigger::pwmGenerator1 },
            { "pwm2", hal::tiva::Adc::Trigger::pwmGenerator2 },
            { "pwm3", hal::tiva::Adc::Trigger::pwmGenerator3 },
        } };

        uint8_t AdcOf(uint16_t key)
        {
            return static_cast<uint8_t>(key / sequencerDepths.size());
        }

        uint8_t SequencerOf(uint16_t key)
        {
            return static_cast<uint8_t>(key % sequencerDepths.size());
        }
    }

    TivaAdcFactory::TivaAdcFactory(const services::HilPinNaming& naming)
        : naming(naming)
    {}

    std::size_t TivaAdcFactory::KeyPositionals() const
    {
        return 2;
    }

    HilStatus TivaAdcFactory::ParseKey(const services::HilArguments& arguments, uint16_t& key) const
    {
        uint32_t adc = 0;
        uint32_t sequencer = 0;
        HilStatus status = HilStatus::done;
        arguments.NumberAt(0, adc, 0, board::adcs - 1, status);
        arguments.NumberAt(1, sequencer, 0, sequencerDepths.size() - 1, status);
        key = static_cast<uint16_t>(adc * sequencerDepths.size() + sequencer);
        return status;
    }

    infra::MemoryRange<const char* const> TivaAdcFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    HilStatus TivaAdcFactory::Prepare(uint16_t key, const services::HilArguments& arguments)
    {
        Request request;
        return Parse(key, arguments, request);
    }

    HilStatus TivaAdcFactory::Open(std::size_t slot, uint16_t key, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilAdcHandle& handle)
    {
        Request request;
        Parse(key, arguments, request);

        really_assert(slot < slots.size() && !slots[slot]);
        auto& opened = slots[slot].emplace();
        opened.key = key;

        HilStatus status = HilStatus::done;
        std::size_t comparatorSteps = 0;
        if (auto list = arguments.Key("dcmp"))
            status = ParseComparators(*list, request.steps, opened, comparatorSteps);

        for (std::size_t i = 0; i != request.steps && status == HilStatus::done; ++i)
        {
            hal::GpioPin* pin = nullptr;
            status = pins.ClaimAnalog(request.pins[i], pin);
            if (status == HilStatus::done)
                opened.inputs.emplace_back(PinOrDummy(pin));
        }

        if (status != HilStatus::done)
        {
            opened.inputs.clear();
            slots[slot] = std::nullopt;
            return status;
        }

        Construct(opened, request, comparatorSteps);
        handle.samplesPerRun = request.steps - comparatorSteps;

        if (auto synchronous = std::get_if<hal::tiva::SynchronousAdc>(&opened.driver))
            handle.synchronous = synchronous;
        else
            handle.adc = &std::get<hal::tiva::Adc>(opened.driver);

        return HilStatus::done;
    }

    void TivaAdcFactory::Close(std::size_t slot, uint16_t, const infra::Function<void()>& onClosed)
    {
        really_assert(slot < slots.size() && slots[slot]);
        slots[slot]->driver.emplace<std::monostate>();
        slots[slot]->inputs.clear();
        slots[slot] = std::nullopt;

        onClosed();
    }

    HilStatus TivaAdcFactory::Parse(uint16_t key, const services::HilArguments& arguments, Request& request) const
    {
        const auto adc = AdcOf(key);
        const auto sequencer = SequencerOf(key);
        const bool supply = adc == supplyAdc && sequencer == eFocSequencer;
        const bool phaseCurrent = adc == phaseCurrentAdc && sequencer == eFocSequencer;
        request.synchronous = supply;
        request.sampleAndHold = supply ? 6 : 1;
        request.oversampling = supply ? 3 : 1;
        request.delayEnabled = !supply;
        request.trigger = board::adcTrigger;

        HilStatus status = HilStatus::done;
        arguments.Select("sh", request.sampleAndHold, sampleAndHolds, status);
        arguments.Select("avg", request.oversampling, oversamplings, status);
        arguments.Select("trigger", request.trigger, triggers, status);
        arguments.Flag("sync", request.synchronous, status);
        if (status == HilStatus::done && arguments.Has("delay"))
        {
            request.delayEnabled = arguments.Key("delay") != "off";
            if (request.delayEnabled)
                arguments.Number("delay", request.delay, 0, maximumDelay, status);
        }
        if (status != HilStatus::done)
            return status;

        if (request.synchronous && (arguments.Has("delay") || arguments.Has("trigger") || arguments.Has("dcmp")))
            return HilStatus::unsupported;

        if (auto list = arguments.Key("pins"))
        {
            infra::Tokenizer tokens(*list, ',');
            request.steps = tokens.Size();
            if (request.steps == 0 || request.steps > maximumSteps)
                return HilStatus::range;

            for (std::size_t i = 0; i != request.steps; ++i)
            {
                auto pin = services::HilArguments::ParsePin(tokens.Token(i), naming);
                if (!pin)
                    return HilStatus::pin;

                request.pins[i] = *pin;
            }
        }
        else if (phaseCurrent)
            request.steps = std::copy(board::phaseCurrentPins.begin(), board::phaseCurrentPins.end(), request.pins.begin()) - request.pins.begin();
        else if (supply)
            request.steps = std::copy(board::supplyPins.begin(), board::supplyPins.end(), request.pins.begin()) - request.pins.begin();
        else
            return HilStatus::usage;

        if (request.steps > sequencerDepths[sequencer])
            return HilStatus::range;

        return HilStatus::done;
    }

    HilStatus TivaAdcFactory::ParseComparators(infra::BoundedConstString text, std::size_t steps, Sequencer& sequencer, std::size_t& comparatorSteps) const
    {
        infra::Tokenizer entries(text, ',');
        comparatorSteps = entries.Size();

        if (comparatorSteps == 0 || comparatorSteps >= steps)
            return HilStatus::range;

        uint32_t used = 0;
        for (std::size_t i = 0; i != comparatorSteps; ++i)
        {
            infra::Tokenizer fields(entries.Token(i), ':');
            if (fields.Size() != 3)
                return HilStatus::usage;

            auto comparator = services::HilArguments::ParseNumber(fields.Token(0));
            auto low = services::HilArguments::ParseNumber(fields.Token(1));
            auto high = services::HilArguments::ParseNumber(fields.Token(2));
            if (!comparator || !low || !high)
                return HilStatus::usage;

            if (*comparator > maximumDigitalComparator || *high > maximumCode || *low > *high || (used & (1u << *comparator)) != 0)
                return HilStatus::range;

            used |= 1u << *comparator;
            sequencer.comparators[steps - comparatorSteps + i] = hal::tiva::Adc::DigitalComparatorConfig{ static_cast<uint8_t>(*comparator), static_cast<uint16_t>(*low), static_cast<uint16_t>(*high),
                hal::tiva::Adc::ComparatorCondition::highBand, hal::tiva::Adc::ComparatorMode::always };
        }

        return HilStatus::done;
    }

    void TivaAdcFactory::Construct(Sequencer& opened, const Request& request, std::size_t comparatorSteps)
    {
        const auto adc = AdcOf(opened.key);
        const auto sequencer = SequencerOf(opened.key);
        const auto oversampling = request.oversampling != 0 ? std::make_optional(request.oversampling) : std::nullopt;

        if (request.synchronous)
        {
            opened.syncConfig.sampleAndHold = static_cast<hal::tiva::SynchronousAdc::SampleAndHold>(request.sampleAndHold);
            opened.syncConfig.priority = static_cast<hal::tiva::SynchronousAdc::Priority>(sequencer);
            opened.syncConfig.oversampling = oversampling ? std::make_optional(static_cast<hal::tiva::SynchronousAdc::Oversampling>(*oversampling)) : std::nullopt;
            opened.driver.emplace<hal::tiva::SynchronousAdc>(adc, sequencer, infra::MakeRange(opened.inputs), opened.syncConfig);
        }
        else
        {
            opened.asyncConfig.externalReference = false;
            opened.asyncConfig.priority = sequencer;
            opened.asyncConfig.trigger = request.trigger;
            opened.asyncConfig.sampleAndHold = static_cast<hal::tiva::Adc::SampleAndHold>(request.sampleAndHold);
            opened.asyncConfig.oversampling = oversampling ? std::make_optional(static_cast<hal::tiva::Adc::Oversampling>(*oversampling)) : std::nullopt;
            opened.asyncConfig.samplingDelay = request.delayEnabled ? std::make_optional(hal::tiva::Adc::SamplingDelay(static_cast<uint8_t>(request.delay))) : std::nullopt;
            opened.asyncConfig.digitalComparators = comparatorSteps != 0 ? infra::MemoryRange<const hal::tiva::Adc::DigitalComparatorConfig>(opened.comparators.data(), opened.comparators.data() + request.steps) : infra::MemoryRange<const hal::tiva::Adc::DigitalComparatorConfig>();
            opened.asyncConfig.interruptPriority = hal::cortex::InterruptPriority::highest;
            opened.driver.emplace<hal::tiva::Adc>(adc, sequencer, infra::MakeRange(opened.inputs), opened.asyncConfig);
        }
    }
}
