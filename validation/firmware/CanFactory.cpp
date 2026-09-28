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
        using services::HilStatus;

        constexpr uint32_t maximumStandardId = 0x7ff;
        constexpr uint32_t maximumExtendedId = 0x1fffffff;
        constexpr uint32_t maximumBitRate = 1000000;

        constexpr uint32_t maximumPhaseSegment1 = 16;
        constexpr uint32_t maximumPhaseSegment2 = 8;
        constexpr uint32_t maximumJumpWidth = 4;
        constexpr uint32_t maximumPrescaler = 1024;

        constexpr std::array<const char*, 7> openKeys{ { "rx", "tx", "bitrate", "timing", "filter", "loopback", "recover" } };

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

        HilStatus ParseFilter(infra::BoundedConstString text, hal::tiva::Can::Config& config)
        {
            infra::Tokenizer fields(text, ',');
            auto id = services::HilArguments::ParseNumber(fields.Token(0));
            auto mask = services::HilArguments::ParseNumber(fields.Token(1));
            auto extended = services::HilArguments::ParseNumber(fields.Token(2));
            auto match = fields.Size() == 4 ? services::HilArguments::ParseNumber(fields.Token(3)) : std::optional<uint32_t>(1);

            if (fields.Size() < 3 || fields.Size() > 4 || !id || !mask || !extended || !match || *extended > 1 || *match > 1)
                return HilStatus::usage;

            const auto maximumId = *extended != 0 ? maximumExtendedId : maximumStandardId;
            if (*id > maximumId || *mask > maximumId)
                return HilStatus::range;

            config.filter = hal::tiva::Can::Filter{ *id, *mask, *extended != 0, *match != 0 };
            return HilStatus::done;
        }

        HilStatus ParseTiming(infra::BoundedConstString text, hal::tiva::Can::Config& config)
        {
            infra::Tokenizer fields(text, ',');
            std::array<uint32_t, 4> values{};
            constexpr std::array<uint32_t, 4> maxima{ { maximumPhaseSegment1, maximumPhaseSegment2, maximumJumpWidth, maximumPrescaler } };

            if (fields.Size() != values.size())
                return HilStatus::usage;

            for (std::size_t i = 0; i != values.size(); ++i)
            {
                auto value = services::HilArguments::ParseNumber(fields.Token(i));
                if (!value)
                    return HilStatus::usage;

                if (*value == 0 || *value > maxima[i])
                    return HilStatus::range;

                values[i] = *value;
            }

            config.timing = hal::tiva::Can::BitTiming{ static_cast<uint8_t>(values[0]), static_cast<uint8_t>(values[1]), static_cast<uint8_t>(values[2]), static_cast<uint16_t>(values[3]) };
            return HilStatus::done;
        }
    }

    TivaCanFactory::TivaCanFactory(const services::HilPinNaming& naming)
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

    HilStatus TivaCanFactory::Prepare(uint8_t index, const services::HilArguments& arguments)
    {
        Request request;
        return Parse(index, arguments, request);
    }

    HilStatus TivaCanFactory::Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& opened)
    {
        Request request;
        Parse(index, arguments, request);

        hal::GpioPin* rx = nullptr;
        hal::GpioPin* tx = nullptr;
        HilStatus status = pins.ClaimFunction(request.rx, Function(hal::tiva::PinConfigPeripheral::canRx), index, rx);
        if (status == HilStatus::done)
            status = pins.ClaimFunction(request.tx, Function(hal::tiva::PinConfigPeripheral::canTx), index, tx);
        if (status != HilStatus::done)
            return status;

        this->onError = onError;
        opened = &can.emplace(index, PinOrDummy(rx), PinOrDummy(tx), request.config, [this](hal::tiva::Can::Error error)
            {
                this->onError(errorNames[static_cast<std::size_t>(error)]);
            });

        return HilStatus::done;
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

    HilStatus TivaCanFactory::Parse(uint8_t index, const services::HilArguments& arguments, Request& request) const
    {
        HilStatus status = HilStatus::done;
        arguments.Pin("rx", naming, request.rx, status);
        arguments.Pin("tx", naming, request.tx, status);
        arguments.Number("bitrate", request.bitRate, 1, maximumBitRate, status);
        arguments.Flag("loopback", request.config.testMode, status);
        arguments.Flag("recover", request.config.autoBusOffRecovery, status);

        if (status == HilStatus::done && arguments.Has("filter"))
            status = ParseFilter(*arguments.Key("filter"), request.config);

        if (status == HilStatus::done && arguments.Has("timing"))
            status = arguments.Has("bitrate") ? HilStatus::usage : ParseTiming(*arguments.Key("timing"), request.config);
        else if (status == HilStatus::done && !BitRateAchievable(request.bitRate))
            status = HilStatus::range;
        else
            request.config.timing = request.bitRate;

        if (status != HilStatus::done)
            return status;

        if (!request.rx && !request.tx)
        {
            if (!board::defaultCan || board::defaultCan->index != index)
                return HilStatus::usage;

            request.rx = board::defaultCan->rx;
            request.tx = board::defaultCan->tx;
        }

        if (!request.rx || !request.tx)
            return HilStatus::usage;

        return HilStatus::done;
    }
}
