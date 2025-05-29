#ifndef HAL_UART_WITH_DMA_TIVA_HPP
#define HAL_UART_WITH_DMA_TIVA_HPP

#include DEVICE_HEADER
#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/Uart.hpp"

namespace hal::tiva
{
    class UartWithDma
        : public Uart
    {
    public:
        UartWithDma(uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, Dma& dma, const Config& config = Config(true, true));
        UartWithDma(uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, GpioPin& uartRts, GpioPin& uartCts, Dma& dma, const Config& config = Config(true, true));
        ~UartWithDma();

        void SendData(infra::MemoryRange<const uint8_t> data, infra::Function<void()> actionOnCompletion = infra::emptyFunction) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        void Initialize() const;
        void SendData() const;
        void ReceiveData() const;
        void Invoke() override;

    private:
        const DmaChannel::Attributes allAttributes{ false, true, true, true };
        const DmaChannel::ControlBlock controlBlockTx{ DmaChannel::Increment::_8_bits, DmaChannel::Increment::none, DmaChannel::DataSize::_8_bits, DmaChannel::ArbitrationSize::_4_items };
        const DmaChannel::ControlBlock controlBlockRx{ DmaChannel::Increment::none, DmaChannel::Increment::_8_bits, DmaChannel::DataSize::_8_bits, DmaChannel::ArbitrationSize::_4_items };
        DmaChannel dmaTx;
        DmaChannel dmaRx;
        std::size_t bytesSent = 0;
    };
}

#endif
