#ifndef VALIDATION_UART_FACTORY_HPP
#define VALIDATION_UART_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousUart.hpp"
#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/Uart.hpp"
#include "hal_tiva/tiva/UartWithDma.hpp"
#include "services/hil/commands/UartCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <optional>
#include <variant>

namespace validation
{
    // hal::tiva::Uart only inherits UartBase's protected constructors, so it cannot be constructed directly
    class InterruptUart
        : public hal::tiva::Uart
    {
    public:
        InterruptUart(uint8_t index, hal::tiva::GpioPin& tx, hal::tiva::GpioPin& rx, const Config& config);
        InterruptUart(uint8_t index, hal::tiva::GpioPin& tx, hal::tiva::GpioPin& rx, hal::tiva::GpioPin& rts, hal::tiva::GpioPin& cts, const Config& config);
    };

    class TivaUartFactory
        : public services::hil::UartFactory
    {
    public:
        TivaUartFactory(const services::hil::PinNaming& naming, hal::tiva::Dma& dma);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::hil::Status Prepare(uint8_t index, const services::hil::Arguments& arguments) override;
        services::hil::Status Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, hal::TimeKeeper& timeKeeper, services::hil::UartHandle& handle) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        using Base = hal::tiva::UartBase;
        using SynchronousUart = hal::tiva::SynchronousUart::WithStorage<64>;
        using UartWithDma = hal::tiva::UartWithDma::WithRxBuffer<64>;

        struct Request
        {
            std::optional<PinId> tx;
            std::optional<PinId> rx;
            std::optional<PinId> rts;
            std::optional<PinId> cts;
            Base::Baudrate baud = Base::Baudrate::_115200_bps;
            Base::Parity parity = Base::Parity::none;
            Base::StopBits stop = Base::StopBits::one;
            Base::FlowControl flow = Base::FlowControl::none;
            bool dma = false;
            bool synchronous = false;
        };

        services::hil::Status Parse(const services::hil::Arguments& arguments, Request& request) const;
        services::hil::Status Validate(uint8_t index, Request& request) const;
        void Construct(uint8_t index, const Request& request, hal::GpioPin* tx, hal::GpioPin* rx, hal::GpioPin* rts, hal::GpioPin* cts, hal::TimeKeeper& timeKeeper, services::hil::UartHandle& handle);

    private:
        const services::hil::PinNaming& naming;
        hal::tiva::Dma& dma;
        std::variant<std::monostate, InterruptUart, UartWithDma, SynchronousUart> driver;
    };
}

#endif
