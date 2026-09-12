#pragma once

#include <openece/core/sampled_signal.hpp>
#include <openece/dsp/window.hpp>

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
    Window window = Window::Rectangular;
    double coherent_gain = 1.0; // Sum of the original L window weights divided by L.

    [[nodiscard]] double bin_width_hz() const noexcept {
        return sample_rate_hz / static_cast<double>(fft_size);
    }
};

// Window a copy, then zero-pad to next power of two. Divide by sum(window weights);
// double interior positive-frequency bins, never DC or Nyquist. Default preserves v0.1.
[[nodiscard]] AmplitudeSpectrum amplitude_spectrum(const SampledSignal& signal,
                                                   Window window = Window::Rectangular);

} // namespace openece::dsp
