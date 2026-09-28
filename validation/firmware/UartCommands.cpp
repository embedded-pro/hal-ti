#include "validation/firmware/UartCommands.hpp"
#include "BoardProfile.hpp"

namespace validation
{
    namespace
    {
        using Base = hal::tiva::UartBase;
        using Config = Base::Config;

        constexpr std::array<Choice<Base::Baudrate>, 12> baudRates{ {
            { "600", Base::Baudrate::_600_bps },
            { "1200", Base::Baudrate::_1200_bps },
            { "2400", Base::Baudrate::_2400_bps },
            { "4800", Base::Baudrate::_4800_bps },
            { "9600", Base::Baudrate::_9600_bps },
            { "19200", Base::Baudrate::_19200_bps },
            { "38400", Base::Baudrate::_38400_bps },
            { "57600", Base::Baudrate::_57600_bps },
            { "115200", Base::Baudrate::_115200_bps },
            { "230400", Base::Baudrate::_230400_bps },
            { "460800", Base::Baudrate::_460800_bps },
            { "921600", Base::Baudrate::_921600_bps },
        } };

        constexpr std::array<Choice<Base::Parity>, 3> parities{ {
            { "none", Base::Parity::none },
            { "even", Base::Parity::even },
            { "odd", Base::Parity::odd },
        } };

        constexpr std::array<Choice<Base::StopBits>, 2> stopBits{ {
            { "1", Base::StopBits::one },
            { "2", Base::StopBits::two },
        } };

        constexpr std::array<Choice<Base::FlowControl>, 4> flowControls{ {
            { "none", Base::FlowControl::none },
            { "rts", Base::FlowControl::rts },
            { "cts", Base::FlowControl::cts },
            { "rtscts", Base::FlowControl::rtsAndCts },
        } };

        constexpr std::array<uint32_t, 12> baudRateValues{ { 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 } };

        constexpr uint32_t maximumTimeoutMs = 10000;
        constexpr uint32_t sendMarginMs = 1000;
        constexpr uint32_t bitsPerFrame = 12;

        bool UsesRts(Base::FlowControl flow)
        {
            return flow == Base::FlowControl::rts || flow == Base::FlowControl::rtsAndCts;
        }

        bool UsesCts(Base::FlowControl flow)
        {
            return flow == Base::FlowControl::cts || flow == Base::FlowControl::rtsAndCts;
        }
    }

    InterruptUart::InterruptUart(uint8_t index, hal::tiva::GpioPin& tx, hal::tiva::GpioPin& rx, const Config& config)
        : hal::tiva::Uart(index, tx, rx, config)
    {}

    InterruptUart::InterruptUart(uint8_t index, hal::tiva::GpioPin& tx, hal::tiva::GpioPin& rx, hal::tiva::GpioPin& rts, hal::tiva::GpioPin& cts, const Config& config)
        : hal::tiva::Uart(index, tx, rx, rts, cts, config)
    {}

    void DeadlineTimeKeeper::Arm(infra::Duration duration)
    {
        deadline = infra::Now() + duration;
    }

    bool DeadlineTimeKeeper::Timeout()
    {
        return infra::Now() >= deadline;
    }

    void DeadlineTimeKeeper::Reset()
    {}

    UartCommands::UartCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , received([this]()
              {
                  CheckReceive();
              })
        , commands{ {
              Bind<UartCommands, &UartCommands::Open>("uart.open", "<index> [tx=] [rx=] [rts=] [cts=] [baud=] [parity=] [stop=] [flow=] [dma=] [sync=]", *this, context.response),
              Bind<UartCommands, &UartCommands::Send>("uart.send", "<index> <hex>", *this, context.response),
              Bind<UartCommands, &UartCommands::Receive>("uart.recv", "<index> [timeout=] [len=]", *this, context.response),
              Bind<UartCommands, &UartCommands::Close>("uart.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> UartCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status UartCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "tx", "rx", "rts", "cts", "baud", "parity", "stop", "flow", "dma", "sync" }))
            return Status::usage;

        uint32_t requested = 0;
        std::optional<PinId> tx;
        std::optional<PinId> rx;
        std::optional<PinId> rts;
        std::optional<PinId> cts;
        auto baud = Base::Baudrate::_115200_bps;
        auto parity = Base::Parity::none;
        auto stop = Base::StopBits::one;
        auto flow = Base::FlowControl::none;
        bool dma = false;
        bool synchronous = false;

        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::uarts - 1, status);
        arguments.Pin("tx", tx, status);
        arguments.Pin("rx", rx, status);
        arguments.Pin("rts", rts, status);
        arguments.Pin("cts", cts, status);
        arguments.Select("baud", baud, baudRates, status);
        arguments.Select("parity", parity, parities, status);
        arguments.Select("stop", stop, stopBits, status);
        arguments.Select("flow", flow, flowControls, status);
        arguments.Flag("dma", dma, status);
        arguments.Flag("sync", synchronous, status);
        if (status != Status::done)
            return status;

        const auto uart = static_cast<uint8_t>(requested);

        if (uart == board::terminal.index || index)
            return Status::busy;

        if (!tx && !rx)
        {
            if (!board::defaultUart || board::defaultUart->index != uart)
                return Status::usage;

            tx = board::defaultUart->tx;
            rx = board::defaultUart->rx;
        }

        if (!tx || !rx || (UsesRts(flow) && !rts) || (UsesCts(flow) && !cts))
            return Status::usage;

        if (dma && synchronous)
            return Status::usage;

        if (synchronous && (parity != Base::Parity::none || stop != Base::StopBits::one))
            return Status::unsupported;

        hal::tiva::GpioPin* txPin = nullptr;
        hal::tiva::GpioPin* rxPin = nullptr;
        hal::tiva::GpioPin* rtsPin = nullptr;
        hal::tiva::GpioPin* ctsPin = nullptr;
        status = context.pins.ClaimFunction(tx, owner::uart, hal::tiva::PinConfigPeripheral::uartTx, uart, txPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(rx, owner::uart, hal::tiva::PinConfigPeripheral::uartRx, uart, rxPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(rts, owner::uart, hal::tiva::PinConfigPeripheral::uartRts, uart, rtsPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(cts, owner::uart, hal::tiva::PinConfigPeripheral::uartCts, uart, ctsPin);

        if (status != Status::done)
        {
            context.pins.Release(owner::uart);
            return status;
        }

        while (!received.Empty())
            received.Get();

        index = uart;
        baudRate = baudRateValues[static_cast<std::size_t>(baud)];
        const bool handshake = rts || cts;
        const Config config{ true, true, baud, flow, parity, stop, Base::NumberOfBytes::_8_bytes, std::nullopt };
        auto onReceive = [this](infra::ConstByteRange data)
        {
            Received(data);
        };

        if (synchronous)
        {
            if (handshake)
                driver.emplace<SynchronousUart>(uart, *txPin, *rxPin, PinOrDummy(rtsPin), PinOrDummy(ctsPin), timeKeeper, hal::tiva::SynchronousUart::HwFlowControl{ UsesRts(flow), UsesCts(flow) }, baudRate);
            else
                driver.emplace<SynchronousUart>(uart, *txPin, *rxPin, timeKeeper, baudRate);
        }
        else if (dma)
        {
            if (handshake)
                driver.emplace<UartWithDma>(uart, *txPin, *rxPin, PinOrDummy(rtsPin), PinOrDummy(ctsPin), context.dma, config);
            else
                driver.emplace<UartWithDma>(uart, *txPin, *rxPin, context.dma, config);

            std::get<UartWithDma>(driver).ReceiveData(onReceive);
        }
        else
        {
            if (handshake)
                driver.emplace<InterruptUart>(uart, *txPin, *rxPin, PinOrDummy(rtsPin), PinOrDummy(ctsPin), config);
            else
                driver.emplace<InterruptUart>(uart, *txPin, *rxPin, config);

            std::get<InterruptUart>(driver).ReceiveData(onReceive);
        }

        context.response.Ok();
        return Status::done;
    }

    Status UartCommands::Send(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        if (transmitting)
            return Status::busy;

        std::size_t size = 0;
        status = ParseHex(arguments.Positional(1), transmitBuffer, size);
        if (status != Status::done)
            return status;

        if (size == 0)
            return Status::usage;

        auto data = infra::ConstByteRange(transmitBuffer.data(), transmitBuffer.data() + size);

        if (auto synchronous = std::get_if<SynchronousUart>(&driver))
        {
            synchronous->SendData(data);
            context.response.Ok();
            return Status::done;
        }

        transmitting = true;
        awaitingSend = true;
        const auto generation = ++sendGeneration;
        const auto timeout = std::chrono::milliseconds(sendMarginMs + size * bitsPerFrame * 1000 / baudRate);
        sendTimer.Start(timeout, [this]()
            {
                SendTimeout();
            });

        auto onDone = [this, generation]()
        {
            SendDone(generation);
        };

        if (auto uart = std::get_if<InterruptUart>(&driver))
            uart->SendData(data, onDone);
        else
            std::get<UartWithDma>(driver).SendData(data, onDone);

        return Status::done;
    }

    Status UartCommands::Receive(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "timeout", "len" }))
            return Status::usage;

        uint32_t timeout = 1000;
        uint32_t length = 0;
        Status status = Find(arguments);
        arguments.Number("timeout", timeout, 0, maximumTimeoutMs, status);
        arguments.Number("len", length, 1, receiveCapacity, status);
        if (status != Status::done)
            return status;

        if (receiveWanted)
            return Status::busy;

        if (std::holds_alternative<SynchronousUart>(driver))
        {
            timeKeeper.Arm(arguments.Has("len") ? infra::Duration(std::chrono::milliseconds(timeout)) : infra::Duration());
            DrainSynchronous(length);
            FinishReceive();
            return Status::done;
        }

        if (!arguments.Has("len") || received.Size() >= length)
        {
            FinishReceive();
            return Status::done;
        }

        receiveWanted = length;
        receiveTimer.Start(std::chrono::milliseconds(timeout), [this]()
            {
                FinishReceive();
            });

        return Status::done;
    }

    Status UartCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        driver.emplace<std::monostate>();
        ++sendGeneration;
        transmitting = false;
        awaitingSend = false;
        receiveWanted = std::nullopt;
        sendTimer.Cancel();
        receiveTimer.Cancel();
        context.pins.Release(owner::uart);
        index = std::nullopt;
        context.response.Ok();
        return Status::done;
    }

    Status UartCommands::Find(const Arguments& arguments)
    {
        uint32_t requested = 0;
        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::uarts - 1, status);
        if (status != Status::done)
            return status;

        if (index != requested)
            return Status::notOpen;

        return Status::done;
    }

    void UartCommands::Received(infra::ConstByteRange data)
    {
        for (auto byte : data)
            if (!received.Full())
                received.AddFromInterrupt(byte);
    }

    void UartCommands::DrainSynchronous(std::size_t wanted)
    {
        auto& uart = std::get<SynchronousUart>(driver);
        uint8_t byte = 0;

        while (!received.Full())
        {
            if (received.Size() >= wanted)
                timeKeeper.Arm(infra::Duration());

            if (!uart.ReceiveData(infra::MakeByteRange(byte)))
                break;

            received.AddFromInterrupt(byte);
        }
    }

    void UartCommands::CheckReceive()
    {
        if (receiveWanted && received.Size() >= *receiveWanted)
        {
            receiveTimer.Cancel();
            FinishReceive();
        }
    }

    void UartCommands::FinishReceive()
    {
        receiveWanted = std::nullopt;
        std::array<uint8_t, receiveCapacity> data;
        std::size_t size = 0;

        while (!received.Empty())
            data[size++] = received.Get();

        (context.response.Ok() << " data=").Hex(infra::ConstByteRange(data.data(), data.data() + size));
    }

    void UartCommands::SendDone(uint32_t generation)
    {
        if (generation != sendGeneration)
            return;

        transmitting = false;

        if (awaitingSend)
        {
            awaitingSend = false;
            sendTimer.Cancel();
            context.response.Ok();
        }
    }

    void UartCommands::SendTimeout()
    {
        if (!awaitingSend)
            return;

        awaitingSend = false;
        context.response.Error(Status::timeout);
    }
}
