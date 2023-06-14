#ifndef HAL_TI_DISCOVERY_TRACER_INFRASTRUCTURE_HPP
#define HAL_TI_DISCOVERY_TRACER_INFRASTRUCTURE_HPP

#include "hal_tiva/instantiations/StmTracerInfrastructure.hpp"

namespace main_
{
    struct Discovery32f746TracerInfrastructure
    {
        hal::GpioPinStm traceUartTx{ hal::Port::A, 9 };
        hal::GpioPinStm traceUartRx{ hal::Port::B, 7 };

        main_::StmTracerInfrastructure tracerInfrastructure{ { 1, traceUartTx, traceUartRx } };
        services::Tracer& tracer{ tracerInfrastructure.tracer };
    };
}

#endif
