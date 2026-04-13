#include "hal_tiva/tiva/UartBase.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/BitLogic.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    extern "C" void Uart0_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART0_IRQn);
    }

    extern "C" void Uart1_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART1_IRQn);
    }

    extern "C" void Uart2_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART2_IRQn);
    }

    extern "C" void Uart3_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART3_IRQn);
    }

    extern "C" void Uart4_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART4_IRQn);
    }

    extern "C" void Uart5_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART5_IRQn);
    }

    extern "C" void Uart6_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART6_IRQn);
    }

    extern "C" void Uart7_Handler()
    {
        hal::InterruptTable::Instance().Invoke(UART7_IRQn);
    }
}

namespace hal::tiva
{
    namespace
    {
        // NOLINTBEGIN
        constexpr uint32_t UART_FR_BUSY = 0x00000008;

        constexpr uint32_t UART_LCRH_WLEN_8 = 0x00000060;
        constexpr uint32_t UART_LCRH_FEN    = 0x00000010;

        constexpr uint32_t UART_CTL_CTSEN  = 0x00008000;
        constexpr uint32_t UART_CTL_RTSEN  = 0x00004000;
        constexpr uint32_t UART_CTL_RXE    = 0x00000200;
        constexpr uint32_t UART_CTL_TXE    = 0x00000100;
        constexpr uint32_t UART_CTL_HSE    = 0x00000020;
        constexpr uint32_t UART_CTL_EOT    = 0x00000010;
        constexpr uint32_t UART_CTL_UARTEN = 0x00000001;

        constexpr uint32_t UART_IFLS_RX7_8 = 0x00000020;
        constexpr uint32_t UART_IFLS_TX7_8 = 0x00000004;

        constexpr uint32_t UART_IM_DMATXIM = 0x00020000;
        constexpr uint32_t UART_IM_DMARXIM = 0x00010000;
        constexpr uint32_t UART_IM_OEIM    = 0x00000400;
        constexpr uint32_t UART_IM_RTIM    = 0x00000040;

        constexpr uint32_t UART_ICR_DMATXIC = 0x00020000;
        constexpr uint32_t UART_ICR_DMARXIC = 0x00010000;
        constexpr uint32_t UART_ICR_9BITIC  = 0x00001000;
        constexpr uint32_t UART_ICR_EOTIC   = 0x00000800;
        constexpr uint32_t UART_ICR_OEIC    = 0x00000400;
        constexpr uint32_t UART_ICR_BEIC    = 0x00000200;
        constexpr uint32_t UART_ICR_PEIC    = 0x00000100;
        constexpr uint32_t UART_ICR_FEIC    = 0x00000080;
        constexpr uint32_t UART_ICR_RTIC    = 0x00000040;
        constexpr uint32_t UART_ICR_TXIC    = 0x00000020;
        constexpr uint32_t UART_ICR_RXIC    = 0x00000010;
        constexpr uint32_t UART_ICR_DSRMIC  = 0x00000008;
        constexpr uint32_t UART_ICR_DCDMIC  = 0x00000004;
        constexpr uint32_t UART_ICR_CTSMIC  = 0x00000002;
        constexpr uint32_t UART_ICR_RIMIC   = 0x00000001;

        constexpr uint32_t UART_DMACTL_TXDMAE = 0x00000002;
        constexpr uint32_t UART_DMACTL_RXDMAE = 0x00000001;

        constexpr uint32_t UART_CC_CS_SYSCLK = 0x00000000;
        // NOLINTEND

        constexpr std::array<uint32_t, 12> baudRateTiva{ {
            600,
            1200,
            2400,
            4800,
            9600,
            19200,
            38600,
            56700,
            115200,
            230400,
            460800,
            921000,
        } };

        constexpr std::array<uint32_t, 3> parityTiva{ { 0x0, 0x6, 0x2 } };

        constexpr std::array<uint32_t, 2> stopBitsTiva{ { 0x0, 0x8 } };

        constexpr std::array<uint32_t, 8> peripheralUartArray{ {
            UART0_BASE,
            UART1_BASE,
            UART2_BASE,
            UART3_BASE,
            UART4_BASE,
            UART5_BASE,
            UART6_BASE,
            UART7_BASE,
        } };

        constexpr std::array<IRQn_Type, 8> peripheralIrqUartArray{ {
            UART0_IRQn,
            UART1_IRQn,
            UART2_IRQn,
            UART3_IRQn,
            UART4_IRQn,
            UART5_IRQn,
            UART6_IRQn,
            UART7_IRQn,
        } };

        const infra::MemoryRange<UART0_Type* const> peripheralUart =
            infra::ReinterpretCastMemoryRange<UART0_Type* const>(infra::MakeRange(peripheralUartArray));

        extern "C" uint32_t SystemCoreClock;
    }

    UartBase::UartBase(uint8_t aUartIndex, GpioPin& uartTxPin, GpioPin& uartRxPin, const Config& config)
        : uartIndex(aUartIndex)
        , uartTx(uartTxPin, PinConfigPeripheral::uartTx)
        , uartRx(uartRxPin, PinConfigPeripheral::uartRx)
    {
        uartArray = peripheralUart;
        irqArray = infra::MakeRange(peripheralIrqUartArray);
        EnableClock();
        Initialization(config);
        RegisterInterrupt(config);
    }

    UartBase::UartBase(uint8_t aUartIndex, GpioPin& uartTxPin, GpioPin& uartRxPin, GpioPin& uartRtsPin, GpioPin& uartCtsPin, const Config& config)
        : uartIndex(aUartIndex)
        , uartTx(uartTxPin, PinConfigPeripheral::uartTx)
        , uartRx(uartRxPin, PinConfigPeripheral::uartRx)
        , uartRts(std::in_place, uartRtsPin, PinConfigPeripheral::uartRts)
        , uartCts(std::in_place, uartCtsPin, PinConfigPeripheral::uartCts)
    {
        uartArray = peripheralUart;
        irqArray = infra::MakeRange(peripheralIrqUartArray);
        EnableClock();
        Initialization(config);
        RegisterInterrupt(config);
    }

    UartBase::~UartBase()
    {
        Unregister();
        uartArray[uartIndex]->IM = 0;
        DisableUart();
        DisableClock();
    }

    void UartBase::Initialization(const Config& config)
    {
        bool isHse = baudRateTiva.at(static_cast<uint8_t>(config.baudrate)) * 16 > SystemCoreClock;
        uint32_t baudrate = isHse
            ? baudRateTiva.at(static_cast<uint8_t>(config.baudrate)) / 2
            : baudRateTiva.at(static_cast<uint8_t>(config.baudrate));
        uint32_t div = (((SystemCoreClock * 8) / baudrate) + 1) / 2;
        uint32_t lcrh = parityTiva.at(static_cast<uint8_t>(config.parity));
        lcrh |= stopBitsTiva.at(static_cast<uint8_t>(config.stopbits));
        lcrh |= UART_LCRH_WLEN_8;

        if (config.enableRx)
            enableRx = UART_CTL_RXE;
        if (config.enableTx)
            enableTx = UART_CTL_TXE;

        DisableUart();
        uartArray[uartIndex]->CC = UART_CC_CS_SYSCLK;
        uartArray[uartIndex]->CTL = (uartArray[uartIndex]->CTL & ~UART_CTL_HSE) | (isHse ? UART_CTL_HSE : 0);
        uartArray[uartIndex]->CTL |= config.enableTx ? UART_CTL_EOT : 0;
        uartArray[uartIndex]->IBRD = div / 64;
        uartArray[uartIndex]->FBRD = div % 64;
        uartArray[uartIndex]->LCRH = lcrh;
        uartArray[uartIndex]->FR = 0;
        uartArray[uartIndex]->IFLS = UART_IFLS_RX7_8 | UART_IFLS_TX7_8;
        uartArray[uartIndex]->IM |= UART_IM_OEIM;
        EnableUart();

        if (config.hwFlowControl == FlowControl::rts || config.hwFlowControl == FlowControl::rtsAndCts)
            uartArray[uartIndex]->CTL |= UART_CTL_RTSEN;
        if (config.hwFlowControl == FlowControl::cts || config.hwFlowControl == FlowControl::rtsAndCts)
            uartArray[uartIndex]->CTL |= UART_CTL_CTSEN;
    }

    void UartBase::RegisterInterrupt(const Config& config)
    {
        if (config.priority)
            Register(irqArray[uartIndex], *config.priority);
        else
            Register(irqArray[uartIndex]);
    }

    void UartBase::TransferComplete()
    {
        sending = false;
        infra::EventDispatcher::Instance().Schedule(transferDataComplete);
        transferDataComplete = nullptr;
    }

    void UartBase::DisableUart() const
    {
        while (uartArray[uartIndex]->FR & UART_FR_BUSY)
        {}
        uartArray[uartIndex]->LCRH &= ~UART_LCRH_FEN;
        uartArray[uartIndex]->CTL &= ~(UART_CTL_UARTEN | enableTx | enableRx);
    }

    void UartBase::EnableUart() const
    {
        uartArray[uartIndex]->LCRH |= UART_LCRH_FEN;
        uartArray[uartIndex]->CTL |= UART_CTL_UARTEN | enableTx | enableRx;
    }

    void UartBase::SetFifo(Fifo fifoRx, Fifo fifoTx) const
    {
        uartArray[uartIndex]->IFLS = (static_cast<uint32_t>(fifoRx) << 3) | static_cast<uint32_t>(fifoTx);
    }

    uint32_t UartBase::InterruptStatus() const
    {
        return uartArray[uartIndex]->RIS;
    }

    void UartBase::InterruptClear(uint32_t mask) const
    {
        constexpr uint32_t writableMask =
            UART_ICR_DMATXIC | UART_ICR_DMARXIC |
            UART_ICR_9BITIC  | UART_ICR_EOTIC   |
            UART_ICR_OEIC    | UART_ICR_BEIC     | UART_ICR_PEIC   | UART_ICR_FEIC |
            UART_ICR_RTIC    | UART_ICR_TXIC     | UART_ICR_RXIC   |
            UART_ICR_DSRMIC  | UART_ICR_DCDMIC   | UART_ICR_CTSMIC | UART_ICR_RIMIC;

        uartArray[uartIndex]->ICR = mask & writableMask;
    }

    void UartBase::EnableRxDma() const
    {
        uartArray[uartIndex]->IM |= UART_IM_DMARXIM | UART_IM_RTIM;
        uartArray[uartIndex]->DMACTL |= UART_DMACTL_RXDMAE;
    }

    void UartBase::EnableTxDma() const
    {
        uartArray[uartIndex]->IM |= UART_IM_DMATXIM;
        uartArray[uartIndex]->DMACTL |= UART_DMACTL_TXDMAE;
    }

    void UartBase::DisableRxDma() const
    {
        uartArray[uartIndex]->IM &= ~(UART_IM_DMARXIM | UART_IM_RTIM);
        uartArray[uartIndex]->DMACTL &= ~UART_DMACTL_RXDMAE;
    }

    void UartBase::DisableTxDma() const
    {
        uartArray[uartIndex]->IM &= ~UART_IM_DMATXIM;
        uartArray[uartIndex]->DMACTL &= ~UART_DMACTL_TXDMAE;
    }

    void UartBase::EnableClock() const
    {
        infra::ReplaceBit(SYSCTL->RCGCUART, true, uartIndex);

        while (!infra::IsBitSet(SYSCTL->PRUART, uartIndex))
        {}
    }

    void UartBase::DisableClock() const
    {
        infra::ReplaceBit(SYSCTL->RCGCUART, false, uartIndex);
    }
}
