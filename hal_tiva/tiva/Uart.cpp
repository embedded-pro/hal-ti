#include "hal_tiva/tiva/Uart.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace hal::tiva
{
    namespace
    {
        // NOLINTBEGIN
        constexpr uint32_t UART_RIS_OERIS = 0x00000400; // UART Overrun Error Raw Interrupt Status
        constexpr uint32_t UART_RIS_RXRIS = 0x00000010; // UART Receive Raw Interrupt Status
        constexpr uint32_t UART_RIS_TXRIS = 0x00000020; // UART Transmit Raw Interrupt Status

        constexpr uint32_t UART_ICR_RXIC = 0x00000010; // Receive Interrupt Clear
        constexpr uint32_t UART_ICR_TXIC = 0x00000020; // Transmit Interrupt Clear

        constexpr uint32_t UART_IM_TXIM = 0x00000020; // UART Transmit Interrupt Mask
        constexpr uint32_t UART_IM_RXIM = 0x00000010; // UART Receive Interrupt Mask
        // NOLINTEND
    }

    void Uart::SendData(infra::MemoryRange<const uint8_t> data, infra::Function<void()> actionOnCompletion)
    {
        if (enableTx)
        {
            transferDataComplete = actionOnCompletion;
            sendData = data;
            sending = true;

            uartArray[uartIndex]->IM |= UART_IM_TXIM; /* Enable TX interrupt */
        }
    }

    void Uart::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        this->dataReceived = dataReceived;

        uartArray[uartIndex]->IM = (uartArray[uartIndex]->IM & ~UART_IM_RXIM) | (!dataReceived ? UART_IM_RXIM : 0); /* Enable RX interrupt */
    }

    void Uart::Invoke()
    {
        really_assert(!(uartArray[uartIndex]->RIS & UART_RIS_OERIS));

        if (uartArray[uartIndex]->RIS & UART_RIS_RXRIS)
        {
            infra::BoundedVector<uint8_t>::WithMaxSize<8> buffer;

            while (!buffer.full() && (uartArray[uartIndex]->RIS & UART_RIS_RXRIS))
            {
                uartArray[uartIndex]->ICR |= UART_ICR_RXIC;

                auto receivedByte = static_cast<uint8_t>(uartArray[uartIndex]->DR);
                buffer.push_back(receivedByte);
            }

            if (dataReceived != nullptr)
                dataReceived(buffer.range());
        }

        if (sending)
        {
            if (!sendData.empty() && (uartArray[uartIndex]->RIS & UART_RIS_TXRIS))
            {
                uartArray[uartIndex]->ICR |= UART_ICR_TXIC;

                uartArray[uartIndex]->DR = sendData.front();
                sendData.pop_front();
            }

            if (sendData.empty())
            {
                TransferComplete();
                uartArray[uartIndex]->IM &= ~UART_IM_TXIM; /* Disable TX interrupt */
            }
        }
    }

}
