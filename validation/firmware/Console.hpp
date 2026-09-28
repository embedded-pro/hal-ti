#ifndef VALIDATION_CONSOLE_HPP
#define VALIDATION_CONSOLE_HPP

#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/UartWithDma.hpp"
#include "infra/stream/OutputStream.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Response.hpp"

namespace validation
{
    class ValidationTerminal
        : public services::TerminalWithCommandsImpl
    {
    public:
        using History = infra::BoundedDeque<infra::BoundedString::WithStorage<services::TerminalWithCommandsImpl::MaxBuffer>>;

        template<std::size_t MaxQueueSize, std::size_t MaxHistory>
        using WithMaxQueueAndMaxHistory = infra::WithStorage<infra::WithStorage<ValidationTerminal, std::array<uint8_t, MaxQueueSize + 1>>, typename History::template WithMaxSize<MaxHistory>>;

        ValidationTerminal(infra::MemoryRange<uint8_t> bufferQueue, History& history, hal::SerialCommunication& communication, services::Tracer& tracer, Response& response);

    private:
        void OnData(infra::BoundedConstString data) override;

    private:
        Response& response;
    };

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
        Response response{ tracer };
        ValidationTerminal::WithMaxQueueAndMaxHistory<queueSize, 1> terminal{ uart, tracer, response };
    };
}

#endif
