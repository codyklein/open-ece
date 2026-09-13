#include <openece/dsp/fft.hpp>
#include <openece/dsp/fir.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {
using namespace openece::dsp;
using openece::SampledSignal;
constexpr double pi = std::numbers::pi;

TEST(Fir, OwnsCoefficientsWithoutNormalizingAndValidates) {
    std::vector taps{2., -3., 4.};
    const FirCoefficients filter(taps);
    taps[0] = 99.;
    EXPECT_EQ(std::vector(filter.taps().begin(), filter.taps().end()), (std::vector{2., -3., 4.}));
    const auto copy = filter;
    EXPECT_EQ(copy.size(), 3U);
    EXPECT_THROW((void)FirCoefficients({}), std::invalid_argument);
    EXPECT_THROW((void)FirCoefficients(std::vector<double>(openece::max_signal_samples + 1)),
                 std::invalid_argument);
    for (double v :
         {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        EXPECT_THROW((void)FirCoefficients({v}), std::invalid_argument);
    EXPECT_EQ(FirCoefficients({0.}).size(), 1U);
}

TEST(Fir, FullSignalOutputPreservesRateOriginDurationAndInput) {
    const SampledSignal input({1., 2., 3.}, 8.);
    const FirCoefficients filter({.5, .5});
    const auto output = apply_fir_full(input, filter);
    EXPECT_EQ(std::vector(output.samples().begin(), output.samples().end()),
              (std::vector{.5, 1.5, 2.5, 1.5}));
    EXPECT_EQ(std::vector(input.samples().begin(), input.samples().end()),
              (std::vector{1., 2., 3.}));
    EXPECT_DOUBLE_EQ(output.sample_rate_hz(), 8.);
    EXPECT_DOUBLE_EQ(output.time_seconds(0), 0.);
    EXPECT_DOUBLE_EQ(output.duration_seconds(), input.duration_seconds() + 1. / 8.);
    EXPECT_DOUBLE_EQ(output.time_seconds(output.size() - 1), 3. / 8.);
    const auto impulse = apply_fir_full(SampledSignal({1.}, 8.), FirCoefficients({2., -1., 3.}));
    EXPECT_EQ(std::vector(impulse.samples().begin(), impulse.samples().end()),
              (std::vector{2., -1., 3.}));
    const auto huge = std::numeric_limits<double>::max();
    EXPECT_THROW((void)apply_fir_full(SampledSignal({huge}, 1.), FirCoefficients({2.})),
                 std::overflow_error);
    // Input duration is representable, but the full filtered record's duration is not.
    EXPECT_THROW((void)apply_fir_full(SampledSignal({1.}, 1e-308), FirCoefficients({1., 1.})),
                 std::invalid_argument);
}

TEST(FirResponse, AnalyticalIdentityGainDelayAndMovingAverage) {
    const auto identity = frequency_response(FirCoefficients({2.}), 100., 9);
    EXPECT_DOUBLE_EQ(identity.bin_width_hz(), 6.25);
    for (auto value : identity.values)
        EXPECT_EQ(value, std::complex<double>(2., 0.));
    const auto delay = frequency_response(FirCoefficients({0., 0., 1.}), 100., 9);
    const auto average = frequency_response(FirCoefficients({.5, .5}), 100., 9);
    for (std::size_t i = 0; i < 9; ++i) {
        const double omega = pi * static_cast<double>(i) / 8.;
        EXPECT_NEAR(std::abs(delay.values[i] - std::polar(1., -2 * omega)), 0., 1e-14);
        EXPECT_NEAR(std::abs(average.values[i] - std::polar(std::cos(omega / 2.), -omega / 2.)), 0.,
                    1e-14);
    }
    EXPECT_EQ(average.values.front(), std::complex<double>(1., 0.));
    EXPECT_EQ(average.values.back(), std::complex<double>(0., 0.));
    for (auto value : frequency_response(FirCoefficients({0., 0.}), 100., 3).values)
        EXPECT_EQ(value, std::complex<double>(0., 0.));
}

TEST(FirResponse, MatchesIndependentComplexPolynomialAndExistingFft) {
    const FirCoefficients filter({.2, -.1, .7, 1.3, -.4});
    const auto response = frequency_response(filter, 64., 17);
    std::vector<std::complex<double>> padded(32);
    std::copy(filter.taps().begin(), filter.taps().end(), padded.begin());
    const auto transformed = fft(padded);
    for (std::size_t i = 0; i < response.values.size(); ++i) {
        const auto z =
            std::polar(1.L, -std::numbers::pi_v<long double> * static_cast<long double>(i) / 16.L);
        // Horner polynomial evaluation is independent of the production trigonometric sum.
        std::complex<long double> expected{};
        for (auto it = filter.taps().rbegin(); it != filter.taps().rend(); ++it)
            expected = expected * z + static_cast<long double>(*it);
        EXPECT_NEAR(std::abs(response.values[i] - static_cast<std::complex<double>>(expected)), 0.,
                    1e-13);
        EXPECT_NEAR(std::abs(response.values[i] - transformed[i]), 0., 1e-13);
    }
}

TEST(FirResponse, PointCountEndpointsRatesLimitsAndOverflow) {
    const FirCoefficients filter({1., 2., 3.});
    const auto endpoints = frequency_response(filter, 20., 2);
    ASSERT_EQ(endpoints.values.size(), 2U);
    EXPECT_EQ(endpoints.values[0], std::complex<double>(6., 0.));
    EXPECT_EQ(endpoints.values[1], std::complex<double>(2., 0.));
    EXPECT_DOUBLE_EQ(endpoints.bin_width_hz(), 10.);
    for (std::size_t count : {0U, 1U, static_cast<unsigned>(openece::max_signal_samples + 1)})
        EXPECT_THROW((void)frequency_response(filter, 20., count), std::invalid_argument);
    for (double rate :
         {0., -1., std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::denorm_min()})
        EXPECT_THROW((void)frequency_response(filter, rate, 3), std::invalid_argument);
    const auto maximum = frequency_response(FirCoefficients({1.}), 2., openece::max_signal_samples);
    EXPECT_EQ(maximum.values.size(), openece::max_signal_samples);
    EXPECT_EQ(maximum.values.back(), std::complex<double>(1., 0.));
    EXPECT_THROW((void)frequency_response(FirCoefficients(std::vector<double>(8000)), 2., 8001),
                 std::invalid_argument);
    const auto large = std::numeric_limits<double>::max();
    EXPECT_THROW((void)frequency_response(FirCoefficients({large, large}), 2., 2),
                 std::overflow_error);
}

TEST(Lowpass, ThreeTapAnalyticalHammingSincAndRateScaling) {
    const auto filter = design_lowpass(3, 25., 100.);
    // At fc/fs=1/4: unwindowed [1/pi, 1/2, 1/pi], Hamming [0.08,1,0.08].
    const double edge = .08 / pi, sum = .5 + 2 * edge;
    EXPECT_NEAR(filter.taps()[0], edge / sum, 1e-15);
    EXPECT_NEAR(filter.taps()[1], .5 / sum, 1e-15);
    EXPECT_DOUBLE_EQ(filter.taps()[0], filter.taps()[2]);
    const auto scaled = design_lowpass(3, 250., 1000.);
    EXPECT_EQ(std::vector(filter.taps().begin(), filter.taps().end()),
              std::vector(scaled.taps().begin(), scaled.taps().end()));
}

TEST(Lowpass, SymmetryUnityDcAndVisibleImpulseDelay) {
    for (std::size_t count : {3U, 31U, 63U, 511U}) {
        const auto filter = design_lowpass(count, 100., 1000.);
        EXPECT_NEAR(std::accumulate(filter.taps().begin(), filter.taps().end(), 0.), 1., 1e-14);
        for (std::size_t k = 0; k < count; ++k)
            EXPECT_DOUBLE_EQ(filter.taps()[k], filter.taps()[count - 1 - k]);
        const auto impulse = apply_fir_full(SampledSignal({1.}, 1000.), filter);
        const auto peak = std::max_element(impulse.samples().begin(), impulse.samples().end());
        EXPECT_EQ(static_cast<std::size_t>(peak - impulse.samples().begin()), (count - 1) / 2);
        const auto response = frequency_response(filter, 1000., 101);
        // De-rotate the known linear phase analytically; imaginary residual must vanish.
        for (std::size_t k = 0; k < response.values.size(); ++k) {
            const auto rotated =
                response.values[k] * std::polar(1., pi * static_cast<double>(k) / 100. *
                                                        static_cast<double>((count - 1) / 2));
            EXPECT_NEAR(rotated.imag(), 0., 1e-13);
        }
    }
}

TEST(Lowpass, RejectsInvalidDesignRequests) {
    for (std::size_t count :
         {0U, 1U, 2U, 4U, static_cast<unsigned>(openece::max_signal_samples + 1)})
        EXPECT_THROW((void)design_lowpass(count, 10., 100.), std::invalid_argument);
    for (double cutoff : {-1., 0., 50., 51., std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()})
        EXPECT_THROW((void)design_lowpass(31, cutoff, 100.), std::invalid_argument);
    for (double rate : {-1., 0., std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()})
        EXPECT_THROW((void)design_lowpass(31, 10., rate), std::invalid_argument);
    EXPECT_THROW((void)design_lowpass(31, std::numeric_limits<double>::denorm_min(),
                                      std::numeric_limits<double>::max()),
                 std::invalid_argument);
}

TEST(Lowpass, TwoToneSteadyStateGainDelayAndStartup) {
    // fs=1024; the 64-sample delay is a whole cycle at 16 and 256 Hz.
    const auto filter = design_lowpass(129, 80., 1024.);
    std::vector<double> samples(2176);
    for (std::size_t n = 0; n < samples.size(); ++n)
        samples[n] = std::sin(2 * pi * 16 * static_cast<double>(n) / 1024.) +
                     .5 * std::sin(2 * pi * 256 * static_cast<double>(n) / 1024.);
    const auto output = apply_fir_full(SampledSignal(samples, 1024.), filter);
    double low = 0., high = 0.;
    // Only fully immersed samples; exactly 2048 samples for orthogonal projections.
    for (std::size_t n = 128; n < 2176; ++n) {
        low += output.samples()[n] * std::sin(2 * pi * 16 * static_cast<double>(n) / 1024.);
        high += output.samples()[n] * std::sin(2 * pi * 256 * static_cast<double>(n) / 1024.);
    }
    low *= 2. / 2048.;
    high *= 2. / 2048.;
    EXPECT_NEAR(low, 1., .01);
    EXPECT_LT(std::abs(high), .001);
    EXPECT_DOUBLE_EQ(output.samples()[0], 0.);
    EXPECT_EQ(output.size(), samples.size() + 128);
    const auto response = frequency_response(filter, 1024., 513);
    EXPECT_NEAR(low, response.values[16].real(), 1e-13);
    EXPECT_NEAR(high, .5 * response.values[256].real(), 1e-13);
}

TEST(Fir, FilteredSpectrumMatchesIndependentWindowedDft) {
    const auto output = apply_fir_full(SampledSignal({1., 2., -1.}, 8.), FirCoefficients({.5, .5}));
    const std::vector expected{.5, 1.5, .5, -.5};
    for (auto window : {Window::Rectangular, Window::Hann}) {
        const auto spectrum = amplitude_spectrum(output, window);
        const std::vector weights = window == Window::Rectangular ? std::vector{1., 1., 1., 1.}
                                                                  : std::vector{0., .5, 1., .5};
        for (std::size_t k = 0; k <= 2; ++k) {
            std::complex<double> dft{};
            for (std::size_t n = 0; n < 4; ++n)
                dft += expected[n] * weights[n] *
                       std::polar(1., -2 * pi * static_cast<double>(k * n) / 4.);
            const double amplitude = std::abs(dft) /
                                     std::accumulate(weights.begin(), weights.end(), 0.) *
                                     (k == 1 ? 2. : 1.);
            EXPECT_NEAR(spectrum.amplitudes[k], amplitude, 1e-14);
        }
    }
}
} // namespace
