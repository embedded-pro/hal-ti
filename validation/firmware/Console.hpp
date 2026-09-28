#ifndef VALIDATION_CONSOLE_HPP
#define VALIDATION_CONSOLE_HPP

#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/UartWithDma.hpp"
#include "infra/stream/OutputStream.hpp"
#include "services/hil/HilTerminal.hpp"
#include "services/hil/Response.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/Tracer.hpp"

namespace validation
{
    struct Console
    {
        static constexpr std::size_t receiveBufferSize = 128;
        static constexpr std::size_t transmitBufferSize = 1024;
        static constexpr std::size_t queueSize = 256;

        static_assert(queueSize >= receiveBufferSize / 2, "terminal queue must absorb a whole DMA half-buffer");

        Console();

        hal::tiva::GpioPin tx;
        hal::tiva::GpioPin rx;
        hal::tiva::Dma dma{ infra::emptyFunction };
        hal::tiva::UartWithDma::WithRxBuffer<receiveBufferSize> uart;
        services::StreamWriterOnSerialCommunication::WithStorage<transmitBufferSize> writer{ uart };
        infra::TextOutputStream::WithErrorPolicy stream{ writer };
        services::TracerToStream tracer{ stream };
        services::hil::Response response{ tracer };
        services::hil::HilTerminal::WithMaxQueueAndMaxHistory<queueSize, 1> terminal{ uart, tracer, response };
    };
}

#endif
