#include "validation/firmware/CanFactory.hpp"
#include "BoardProfile.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/Tokenizer.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include DEVICE_HEADER

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        using services::hil::Status;

        constexpr uint32_t maximumStandardId = 0x7ff;
        constexpr uint32_t maximumExtendedId = 0x1fffffff;
        constexpr uint32_t maximumBitRate = 1000000;

        constexpr std::array<const char*, 6> openKeys{ { "rx", "tx", "bitrate", "filter", "loopback", "recover" } };

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

    TivaCanFactory::TivaCanFactory(const services::hil::PinNaming& naming)
        : naming(naming)
    {}

    uint8_t TivaCanFactory::Instances() const
    {
        return board::cans;
    }

    infra::MemoryRange<const char* const> TivaCanFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    Status TivaCanFactory::Prepare(uint8_t index, const services::hil::Arguments& arguments)
    {
        Request request;
        return Parse(index, arguments, request);
    }

    Status TivaCanFactory::Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& opened)
    {
        Request request;
        Parse(index, arguments, request);

        hal::GpioPin* rx = nullptr;
        hal::GpioPin* tx = nullptr;
        Status status = pins.ClaimFunction(request.rx, Function(hal::tiva::PinConfigPeripheral::canRx), index, rx);
        if (status == Status::done)
            status = pins.ClaimFunction(request.tx, Function(hal::tiva::PinConfigPeripheral::canTx), index, tx);
        if (status != Status::done)
            return status;

        this->onError = onError;
        request.config.timing = request.bitRate;
        opened = &can.emplace(index, PinOrDummy(rx), PinOrDummy(tx), request.config, [this](hal::tiva::Can::Error error)
            {
                this->onError(errorNames[static_cast<std::size_t>(error)]);
            });

        return Status::done;
    }

    void TivaCanFactory::Close(uint8_t index, const infra::Function<void()>& onClosed)
    {
        this->onClosed = onClosed;

        // The driver schedules events capturing itself from its ISR; with the interrupt masked, destroying it
        // behind the already queued events guarantees none of them runs on a destroyed driver
        NVIC_DisableIRQ(index == 0 ? CAN0_IRQn : CAN1_IRQn);
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                can = std::nullopt;
                this->onClosed();
            });
    }

    Status TivaCanFactory::Parse(uint8_t index, const services::hil::Arguments& arguments, Request& request) const
    {
        Status status = Status::done;
        arguments.Pin("rx", naming, request.rx, status);
        arguments.Pin("tx", naming, request.tx, status);
        arguments.Number("bitrate", request.bitRate, 1, maximumBitRate, status);
        arguments.Flag("loopback", request.config.testMode, status);
        arguments.Flag("recover", request.config.autoBusOffRecovery, status);

        if (status == Status::done && arguments.Has("filter"))
        {
            infra::Tokenizer fields(*arguments.Key("filter"), ',');
            auto id = services::hil::ParseNumber(fields.Token(0));
            auto mask = services::hil::ParseNumber(fields.Token(1));
            auto extended = services::hil::ParseNumber(fields.Token(2));

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
                request.config.filter = filter;
            }
        }

        if (status != Status::done)
            return status;

        if (!BitRateAchievable(request.bitRate))
            return Status::range;

        if (!request.rx && !request.tx)
        {
            if (index != board::canIndex)
                return Status::usage;

            request.rx = board::canRx;
            request.tx = board::canTx;
        }

        if (!request.rx || !request.tx)
            return Status::usage;

        return Status::done;
    }
}
