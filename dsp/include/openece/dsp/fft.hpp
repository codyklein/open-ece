#pragma once

#include <openece/core/sampled_signal.hpp>

#include <complex>
#include <span>
#include <vector>

namespace openece::dsp {

// Unnormalized forward DFT with negative exponential. Length must be a power of two.
[[nodiscard]] std::vector<std::complex<double>> fft(std::span<const std::complex<double>> input);

struct AmplitudeSpectrum {
    std::vector<double> amplitudes;
    double sample_rate_hz;
    std::size_t sample_count;
    std::size_t fft_size;

    [[nodiscard]] double bin_width_hz() const noexcept {
        return sample_rate_hz / static_cast<double>(fft_size);
    }
};

// Rectangular window, zero-pad to next power of two. Divide by original sample count;
// double positive-frequency bins except Nyquist. DC and Nyquist are not doubled.
[[nodiscard]] AmplitudeSpectrum amplitude_spectrum(const SampledSignal& signal);

} // namespace openece::dsp
