#include <openece/dsp/window.hpp>

#include <openece/core/sampled_signal.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace openece::dsp {

std::vector<double> window_coefficients(Window window, std::size_t sample_count) {
    if (sample_count == 0 || sample_count > max_signal_samples) {
        throw std::invalid_argument("Window length must be from 1 to 1048576");
    }
    if (window != Window::Rectangular && window != Window::Hann) {
        throw std::invalid_argument("Unknown spectral window");
    }
    std::vector<double> weights(sample_count, 1.0);
    // Identity at L=1 avoids a zero-gain window with no usable observation.
    if (window == Window::Hann && sample_count > 1) {
        for (std::size_t n = 0; n < sample_count; ++n) {
            weights[n] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(n) /
                                              static_cast<double>(sample_count));
        }
    }
    return weights;
}

} // namespace openece::dsp
