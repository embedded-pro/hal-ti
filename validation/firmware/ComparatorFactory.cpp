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
        using services::HilChoice;
        using services::HilStatus;

        constexpr uint32_t maximumReferenceStep = 15;

        constexpr std::array<const char*, 7> openKeys{ { "pos", "neg", "out", "src", "ref", "invert", "sync" } };

        constexpr std::array<HilChoice<Comparator::PositiveInputSource>, 3> sources{ {
            { "pin", Comparator::PositiveInputSource::externalPin },
            { "c0", Comparator::PositiveInputSource::sharedC0PlusPin },
            { "ref", Comparator::PositiveInputSource::internalReference },
        } };

        constexpr std::array<HilChoice<Comparator::ReferenceRange>, 2> ranges{ {
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

    TivaComparatorFactory::TivaComparatorFactory(const services::HilPinNaming& naming)
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

    HilStatus TivaComparatorFactory::Prepare(uint8_t, const services::HilArguments& arguments)
    {
        Request request;
        return Parse(arguments, request);
    }

    HilStatus TivaComparatorFactory::Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilComparatorHandle& handle)
    {
        Request request;
        Parse(arguments, request);

        hal::GpioPin* positive = nullptr;
        hal::GpioPin* negative = nullptr;
        hal::GpioPin* output = nullptr;
        HilStatus status = pins.Claim(*request.negative, services::HilPinPool::Use::analog, negative);
        if (status == HilStatus::done && request.positive)
            status = pins.Claim(*request.positive, services::HilPinPool::Use::analog, positive);
        if (status == HilStatus::done)
            status = pins.ClaimFunction(request.output, Function(hal::tiva::PinConfigPeripheral::comparatorOutput), index, output);
        if (status != HilStatus::done)
            return status;

        request.config.outputToPin = output != nullptr;
        ResetComparatorModule();

        if (request.synchronous)
            handle.synchronous = &driver.emplace<hal::tiva::SynchronousAnalogComparator>(index, PinOrDummy(positive), PinOrDummy(negative), PinOrDummy(output), request.config);
        else
            handle.comparator = &driver.emplace<hal::tiva::AnalogComparator>(index, PinOrDummy(positive), PinOrDummy(negative), PinOrDummy(output), request.config);

        return HilStatus::done;
    }

    void TivaComparatorFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        driver.emplace<std::monostate>();
        onClosed();
    }

    HilStatus TivaComparatorFactory::Parse(const services::HilArguments& arguments, Request& request) const
    {
        auto& config = request.config;
        config.positiveSource = arguments.Has("ref") ? Comparator::PositiveInputSource::internalReference : Comparator::PositiveInputSource::externalPin;

        HilStatus status = HilStatus::done;
        arguments.Pin("pos", naming, request.positive, status);
        arguments.Pin("neg", naming, request.negative, status);
        arguments.Pin("out", naming, request.output, status);
        arguments.Select("src", config.positiveSource, sources, status);
        arguments.Flag("invert", config.invertOutput, status);
        arguments.Flag("sync", request.synchronous, status);

        if (status == HilStatus::done && arguments.Has("ref"))
        {
            infra::Tokenizer fields(*arguments.Key("ref"), ',');
            auto range = services::HilArguments::ParseChoice(fields.Token(0), ranges);
            auto step = services::HilArguments::ParseNumber(fields.Token(1));

            if (fields.Size() != 2 || !range || !step)
                status = HilStatus::usage;
            else if (*step > maximumReferenceStep)
                status = HilStatus::range;
            else
                config.internalReference = Comparator::InternalReference{ *range, static_cast<uint8_t>(*step) };
        }

        if (status != HilStatus::done)
            return status;

        const bool usesReference = config.positiveSource == Comparator::PositiveInputSource::internalReference;
        if (!request.negative || (config.positiveSource == Comparator::PositiveInputSource::externalPin && !request.positive) || usesReference != config.internalReference.has_value())
            return HilStatus::usage;

        return HilStatus::done;
    }
}
