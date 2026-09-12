#include <openece/signals/sine.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace openece::signals {

SampledSignal generate_sine(const SineParameters& p) {
    if (!std::isfinite(p.amplitude) || p.amplitude < 0.0 || !std::isfinite(p.frequency_hz) ||
        p.frequency_hz < 0.0 || !std::isfinite(p.phase_radians) ||
        !std::isfinite(p.sample_rate_hz) || p.sample_rate_hz <= 0.0 ||
        !std::isfinite(p.duration_seconds) || p.duration_seconds <= 0.0) {
        throw std::invalid_argument("Sine parameters must be finite; amplitude/frequency "
                                    "nonnegative, rate/duration positive");
    }
    if (p.frequency_hz > p.sample_rate_hz / 2.0) {
        throw std::invalid_argument(
            "Frequency must not exceed the Nyquist frequency (sample rate / 2)");
    }
    const double count = std::floor(p.sample_rate_hz * p.duration_seconds + 0.5);
    if (!std::isfinite(count) || count < 1.0 || count > static_cast<double>(max_signal_samples)) {
        throw std::invalid_argument("Requested duration must produce 1 to 1048576 samples");
    }

    std::vector<double> samples(static_cast<std::size_t>(count));
    constexpr double tau = 2.0 * std::numbers::pi;
    const double phase = std::remainder(p.phase_radians, tau);
    // Divide before multiplying to avoid overflow in f*n for high sample rates.
    const double radians_per_sample = tau * (p.frequency_hz / p.sample_rate_hz);
    for (std::size_t n = 0; n < samples.size(); ++n) {
        samples[n] = p.amplitude * std::sin(radians_per_sample * static_cast<double>(n) + phase);
    }
    return SampledSignal(std::move(samples), p.sample_rate_hz);
}

} // namespace openece::signals
