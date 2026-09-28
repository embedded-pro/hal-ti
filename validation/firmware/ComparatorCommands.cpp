#include "validation/firmware/ComparatorCommands.hpp"
#include "BoardProfile.hpp"
#include "infra/util/Tokenizer.hpp"
#include DEVICE_HEADER

namespace validation
{
    namespace
    {
        using Config = hal::tiva::AnalogComparator::Config;

        constexpr uint32_t maximumReferenceStep = 15;

        constexpr std::array<Choice<hal::tiva::AnalogComparator::PositiveInputSource>, 3> sources{ {
            { "pin", hal::tiva::AnalogComparator::PositiveInputSource::externalPin },
            { "c0", hal::tiva::AnalogComparator::PositiveInputSource::sharedC0PlusPin },
            { "ref", hal::tiva::AnalogComparator::PositiveInputSource::internalReference },
        } };

        constexpr std::array<Choice<hal::tiva::AnalogComparator::ReferenceRange>, 2> ranges{ {
            { "low", hal::tiva::AnalogComparator::ReferenceRange::low },
            { "high", hal::tiva::AnalogComparator::ReferenceRange::high },
        } };

        enum class Edge : uint8_t
        {
            rising,
            falling,
            both,
            off,
        };

        constexpr std::array<Choice<Edge>, 4> edges{ {
            { "rising", Edge::rising },
            { "falling", Edge::falling },
            { "both", Edge::both },
            { "off", Edge::off },
        } };

        // The drivers leave ACREFCTL enabled when they are destroyed and assert when a later instance asks for a different reference
        void ResetComparatorModule()
        {
            SYSCTL->SRACMP |= 1u;
            SYSCTL->SRACMP &= ~1u;
        }
    }

    ComparatorCommands::ComparatorCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<ComparatorCommands, &ComparatorCommands::Open>("comp.open", "<index> [pos=] neg= [out=] [src=] [ref=] [invert=] [sync=]", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Read>("comp.read", "<index>", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Interrupt>("comp.irq", "<index> <rising|falling|both|off>", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Count>("comp.count", "<index> [clear=]", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Close>("comp.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> ComparatorCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status ComparatorCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "pos", "neg", "out", "src", "ref", "invert", "sync" }))
            return Status::usage;

        uint32_t requested = 0;
        std::optional<PinId> positive;
        std::optional<PinId> negative;
        std::optional<PinId> output;
        Config config;
        bool synchronous = false;
        config.positiveSource = arguments.Has("ref") ? hal::tiva::AnalogComparator::PositiveInputSource::internalReference : hal::tiva::AnalogComparator::PositiveInputSource::externalPin;

        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::comparators - 1, status);
        arguments.Pin("pos", positive, status);
        arguments.Pin("neg", negative, status);
        arguments.Pin("out", output, status);
        arguments.Select("src", config.positiveSource, sources, status);
        arguments.Flag("invert", config.invertOutput, status);
        arguments.Flag("sync", synchronous, status);

        if (status == Status::done && arguments.Has("ref"))
        {
            infra::Tokenizer fields(*arguments.Key("ref"), ',');
            auto range = ParseChoice(fields.Token(0), ranges);
            auto step = ParseNumber(fields.Token(1));

            if (fields.Size() != 2 || !range || !step)
                status = Status::usage;
            else if (*step > maximumReferenceStep)
                status = Status::range;
            else
                config.internalReference = hal::tiva::AnalogComparator::InternalReference{ *range, static_cast<uint8_t>(*step) };
        }

        if (status != Status::done)
            return status;

        const bool usesReference = config.positiveSource == hal::tiva::AnalogComparator::PositiveInputSource::internalReference;
        if (!negative || (config.positiveSource == hal::tiva::AnalogComparator::PositiveInputSource::externalPin && !positive) || usesReference != config.internalReference.has_value())
            return Status::usage;

        if (index)
            return Status::busy;

        const auto comparator = static_cast<uint8_t>(requested);
        hal::tiva::GpioPin* positivePin = nullptr;
        hal::tiva::GpioPin* negativePin = nullptr;
        hal::tiva::GpioPin* outputPin = nullptr;
        status = context.pins.Claim(*negative, owner::comparator, PinPool::Use::analog, negativePin);
        if (status == Status::done && positive)
            status = context.pins.Claim(*positive, owner::comparator, PinPool::Use::analog, positivePin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(output, owner::comparator, hal::tiva::PinConfigPeripheral::comparatorOutput, comparator, outputPin);

        if (status != Status::done)
        {
            context.pins.Release(owner::comparator);
            return status;
        }

        config.outputToPin = outputPin != nullptr;
        ResetComparatorModule();

        if (synchronous)
            driver.emplace<hal::tiva::SynchronousAnalogComparator>(comparator, PinOrDummy(positivePin), *negativePin, PinOrDummy(outputPin), config);
        else
            driver.emplace<hal::tiva::AnalogComparator>(comparator, PinOrDummy(positivePin), *negativePin, PinOrDummy(outputPin), config);

        index = comparator;
        count = 0;
        context.response.Ok();
        return Status::done;
    }

    Status ComparatorCommands::Read(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        bool output = false;
        WithDriver(driver, [&output](auto& comparator)
            {
                output = comparator.GetOutput();
            });

        context.response.Ok() << " out=" << (output ? 1u : 0u);
        return Status::done;
    }

    Status ComparatorCommands::Interrupt(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        auto edge = Edge::off;
        Status status = Find(arguments);
        arguments.SelectAt(1, edge, edges, status);
        if (status != Status::done)
            return status;

        auto comparator = std::get_if<hal::tiva::AnalogComparator>(&driver);
        if (comparator == nullptr)
            return Status::unsupported;

        comparator->Disable();

        if (edge != Edge::off)
            comparator->Enable([this](bool)
                {
                    count.fetch_add(1, std::memory_order_relaxed);
                },
                static_cast<hal::InterruptTrigger>(edge));

        context.response.Ok();
        return Status::done;
    }

    Status ComparatorCommands::Count(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "clear" }))
            return Status::usage;

        bool clear = false;
        Status status = Find(arguments);
        arguments.Flag("clear", clear, status);
        if (status != Status::done)
            return status;

        context.response.Ok() << " count=" << (clear ? count.exchange(0) : count.load());
        return Status::done;
    }

    Status ComparatorCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        driver.emplace<std::monostate>();
        context.pins.Release(owner::comparator);
        index = std::nullopt;
        context.response.Ok();
        return Status::done;
    }

    Status ComparatorCommands::Find(const Arguments& arguments)
    {
        uint32_t requested = 0;
        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::comparators - 1, status);
        if (status != Status::done)
            return status;

        if (index != requested)
            return Status::notOpen;

        return Status::done;
    }
}
