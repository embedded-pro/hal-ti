#include "validation/firmware/ComparatorFactory.hpp"
#include "BoardProfile.hpp"
#include "infra/util/Tokenizer.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include DEVICE_HEADER

namespace validation
{
    namespace
    {
        using Comparator = hal::tiva::AnalogComparator;
        using services::hil::Choice;
        using services::hil::Status;

        constexpr uint32_t maximumReferenceStep = 15;

        constexpr std::array<const char*, 7> openKeys{ { "pos", "neg", "out", "src", "ref", "invert", "sync" } };

        constexpr std::array<Choice<Comparator::PositiveInputSource>, 3> sources{ {
            { "pin", Comparator::PositiveInputSource::externalPin },
            { "c0", Comparator::PositiveInputSource::sharedC0PlusPin },
            { "ref", Comparator::PositiveInputSource::internalReference },
        } };

        constexpr std::array<Choice<Comparator::ReferenceRange>, 2> ranges{ {
            { "low", Comparator::ReferenceRange::low },
            { "high", Comparator::ReferenceRange::high },
        } };

        // The drivers leave ACREFCTL enabled when they are destroyed and assert when a later instance asks for a different reference
        void ResetComparatorModule()
        {
            SYSCTL->SRACMP |= 1u;
            SYSCTL->SRACMP &= ~1u;
        }
    }

    TivaComparatorFactory::TivaComparatorFactory(const services::hil::PinNaming& naming)
        : naming(naming)
    {}

    uint8_t TivaComparatorFactory::Instances() const
    {
        return board::comparators;
    }

    infra::MemoryRange<const char* const> TivaComparatorFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    Status TivaComparatorFactory::Prepare(uint8_t, const services::hil::Arguments& arguments)
    {
        Request request;
        return Parse(arguments, request);
    }

    Status TivaComparatorFactory::Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, services::hil::ComparatorHandle& handle)
    {
        Request request;
        Parse(arguments, request);

        hal::GpioPin* positive = nullptr;
        hal::GpioPin* negative = nullptr;
        hal::GpioPin* output = nullptr;
        Status status = pins.Claim(*request.negative, services::hil::PinPool::Use::analog, negative);
        if (status == Status::done && request.positive)
            status = pins.Claim(*request.positive, services::hil::PinPool::Use::analog, positive);
        if (status == Status::done)
            status = pins.ClaimFunction(request.output, Function(hal::tiva::PinConfigPeripheral::comparatorOutput), index, output);
        if (status != Status::done)
            return status;

        request.config.outputToPin = output != nullptr;
        ResetComparatorModule();

        if (request.synchronous)
            handle.synchronous = &driver.emplace<hal::tiva::SynchronousAnalogComparator>(index, PinOrDummy(positive), PinOrDummy(negative), PinOrDummy(output), request.config);
        else
            handle.comparator = &driver.emplace<hal::tiva::AnalogComparator>(index, PinOrDummy(positive), PinOrDummy(negative), PinOrDummy(output), request.config);

        return Status::done;
    }

    void TivaComparatorFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        driver.emplace<std::monostate>();
        onClosed();
    }

    Status TivaComparatorFactory::Parse(const services::hil::Arguments& arguments, Request& request) const
    {
        auto& config = request.config;
        config.positiveSource = arguments.Has("ref") ? Comparator::PositiveInputSource::internalReference : Comparator::PositiveInputSource::externalPin;

        Status status = Status::done;
        arguments.Pin("pos", naming, request.positive, status);
        arguments.Pin("neg", naming, request.negative, status);
        arguments.Pin("out", naming, request.output, status);
        arguments.Select("src", config.positiveSource, sources, status);
        arguments.Flag("invert", config.invertOutput, status);
        arguments.Flag("sync", request.synchronous, status);

        if (status == Status::done && arguments.Has("ref"))
        {
            infra::Tokenizer fields(*arguments.Key("ref"), ',');
            auto range = services::hil::ParseChoice(fields.Token(0), ranges);
            auto step = services::hil::ParseNumber(fields.Token(1));

            if (fields.Size() != 2 || !range || !step)
                status = Status::usage;
            else if (*step > maximumReferenceStep)
                status = Status::range;
            else
                config.internalReference = Comparator::InternalReference{ *range, static_cast<uint8_t>(*step) };
        }

        if (status != Status::done)
            return status;

        const bool usesReference = config.positiveSource == Comparator::PositiveInputSource::internalReference;
        if (!request.negative || (config.positiveSource == Comparator::PositiveInputSource::externalPin && !request.positive) || usesReference != config.internalReference.has_value())
            return Status::usage;

        return Status::done;
    }
}
