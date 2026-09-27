#include "hal_tiva/instantiations/TracingReset.hpp"

namespace instantiations
{
    TracingReset::TracingReset(services::Tracer& tracer)
        : tracingReset(reset, tracer)
    {}
}
