#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace openece {

// An explicit v0.1 resource bound; streaming will need a different representation.
inline constexpr std::size_t max_signal_samples = 1U << 20;

// Owns a nonempty, finite, uniformly sampled real record starting at t = 0 seconds.
// Sample magnitudes are unitless; physical units and multichannel data are future work.
class SampledSignal {
  public:
    SampledSignal(std::vector<double> samples, double sample_rate_hz);

    [[nodiscard]] std::span<const double> samples() const& noexcept { return samples_; }
    std::span<const double> samples() const&& = delete;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] double sample_rate_hz() const noexcept { return sample_rate_hz_; }
    [[nodiscard]] double duration_seconds() const noexcept;
    [[nodiscard]] double time_seconds(std::size_t index) const;

  private:
    std::vector<double> samples_;
    double sample_rate_hz_;
};

} // namespace openece
