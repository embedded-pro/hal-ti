#pragma once

#include "hal/interfaces/Can.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/event/QueueForOneReaderOneIrqWriter.hpp"
#include <cstdint>
#include <optional>
#include DEVICE_HEADER

namespace hal::tiva
{
    struct CanRxEntry
    {
        uint32_t id;
        uint8_t data[8];
        uint8_t length;
        bool is29Bit;
    };

    class Can
        : public hal::Can
        , private hal::ImmediateInterruptHandler
    {
    public:
        template<std::size_t StorageSize>
        using WithMaxRxBuffer = infra::WithStorage<Can, std::array<CanRxEntry, StorageSize + 1>>;

        enum class Error : uint8_t
        {
            stuffError,
            formError,
            ackError,
            bit1Error,
            bit0Error,
            crcError,
            busOff,
            errorWarning,
            errorPassive,
            messageLost,
        };

        struct BitTiming
        {
            uint8_t phaseSegment1 = 0;
            uint8_t phaseSegment2 = 0;
            uint8_t synchronizationJumpWidth = 0;
            uint8_t baudratePrescaler = 0;
        };

        struct Config
        {
            constexpr Config()
            {}

            bool testMode = false;
            uint32_t bitRate = 1000000;
            std::optional<BitTiming> bitTiming = std::nullopt;
        };

        Can(infra::MemoryRange<CanRxEntry> rxStorage, uint8_t canIndex, GpioPin& rxPin, GpioPin& txPin, const Config& config, const infra::Function<void(Error)>& onError);
        ~Can();

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    private:
        void EnableClock() const;
        void DisableClock() const;
        void HandleInterrupt();
        void ScheduleError(Error error) const;
        void HandleStatusInterrupt(CAN0_Type& can) const;
        void HandleTxInterrupt(CAN0_Type& can);
        void HandleRxInterrupt(CAN0_Type& can);
        void ProcessRxBuffer();
        void ConfigureBitTiming() const;
        void ConfigureReceiveMessageObject() const;

    private:
        static constexpr uint8_t txMessageObject = 1;
        static constexpr uint8_t rxMessageObject = 2;

        infra::QueueForOneReaderOneIrqWriter<CanRxEntry> rxQueue;
        uint8_t canIndex;
        PeripheralPin rxPin;
        PeripheralPin txPin;
        Config config;
        infra::Function<void(bool success)> onSendComplete;
        infra::Function<void(Id id, const Message& data)> onReceive;
        infra::Function<void(Error)> onError;
        volatile bool sending = false;
    };
}
