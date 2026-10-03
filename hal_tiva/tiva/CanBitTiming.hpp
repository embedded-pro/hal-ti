#ifndef HAL_TIVA_CAN_BIT_TIMING_HPP
#define HAL_TIVA_CAN_BIT_TIMING_HPP

#include "infra/util/ReallyAssert.hpp"
#include <cstdint>

namespace hal::tiva
{
    struct CanBitTiming
    {
        uint8_t phaseSegment1;
        uint8_t phaseSegment2;
        uint8_t synchronizationJumpWidth;
        uint16_t baudratePrescaler;
    };

    namespace canBitTiming
    {
        constexpr uint32_t minTimeQuanta = 8;
        constexpr uint32_t maxTimeQuanta = 25;
        constexpr uint32_t maxTseg1 = 16;
        constexpr uint32_t maxTseg2 = 8;
        constexpr uint32_t maxSjw = 4;
        constexpr uint32_t maxPrescaler = 1024;
        constexpr uint32_t targetSamplePointPermille = 875;
    }

    // Picks the time-quanta count in the C_CAN range whose sample point is closest to 87.5 % (CiA-301);
    // higher tq counts win on a tie because they give finer SJW resolution.
    inline CanBitTiming CalculateCanBitTiming(uint32_t sysclk, uint32_t bitRate)
    {
        using namespace canBitTiming;

        really_assert(bitRate > 0);
        really_assert(sysclk % bitRate == 0);

        uint32_t bitClocks = sysclk / bitRate;

        CanBitTiming best{};
        uint32_t bestDistance = UINT32_MAX;

        for (uint32_t tq = maxTimeQuanta; tq >= minTimeQuanta; --tq)
        {
            if (bitClocks % tq != 0)
                continue;

            uint32_t prescaler = bitClocks / tq;
            if (prescaler < 1 || prescaler > maxPrescaler)
                continue;

            uint32_t quantaBeforeSample = (targetSamplePointPermille * tq + 500u) / 1000u;
            if (quantaBeforeSample < 3)
                continue;
            uint32_t phaseSeg1 = quantaBeforeSample - 1u;
            if (phaseSeg1 > maxTseg1)
                continue;
            uint32_t phaseSeg2 = tq - 1u - phaseSeg1;
            if (phaseSeg2 < 1 || phaseSeg2 > maxTseg2)
                continue;

            uint32_t samplePointPermille = quantaBeforeSample * 1000u / tq;
            uint32_t distance = samplePointPermille > targetSamplePointPermille
                                    ? samplePointPermille - targetSamplePointPermille
                                    : targetSamplePointPermille - samplePointPermille;

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best.baudratePrescaler = static_cast<uint16_t>(prescaler);
                best.phaseSegment1 = static_cast<uint8_t>(phaseSeg1);
                best.phaseSegment2 = static_cast<uint8_t>(phaseSeg2);
                best.synchronizationJumpWidth = static_cast<uint8_t>(phaseSeg2 < maxSjw ? phaseSeg2 : maxSjw);
            }
        }

        really_assert(bestDistance != UINT32_MAX);
        return best;
    }
}

#endif
