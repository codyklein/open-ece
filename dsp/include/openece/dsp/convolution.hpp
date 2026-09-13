#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace openece::dsp {

// Resource policy for direct O(N*M) convolution, not a mathematical limit.
inline constexpr std::size_t max_convolution_products = 64'000'000;

// Full causal linear convolution: N+M-1 samples, zero extension on both sides.
// No cropping, wrapping, reflection, normalization, or delay compensation.
// Inputs must be nonempty and finite; output <= max_signal_samples and
// N*M <= max_convolution_products. Invalid requests throw invalid_argument;
// nonfinite arithmetic throws overflow_error. Neither input is modified.
[[nodiscard]] std::vector<double> convolve_full(std::span<const double> input,
                                                std::span<const double> kernel);

} // namespace openece::dsp
