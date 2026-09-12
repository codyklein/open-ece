#include <openece/dsp/fft.hpp>

#include <bit>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace openece::dsp {
namespace {
bool finite(std::complex<double> value) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}
} // namespace

std::vector<std::complex<double>> fft(std::span<const std::complex<double>> input) {
    const auto size = input.size();
    if (!std::has_single_bit(size) || size > max_signal_samples) {
        throw std::invalid_argument("FFT length must be a power of two from 1 to 1048576");
    }
    for (const auto value : input) {
        if (!finite(value)) {
            throw std::invalid_argument("FFT input must be finite");
        }
    }
    std::vector<std::complex<double>> output(input.begin(), input.end());

    // Bit reversal places samples in the order needed by iterative radix-2 butterflies.
    for (std::size_t i = 1, j = 0; i < size; ++i) {
        auto bit = size >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(output[i], output[j]);
        }
    }
    for (std::size_t length = 2; length <= size; length <<= 1) {
        const auto half = length / 2;
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(length);
        const auto step = std::polar(1.0, angle);
        for (std::size_t start = 0; start < size; start += length) {
            std::complex<double> twiddle{1.0, 0.0};
            for (std::size_t j = 0; j < half; ++j) {
                const auto even = output[start + j];
                const auto odd = twiddle * output[start + j + half];
                output[start + j] = even + odd;
                output[start + j + half] = even - odd;
                twiddle *= step;
            }
        }
    }
    for (const auto value : output) {
        if (!finite(value)) {
            throw std::overflow_error("FFT overflow: reduce the signal amplitude");
        }
    }
    return output;
}

AmplitudeSpectrum amplitude_spectrum(const SampledSignal& signal, Window window) {
    const auto weights = window_coefficients(window, signal.size());
    const double normalization = std::accumulate(weights.begin(), weights.end(), 0.0);
    const auto size = std::bit_ceil(signal.size());
    std::vector<std::complex<double>> padded(size, 0.0);
    for (std::size_t i = 0; i < signal.size(); ++i) {
        padded[i] = signal.samples()[i] * weights[i];
    }
    const auto transformed = fft(padded);
    AmplitudeSpectrum spectrum{{},
                               signal.sample_rate_hz(),
                               signal.size(),
                               size,
                               window,
                               normalization / static_cast<double>(signal.size())};
    spectrum.amplitudes.reserve(size / 2 + 1);
    for (std::size_t k = 0; k <= size / 2; ++k) {
        const bool paired_bin = k != 0 && k != size / 2;
        // Scale components before taking the magnitude to avoid unnecessary overflow.
        const double amplitude =
            std::abs(transformed[k] / normalization) * (paired_bin ? 2.0 : 1.0);
        if (!std::isfinite(amplitude)) {
            throw std::overflow_error("Spectrum overflow: reduce the signal amplitude");
        }
        spectrum.amplitudes.push_back(amplitude);
    }
    return spectrum;
}

} // namespace openece::dsp
