#ifndef HAL_UART_WITH_DMA_TIVA_HPP
#define HAL_UART_WITH_DMA_TIVA_HPP

#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/UartBase.hpp"

namespace hal::tiva
{
    class UartWithDma
        : public UartBase
    {
    public:
        template<std::size_t RxBufferSize>
        using WithRxBuffer = infra::WithStorage<UartWithDma, std::array<uint8_t, RxBufferSize>>;

        UartWithDma(infra::MemoryRange<uint8_t> rxBuffer, uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, Dma& dma, const Config& config = Config(true, true));
        UartWithDma(infra::MemoryRange<uint8_t> rxBuffer, uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, GpioPin& uartRts, GpioPin& uartCts, Dma& dma, const Config& config = Config(true, true));
        ~UartWithDma();

        void SendData(infra::MemoryRange<const uint8_t> data, infra::Function<void()> actionOnCompletion = infra::emptyFunction) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        void Initialize() const;
        void SendData() const;
        void ReceiveData() const;
        void Invoke() override;
        void ProcessDmaTx();
        void ProcessDmaRx() const;
        void ProcessRxTimeout() const;

    private:
        DmaChannel dmaTx;
        DmaChannel dmaRx;
        std::size_t bytesSent = 0;
        infra::MemoryRange<uint8_t> rxBufferPrimary;
        infra::MemoryRange<uint8_t> rxBufferAlternate;
    };
}

#endif
