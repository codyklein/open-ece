#pragma once

#include <cstddef>
#include <vector>

namespace openece::dsp {

enum class Window { Rectangular, Hann };

// Weights for the original record, before padding. Hann is periodic:
// w[n] = 0.5 - 0.5*cos(2*pi*n/L). For L=1, both windows are defined as {1}.
// Rejects empty/oversized records and unknown enum values.
[[nodiscard]] std::vector<double> window_coefficients(Window window, std::size_t sample_count);

} // namespace openece::dsp
