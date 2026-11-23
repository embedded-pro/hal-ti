#pragma once

#include "hal/cortex_m/Reset.hpp"
#include "services/tracer/TracingReset.hpp"

namespace instantiations
{
    struct TracingReset
    {
    private:
        hal::cortex::Reset reset;

    public:
        TracingReset(services::Tracer& tracer);

        services::TracingReset tracingReset;
    };
}
