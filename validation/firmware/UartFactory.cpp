#include "validation/firmware/UartFactory.hpp"
#include "BoardProfile.hpp"
#include "validation/firmware/TivaPinFactory.hpp"

namespace validation
{
    namespace
    {
        using Base = hal::tiva::UartBase;
        using Config = Base::Config;
        using services::HilChoice;
        using services::HilStatus;

        constexpr std::array<const char*, 10> openKeys{ { "tx", "rx", "rts", "cts", "baud", "parity", "stop", "flow", "dma", "sync" } };

        constexpr std::array<HilChoice<Base::Baudrate>, 12> baudRates{ {
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

        constexpr std::array<HilChoice<Base::Parity>, 3> parities{ {
            { "none", Base::Parity::none },
            { "even", Base::Parity::even },
            { "odd", Base::Parity::odd },
        } };

        constexpr std::array<HilChoice<Base::StopBits>, 2> stopBits{ {
            { "1", Base::StopBits::one },
            { "2", Base::StopBits::two },
        } };

        constexpr std::array<HilChoice<Base::FlowControl>, 4> flowControls{ {
            { "none", Base::FlowControl::none },
            { "rts", Base::FlowControl::rts },
            { "cts", Base::FlowControl::cts },
            { "rtscts", Base::FlowControl::rtsAndCts },
        } };

        constexpr std::array<uint32_t, 12> baudRateValues{ { 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 } };

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

    TivaUartFactory::TivaUartFactory(const services::HilPinNaming& naming, hal::tiva::Dma& dma)
        : naming(naming)
        , dma(dma)
    {}

    uint8_t TivaUartFactory::Instances() const
    {
        return board::uarts;
    }

    infra::MemoryRange<const char* const> TivaUartFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    HilStatus TivaUartFactory::Prepare(uint8_t index, const services::HilArguments& arguments)
    {
        Request request;
        HilStatus status = Parse(arguments, request);
        if (status != HilStatus::done)
            return status;

        if (index == board::terminal.index)
            return HilStatus::busy;

        return HilStatus::done;
    }

    HilStatus TivaUartFactory::Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, hal::TimeKeeper& timeKeeper, services::HilUartHandle& handle)
    {
        Request request;
        Parse(arguments, request);
        HilStatus status = Validate(index, request);
        if (status != HilStatus::done)
            return status;

        hal::GpioPin* tx = nullptr;
        hal::GpioPin* rx = nullptr;
        hal::GpioPin* rts = nullptr;
        hal::GpioPin* cts = nullptr;
        status = pins.ClaimFunction(request.tx, Function(hal::tiva::PinConfigPeripheral::uartTx), index, tx);
        if (status == HilStatus::done)
            status = pins.ClaimFunction(request.rx, Function(hal::tiva::PinConfigPeripheral::uartRx), index, rx);
        if (status == HilStatus::done)
            status = pins.ClaimFunction(request.rts, Function(hal::tiva::PinConfigPeripheral::uartRts), index, rts);
        if (status == HilStatus::done)
            status = pins.ClaimFunction(request.cts, Function(hal::tiva::PinConfigPeripheral::uartCts), index, cts);
        if (status != HilStatus::done)
            return status;

        Construct(index, request, tx, rx, rts, cts, timeKeeper, handle);
        return HilStatus::done;
    }

    void TivaUartFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        driver.emplace<std::monostate>();
        onClosed();
    }

    HilStatus TivaUartFactory::Parse(const services::HilArguments& arguments, Request& request) const
    {
        HilStatus status = HilStatus::done;
        arguments.Pin("tx", naming, request.tx, status);
        arguments.Pin("rx", naming, request.rx, status);
        arguments.Pin("rts", naming, request.rts, status);
        arguments.Pin("cts", naming, request.cts, status);
        arguments.Select("baud", request.baud, baudRates, status);
        arguments.Select("parity", request.parity, parities, status);
        arguments.Select("stop", request.stop, stopBits, status);
        arguments.Select("flow", request.flow, flowControls, status);
        arguments.Flag("dma", request.dma, status);
        arguments.Flag("sync", request.synchronous, status);
        return status;
    }

    HilStatus TivaUartFactory::Validate(uint8_t index, Request& request) const
    {
        if (!request.tx && !request.rx)
        {
            if (!board::defaultUart || board::defaultUart->index != index)
                return HilStatus::usage;

            request.tx = board::defaultUart->tx;
            request.rx = board::defaultUart->rx;
        }

        if (!request.tx || !request.rx || (UsesRts(request.flow) && !request.rts) || (UsesCts(request.flow) && !request.cts))
            return HilStatus::usage;

        if (request.dma && request.synchronous)
            return HilStatus::usage;

        if (request.synchronous && (request.parity != Base::Parity::none || request.stop != Base::StopBits::one))
            return HilStatus::unsupported;

        return HilStatus::done;
    }

    void TivaUartFactory::Construct(uint8_t index, const Request& request, hal::GpioPin* tx, hal::GpioPin* rx, hal::GpioPin* rts, hal::GpioPin* cts, hal::TimeKeeper& timeKeeper, services::HilUartHandle& handle)
    {
        const bool handshake = rts != nullptr || cts != nullptr;
        const Config config{ true, true, request.baud, request.flow, request.parity, request.stop, Base::NumberOfBytes::_8_bytes, std::nullopt };
        handle.baudRate = baudRateValues[static_cast<std::size_t>(request.baud)];

        if (request.synchronous)
        {
            if (handshake)
                handle.synchronous = &driver.emplace<SynchronousUart>(index, PinOrDummy(tx), PinOrDummy(rx), PinOrDummy(rts), PinOrDummy(cts), timeKeeper, hal::tiva::SynchronousUart::HwFlowControl{ UsesRts(request.flow), UsesCts(request.flow) }, handle.baudRate);
            else
                handle.synchronous = &driver.emplace<SynchronousUart>(index, PinOrDummy(tx), PinOrDummy(rx), timeKeeper, handle.baudRate);
        }
        else if (request.dma)
        {
            if (handshake)
                handle.serial = &driver.emplace<UartWithDma>(index, PinOrDummy(tx), PinOrDummy(rx), PinOrDummy(rts), PinOrDummy(cts), dma, config);
            else
                handle.serial = &driver.emplace<UartWithDma>(index, PinOrDummy(tx), PinOrDummy(rx), dma, config);
        }
        else
        {
            if (handshake)
                handle.serial = &driver.emplace<InterruptUart>(index, PinOrDummy(tx), PinOrDummy(rx), PinOrDummy(rts), PinOrDummy(cts), config);
            else
                handle.serial = &driver.emplace<InterruptUart>(index, PinOrDummy(tx), PinOrDummy(rx), config);
        }
    }
}
