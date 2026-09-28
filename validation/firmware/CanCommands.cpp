#include "validation/firmware/CanCommands.hpp"
#include "BoardProfile.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/Tokenizer.hpp"
#include DEVICE_HEADER

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        constexpr uint32_t maximumStandardId = 0x7ff;
        constexpr uint32_t maximumExtendedId = 0x1fffffff;
        constexpr uint32_t maximumBitRate = 1000000;
        constexpr std::size_t maximumData = 8;
        constexpr infra::Duration sendTimeout = std::chrono::milliseconds(1000);
        constexpr infra::Duration errorRepeatInterval = std::chrono::milliseconds(100);

        constexpr std::array<const char*, 10> errorNames{ {
            "stuffError",
            "formError",
            "ackError",
            "bit1Error",
            "bit0Error",
            "crcError",
            "busOff",
            "errorWarning",
            "errorPassive",
            "messageLost",
        } };

        // Mirrors the driver's bit timing search, which asserts when no C_CAN compatible timing exists
        bool BitRateAchievable(uint32_t bitRate)
        {
            if (bitRate == 0 || SystemCoreClock % bitRate != 0)
                return false;

            const auto bitClocks = SystemCoreClock / bitRate;

            for (uint32_t quanta = 8; quanta <= 25; ++quanta)
            {
                if (bitClocks % quanta != 0 || bitClocks / quanta > 1024)
                    continue;

                const auto beforeSample = (875 * quanta + 500) / 1000;
                const auto phaseSegment1 = beforeSample - 1;
                const auto phaseSegment2 = quanta - 1 - phaseSegment1;

                if (beforeSample >= 3 && phaseSegment1 <= 16 && phaseSegment2 >= 1 && phaseSegment2 <= 8)
                    return true;
            }

            return false;
        }
    }

    CanCommands::CanCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<CanCommands, &CanCommands::Open>("can.open", "<index> [rx=] [tx=] [bitrate=] [filter=] [loopback=] [recover=]", *this, context.response),
              Bind<CanCommands, &CanCommands::Send>("can.send", "<index> <id> <hex> [ext=]", *this, context.response),
              Bind<CanCommands, &CanCommands::Close>("can.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> CanCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status CanCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "rx", "tx", "bitrate", "filter", "loopback", "recover" }))
            return Status::usage;

        uint32_t requested = 0;
        std::optional<PinId> rx;
        std::optional<PinId> tx;
        uint32_t bitRate = 500000;
        hal::tiva::Can::Config config;

        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::cans - 1, status);
        arguments.Pin("rx", rx, status);
        arguments.Pin("tx", tx, status);
        arguments.Number("bitrate", bitRate, 1, maximumBitRate, status);
        arguments.Flag("loopback", config.testMode, status);
        arguments.Flag("recover", config.autoBusOffRecovery, status);

        if (status == Status::done && arguments.Has("filter"))
        {
            infra::Tokenizer fields(*arguments.Key("filter"), ',');
            auto id = ParseNumber(fields.Token(0));
            auto mask = ParseNumber(fields.Token(1));
            auto extended = ParseNumber(fields.Token(2));

            if (fields.Size() != 3 || !id || !mask || !extended || *extended > 1)
                status = Status::usage;
            else if (*id > (*extended != 0 ? maximumExtendedId : maximumStandardId) || *mask > (*extended != 0 ? maximumExtendedId : maximumStandardId))
                status = Status::range;
            else
            {
                hal::tiva::Can::Filter filter;
                filter.id = *id;
                filter.mask = *mask;
                filter.extended = *extended != 0;
                filter.matchIdType = true;
                config.filter = filter;
            }
        }

        if (status != Status::done)
            return status;

        if (!BitRateAchievable(bitRate))
            return Status::range;

        if (!rx && !tx)
        {
            if (requested != board::canIndex)
                return Status::usage;

            rx = board::canRx;
            tx = board::canTx;
        }

        if (!rx || !tx)
            return Status::usage;

        if (index)
            return Status::busy;

        const auto controller = static_cast<uint8_t>(requested);
        hal::tiva::GpioPin* rxPin = nullptr;
        hal::tiva::GpioPin* txPin = nullptr;
        status = context.pins.ClaimFunction(rx, owner::can, hal::tiva::PinConfigPeripheral::canRx, controller, rxPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(tx, owner::can, hal::tiva::PinConfigPeripheral::canTx, controller, txPin);

        if (status != Status::done)
        {
            context.pins.Release(owner::can);
            return status;
        }

        config.timing = bitRate;
        lastError = std::nullopt;
        can.emplace(controller, *rxPin, *txPin, config, [this](hal::tiva::Can::Error error)
            {
                Error(error);
            });
        can->ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                Received(id, data);
            });

        index = controller;
        context.response.Ok();
        return Status::done;
    }

    Status CanCommands::Send(const Arguments& arguments)
    {
        if (!arguments.Shape(3, 3, { "ext" }))
            return Status::usage;

        uint32_t id = 0;
        bool extended = false;
        std::array<uint8_t, maximumData> payload{};
        std::size_t size = 0;
        Status status = Find(arguments);
        arguments.Flag("ext", extended, status);
        arguments.NumberAt(1, id, 0, extended ? maximumExtendedId : maximumStandardId, status);
        if (status == Status::done)
            status = ParseHex(arguments.Positional(2), payload, size);
        if (status != Status::done)
            return status;

        if (transmitting)
            return Status::busy;

        hal::Can::Message message;
        for (std::size_t i = 0; i != size; ++i)
            message.push_back(payload[i]);

        transmitting = true;
        awaiting = true;
        const auto current = ++generation;
        timer.Start(sendTimeout, [this]()
            {
                SendTimeout();
            });

        can->SendData(extended ? hal::Can::Id::Create29BitId(id) : hal::Can::Id::Create11BitId(id), message, [this, current](bool success)
            {
                SendDone(current, success);
            });

        return Status::done;
    }

    Status CanCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        ++generation;
        transmitting = false;
        awaiting = false;
        closing = true;
        timer.Cancel();

        // The driver schedules events capturing itself from its ISR; with the interrupt masked, destroying it
        // behind the already queued events guarantees none of them runs on a destroyed driver
        NVIC_DisableIRQ(*index == 0 ? CAN0_IRQn : CAN1_IRQn);
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                can = std::nullopt;
                context.pins.Release(owner::can);
                index = std::nullopt;
                closing = false;
                context.response.Ok();
            });

        return Status::done;
    }

    Status CanCommands::Find(const Arguments& arguments)
    {
        uint32_t requested = 0;
        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::cans - 1, status);
        if (status != Status::done)
            return status;

        if (index != requested || closing)
            return Status::notOpen;

        return Status::done;
    }

    void CanCommands::Received(hal::Can::Id id, const hal::Can::Message& data)
    {
        const bool extended = id.Is29BitId();
        (context.response.Event("can") << " index=" << *index << " id=" << (extended ? id.Get29BitId() : id.Get11BitId()) << " ext=" << (extended ? 1u : 0u) << " data=")
            .Hex(infra::ConstByteRange(data.begin(), data.end()));
    }

    void CanCommands::Error(hal::tiva::Can::Error error)
    {
        if (!index)
            return;

        const auto now = infra::Now();
        if (lastError == error && now - lastErrorTime < errorRepeatInterval)
            return;

        lastError = error;
        lastErrorTime = now;
        context.response.Event("can") << " index=" << *index << " error=" << errorNames[static_cast<std::size_t>(error)];
    }

    void CanCommands::SendDone(uint32_t current, bool success)
    {
        if (current != generation)
            return;

        transmitting = false;

        if (awaiting)
        {
            awaiting = false;
            timer.Cancel();

            if (success)
                context.response.Ok();
            else
                context.response.Error(Status::failed);
        }
    }

    void CanCommands::SendTimeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(Status::timeout);
    }
}
