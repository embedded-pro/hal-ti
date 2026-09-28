#include "validation/firmware/QeiFactory.hpp"
#include "BoardProfile.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include <limits>

namespace validation
{
    namespace
    {
        using Config = hal::tiva::QuadratureEncoder::Config;
        using services::hil::Choice;
        using services::hil::Status;

        constexpr uint32_t maximumVelocityPeriodUs = 1000000;

        constexpr std::array<const char*, 12> openKeys{ { "a", "b", "idx", "res", "offset", "inva", "invb", "invi", "reset", "cap", "sig", "vel" } };

        constexpr std::array<Choice<Config::ResetMode>, 2> resetModes{ {
            { "max", Config::ResetMode::onMaxPosition },
            { "index", Config::ResetMode::onIndexPulse },
        } };

        constexpr std::array<Choice<Config::CaptureMode>, 2> captureModes{ {
            { "a", Config::CaptureMode::onlyPhaseA },
            { "ab", Config::CaptureMode::phaseAandPhaseB },
        } };

        constexpr std::array<Choice<Config::SignalMode>, 2> signalModes{ {
            { "quad", Config::SignalMode::quadrature },
            { "clkdir", Config::SignalMode::clockAndDirection },
        } };
    }

    TivaQeiFactory::TivaQeiFactory(const services::hil::PinNaming& naming)
        : naming(naming)
    {}

    uint8_t TivaQeiFactory::Instances() const
    {
        return board::qeis;
    }

    infra::MemoryRange<const char* const> TivaQeiFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    Status TivaQeiFactory::Prepare(uint8_t index, const services::hil::Arguments& arguments)
    {
        Request request;
        return Parse(index, arguments, request);
    }

    Status TivaQeiFactory::Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, hal::SynchronousQuadratureEncoder*& opened)
    {
        Request request;
        Parse(index, arguments, request);

        hal::GpioPin* a = nullptr;
        hal::GpioPin* b = nullptr;
        hal::GpioPin* indexPin = nullptr;
        Status status = pins.ClaimFunction(request.a, Function(hal::tiva::PinConfigPeripheral::qeiPhaseA), index, a);
        if (status == Status::done)
            status = pins.ClaimFunction(request.b, Function(hal::tiva::PinConfigPeripheral::qeiPhaseB), index, b);
        if (status == Status::done)
            status = pins.ClaimFunction(request.index, Function(hal::tiva::PinConfigPeripheral::qeiIndex), index, indexPin);
        if (status != Status::done)
            return status;

        opened = &encoder.emplace(index, PinOrDummy(a), PinOrDummy(b), PinOrDummy(indexPin), request.config);
        return Status::done;
    }

    void TivaQeiFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        encoder = std::nullopt;
        onClosed();
    }

    Status TivaQeiFactory::Parse(uint8_t index, const services::hil::Arguments& arguments, Request& request) const
    {
        auto& config = request.config;
        uint32_t velocityPeriod = 1000;

        Status status = Status::done;
        arguments.Pin("a", naming, request.a, status);
        arguments.Pin("b", naming, request.b, status);
        arguments.Pin("idx", naming, request.index, status);
        arguments.Number("res", config.resolution, 1, std::numeric_limits<uint32_t>::max(), status);
        arguments.Number("offset", config.offset, 0, std::numeric_limits<uint32_t>::max(), status);
        arguments.Flag("inva", config.invertPhaseA, status);
        arguments.Flag("invb", config.invertPhaseB, status);
        arguments.Flag("invi", config.invertIndex, status);
        arguments.Select("reset", config.resetMode, resetModes, status);
        arguments.Select("cap", config.captureMode, captureModes, status);
        arguments.Select("sig", config.signalMode, signalModes, status);
        arguments.Number("vel", velocityPeriod, 1, maximumVelocityPeriodUs, status);
        if (status != Status::done)
            return status;

        if (config.offset >= config.resolution)
            return Status::range;

        if (!request.a && !request.b && !request.index)
        {
            if (index != board::qeiIndex)
                return Status::usage;

            request.a = board::encoderA;
            request.b = board::encoderB;
            request.index = board::encoderZ;
        }

        if (!request.a || !request.b)
            return Status::usage;

        config.velocityPeriod = std::chrono::microseconds(velocityPeriod);
        return Status::done;
    }
}
