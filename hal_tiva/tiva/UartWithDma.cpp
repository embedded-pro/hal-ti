#include "hal_tiva/tiva/UartWithDma.hpp"
#include "hal_tiva/tiva/Dma.hpp"
#include "infra/util/MemoryRange.hpp"

namespace hal::tiva
{
    namespace
    {
        constexpr const uint32_t UART_RIS_DMATXRIS = 0x00020000; // Transmit DMA Raw Interrupt Status
        constexpr const uint32_t UART_RIS_DMARXRIS = 0x00010000; // Receive DMA Raw Interrupt Status
        constexpr const uint32_t UART_RIS_9BITRIS = 0x00001000;  // 9-Bit Mode Raw Interrupt Status
        constexpr const uint32_t UART_RIS_EOTRIS = 0x00000800;   // End of Transmission Raw Interrupt Status
        constexpr const uint32_t UART_RIS_OERIS = 0x00000400;    // UART Overrun Error Raw Interrupt Status
        constexpr const uint32_t UART_RIS_BERIS = 0x00000200;    // UART Break Error Raw Interrupt Status
        constexpr const uint32_t UART_RIS_PERIS = 0x00000100;    // UART Parity Error Raw Interrupt Status
        constexpr const uint32_t UART_RIS_FERIS = 0x00000080;    // UART Framing Error Raw Interrupt Status
        constexpr const uint32_t UART_RIS_RTRIS = 0x00000040;    // UART Receive Time-Out Raw Interrupt Status
        constexpr const uint32_t UART_RIS_TXRIS = 0x00000020;    // UART Transmit Raw Interrupt Status
        constexpr const uint32_t UART_RIS_RXRIS = 0x00000010;    // UART Receive Raw Interrupt Status
        constexpr const uint32_t UART_RIS_DSRRIS = 0x00000008;   // UART Data Set Ready Modem Raw Interrupt Status
        constexpr const uint32_t UART_RIS_DCDRIS = 0x00000004;   // UART Data Carrier Detect Modem Raw Interrupt Status
        constexpr const uint32_t UART_RIS_CTSRIS = 0x00000002;   // UART Clear to Send Modem Raw Interrupt Status
        constexpr const uint32_t UART_RIS_RIRIS = 0x00000001;    // UART Ring Indicator Modem Raw Interrupt Status

        constexpr const uint32_t UART_ICR_DMATXIC = 0x00020000; // Transmit DMA Interrupt Clear
        constexpr const uint32_t UART_ICR_DMARXIC = 0x00010000; // Receive DMA Interrupt Clear
        constexpr const uint32_t UART_ICR_9BITIC = 0x00001000;  // 9-Bit Mode Interrupt Clear
        constexpr const uint32_t UART_ICR_EOTIC = 0x00000800;   // End of Transmission Interrupt Clear
        constexpr const uint32_t UART_ICR_OEIC = 0x00000400;    // Overrun Error Interrupt Clear
        constexpr const uint32_t UART_ICR_BEIC = 0x00000200;    // Break Error Interrupt Clear
        constexpr const uint32_t UART_ICR_PEIC = 0x00000100;    // Parity Error Interrupt Clear
        constexpr const uint32_t UART_ICR_FEIC = 0x00000080;    // Framing Error Interrupt Clear
        constexpr const uint32_t UART_ICR_RTIC = 0x00000040;    // Receive Time-Out Interrupt Clear
        constexpr const uint32_t UART_ICR_TXIC = 0x00000020;    // Transmit Interrupt Clear
        constexpr const uint32_t UART_ICR_RXIC = 0x00000010;    // Receive Interrupt Clear
        constexpr const uint32_t UART_ICR_DSRMIC = 0x00000008;  // UART Data Set Ready Modem Interrupt Clear
        constexpr const uint32_t UART_ICR_DCDMIC = 0x00000004;  // UART Data Carrier Detect Modem Interrupt Clear
        constexpr const uint32_t UART_ICR_CTSMIC = 0x00000002;  // UART Clear to Send Modem Interrupt Clear
        constexpr const uint32_t UART_ICR_RIMIC = 0x00000001;   // UART Ring Indicator Modem Interrupt Clear

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
    }

    UartWithDma::UartWithDma(uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, Dma& dma, const Config& config)
        : Uart(aUartIndex, uartTx, uartRx, config)
        , dmaTx{ dma, uartDmaChannels[aUartIndex].tx, DmaChannel::Configuration{ allAttributes, controlBlockTx } }
        , dmaRx{ dma, uartDmaChannels[aUartIndex].rx, DmaChannel::Configuration{ allAttributes, controlBlockRx } }
    {
        Initialize();
    }

    UartWithDma::UartWithDma(uint8_t aUartIndex, GpioPin& uartTx, GpioPin& uartRx, GpioPin& uartRts, GpioPin& uartCts, Dma& dma, const Config& config)
        : Uart{ aUartIndex, uartTx, uartRx, uartRts, uartCts, config }
        , dmaTx{ dma, uartDmaChannels[aUartIndex].tx, DmaChannel::Configuration{ allAttributes, controlBlockTx } }
        , dmaRx{ dma, uartDmaChannels[aUartIndex].rx, DmaChannel::Configuration{ allAttributes, controlBlockRx } }
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
        SetFifo(Uart::Fifo::_1_8, Uart::Fifo::_4_8);
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
    }

    void UartWithDma::SendData() const
    {
        dmaTx.StartTransfer(DmaChannel::Transfer::basic, infra::ConstCastMemoryRange(sendData).begin(), &uartArray[uartIndex]->DR, bytesSent);
    }

    void UartWithDma::Invoke()
    {
        auto status = InterruptStatus();

        if (status & UART_RIS_DMATXRIS)
        {
            InterruptClear(UART_ICR_DMATXIC);

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
    }
}
