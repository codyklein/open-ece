#pragma once

#include <openece/core/sampled_signal.hpp>

namespace openece::signals {

struct SineParameters {
    double amplitude = 1.0;
    double frequency_hz = 8.0;
    double phase_radians = 0.0;
    double sample_rate_hz = 1024.0;
    double duration_seconds = 1.0;
};

// x[n] = A sin(2*pi*f*n/fs + phase), n in [0, round(fs*duration)).
// Rounds positive half-samples upward. Rejects frequencies above Nyquist.
[[nodiscard]] SampledSignal generate_sine(const SineParameters& parameters);

} // namespace openece::signals
