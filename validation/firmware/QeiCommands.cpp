#include "validation/firmware/QeiCommands.hpp"
#include "BoardProfile.hpp"
#include <limits>

namespace validation
{
    namespace
    {
        using Config = hal::tiva::QuadratureEncoder::Config;

        constexpr uint32_t maximumVelocityPeriodUs = 1000000;

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

    QeiCommands::QeiCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<QeiCommands, &QeiCommands::Open>("qei.open", "<index> [a=] [b=] [idx=] [res=] [offset=] [inva=] [invb=] [invi=] [reset=] [cap=] [sig=] [vel=]", *this, context.response),
              Bind<QeiCommands, &QeiCommands::Read>("qei.read", "<index>", *this, context.response),
              Bind<QeiCommands, &QeiCommands::Close>("qei.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> QeiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status QeiCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "a", "b", "idx", "res", "offset", "inva", "invb", "invi", "reset", "cap", "sig", "vel" }))
            return Status::usage;

        uint32_t requested = 0;
        std::optional<PinId> a;
        std::optional<PinId> b;
        std::optional<PinId> indexPin;
        Config config;
        uint32_t velocityPeriod = 1000;

        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::qeis - 1, status);
        arguments.Pin("a", a, status);
        arguments.Pin("b", b, status);
        arguments.Pin("idx", indexPin, status);
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

        if (!a && !b && !indexPin)
        {
            if (requested != board::qeiIndex)
                return Status::usage;

            a = board::encoderA;
            b = board::encoderB;
            indexPin = board::encoderZ;
        }

        if (!a || !b)
            return Status::usage;

        if (index)
            return Status::busy;

        config.velocityPeriod = std::chrono::microseconds(velocityPeriod);

        const auto qei = static_cast<uint8_t>(requested);
        hal::tiva::GpioPin* aPin = nullptr;
        hal::tiva::GpioPin* bPin = nullptr;
        hal::tiva::GpioPin* indexGpio = nullptr;
        status = context.pins.ClaimFunction(a, owner::qei, hal::tiva::PinConfigPeripheral::qeiPhaseA, qei, aPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(b, owner::qei, hal::tiva::PinConfigPeripheral::qeiPhaseB, qei, bPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(indexPin, owner::qei, hal::tiva::PinConfigPeripheral::qeiIndex, qei, indexGpio);

        if (status != Status::done)
        {
            context.pins.Release(owner::qei);
            return status;
        }

        encoder.emplace(qei, *aPin, *bPin, PinOrDummy(indexGpio), config);
        index = qei;
        context.response.Ok();
        return Status::done;
    }

    Status QeiCommands::Read(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        const auto direction = encoder->Direction() == hal::SynchronousQuadratureEncoder::MotionDirection::forward ? "fwd" : "rev";
        context.response.Ok() << " pos=" << encoder->Position() << " dir=" << direction << " speed=" << encoder->Speed() << " res=" << encoder->Resolution();
        return Status::done;
    }

    Status QeiCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        encoder = std::nullopt;
        context.pins.Release(owner::qei);
        index = std::nullopt;
        context.response.Ok();
        return Status::done;
    }

    Status QeiCommands::Find(const Arguments& arguments)
    {
        uint32_t requested = 0;
        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::qeis - 1, status);
        if (status != Status::done)
            return status;

        if (index != requested)
            return Status::notOpen;

        return Status::done;
    }
}
