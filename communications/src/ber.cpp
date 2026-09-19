#include "random.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <openece/communications/ber.hpp>
#include <openece/communications/link.hpp>
namespace openece::communications {
double theoretical_ber(Modulation modulation, double eb_n0_db) {
    (void)noise_standard_deviation(modulation, eb_n0_db);
    return .5 * std::erfc(std::sqrt(std::pow(10.0, eb_n0_db / 10)));
}
std::optional<double> measured_ber(const BerPoint& point) {
    if (point.bit_errors > point.bits_tested || point.bits_tested > point.requested_bits)
        throw Error(ErrorCode::invalid_value, "Inconsistent BER counts.");
    if (!point.bits_tested)
        return std::nullopt;
    return static_cast<double>(point.bit_errors) / static_cast<double>(point.bits_tested);
}
double zero_error_upper_bound95(std::uint64_t count) {
    if (!count)
        throw Error(ErrorCode::invalid_value, "A zero-error bound requires tested bits.");
    return -std::expm1(std::log(.05) / static_cast<double>(count));
}
struct BerExperiment::Impl {
    struct Point {
        BerPoint result;
        detail::Random bits, noise;
        double sigma;
        Point(const BerRequest& r, std::size_t i)
            : result{i, r.eb_n0_db[i], 0, 0, r.bits_per_point},
              bits(r.bit_seed, detail::ber_bits, i), noise(r.noise_seed, detail::ber_noise, i),
              sigma(noise_standard_deviation(r.modulation, r.eb_n0_db[i])) {}
    };
    std::size_t k;
    std::vector<Point> points;
    explicit Impl(const BerRequest& r) : k(bits_per_symbol(r.modulation)) {
        if (r.eb_n0_db.empty() || r.eb_n0_db.size() > limits::ber_points || !r.bits_per_point ||
            r.bits_per_point > limits::bits_per_ber_point ||
            r.bits_per_point > limits::aggregate_ber_bits / r.eb_n0_db.size())
            throw Error(ErrorCode::resource_limit,
                        "BER point or aggregate bit budget exceeds supported limits.");
        if (r.bits_per_point % k)
            throw Error(ErrorCode::incompatible_length, "QPSK BER budgets must be even.");
        for (double value : r.eb_n0_db)
            (void)noise_standard_deviation(r.modulation, value);
        points.reserve(r.eb_n0_db.size());
        for (std::size_t i = 0; i < r.eb_n0_db.size(); ++i)
            points.emplace_back(r, i);
    }
};
BerExperiment::BerExperiment(BerRequest request) : impl_(std::make_unique<Impl>(request)) {}
BerExperiment::~BerExperiment() = default;
BerExperiment::BerExperiment(BerExperiment&&) noexcept = default;
BerExperiment& BerExperiment::operator=(BerExperiment&&) noexcept = default;
void BerExperiment::advance(std::size_t index, std::uint64_t budget) {
    if (index >= impl_->points.size())
        throw std::out_of_range("BER point index");
    if (!budget || budget > limits::chunk_bits)
        throw Error(ErrorCode::resource_limit, "BER chunk budget exceeds supported limits.");
    if (budget % impl_->k)
        throw Error(ErrorCode::incompatible_length, "QPSK chunk budgets must be even.");
    auto& p = impl_->points[index];
    const auto count = std::min(budget, p.result.requested_bits - p.result.bits_tested);
    const double amplitude = impl_->k == 1 ? 1 : 1 / std::numbers::sqrt2;
    for (std::uint64_t i = 0; i < count; i += impl_->k) {
        const auto first = p.bits.bit();
        const auto second = impl_->k == 2 ? p.bits.bit() : std::uint8_t{0};
        const double real_noise = p.noise.gaussian(), imag_noise = p.noise.gaussian();
        const double real = (first == 0 ? amplitude : -amplitude) + p.sigma * real_noise;
        p.result.bit_errors += (real < 0) != static_cast<bool>(first);
        if (impl_->k == 2) {
            const double imag = (second == 0 ? amplitude : -amplitude) + p.sigma * imag_noise;
            p.result.bit_errors += (imag < 0) != static_cast<bool>(second);
        }
        p.result.bits_tested += impl_->k;
    }
}
void BerExperiment::run() {
    for (std::size_t i = 0; i < impl_->points.size(); ++i)
        while (!impl_->points[i].result.complete())
            advance(i, limits::chunk_bits);
}
std::vector<BerPoint> BerExperiment::results() const {
    std::vector<BerPoint> result;
    result.reserve(impl_->points.size());
    for (const auto& p : impl_->points)
        result.push_back(p.result);
    return result;
}
bool BerExperiment::complete() const {
    return std::ranges::all_of(impl_->points, [](const auto& p) { return p.result.complete(); });
}
} // namespace openece::communications
