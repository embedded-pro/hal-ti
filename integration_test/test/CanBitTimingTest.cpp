#include "hal_tiva/tiva/CanBitTiming.hpp"
#include "gtest/gtest.h"
#include <algorithm>

namespace
{
    struct Expected
    {
        uint32_t sysclk;
        uint32_t bitRate;
        uint16_t prescaler;
        uint8_t phaseSegment1;
        uint8_t phaseSegment2;
    };

    class CanBitTimingTest
        : public testing::TestWithParam<Expected>
    {};
}

TEST_P(CanBitTimingTest, matches_reference_timing)
{
    const auto& expected = GetParam();
    const auto timing = hal::tiva::CalculateCanBitTiming(expected.sysclk, expected.bitRate);

    EXPECT_EQ(expected.prescaler, timing.baudratePrescaler);
    EXPECT_EQ(expected.phaseSegment1, timing.phaseSegment1);
    EXPECT_EQ(expected.phaseSegment2, timing.phaseSegment2);
    EXPECT_EQ(std::min<uint8_t>(expected.phaseSegment2, 4), timing.synchronizationJumpWidth);
}

TEST_P(CanBitTimingTest, produces_the_requested_bit_rate_within_controller_limits)
{
    const auto& expected = GetParam();
    const auto timing = hal::tiva::CalculateCanBitTiming(expected.sysclk, expected.bitRate);
    const uint32_t quanta = 1u + timing.phaseSegment1 + timing.phaseSegment2;

    EXPECT_EQ(expected.sysclk, static_cast<uint32_t>(timing.baudratePrescaler) * quanta * expected.bitRate);
    EXPECT_GE(quanta, hal::tiva::canBitTiming::minTimeQuanta);
    EXPECT_LE(quanta, hal::tiva::canBitTiming::maxTimeQuanta);
    EXPECT_LE(timing.phaseSegment1, hal::tiva::canBitTiming::maxTseg1);
    EXPECT_LE(timing.phaseSegment2, hal::tiva::canBitTiming::maxTseg2);
    EXPECT_GE(timing.synchronizationJumpWidth, 1);
    EXPECT_LE(timing.synchronizationJumpWidth, hal::tiva::canBitTiming::maxSjw);
}

INSTANTIATE_TEST_SUITE_P(CommonRates, CanBitTimingTest,
    testing::Values(
        Expected{ 120000000, 100000, 75, 13, 2 },
        Expected{ 120000000, 125000, 60, 13, 2 },
        Expected{ 120000000, 250000, 30, 13, 2 },
        Expected{ 120000000, 500000, 15, 13, 2 },
        Expected{ 120000000, 1000000, 15, 6, 1 },
        Expected{ 80000000, 100000, 50, 13, 2 },
        Expected{ 80000000, 125000, 40, 13, 2 },
        Expected{ 80000000, 250000, 20, 13, 2 },
        Expected{ 80000000, 500000, 10, 13, 2 },
        Expected{ 80000000, 1000000, 5, 13, 2 }));
