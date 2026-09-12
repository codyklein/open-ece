#include <openece/dsp/fft.hpp>
#include <openece/dsp/window.hpp>
#include <openece/signals/sine.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {
using openece::SampledSignal;
using openece::dsp::amplitude_spectrum;
using openece::dsp::Window;
using openece::dsp::window_coefficients;
constexpr double pi = std::numbers::pi;

TEST(Window, RectangularHasUnitWeights) {
    for (std::size_t size : {1U, 2U, 3U, 16U, 1024U}) {
        const auto weights = window_coefficients(Window::Rectangular, size);
        ASSERT_EQ(weights.size(), size);
        for (double weight : weights) {
            EXPECT_DOUBLE_EQ(weight, 1.0);
        }
    }
}

TEST(Window, HannUsesPeriodicRatherThanSymmetricCoefficients) {
    const auto four = window_coefficients(Window::Hann, 4);
    const std::vector<double> expected{0.0, 0.5, 1.0, 0.5};
    ASSERT_EQ(four.size(), expected.size());
    for (std::size_t i = 0; i < four.size(); ++i) {
        EXPECT_NEAR(four[i], expected[i], 1e-14);
    }
    const auto three = window_coefficients(Window::Hann, 3);
    EXPECT_DOUBLE_EQ(three[0], 0.0);
    EXPECT_NEAR(three[1], 0.75, 1e-14);
    EXPECT_NEAR(three[2], 0.75, 1e-14);
}

TEST(Window, HannHasHalfCoherentGainExceptIdentitySingleton) {
    EXPECT_EQ(window_coefficients(Window::Hann, 1), std::vector<double>{1.0});
    EXPECT_EQ(window_coefficients(Window::Hann, 2), (std::vector<double>{0.0, 1.0}));
    for (std::size_t size : {2U, 3U, 4U, 19U, 1024U}) {
        const auto weights = window_coefficients(Window::Hann, size);
        const double gain =
            std::accumulate(weights.begin(), weights.end(), 0.0) / static_cast<double>(size);
        EXPECT_NEAR(gain, 0.5, 1e-14);
        for (double weight : weights) {
            EXPECT_GE(weight, 0.0);
            EXPECT_LE(weight, 1.0);
        }
    }
}

TEST(Window, RejectsInvalidLengthAndSelection) {
    for (auto window : {Window::Rectangular, Window::Hann}) {
        EXPECT_THROW((void)window_coefficients(window, 0), std::invalid_argument);
        EXPECT_THROW((void)window_coefficients(window, openece::max_signal_samples + 1),
                     std::invalid_argument);
    }
    EXPECT_THROW((void)window_coefficients(static_cast<Window>(99), 8), std::invalid_argument);
    EXPECT_THROW((void)amplitude_spectrum(SampledSignal({1.0}, 1.0), static_cast<Window>(99)),
                 std::invalid_argument);
}

TEST(WindowedSpectrum, RectangularMatchesDefaultAndNeitherWindowModifiesInput) {
    const auto signal = openece::signals::generate_sine({1.7, 7.3, 0.4, 64.0, 0.3});
    const std::vector<double> original(signal.samples().begin(), signal.samples().end());
    const auto implicit = amplitude_spectrum(signal);
    const auto rectangular = amplitude_spectrum(signal, Window::Rectangular);
    EXPECT_EQ(implicit.amplitudes, rectangular.amplitudes);
    EXPECT_EQ(rectangular.window, Window::Rectangular);
    EXPECT_DOUBLE_EQ(rectangular.coherent_gain, 1.0);
    const auto hann = amplitude_spectrum(signal, Window::Hann);
    EXPECT_EQ(hann.window, Window::Hann);
    EXPECT_NEAR(hann.coherent_gain, 0.5, 1e-14);
    EXPECT_EQ(hann.sample_count, signal.size());
    EXPECT_DOUBLE_EQ(hann.sample_rate_hz, signal.sample_rate_hz());
    EXPECT_EQ(std::vector<double>(signal.samples().begin(), signal.samples().end()), original);
}

TEST(WindowedSpectrum, RecoversCoherentToneAmplitudeAcrossPhasesAndBins) {
    for (auto window : {Window::Rectangular, Window::Hann}) {
        for (double frequency : {1.0, 8.0, 63.0}) {
            for (double phase : {0.0, 0.3, -pi / 2.0}) {
                SCOPED_TRACE(static_cast<int>(window));
                SCOPED_TRACE(frequency);
                SCOPED_TRACE(phase);
                const auto signal =
                    openece::signals::generate_sine({2.5, frequency, phase, 128.0, 1.0});
                const auto spectrum = amplitude_spectrum(signal, window);
                EXPECT_NEAR(spectrum.amplitudes[static_cast<std::size_t>(frequency)], 2.5, 1e-12);
            }
        }
    }
}

TEST(WindowedSpectrum, CompensatesDcAndNyquistWithoutDoublingEndpoints) {
    for (auto window : {Window::Rectangular, Window::Hann}) {
        const auto dc =
            amplitude_spectrum(SampledSignal(std::vector<double>(16, -3.0), 16.0), window);
        const auto nyquist = amplitude_spectrum(
            SampledSignal({2, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -2}, 16.0), window);
        EXPECT_NEAR(dc.amplitudes[0], 3.0, 1e-13);
        EXPECT_NEAR(nyquist.amplitudes.back(), 2.0, 1e-13);
        if (window == Window::Hann) {
            // Windowing spreads endpoint tones into neighboring bins; do not suppress them.
            EXPECT_NEAR(dc.amplitudes[1], 3.0, 1e-13);
            EXPECT_NEAR(nyquist.amplitudes[7], 2.0, 1e-13);
        }
    }
}

TEST(WindowedSpectrum, HannBeforePaddingMatchesIndependentFourPointDft) {
    // L=3: analytical Hann weights are [0, 3/4, 3/4], sum=3/2.
    // Applying an N=4 window instead, or dividing by N/2, would give a different result.
    const auto spectrum = amplitude_spectrum(SampledSignal({1, 2, 3}, 12.0), Window::Hann);
    EXPECT_EQ(spectrum.sample_count, 3U);
    EXPECT_EQ(spectrum.fft_size, 4U);
    EXPECT_DOUBLE_EQ(spectrum.bin_width_hz(), 3.0);
    const std::vector<double> weighted{0, 1.5, 2.25, 0};
    for (std::size_t k = 0; k < spectrum.amplitudes.size(); ++k) {
        std::complex<double> expected{0.0, 0.0};
        for (std::size_t n = 0; n < weighted.size(); ++n) {
            const double angle = -2.0 * pi * static_cast<double>(k * n) / 4.0;
            expected += weighted[n] * std::complex<double>(std::cos(angle), std::sin(angle));
        }
        const double factor = k == 1 ? 2.0 : 1.0;
        EXPECT_NEAR(spectrum.amplitudes[k], factor * std::abs(expected) / 1.5, 1e-13);
    }
}

TEST(WindowedSpectrum, HandlesSingletonTwoSamplesAndSilence) {
    for (auto window : {Window::Rectangular, Window::Hann}) {
        const auto singleton = amplitude_spectrum(SampledSignal({-2.0}, 8.0), window);
        EXPECT_EQ(singleton.amplitudes, std::vector<double>{2.0});
        EXPECT_DOUBLE_EQ(singleton.coherent_gain, 1.0);
        const auto silence = amplitude_spectrum(SampledSignal(std::vector<double>(7), 8.0), window);
        for (double value : silence.amplitudes) {
            EXPECT_DOUBLE_EQ(value, 0.0);
        }
    }
    const auto two = amplitude_spectrum(SampledSignal({9.0, -2.0}, 8.0), Window::Hann);
    // L=2 periodic Hann discards the first sample: correction cannot restore that information.
    EXPECT_EQ(two.amplitudes, (std::vector<double>{2.0, 2.0}));
}

TEST(WindowedSpectrum, HannReducesDistantSidelobesForOffBinTone) {
    for (double phase : {0.0, 0.7}) {
        const auto signal = openece::signals::generate_sine({1.0, 32.5, phase, 256.0, 1.0});
        const auto rectangular = amplitude_spectrum(signal, Window::Rectangular);
        const auto hann = amplitude_spectrum(signal, Window::Hann);
        double rectangular_peak = 0.0, hann_peak = 0.0;
        double rectangular_energy = 0.0, hann_energy = 0.0;
        for (std::size_t k = 0; k < hann.amplitudes.size(); ++k) {
            // Exclude BOTH main lobes using the same +/-4 Hz band, in original fs/L units.
            // Do not compare all non-peak bins: Hann intentionally broadens the main lobe.
            if (std::abs(static_cast<double>(k) - 32.5) > 4.0) {
                rectangular_peak = std::max(rectangular_peak, rectangular.amplitudes[k]);
                hann_peak = std::max(hann_peak, hann.amplitudes[k]);
                rectangular_energy += std::pow(rectangular.amplitudes[k], 2);
                hann_energy += std::pow(hann.amplitudes[k], 2);
            }
        }
        ASSERT_GT(rectangular_peak, 0.01);
        EXPECT_LT(hann_peak, 0.1 * rectangular_peak);
        EXPECT_LT(hann_energy, 0.01 * rectangular_energy);
        // These are comparisons of corrected spectral magnitudes, not a PSD estimator.
    }
}
} // namespace
