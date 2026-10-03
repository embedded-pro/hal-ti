#include "hal_tiva/tiva/SpiClockDivisor.hpp"
#include "gtest/gtest.h"
#include <tuple>

namespace
{
    uint32_t ActualRate(uint32_t systemClock, const hal::tiva::SpiClockDivisors& divisors)
    {
        return systemClock / (divisors.cpsdvsr * (divisors.scr + 1));
    }

    class SpiClockDivisorTest
        : public testing::TestWithParam<std::tuple<uint32_t, uint32_t>>
    {};
}

TEST(SpiClockDivisor, exact_rate_uses_the_smallest_prescaler)
{
    const auto divisors = hal::tiva::CalculateSpiClockDivisors(80000000, 1000000);

    EXPECT_EQ(2u, divisors.cpsdvsr);
    EXPECT_EQ(39u, divisors.scr);
}

TEST(SpiClockDivisor, inexact_rate_rounds_down_to_the_next_achievable_rate)
{
    const auto divisors = hal::tiva::CalculateSpiClockDivisors(120000000, 7000000);

    EXPECT_EQ(2u, divisors.cpsdvsr);
    EXPECT_EQ(8u, divisors.scr);
    EXPECT_EQ(6666666u, ActualRate(120000000, divisors));
}

TEST(SpiClockDivisor, low_rate_needs_a_large_prescaler)
{
    const auto divisors = hal::tiva::CalculateSpiClockDivisors(120000000, 2000);

    EXPECT_EQ(2000u, ActualRate(120000000, divisors));
}

TEST_P(SpiClockDivisorTest, never_exceeds_the_requested_rate_and_respects_register_limits)
{
    const auto [systemClock, baudRate] = GetParam();
    const auto divisors = hal::tiva::CalculateSpiClockDivisors(systemClock, baudRate);

    EXPECT_EQ(0u, divisors.cpsdvsr % 2);
    EXPECT_GE(divisors.cpsdvsr, 2u);
    EXPECT_LE(divisors.cpsdvsr, 254u);
    EXPECT_LE(divisors.scr, 255u);
    EXPECT_LE(ActualRate(systemClock, divisors), baudRate);
    EXPECT_GT(ActualRate(systemClock, divisors), 0u);
}

INSTANTIATE_TEST_SUITE_P(Rates, SpiClockDivisorTest,
    testing::Combine(
        testing::Values(16000000u, 50000000u, 80000000u, 120000000u),
        testing::Values(8000000u, 4000000u, 1000000u, 400000u, 100000u, 10000u)));
