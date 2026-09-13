#include <openece/dsp/convolution.hpp>

#include <openece/core/sampled_signal.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace openece::dsp {

std::vector<double> convolve_full(std::span<const double> input, std::span<const double> kernel) {
    const auto n = input.size();
    const auto m = kernel.size();
    // Check before addition/multiplication so even oversized requests cannot wrap.
    if (n == 0 || m == 0 || n > max_signal_samples || m > max_signal_samples ||
        n > max_signal_samples - (m - 1) || n > max_convolution_products / m) {
        throw std::invalid_argument("Convolution requires nonempty inputs, at most 1048576 output "
                                    "samples and 64000000 products");
    }
    const auto finite = [](double value) { return std::isfinite(value); };
    if (!std::ranges::all_of(input, finite) || !std::ranges::all_of(kernel, finite)) {
        throw std::invalid_argument("Convolution inputs must be finite");
    }
    std::vector<double> output(n + m - 1, 0.0);
    // Each input sample contributes a shifted, scaled copy of the impulse response.
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t k = 0; k < m; ++k) {
            output[i + k] += input[i] * kernel[k];
        }
    }
    if (!std::ranges::all_of(output, finite)) {
        throw std::overflow_error("Convolution overflow: reduce sample or coefficient magnitudes");
    }
    return output;
}

} // namespace openece::dsp
