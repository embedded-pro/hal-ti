#pragma once

#include <cstdint>

namespace hal::tiva
{
    // The sequencers of one ADC share its clock gate, so it stays enabled while any sequencer driver exists
    void AcquireAdcClock(uint8_t adcIndex);
    void ReleaseAdcClock(uint8_t adcIndex);
}
