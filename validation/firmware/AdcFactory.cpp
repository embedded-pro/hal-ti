#include "validation/firmware/AdcFactory.hpp"
#include "BoardProfile.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "infra/util/Tokenizer.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include DEVICE_HEADER

namespace validation
{
    namespace
    {
        using services::HilChoice;
        using services::HilStatus;

        constexpr std::array<uint8_t, 4> sequencerDepths{ { 8, 4, 4, 1 } };
        constexpr uint8_t maximumDigitalComparator = 7;
        constexpr uint32_t maximumCode = 0x0fff;
        constexpr uint32_t maximumDelay = 15;

        constexpr uint32_t maximumPriority = 3;

        constexpr std::array<const char*, 9> openKeys{ { "pins", "sh", "avg", "delay", "trigger", "dcmp", "ref", "prio", "sync" } };

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

        constexpr std::array<HilChoice<bool>, 2> references{ {
            { "int", false },
            { "ext", true },
        } };

        constexpr std::array<HilChoice<hal::tiva::Adc::ComparatorCondition>, 3> bands{ {
            { "low", hal::tiva::Adc::ComparatorCondition::lowBand },
            { "mid", hal::tiva::Adc::ComparatorCondition::midBand },
            { "high", hal::tiva::Adc::ComparatorCondition::highBand },
        } };

        constexpr std::array<HilChoice<hal::tiva::Adc::ComparatorMode>, 4> comparatorModes{ {
            { "always", hal::tiva::Adc::ComparatorMode::always },
            { "once", hal::tiva::Adc::ComparatorMode::once },
            { "hyst", hal::tiva::Adc::ComparatorMode::hysteresisAlways },
            { "hystonce", hal::tiva::Adc::ComparatorMode::hysteresisOnce },
        } };

        constexpr std::array<HilChoice<std::optional<hal::tiva::Adc::Trigger>>, 4> triggers{ {
            { "pwm0", hal::tiva::Adc::Trigger::pwmGenerator0 },
            { "pwm1", hal::tiva::Adc::Trigger::pwmGenerator1 },
            { "pwm2", hal::tiva::Adc::Trigger::pwmGenerator2 },
            { "pwm3", hal::tiva::Adc::Trigger::pwmGenerator3 },
        } };

        // Averaging and sampling delay are module wide and the drivers only write them when enabled, so a module no other sequencer uses starts from reset
        void ResetAdcModule(uint8_t adc)
        {
            SYSCTL->RCGCADC |= 1u << adc;
            while ((SYSCTL->PRADC & (1u << adc)) == 0)
            {
            }

            SYSCTL->SRADC |= 1u << adc;
            SYSCTL->SRADC &= ~(1u << adc);
        }

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

        Construct(opened, request);
        handle.samplesPerRun = request.steps - request.comparatorSteps;

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
        HilStatus status = HilStatus::done;
        arguments.Select("sh", request.sampleAndHold, sampleAndHolds, status);
        arguments.Select("avg", request.oversampling, oversamplings, status);
        arguments.Select("trigger", request.trigger, triggers, status);
        arguments.Select("ref", request.externalReference, references, status);
        arguments.Flag("sync", request.synchronous, status);
        if (status == HilStatus::done && arguments.Has("delay") && arguments.Key("delay") != "off")
            arguments.Number("delay", request.delay.emplace(), 0, maximumDelay, status);
        if (status == HilStatus::done && arguments.Has("prio"))
            arguments.Number("prio", request.priority.emplace(), 0, maximumPriority, status);
        if (status != HilStatus::done)
            return status;

        auto list = arguments.Key("pins");
        if (!list || (!request.synchronous && !request.trigger))
            return HilStatus::usage;

        infra::Tokenizer tokens(*list, ',');
        request.steps = tokens.Size();
        if (request.steps == 0 || request.steps > sequencerDepths[SequencerOf(key)])
            return HilStatus::range;

        for (std::size_t i = 0; i != request.steps; ++i)
        {
            auto pin = services::HilArguments::ParsePin(tokens.Token(i), naming);
            if (!pin)
                return HilStatus::pin;

            request.pins[i] = *pin;
        }

        if (request.synchronous && (request.delay || request.trigger || request.externalReference || arguments.Has("dcmp")))
            return HilStatus::unsupported;

        if (auto comparators = arguments.Key("dcmp"))
            return ParseComparators(*comparators, request);

        return HilStatus::done;
    }

    HilStatus TivaAdcFactory::ParseComparators(infra::BoundedConstString text, Request& request)
    {
        infra::Tokenizer entries(text, ',');
        const auto comparatorSteps = entries.Size();

        if (comparatorSteps == 0 || comparatorSteps >= request.steps)
            return HilStatus::range;

        uint32_t used = 0;
        for (std::size_t i = 0; i != comparatorSteps; ++i)
        {
            infra::Tokenizer fields(entries.Token(i), ':');
            if (fields.Size() < 3 || fields.Size() > 5)
                return HilStatus::usage;

            auto comparator = services::HilArguments::ParseNumber(fields.Token(0));
            auto low = services::HilArguments::ParseNumber(fields.Token(1));
            auto high = services::HilArguments::ParseNumber(fields.Token(2));
            auto band = fields.Size() > 3 ? services::HilArguments::ParseChoice(fields.Token(3), bands) : hal::tiva::Adc::ComparatorCondition::highBand;
            auto mode = fields.Size() > 4 ? services::HilArguments::ParseChoice(fields.Token(4), comparatorModes) : hal::tiva::Adc::ComparatorMode::always;
            if (!comparator || !low || !high || !band || !mode)
                return HilStatus::usage;

            if (*comparator > maximumDigitalComparator || *high > maximumCode || *low > *high || (used & (1u << *comparator)) != 0)
                return HilStatus::range;

            used |= 1u << *comparator;
            request.comparators[request.steps - comparatorSteps + i] = hal::tiva::Adc::DigitalComparatorConfig{ static_cast<uint8_t>(*comparator), static_cast<uint16_t>(*low), static_cast<uint16_t>(*high), *band, *mode };
        }

        request.comparatorSteps = comparatorSteps;
        return HilStatus::done;
    }

    void TivaAdcFactory::Construct(Sequencer& opened, const Request& request)
    {
        const auto adc = AdcOf(opened.key);
        const auto sequencer = SequencerOf(opened.key);
        const auto priority = static_cast<uint8_t>(request.priority.value_or(sequencer));

        bool shared = false;
        for (const auto& other : slots)
            shared = shared || (other && &*other != &opened && AdcOf(other->key) == adc);

        if (!shared)
            ResetAdcModule(adc);

        const auto oversampling = request.oversampling != 0 ? std::make_optional(request.oversampling) : std::nullopt;

        if (request.synchronous)
        {
            opened.syncConfig.sampleAndHold = static_cast<hal::tiva::SynchronousAdc::SampleAndHold>(request.sampleAndHold);
            opened.syncConfig.priority = static_cast<hal::tiva::SynchronousAdc::Priority>(priority);
            opened.syncConfig.oversampling = oversampling ? std::make_optional(static_cast<hal::tiva::SynchronousAdc::Oversampling>(*oversampling)) : std::nullopt;
            opened.driver.emplace<hal::tiva::SynchronousAdc>(adc, sequencer, infra::MakeRange(opened.inputs), opened.syncConfig);
        }
        else
        {
            opened.asyncConfig.externalReference = request.externalReference;
            opened.asyncConfig.priority = priority;
            opened.asyncConfig.trigger = *request.trigger;
            opened.asyncConfig.sampleAndHold = static_cast<hal::tiva::Adc::SampleAndHold>(request.sampleAndHold);
            opened.asyncConfig.oversampling = oversampling ? std::make_optional(static_cast<hal::tiva::Adc::Oversampling>(*oversampling)) : std::nullopt;
            opened.asyncConfig.samplingDelay = request.delay ? std::make_optional(hal::tiva::Adc::SamplingDelay(static_cast<uint8_t>(*request.delay))) : std::nullopt;
            opened.comparators = request.comparators;
            opened.asyncConfig.digitalComparators = request.comparatorSteps != 0 ? infra::MemoryRange<const hal::tiva::Adc::DigitalComparatorConfig>(opened.comparators.data(), opened.comparators.data() + request.steps) : infra::MemoryRange<const hal::tiva::Adc::DigitalComparatorConfig>();
            opened.asyncConfig.interruptPriority = hal::cortex::InterruptPriority::highest;
            opened.driver.emplace<hal::tiva::Adc>(adc, sequencer, infra::MakeRange(opened.inputs), opened.asyncConfig);
        }
    }
}
