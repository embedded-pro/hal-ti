#ifndef HAL_TIVA_SPI_CLOCK_DIVISOR_HPP
#define HAL_TIVA_SPI_CLOCK_DIVISOR_HPP

#include "infra/util/ReallyAssert.hpp"
#include <cstdint>

namespace hal::tiva
{
    struct SpiClockDivisors
    {
        uint32_t cpsdvsr;
        uint32_t scr;
    };

    // SSI bit rate = systemClock / (CPSDVSR * (1 + SCR)) with CPSDVSR even in [2, 254] and SCR in [0, 255];
    // returns the fastest achievable rate that does not exceed the requested one.
    inline SpiClockDivisors CalculateSpiClockDivisors(uint32_t systemClock, uint32_t baudRate)
    {
        really_assert(baudRate > 0 && baudRate <= systemClock / 2);

        SpiClockDivisors best = { 0, 0 };
        uint32_t bestActual = 0;

        for (uint32_t cpsdvsr = 2; cpsdvsr <= 254; cpsdvsr += 2)
        {
            const uint64_t product = static_cast<uint64_t>(cpsdvsr) * baudRate;
            const uint32_t scrPlusOne = static_cast<uint32_t>((static_cast<uint64_t>(systemClock) + product - 1) / product);
            if (scrPlusOne - 1 > 255)
                continue;

            const uint32_t actual = static_cast<uint32_t>(static_cast<uint64_t>(systemClock) / (static_cast<uint64_t>(cpsdvsr) * scrPlusOne));
            if (actual > bestActual)
            {
                bestActual = actual;
                best = { cpsdvsr, scrPlusOne - 1 };
            }
        }

        really_assert(best.cpsdvsr != 0);
        return best;
    }
}

#endif
