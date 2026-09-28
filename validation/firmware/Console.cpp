#include "validation/firmware/Console.hpp"
#include "BoardProfile.hpp"

namespace validation
{
    namespace
    {
        using Uart = hal::tiva::UartWithDma;

        const Uart::Config terminalConfig{ true, true, Uart::Baudrate::_921600_bps, Uart::FlowControl::none, Uart::Parity::none, Uart::StopBits::one, Uart::NumberOfBytes::_8_bytes, std::make_optional(hal::cortex::InterruptPriority::low) };
    }

    Console::Console()
        : tx(PortOf(board::terminal.tx), board::terminal.tx.index)
        , rx(PortOf(board::terminal.rx), board::terminal.rx.index)
        , uart(board::terminal.index, tx, rx, dma, terminalConfig)
    {}
}
