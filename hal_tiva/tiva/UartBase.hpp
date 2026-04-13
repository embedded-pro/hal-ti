#ifndef HAL_UART_BASE_TIVA_HPP
#define HAL_UART_BASE_TIVA_HPP

#include DEVICE_HEADER
#include "hal/interfaces/SerialCommunication.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include <optional>

namespace hal::tiva
{
    class UartBase
        : public hal::SerialCommunication
        , protected hal::InterruptHandler
    {
    public:
        enum class Baudrate : uint32_t
        {
            _600_bps,
            _1200_bps,
            _2400_bps,
            _4800_bps,
            _9600_bps,
            _19200_bps,
            _38600_bps,
            _56700_bps,
            _115200_bps,
            _230400_bps,
            _460800_bps,
            _921000_bps,
        };

        enum class Parity : uint32_t
        {
            none,
            even,
            odd,
        };

        enum class StopBits : uint32_t
        {
            one,
            two,
        };

        enum class NumberOfBytes : uint32_t
        {
            _8_bytes,
            _16_bytes,
        };

        enum class FlowControl : uint32_t
        {
            none,
            rts,
            cts,
            rtsAndCts,
        };

        struct Config
        {
            constexpr Config(bool enableTx = true, bool enableRx = true)
                : enableTx(enableTx)
                , enableRx(enableRx)
            {}

            Config(bool enableTx, bool enableRx, Baudrate baudrate, FlowControl hwFlowControl, Parity parity, StopBits stopbits, NumberOfBytes numberOfBytes, std::optional<InterruptPriority> priority)
                : enableTx(enableTx)
                , enableRx(enableRx)
                , baudrate(baudrate)
                , hwFlowControl(hwFlowControl)
                , parity(parity)
                , stopbits(stopbits)
                , numberOfBytes(numberOfBytes)
                , priority(priority)
            {}

            bool enableTx;
            bool enableRx;
            Baudrate baudrate = Baudrate::_115200_bps;
            FlowControl hwFlowControl = FlowControl::none;
            Parity parity = Parity::none;
            StopBits stopbits = StopBits::one;
            NumberOfBytes numberOfBytes = NumberOfBytes::_8_bytes;
            std::optional<InterruptPriority> priority;
        };

    protected:
        enum class Fifo : uint8_t
        {
            _1_8,
            _2_8,
            _4_8,
            _6_8,
            _7_8,
        };

        UartBase(uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, const Config& config);
        UartBase(uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, GpioPin& uartRts, GpioPin& uartCts, const Config& config);
        ~UartBase();

        void EnableClock() const;
        void DisableClock() const;
        void Initialization(const Config& config);
        void RegisterInterrupt(const Config& config);
        void TransferComplete();
        void DisableUart() const;
        void EnableUart() const;
        void EnableRxDma() const;
        void EnableTxDma() const;
        void DisableRxDma() const;
        void DisableTxDma() const;
        void SetFifo(Fifo fifoRx, Fifo fifoTx) const;
        uint32_t InterruptStatus() const;
        void InterruptClear(uint32_t mask) const;

    protected:
        uint8_t uartIndex;
        PeripheralPin uartTx;
        PeripheralPin uartRx;
        std::optional<PeripheralPin> uartRts;
        std::optional<PeripheralPin> uartCts;
        uint32_t enableTx = 0;
        uint32_t enableRx = 0;

        infra::Function<void()> transferDataComplete;
        infra::Function<void(infra::ConstByteRange data)> dataReceived;

        infra::MemoryRange<const uint8_t> sendData;
        bool sending = false;

        infra::MemoryRange<UART0_Type* const> uartArray;
        infra::MemoryRange<IRQn_Type const> irqArray;
    };
}

#endif
