#ifndef VALIDATION_UART_FACTORY_HPP
#define VALIDATION_UART_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousUart.hpp"
#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/Uart.hpp"
#include "hal_tiva/tiva/UartWithDma.hpp"
#include "services/hil/commands/HilUartCommands.hpp"
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
        : public services::HilUartFactory
    {
    public:
        TivaUartFactory(const services::HilPinNaming& naming, hal::tiva::Dma& dma);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override;
        services::HilStatus Open(uint8_t index, const services::HilArguments& arguments, services::HilPinOwner& pins, hal::TimeKeeper& timeKeeper, services::HilUartHandle& handle) override;
        void Close(uint8_t index, const infra::Function<void()>& onClosed) override;

    private:
        using Base = hal::tiva::UartBase;
        using SynchronousUart = hal::tiva::SynchronousUart::WithStorage<64>;
        using UartWithDma = hal::tiva::UartWithDma::WithRxBuffer<64>;

        struct Request
        {
            std::optional<HilPinId> tx;
            std::optional<HilPinId> rx;
            std::optional<HilPinId> rts;
            std::optional<HilPinId> cts;
            Base::Baudrate baud = Base::Baudrate::_115200_bps;
            Base::Parity parity = Base::Parity::none;
            Base::StopBits stop = Base::StopBits::one;
            Base::FlowControl flow = Base::FlowControl::none;
            bool dma = false;
            bool synchronous = false;
        };

        services::HilStatus Parse(const services::HilArguments& arguments, Request& request) const;
        services::HilStatus Validate(uint8_t index, Request& request) const;
        void Construct(uint8_t index, const Request& request, hal::GpioPin* tx, hal::GpioPin* rx, hal::GpioPin* rts, hal::GpioPin* cts, hal::TimeKeeper& timeKeeper, services::HilUartHandle& handle);

    private:
        const services::HilPinNaming& naming;
        hal::tiva::Dma& dma;
        std::variant<std::monostate, InterruptUart, UartWithDma, SynchronousUart> driver;
    };
}

#endif
