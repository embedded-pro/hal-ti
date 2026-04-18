#include "hal_tiva/tiva/UartWithDma.hpp"
#include "hal_tiva/tiva/Dma.hpp"
#include "infra/util/MemoryRange.hpp"

namespace hal::tiva
{
    namespace
    {
        constexpr const uint32_t UART_FR_RXFE = 0x00000010;      // Receive FIFO Empty
        constexpr const uint32_t UART_RIS_DMATXRIS = 0x00020000; // Transmit DMA Raw Interrupt Status
        constexpr const uint32_t UART_RIS_DMARXRIS = 0x00010000; // Receive DMA Raw Interrupt Status
        constexpr const uint32_t UART_RIS_RTRIS = 0x00000040;    // UART Receive Time-Out Raw Interrupt Status

        constexpr const uint32_t UART_ICR_DMATXIC = 0x00020000; // Transmit DMA Interrupt Clear
        constexpr const uint32_t UART_ICR_DMARXIC = 0x00010000; // Receive DMA Interrupt Clear
        constexpr const uint32_t UART_ICR_RTIC = 0x00000040;    // Receive Time-Out Interrupt Clear

        struct UartDma
        {
            DmaChannel::Channel tx;
            DmaChannel::Channel rx;
        };

        constexpr const std::array<UartDma, 8> uartDmaChannels = { {
            { { 9, 0 }, { 8, 0 } },
            { { 23, 0 }, { 22, 0 } },
            { { 1, 1 }, { 0, 1 } },
            { { 17, 2 }, { 16, 2 } },
            { { 19, 2 }, { 18, 2 } },
            { { 7, 2 }, { 6, 2 } },
            { { 11, 2 }, { 10, 2 } },
            { { 21, 2 }, { 20, 2 } },
        } };

        constexpr DmaChannel::Attributes txAttributes{ false, false, true, false };
        constexpr DmaChannel::Attributes rxAttributes{ true, false, true, false };
        constexpr DmaChannel::ControlBlock controlBlockTx{ DmaChannel::Increment::_8_bits, DmaChannel::Increment::none, DmaChannel::DataSize::_8_bits, DmaChannel::ArbitrationSize::_4_items };
        constexpr DmaChannel::ControlBlock controlBlockRx{ DmaChannel::Increment::none, DmaChannel::Increment::_8_bits, DmaChannel::DataSize::_8_bits, DmaChannel::ArbitrationSize::_4_items };
    }

    UartWithDma::UartWithDma(infra::MemoryRange<uint8_t> rxBuffer, uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, Dma& dma, const Config& config)
        : UartBase(aUartIndex, uartTx, uartRx, config)
        , dmaTx{ dma, uartDmaChannels[aUartIndex].tx, DmaChannel::Configuration{ txAttributes, controlBlockTx } }
        , dmaRx{ dma, uartDmaChannels[aUartIndex].rx, DmaChannel::Configuration{ rxAttributes, controlBlockRx } }
        , rxBufferPrimary{ rxBuffer.begin(), rxBuffer.begin() + rxBuffer.size() / 2 }
        , rxBufferAlternate{ rxBuffer.begin() + rxBuffer.size() / 2, rxBuffer.end() }
    {
        Initialize();
    }

    UartWithDma::UartWithDma(infra::MemoryRange<uint8_t> rxBuffer, uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, GpioPin& uartRts, GpioPin& uartCts, Dma& dma, const Config& config)
        : UartBase{ aUartIndex, uartTx, uartRx, uartRts, uartCts, config }
        , dmaTx{ dma, uartDmaChannels[aUartIndex].tx, DmaChannel::Configuration{ txAttributes, controlBlockTx } }
        , dmaRx{ dma, uartDmaChannels[aUartIndex].rx, DmaChannel::Configuration{ rxAttributes, controlBlockRx } }
        , rxBufferPrimary{ rxBuffer.begin(), rxBuffer.begin() + rxBuffer.size() / 2 }
        , rxBufferAlternate{ rxBuffer.begin() + rxBuffer.size() / 2, rxBuffer.end() }
    {
        Initialize();
    }

    UartWithDma::~UartWithDma()
    {
        DisableTxDma();
        DisableRxDma();
        DisableUart();
    }

    void UartWithDma::Initialize() const
    {
        DisableUart();
        SetFifo(Fifo::_1_8, Fifo::_4_8);
        EnableRxDma();
        EnableTxDma();
        EnableUart();
    }

    void UartWithDma::SendData(infra::MemoryRange<const uint8_t> data, infra::Function<void()> actionOnCompletion)
    {
        transferDataComplete = actionOnCompletion;
        sendData = data;
        sending = true;
        bytesSent = sendData.size() < dmaTx.MaxTransferSize() ? sendData.size() : dmaTx.MaxTransferSize();

        SendData();
    }

    void UartWithDma::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        this->dataReceived = dataReceived;

        ReceiveData();
    }

    void UartWithDma::ReceiveData() const
    {
        DmaChannel::Buffers primaryBuffers{ reinterpret_cast<volatile void*>(&(uartArray[uartIndex]->DR)), rxBufferPrimary.begin(), rxBufferPrimary.size() };
        DmaChannel::Buffers alternateBuffers{ reinterpret_cast<volatile void*>(&(uartArray[uartIndex]->DR)), rxBufferAlternate.begin(), rxBufferAlternate.size() };

        dmaRx.StartPingPongTransfer(primaryBuffers, alternateBuffers);
    }

    void UartWithDma::SendData() const
    {
        dmaTx.StartTransfer(DmaChannel::Transfer::basic, DmaChannel::Buffers{ infra::ConstCastMemoryRange(sendData).begin(), &uartArray[uartIndex]->DR, bytesSent });
    }

    void UartWithDma::ProcessDmaTx()
    {
        sendData.shrink_from_front_to(sendData.size() - bytesSent);
        bytesSent = sendData.size() <= dmaTx.MaxTransferSize() ? sendData.size() : dmaTx.MaxTransferSize();

        if (!sendData.empty())
            SendData();

        if (sendData.empty())
        {
            sending = false;
            infra::EventDispatcher::Instance().Schedule(transferDataComplete);
            transferDataComplete = nullptr;
        }
    }

    void UartWithDma::ProcessDmaRx() const
    {
        auto* drAddr = reinterpret_cast<volatile void*>(&(uartArray[uartIndex]->DR));

        if (dmaRx.IsPrimaryTransferCompleted())
        {
            dmaRx.ReArmPingPongHalf(false, { drAddr, rxBufferPrimary.begin(), rxBufferPrimary.size() });
            if (dataReceived != nullptr)
                dataReceived(rxBufferPrimary);
        }

        if (dmaRx.IsAlternateTransferCompleted())
        {
            dmaRx.ReArmPingPongHalf(true, { drAddr, rxBufferAlternate.begin(), rxBufferAlternate.size() });
            if (dataReceived != nullptr)
                dataReceived(rxBufferAlternate);
        }
    }

    void UartWithDma::ProcessRxTimeout() const
    {
        bool fillingAlternate = dmaRx.IsPrimaryTransferCompleted();
        auto activeBuffer = fillingAlternate ? rxBufferAlternate : rxBufferPrimary;
        std::size_t bytesReceived = activeBuffer.size() - dmaRx.RemainingTransfers(fillingAlternate);
        dmaRx.StopTransfer();

        while (bytesReceived < activeBuffer.size() && (uartArray[uartIndex]->FR & UART_FR_RXFE) == 0)
        {
            activeBuffer[bytesReceived] = static_cast<uint8_t>(uartArray[uartIndex]->DR);
            ++bytesReceived;
        }

        if (bytesReceived > 0 && dataReceived != nullptr)
            dataReceived(infra::MakeRange(activeBuffer.begin(), activeBuffer.begin() + bytesReceived));

        if (dataReceived != nullptr)
            ReceiveData();
    }

    void UartWithDma::Invoke()
    {
        auto rawStatus = InterruptStatus();
        auto maskedStatus = MaskedInterruptStatus();

        if (rawStatus & UART_RIS_DMATXRIS)
        {
            InterruptClear(UART_ICR_DMATXIC);
            ProcessDmaTx();
        }

        if (rawStatus & UART_RIS_DMARXRIS)
        {
            InterruptClear(UART_ICR_DMARXIC);
            ProcessDmaRx();
        }

        if (maskedStatus & UART_RIS_RTRIS)
        {
            InterruptClear(UART_ICR_RTIC);
            ProcessRxTimeout();
        }
    }
}
