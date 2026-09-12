#include <openece/core/sampled_signal.hpp>
#include <openece/signals/sine.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
using openece::SampledSignal;
using openece::signals::generate_sine;
using openece::signals::SineParameters;
constexpr double pi = std::numbers::pi;
constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double inf = std::numeric_limits<double>::infinity();

TEST(SampledSignal, OwnsSamplesAndUsesHalfOpenTimeInterval) {
    std::vector<double> values{1.0, 2.0, 3.0};
    const SampledSignal signal(values, 4.0);
    values[0] = 9.0;
    EXPECT_EQ(signal.samples()[0], 1.0);
    EXPECT_EQ(signal.size(), 3U);
    EXPECT_DOUBLE_EQ(signal.duration_seconds(), 0.75);
    EXPECT_DOUBLE_EQ(signal.time_seconds(0), 0.0);
    EXPECT_DOUBLE_EQ(signal.time_seconds(2), 0.5);
    EXPECT_THROW((void)signal.time_seconds(3), std::out_of_range);
}

TEST(SampledSignal, RejectsInvalidRecords) {
    EXPECT_THROW((SampledSignal({}, 1.0)), std::invalid_argument);
    for (double rate : {0.0, -1.0, nan, inf, std::numeric_limits<double>::denorm_min()}) {
        EXPECT_THROW((SampledSignal({1.0}, rate)), std::invalid_argument);
    }
    for (double value : {nan, inf, -inf}) {
        EXPECT_THROW((SampledSignal({value}, 1.0)), std::invalid_argument);
    }
    EXPECT_THROW((SampledSignal(std::vector<double>(openece::max_signal_samples + 1), 1.0)),
                 std::invalid_argument);
}

TEST(Sine, MatchesKnownQuarterCycleSamplesAndPhase) {
    const auto signal = generate_sine({2.0, 1.0, 0.0, 4.0, 1.0});
    const std::vector<double> expected{0.0, 2.0, 0.0, -2.0};
    ASSERT_EQ(signal.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(signal.samples()[i], expected[i], 1e-14);
    }
    const auto shifted = generate_sine({2.0, 1.0, pi / 2.0, 4.0, 1.0});
    EXPECT_NEAR(shifted.samples()[0], 2.0, 1e-14);
    EXPECT_NEAR(shifted.samples()[2], -2.0, 1e-14);
}

TEST(Sine, RoundsDurationToNearestSampleHalfUp) {
    EXPECT_EQ(generate_sine({1.0, 0.0, 0.0, 10.0, 0.24}).size(), 2U);
    EXPECT_EQ(generate_sine({1.0, 0.0, 0.0, 10.0, 0.25}).size(), 3U);
    EXPECT_DOUBLE_EQ(generate_sine({1.0, 0.0, 0.0, 10.0, 0.25}).duration_seconds(), 0.3);
}

TEST(Sine, HandlesZeroAmplitudeDcAndNyquist) {
    const auto silence = generate_sine({0.0, 1.0, 0.0, 4.0, 1.0});
    for (double sample : silence.samples()) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }
    const auto dc = generate_sine({2.0, 0.0, pi / 2.0, 4.0, 1.0});
    const auto nyquist = generate_sine({2.0, 2.0, pi / 2.0, 4.0, 1.0});
    const auto invisible = generate_sine({2.0, 2.0, 0.0, 4.0, 1.0});
    for (std::size_t i = 0; i < dc.size(); ++i) {
        EXPECT_NEAR(dc.samples()[i], 2.0, 1e-14);
        EXPECT_NEAR(nyquist.samples()[i], i % 2 == 0 ? 2.0 : -2.0, 1e-14);
        EXPECT_NEAR(invisible.samples()[i], 0.0, 1e-14);
    }
}

TEST(Sine, RejectsInvalidParametersAndOversizedRequests) {
    for (auto member :
         {&SineParameters::amplitude, &SineParameters::frequency_hz, &SineParameters::phase_radians,
          &SineParameters::sample_rate_hz, &SineParameters::duration_seconds}) {
        for (double value : {nan, inf, -inf}) {
            SineParameters p;
            p.*member = value;
            EXPECT_THROW((void)generate_sine(p), std::invalid_argument);
        }
    }
    EXPECT_THROW((void)generate_sine({-1.0, 1.0, 0.0, 4.0, 1.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, -1.0, 0.0, 4.0, 1.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 3.0, 0.0, 4.0, 1.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 0.0, 0.0, 0.0, 1.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 0.0, 0.0, -1.0, 1.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 0.0, 0.0, 4.0, 0.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 0.0, 0.0, 4.0, -1.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 0.0, 0.0, 4.0, 0.01}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 0.0, 0.0, 1e9, 1.0}), std::invalid_argument);
    EXPECT_THROW((void)generate_sine({1.0, 0.0, 0.0, 1e300, 1e300}), std::invalid_argument);
}

TEST(Sine, HandlesLargeFinitePhaseAndRateWithoutOverflow) {
    const auto signal = generate_sine({1.0, 1e299, 1e300, 1e300, 8e-300});
    ASSERT_EQ(signal.size(), 8U);
    for (double value : signal.samples()) {
        EXPECT_TRUE(std::isfinite(value));
        EXPECT_LE(std::abs(value), 1.0);
    }
}
} // namespace
