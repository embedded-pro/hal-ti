#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "hal_tiva/instantiations/LaunchPadBsp.hpp"
#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/UartWithDma.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/util/Function.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"

int main()
{
    static instantiations::LaunchPad launchPad;
    static instantiations::EventInfrastructure eventInfrastructure;

    static hal::tiva::Dma dma{ infra::emptyFunction };

    static hal::tiva::GpioPin uartTxPin{ hal::tiva::Port::A, 1 };
    static hal::tiva::GpioPin uartRxPin{ hal::tiva::Port::A, 0 };
    static hal::tiva::UartWithDma::WithRxBuffer<64> uart{ 0, uartTxPin, uartRxPin, dma };

    static services::StreamWriterOnSerialCommunication::WithStorage<256> traceWriter{ uart };
    static infra::TextOutputStream::WithErrorPolicy traceStream{ traceWriter };
    static services::TracerWithDateTime tracer{ traceStream };

    static services::DebugLed debugLed{ launchPad.DebugLed(), std::chrono::milliseconds(100), std::chrono::milliseconds(1400) };

    uart.ReceiveData([](infra::ConstByteRange) {});

    tracer.Trace() << "DMA UART ready";

    eventInfrastructure.Run();
    __builtin_unreachable();
}
