#include "hal_tiva/tiva/Uart.hpp"
#include "infra/util/BoundedVector.hpp"

namespace hal::tiva
{
    namespace
    {
        // NOLINTBEGIN
        constexpr uint32_t UART_RIS_OERIS = 0x00000400; // UART Overrun Error Raw Interrupt Status
        constexpr uint32_t UART_RIS_RXRIS = 0x00000010; // UART Receive Raw Interrupt Status
        constexpr uint32_t UART_RIS_RTRIS = 0x00000040; // UART Receive Time-Out Raw Interrupt Status
        constexpr uint32_t UART_RIS_TXRIS = 0x00000020; // UART Transmit Raw Interrupt Status

        constexpr uint32_t UART_FR_RXFE = 0x00000010; // UART Receive FIFO Empty

        constexpr uint32_t UART_ICR_OEIC = 0x00000400; // Overrun Error Interrupt Clear
        constexpr uint32_t UART_ICR_RTIC = 0x00000040; // Receive Time-Out Interrupt Clear
        constexpr uint32_t UART_ICR_RXIC = 0x00000010; // Receive Interrupt Clear
        constexpr uint32_t UART_ICR_TXIC = 0x00000020; // Transmit Interrupt Clear

        constexpr uint32_t UART_IM_TXIM = 0x00000020; // UART Transmit Interrupt Mask
        constexpr uint32_t UART_IM_RXIM = 0x00000010; // UART Receive Interrupt Mask
        constexpr uint32_t UART_IM_RTIM = 0x00000040; // UART Receive Time-Out Interrupt Mask
        // NOLINTEND
    }

    void Uart::SendData(infra::MemoryRange<const uint8_t> data, infra::Function<void()> actionOnCompletion)
    {
        if (enableTx)
        {
            transferDataComplete = actionOnCompletion;
            sendData = data;
            sending = true;

            uartArray[uartIndex]->IM |= UART_IM_TXIM;
        }
    }

    void Uart::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        this->dataReceived = dataReceived;

        auto imMask = UART_IM_RXIM | UART_IM_RTIM;
        uartArray[uartIndex]->IM = (uartArray[uartIndex]->IM & ~imMask) | (dataReceived ? imMask : 0);
    }

    void Uart::Invoke()
    {
        if (uartArray[uartIndex]->RIS & UART_RIS_OERIS)
            uartArray[uartIndex]->ICR = UART_ICR_OEIC;

        if (uartArray[uartIndex]->RIS & (UART_RIS_RXRIS | UART_RIS_RTRIS))
        {
            uartArray[uartIndex]->ICR = UART_ICR_RXIC | UART_ICR_RTIC;

            while (!(uartArray[uartIndex]->FR & UART_FR_RXFE))
            {
                infra::BoundedVector<uint8_t>::WithMaxSize<8> buffer;

                while (!buffer.full() && !(uartArray[uartIndex]->FR & UART_FR_RXFE))
                    buffer.push_back(static_cast<uint8_t>(uartArray[uartIndex]->DR));

                if (dataReceived != nullptr)
                    dataReceived(buffer.range());
            }
        }

        if (sending)
        {
            if (!sendData.empty() && (uartArray[uartIndex]->RIS & UART_RIS_TXRIS))
            {
                uartArray[uartIndex]->ICR = UART_ICR_TXIC;

                uartArray[uartIndex]->DR = sendData.front();
                sendData.pop_front();
            }

            if (sendData.empty())
            {
                TransferComplete();
                uartArray[uartIndex]->IM &= ~UART_IM_TXIM;
            }
        }
    }

}
