#include <openece/dsp/fir.hpp>

#include <openece/dsp/convolution.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace openece::dsp {

FirCoefficients::FirCoefficients(std::vector<double> taps) : taps_(std::move(taps)) {
    if (taps_.empty() || taps_.size() > max_signal_samples ||
        !std::ranges::all_of(taps_, [](double v) { return std::isfinite(v); })) {
        throw std::invalid_argument("FIR requires 1 to 1048576 finite coefficients");
    }
}

SampledSignal apply_fir_full(const SampledSignal& input, const FirCoefficients& filter) {
    return SampledSignal(convolve_full(input.samples(), filter.taps()), input.sample_rate_hz());
}

FirCoefficients design_lowpass(std::size_t tap_count, double cutoff_hz, double sample_rate_hz) {
    const double r = cutoff_hz / sample_rate_hz;
    if (tap_count < 3 || tap_count > max_signal_samples || tap_count % 2 == 0 ||
        !std::isfinite(sample_rate_hz) || sample_rate_hz <= 0 || !std::isfinite(cutoff_hz) ||
        !std::isfinite(r) || r <= 0 || r >= 0.5) {
        throw std::invalid_argument("Low-pass requires odd tap count >=3 and finite 0 < cutoff < "
                                    "Nyquist with representable cutoff/fs");
    }
    std::vector<double> taps(tap_count);
    const auto center = (tap_count - 1) / 2;
    // Construct mirrored pairs explicitly, preserving exact coefficient symmetry.
    for (std::size_t k = 0; k <= center; ++k) {
        const double distance = static_cast<double>(center - k);
        const double angle = 2.0 * std::numbers::pi * r * distance;
        const double sinc = angle == 0.0 ? 1.0 : std::sin(angle) / angle;
        const double hamming =
            0.54 - 0.46 * std::cos(2.0 * std::numbers::pi * static_cast<double>(k) /
                                   static_cast<double>(tap_count - 1));
        taps[k] = taps[tap_count - 1 - k] = (2.0 * r) * sinc * hamming;
    }
    const double sum = std::accumulate(taps.begin(), taps.end(), 0.0);
    if (!std::isfinite(sum) || sum <= 0.0) {
        throw std::overflow_error("Low-pass normalization is not representable");
    }
    for (auto& tap : taps)
        tap /= sum;
    return FirCoefficients(std::move(taps));
}

FirFrequencyResponse frequency_response(const FirCoefficients& filter, double sample_rate_hz,
                                        std::size_t point_count) {
    if (point_count < 2 || point_count > max_signal_samples || filter.size() == 0 ||
        filter.size() > max_response_products / point_count || !std::isfinite(sample_rate_hz) ||
        sample_rate_hz <= 0.0 ||
        (sample_rate_hz / 2.0) / static_cast<double>(point_count - 1) == 0.0) {
        throw std::invalid_argument(
            "Response requires 2 to 1048576 points, at most 64000000 tap-point products and a "
            "positive representable frequency grid");
    }
    FirFrequencyResponse result{{}, sample_rate_hz};
    result.values.reserve(point_count);
    for (std::size_t p = 0; p < point_count; ++p) {
        std::complex<double> value{0.0, 0.0};
        const double omega =
            std::numbers::pi * (static_cast<double>(p) / static_cast<double>(point_count - 1));
        for (std::size_t k = 0; k < filter.size(); ++k) {
            const double h = filter.taps()[k];
            if (p == 0)
                value += h;
            else if (p == point_count - 1)
                value += k % 2 == 0 ? h : -h;
            else
                value += h * std::complex<double>(std::cos(omega * static_cast<double>(k)),
                                                  -std::sin(omega * static_cast<double>(k)));
        }
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag()) ||
            !std::isfinite(std::abs(value))) {
            throw std::overflow_error("FIR response overflow: reduce coefficient magnitudes");
        }
        result.values.push_back(value);
    }
    return result;
}
} // namespace openece::dsp
