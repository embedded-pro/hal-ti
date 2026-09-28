#include "validation/firmware/QeiFactory.hpp"
#include "BoardProfile.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include <limits>

namespace validation
{
    namespace
    {
        using Config = hal::tiva::QuadratureEncoder::Config;
        using services::HilChoice;
        using services::HilStatus;

        constexpr uint32_t maximumVelocityPeriodUs = 1000000;

        constexpr std::array<const char*, 12> openKeys{ { "a", "b", "idx", "res", "offset", "inva", "invb", "invi", "reset", "cap", "sig", "vel" } };

        constexpr std::array<HilChoice<Config::ResetMode>, 2> resetModes{ {
            { "max", Config::ResetMode::onMaxPosition },
            { "index", Config::ResetMode::onIndexPulse },
        } };

        constexpr std::array<HilChoice<Config::CaptureMode>, 2> captureModes{ {
            { "a", Config::CaptureMode::onlyPhaseA },
            { "ab", Config::CaptureMode::phaseAandPhaseB },
        } };

        constexpr std::array<HilChoice<Config::SignalMode>, 2> signalModes{ {
            { "quad", Config::SignalMode::quadrature },
            { "clkdir", Config::SignalMode::clockAndDirection },
        } };
    }

    TivaQeiFactory::TivaQeiFactory(const services::HilPinNaming& naming)
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

    HilStatus TivaQeiFactory::Prepare(uint8_t index, const services::HilArguments& arguments)
    {
        Request request;
        return Parse(index, arguments, request);
    }

    HilStatus TivaQeiFactory::Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, hal::SynchronousQuadratureEncoder*& opened)
    {
        Request request;
        Parse(index, arguments, request);

        hal::GpioPin* a = nullptr;
        hal::GpioPin* b = nullptr;
        hal::GpioPin* indexPin = nullptr;
        HilStatus status = pins.ClaimFunction(request.a, Function(hal::tiva::PinConfigPeripheral::qeiPhaseA), index, a);
        if (status == HilStatus::done)
            status = pins.ClaimFunction(request.b, Function(hal::tiva::PinConfigPeripheral::qeiPhaseB), index, b);
        if (status == HilStatus::done)
            status = pins.ClaimFunction(request.index, Function(hal::tiva::PinConfigPeripheral::qeiIndex), index, indexPin);
        if (status != HilStatus::done)
            return status;

        opened = &encoder.emplace(index, PinOrDummy(a), PinOrDummy(b), PinOrDummy(indexPin), request.config);
        return HilStatus::done;
    }

    void TivaQeiFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        encoder = std::nullopt;
        onClosed();
    }

    HilStatus TivaQeiFactory::Parse(uint8_t index, const services::HilArguments& arguments, Request& request) const
    {
        auto& config = request.config;
        uint32_t velocityPeriod = 1000;

        HilStatus status = HilStatus::done;
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
        if (status != HilStatus::done)
            return status;

        if (config.offset >= config.resolution)
            return HilStatus::range;

        if (!request.a && !request.b && !request.index)
        {
            if (index != board::qeiIndex)
                return HilStatus::usage;

            request.a = board::encoderA;
            request.b = board::encoderB;
            request.index = board::encoderZ;
        }

        if (!request.a || !request.b)
            return HilStatus::usage;

        config.velocityPeriod = std::chrono::microseconds(velocityPeriod);
        return HilStatus::done;
    }
}
