#pragma once

#include <openece/core/sampled_signal.hpp>

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace openece::dsp {

// Owned, causal real impulse response h[0..M-1]. No rate or implicit normalization.
// Requires 1..max_signal_samples finite taps; arbitrary gain and symmetry allowed.
// As with SampledSignal, moved-from objects are only for destruction/reassignment.
class FirCoefficients {
  public:
    explicit FirCoefficients(std::vector<double> taps);
    [[nodiscard]] std::span<const double> taps() const& noexcept { return taps_; }
    std::span<const double> taps() const&& = delete;
    [[nodiscard]] std::size_t size() const noexcept { return taps_.size(); }

  private:
    std::vector<double> taps_;
};

// Full zero-extended convolution, beginning at t=0 with the input sample rate.
// N+M-1 samples, duration extended by (M-1)/fs. convolve_full resource limits apply.
[[nodiscard]] SampledSignal apply_fir_full(const SampledSignal& input,
                                           const FirCoefficients& filter);

// Odd M in [3, max_signal_samples], finite fs>0 and 0<cutoff/fs<0.5.
// Symmetric Hamming-windowed sinc; normalized to sum(h)=1 (unity DC gain).
// Cutoff is the ideal sinc cutoff, not a promised -3 dB point. Delay=(M-1)/2 samples.
// Symmetric design Hamming is separate from periodic Hann used for signal spectra.
[[nodiscard]] FirCoefficients design_lowpass(std::size_t tap_count, double cutoff_hz,
                                             double sample_rate_hz);

inline constexpr std::size_t max_response_products = 64'000'000;

struct FirFrequencyResponse {
    std::vector<std::complex<double>> values;
    double sample_rate_hz;

    // For results from frequency_response(): f[k]=(fs/2)*k/(P-1), k in [0,P-1].
    [[nodiscard]] double bin_width_hz() const noexcept {
        return (sample_rate_hz / 2.0) / static_cast<double>(values.size() - 1);
    }
};

// Direct H(f)=sum h[k]*exp(-j*2*pi*f*k/fs), with no window, division, or doubling.
// P must be 2..max_signal_samples, M*P<=max_response_products. fs must be finite,
// positive, with representable positive grid spacing. Both DC and Nyquist included;
// P=2 gives only those endpoints, evaluated as exact real sums (to roundoff).
// Invalid requests throw invalid_argument; nonfinite response throws overflow_error.
[[nodiscard]] FirFrequencyResponse
frequency_response(const FirCoefficients& filter, double sample_rate_hz, std::size_t point_count);

} // namespace openece::dsp
