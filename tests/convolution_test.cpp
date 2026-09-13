#include <openece/core/sampled_signal.hpp>
#include <openece/dsp/convolution.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using openece::dsp::convolve_full;

// Output-side definition with extended precision, independent of production's
// input-side scatter accumulation. Small records deliberately include N < M.
std::vector<double> reference(const std::vector<double>& x, const std::vector<double>& h) {
    std::vector<double> y(x.size() + h.size() - 1);
    for (std::size_t n = 0; n < y.size(); ++n) {
        long double sum = 0;
        for (std::size_t k = 0; k < h.size(); ++k) {
            if (n >= k && n - k < x.size()) {
                sum += static_cast<long double>(h[k]) * x[n - k];
            }
        }
        y[n] = static_cast<double>(sum);
    }
    return y;
}

TEST(Convolution, HandCalculatedFullOutputAndZeroExtension) {
    EXPECT_EQ(convolve_full(std::vector{1., 2., 3.}, std::vector{.5, .5}),
              (std::vector{.5, 1.5, 2.5, 1.5}));
    EXPECT_EQ(convolve_full(std::vector{1., -2.}, std::vector{3., 4., 5.}),
              (std::vector{3., -2., -3., -10.}));
    EXPECT_EQ(convolve_full(std::vector{2.}, std::vector{-3.}), std::vector{-6.});
}

TEST(Convolution, IdentityImpulseDelayAndSilence) {
    const std::vector x{1., -2., 3., 4.};
    EXPECT_EQ(convolve_full(x, std::vector{1.}), x);
    EXPECT_EQ(convolve_full(std::vector{1.}, x), x);
    EXPECT_EQ(convolve_full(x, std::vector{0., 0., 1.}), (std::vector{0., 0., 1., -2., 3., 4.}));
    EXPECT_EQ(convolve_full(x, std::vector{0., 0.}), std::vector<double>(5, 0.));
}

TEST(Convolution, MatchesIndependentReferenceAndPreservesInputs) {
    for (std::size_t n : {1U, 2U, 7U, 19U}) {
        for (std::size_t m : {1U, 3U, 8U, 23U}) {
            std::vector<double> x(n), h(m);
            for (std::size_t i = 0; i < n; ++i)
                x[i] = std::sin(0.7 * static_cast<double>(i)) - .3;
            for (std::size_t k = 0; k < m; ++k)
                h[k] = std::cos(1.3 * static_cast<double>(k)) + .2;
            const auto original_x = x, original_h = h;
            const auto expected = reference(x, h), actual = convolve_full(x, h);
            const auto reversed = convolve_full(h, x);
            ASSERT_EQ(actual.size(), expected.size());
            for (std::size_t i = 0; i < actual.size(); ++i) {
                EXPECT_NEAR(actual[i], expected[i], 1e-13);
                EXPECT_NEAR(actual[i], reversed[i], 1e-13);
            }
            EXPECT_EQ(x, original_x);
            EXPECT_EQ(h, original_h);
        }
    }
}

TEST(Convolution, Linearity) {
    const std::vector a{1., -2., 4.}, b{3., .5, -1.}, h{.2, .3, -.1};
    std::vector<double> combined(3);
    for (std::size_t i = 0; i < 3; ++i)
        combined[i] = 2 * a[i] - 3 * b[i];
    const auto ya = convolve_full(a, h), yb = convolve_full(b, h), y = convolve_full(combined, h);
    for (std::size_t i = 0; i < y.size(); ++i)
        EXPECT_NEAR(y[i], 2 * ya[i] - 3 * yb[i], 1e-14);
}

TEST(Convolution, ValidatesInputsAndDetectsOverflow) {
    const std::vector one{1.};
    EXPECT_THROW((void)convolve_full({}, one), std::invalid_argument);
    EXPECT_THROW((void)convolve_full(one, {}), std::invalid_argument);
    for (double v :
         {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_THROW((void)convolve_full(std::vector{v}, one), std::invalid_argument);
        EXPECT_THROW((void)convolve_full(one, std::vector{v}), std::invalid_argument);
    }
    const auto large = std::numeric_limits<double>::max();
    EXPECT_THROW((void)convolve_full(std::vector{large}, std::vector{2.}), std::overflow_error);
    EXPECT_THROW((void)convolve_full(std::vector{large, large}, std::vector{1., 1.}),
                 std::overflow_error);
}

TEST(Convolution, EnforcesOutputAndWorkLimitsIncludingEndpoints) {
    const std::vector<double> maximum(openece::max_signal_samples, 1.);
    EXPECT_EQ(convolve_full(maximum, std::vector{1.}), maximum);
    EXPECT_THROW((void)convolve_full(maximum, std::vector{1., 1.}), std::invalid_argument);
    EXPECT_THROW(
        (void)convolve_full(std::vector<double>(openece::max_signal_samples + 1), std::vector{1.}),
        std::invalid_argument);
    // 8000 squared is exactly the documented 64-million-product budget.
    const std::vector<double> x(8000, 1.);
    const auto y = convolve_full(x, x);
    ASSERT_EQ(y.size(), 15999U);
    for (std::size_t i = 0; i < y.size(); ++i) {
        EXPECT_DOUBLE_EQ(y[i], static_cast<double>(i < 8000 ? i + 1 : y.size() - i));
    }
    EXPECT_THROW((void)convolve_full(x, std::vector<double>(8001)), std::invalid_argument);
}
} // namespace
