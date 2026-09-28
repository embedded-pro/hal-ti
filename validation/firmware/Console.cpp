#include "validation/firmware/Console.hpp"
#include "BoardProfile.hpp"

namespace validation
{
    namespace
    {
        using Uart = hal::tiva::UartWithDma;

        const Uart::Config terminalConfig{ true, true, Uart::Baudrate::_921600_bps, Uart::FlowControl::none, Uart::Parity::none, Uart::StopBits::one, Uart::NumberOfBytes::_8_bytes, std::make_optional(hal::cortex::InterruptPriority::low) };
    }

    ValidationTerminal::ValidationTerminal(infra::MemoryRange<uint8_t> bufferQueue, History& history, hal::SerialCommunication& communication, services::Tracer& tracer, Response& response)
        : services::TerminalWithCommandsImpl(bufferQueue, history, communication, tracer)
        , response(response)
    {}

    void ValidationTerminal::OnData(infra::BoundedConstString data)
    {
        response.BeginCommand();

        bool processed = NotifyObservers([data](services::TerminalCommands& observer)
            {
                return observer.ProcessCommand(data);
            });

        if (!processed)
            response.Error(Status::usage);

        response.EndCommand();
    }

    Console::Console()
        : tx(board::terminal.tx.port, board::terminal.tx.index)
        , rx(board::terminal.rx.port, board::terminal.rx.index)
        , uart(board::terminal.index, tx, rx, dma, terminalConfig)
    {}
}
