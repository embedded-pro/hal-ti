#pragma once

#include "hal/interfaces/Can.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/BoundedDeque.hpp"
#include <cstdint>
#include <optional>
#include DEVICE_HEADER

namespace hal::tiva
{
    class Can
        : public hal::Can
        , private hal::ImmediateInterruptHandler
    {
    public:
        template<std::size_t StorageSize>
        using WithMaxRxBuffer = infra::WithStorage<Can, infra::BoundedDeque<std::pair<Id, Message>>::WithMaxSize<StorageSize>>;

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

        Can(infra::BoundedDeque<std::pair<Id, Message>>& rxBuffer, uint8_t canIndex, GpioPin& high, GpioPin& low, const Config& config, const infra::Function<void(Error)>& onError);
        ~Can();

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    private:
        void EnableClock() const;
        void DisableClock() const;
        void HandleInterrupt();
        void ScheduleError(Error error) const;
        void HandleStatusInterrupt(const CAN0_Type& can) const;
        void HandleTxInterrupt(CAN0_Type& can);
        void HandleRxInterrupt(CAN0_Type& can);
        void ProcessRxBuffer();
        void ConfigureBitTiming() const;
        void ConfigureReceiveMessageObject() const;

    private:
        static constexpr uint8_t txMessageObject = 1;
        static constexpr uint8_t rxMessageObject = 2;

        infra::BoundedDeque<std::pair<Id, Message>>& rxBuffer;
        uint8_t canIndex;
        PeripheralPin rx;
        PeripheralPin tx;
        Config config;
        infra::Function<void(bool success)> onSendComplete;
        infra::Function<void(Id id, const Message& data)> onReceive;
        infra::Function<void(Error)> onError;
        bool sending = false;
    };
}
