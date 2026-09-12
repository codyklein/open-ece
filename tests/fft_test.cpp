#include <openece/dsp/fft.hpp>
#include <openece/signals/sine.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using openece::SampledSignal;
using openece::dsp::amplitude_spectrum;
using openece::dsp::fft;
using Complex = std::complex<double>;
constexpr double pi = std::numbers::pi;

// Independent O(N^2) oracle: deliberately no bit reversal, butterflies, or twiddle recurrence.
std::vector<Complex> direct_dft(const std::vector<Complex>& input) {
    std::vector<Complex> result(input.size());
    for (std::size_t k = 0; k < input.size(); ++k) {
        for (std::size_t n = 0; n < input.size(); ++n) {
            const double angle =
                -2.0 * pi * static_cast<double>(k * n) / static_cast<double>(input.size());
            result[k] += input[n] * Complex(std::cos(angle), std::sin(angle));
        }
    }
    return result;
}

TEST(Fft, AgreesWithDirectDftForDeterministicComplexRecords) {
    std::mt19937 random(42);
    std::uniform_real_distribution<double> distribution(-1.0, 1.0);
    for (std::size_t size : {1U, 2U, 4U, 8U, 16U, 32U, 64U}) {
        SCOPED_TRACE(size);
        std::vector<Complex> input(size);
        for (auto& value : input) {
            value = {distribution(random), distribution(random)};
        }
        const auto expected = direct_dft(input);
        const auto actual = fft(input);
        ASSERT_EQ(actual.size(), size);
        for (std::size_t k = 0; k < size; ++k) {
            EXPECT_NEAR(std::abs(actual[k] - expected[k]), 0.0, 2e-12);
        }
    }
}

TEST(Fft, PreservesInputAndHasUnnormalizedImpulseResponse) {
    const std::vector<Complex> input{1.0, 0.0, 0.0, 0.0};
    const auto output = fft(input);
    EXPECT_EQ(input[0], Complex(1.0));
    EXPECT_EQ(input[1], Complex(0.0));
    for (auto value : output) {
        EXPECT_NEAR(std::abs(value - Complex(1.0)), 0.0, 1e-14);
    }
}

TEST(Fft, UsesNegativeExponentialAndPreservesEnergyWithParsevalScaling) {
    std::vector<Complex> input(1024);
    double input_energy = 0.0;
    for (std::size_t n = 0; n < input.size(); ++n) {
        input[n] = std::polar(1.0, 2.0 * pi * 13.0 * static_cast<double>(n) / 1024.0);
        input_energy += std::norm(input[n]);
    }
    const auto output = fft(input);
    EXPECT_NEAR(std::abs(output[13] - Complex(1024.0)), 0.0, 2e-10);
    double spectral_energy = 0.0;
    for (auto value : output) {
        spectral_energy += std::norm(value);
    }
    EXPECT_NEAR(spectral_energy / 1024.0, input_energy, 2e-10);
}

TEST(Fft, RejectsInvalidLengthsAndNonfiniteInput) {
    EXPECT_THROW((void)fft({}), std::invalid_argument);
    EXPECT_THROW((void)fft(std::vector<Complex>(3)), std::invalid_argument);
    EXPECT_THROW((void)fft(std::vector<Complex>(openece::max_signal_samples * 2)),
                 std::invalid_argument);
    for (double bad :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        EXPECT_THROW((void)fft(std::vector<Complex>{{bad, 0.0}}), std::invalid_argument);
        EXPECT_THROW((void)fft(std::vector<Complex>{{0.0, bad}}), std::invalid_argument);
    }
}

TEST(Fft, ReportsArithmeticOverflow) {
    const double maximum = std::numeric_limits<double>::max();
    EXPECT_THROW((void)fft(std::vector<Complex>{maximum, maximum}), std::overflow_error);
}

TEST(Spectrum, RecoversBinCenteredSineAmplitudeRegardlessOfPhase) {
    for (double phase : {0.0, 0.3, -pi / 2.0}) {
        const auto signal = openece::signals::generate_sine({2.5, 64.0, phase, 1024.0, 1.0});
        const auto spectrum = amplitude_spectrum(signal);
        EXPECT_EQ(spectrum.fft_size, 1024U);
        EXPECT_EQ(spectrum.sample_count, 1024U);
        ASSERT_EQ(spectrum.amplitudes.size(), 513U);
        EXPECT_DOUBLE_EQ(spectrum.bin_width_hz(), 1.0);
        EXPECT_NEAR(spectrum.amplitudes[64], 2.5, 1e-12);
        for (std::size_t k = 0; k < spectrum.amplitudes.size(); ++k) {
            if (k != 64) {
                EXPECT_NEAR(spectrum.amplitudes[k], 0.0, 1e-12);
            }
        }
    }
}

TEST(Spectrum, DoesNotDoubleDcOrNyquist) {
    const auto dc = amplitude_spectrum(SampledSignal(std::vector<double>(8, -3.0), 8.0));
    EXPECT_NEAR(dc.amplitudes[0], 3.0, 1e-14);
    const auto nyquist = amplitude_spectrum(SampledSignal({2, -2, 2, -2, 2, -2, 2, -2}, 8.0));
    ASSERT_EQ(nyquist.amplitudes.size(), 5U);
    EXPECT_NEAR(nyquist.amplitudes.back(), 2.0, 1e-14);
    EXPECT_DOUBLE_EQ(static_cast<double>(nyquist.amplitudes.size() - 1) * nyquist.bin_width_hz(),
                     4.0);
}

TEST(Spectrum, ZeroPadsAndNormalizesByOriginalLength) {
    // A padded three-sample impulse has unit magnitude at all four FFT bins.
    const auto spectrum = amplitude_spectrum(SampledSignal({1, 0, 0}, 12.0));
    EXPECT_EQ(spectrum.sample_count, 3U);
    EXPECT_EQ(spectrum.fft_size, 4U);
    EXPECT_DOUBLE_EQ(spectrum.bin_width_hz(), 3.0);
    ASSERT_EQ(spectrum.amplitudes.size(), 3U);
    EXPECT_NEAR(spectrum.amplitudes[0], 1.0 / 3.0, 1e-14);
    EXPECT_NEAR(spectrum.amplitudes[1], 2.0 / 3.0, 1e-14);
    EXPECT_NEAR(spectrum.amplitudes[2], 1.0 / 3.0, 1e-14);
}

TEST(Spectrum, OffBinRecordAgreesWithPaddedDirectDft) {
    const auto signal = openece::signals::generate_sine({1.7, 7.3, 0.4, 64.0, 0.3});
    const auto spectrum = amplitude_spectrum(signal);
    ASSERT_EQ(signal.size(), 19U);
    ASSERT_EQ(spectrum.fft_size, 32U);
    std::vector<Complex> padded(32);
    for (std::size_t i = 0; i < signal.size(); ++i) {
        padded[i] = signal.samples()[i];
    }
    const auto expected = direct_dft(padded);
    for (std::size_t k = 0; k < spectrum.amplitudes.size(); ++k) {
        const double factor = (k == 0 || k == 16) ? 1.0 : 2.0;
        EXPECT_NEAR(spectrum.amplitudes[k], factor * std::abs(expected[k]) / 19.0, 1e-13);
    }
}

TEST(Spectrum, HandlesSingletonAndSilence) {
    const auto singleton = amplitude_spectrum(SampledSignal({-2.0}, 8.0));
    ASSERT_EQ(singleton.amplitudes.size(), 1U);
    EXPECT_DOUBLE_EQ(singleton.amplitudes[0], 2.0);
    EXPECT_EQ(singleton.fft_size, 1U);
    const auto silence = amplitude_spectrum(SampledSignal(std::vector<double>(16), 8.0));
    for (double amplitude : silence.amplitudes) {
        EXPECT_DOUBLE_EQ(amplitude, 0.0);
    }
}

TEST(Spectrum, SupportsMaximumRecordWithCorrectScaling) {
    const auto spectrum = amplitude_spectrum(
        SampledSignal(std::vector<double>(openece::max_signal_samples, 1.0), 1048576.0));
    EXPECT_EQ(spectrum.fft_size, openece::max_signal_samples);
    EXPECT_DOUBLE_EQ(spectrum.amplitudes[0], 1.0);
    for (std::size_t k = 1; k < spectrum.amplitudes.size(); ++k) {
        EXPECT_DOUBLE_EQ(spectrum.amplitudes[k], 0.0);
    }
}
} // namespace
