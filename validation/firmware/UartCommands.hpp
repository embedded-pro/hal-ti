#ifndef VALIDATION_UART_COMMANDS_HPP
#define VALIDATION_UART_COMMANDS_HPP

#include "hal/synchronous_interfaces/TimeKeeper.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousUart.hpp"
#include "hal_tiva/tiva/Uart.hpp"
#include "hal_tiva/tiva/UartWithDma.hpp"
#include "infra/event/QueueForOneReaderOneIrqWriter.hpp"
#include "infra/timer/Timer.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <optional>
#include <variant>

namespace validation
{
    class DeadlineTimeKeeper
        : public hal::TimeKeeper
    {
    public:
        void Arm(infra::Duration duration);

        bool Timeout() override;
        void Reset() override;

    private:
        infra::TimePoint deadline;
    };

    // hal::tiva::Uart only inherits UartBase's protected constructors, so it cannot be constructed directly
    class InterruptUart
        : public hal::tiva::Uart
    {
    public:
        InterruptUart(uint8_t index, hal::tiva::GpioPin& tx, hal::tiva::GpioPin& rx, const Config& config);
        InterruptUart(uint8_t index, hal::tiva::GpioPin& tx, hal::tiva::GpioPin& rx, hal::tiva::GpioPin& rts, hal::tiva::GpioPin& cts, const Config& config);
    };

    class UartCommands
        : public services::TerminalCommands
    {
    public:
        explicit UartCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        static constexpr std::size_t receiveCapacity = 256;
        static constexpr std::size_t transmitCapacity = 112;

        using SynchronousUart = hal::tiva::SynchronousUart::WithStorage<64>;
        using UartWithDma = hal::tiva::UartWithDma::WithRxBuffer<64>;

        Status Open(const Arguments& arguments);
        Status Send(const Arguments& arguments);
        Status Receive(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status Find(const Arguments& arguments);
        void Received(infra::ConstByteRange data);
        void DrainSynchronous(std::size_t wanted);
        void CheckReceive();
        void FinishReceive();
        void SendDone(uint32_t generation);
        void SendTimeout();

    private:
        Context& context;
        std::optional<uint8_t> index;
        uint32_t baudRate = 0;
        std::variant<std::monostate, InterruptUart, UartWithDma, SynchronousUart> driver;
        DeadlineTimeKeeper timeKeeper;
        infra::QueueForOneReaderOneIrqWriter<uint8_t>::WithStorage<receiveCapacity> received;
        std::array<uint8_t, transmitCapacity> transmitBuffer{};
        uint32_t sendGeneration = 0;
        bool transmitting = false;
        bool awaitingSend = false;
        std::optional<std::size_t> receiveWanted;
        infra::TimerSingleShot sendTimer;
        infra::TimerSingleShot receiveTimer;
        std::array<Command, 4> commands;
    };
}

#endif
