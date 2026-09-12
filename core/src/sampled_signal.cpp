#include <openece/core/sampled_signal.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace openece {

SampledSignal::SampledSignal(std::vector<double> samples, double sample_rate_hz)
    : samples_(std::move(samples)), sample_rate_hz_(sample_rate_hz) {
    if (samples_.empty() || samples_.size() > max_signal_samples) {
        throw std::invalid_argument("Signal must contain 1 to 1048576 samples");
    }
    if (!std::isfinite(sample_rate_hz_) || sample_rate_hz_ <= 0.0 ||
        !std::isfinite(duration_seconds())) {
        throw std::invalid_argument("Sample rate must be positive with a finite record duration");
    }
    if (!std::ranges::all_of(samples_, [](double value) { return std::isfinite(value); })) {
        throw std::invalid_argument("Signal samples must be finite");
    }
}

double SampledSignal::duration_seconds() const noexcept {
    return static_cast<double>(size()) / sample_rate_hz_;
}

double SampledSignal::time_seconds(std::size_t index) const {
    if (index >= size()) {
        throw std::out_of_range("Sample index is outside the record");
    }
    return static_cast<double>(index) / sample_rate_hz_;
}

} // namespace openece
