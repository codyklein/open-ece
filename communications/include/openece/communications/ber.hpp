#pragma once
#include <memory>
#include <openece/communications/model.hpp>
#include <optional>
namespace openece::communications {
namespace limits {
// Execution policies validated by Fedora/Windows checkpoint-3 benchmarks; not latency guarantees.
inline constexpr std::size_t ber_points = 64;
inline constexpr std::uint64_t bits_per_ber_point = 10'000'000, aggregate_ber_bits = 50'000'000;
inline constexpr std::uint64_t chunk_bits = 65'536;
} // namespace limits
struct BerRequest {
    Modulation modulation = Modulation::bpsk;
    std::vector<double> eb_n0_db;
    std::uint64_t bits_per_point = 100'000;
    std::uint64_t bit_seed = 1, noise_seed = 2;
};
struct BerPoint {
    std::size_t point_index;
    double eb_n0_db;
    std::uint64_t bit_errors, bits_tested, requested_bits;
    bool complete() const noexcept { return bits_tested == requested_bits; }
};
// Normalized coherent Gray BPSK/QPSK, 0.5*erfc(sqrt(10^(dB/10))).
double theoretical_ber(Modulation, double eb_n0_db);
// No tests means no estimate. Zero errors remains the exact measured ratio zero.
std::optional<double> measured_ber(const BerPoint&);
double zero_error_upper_bound95(std::uint64_t bits_tested);
class BerExperiment {
  public:
    explicit BerExperiment(BerRequest);
    ~BerExperiment();
    BerExperiment(BerExperiment&&) noexcept;
    BerExperiment& operator=(BerExperiment&&) noexcept;
    BerExperiment(const BerExperiment&) = delete;
    BerExperiment& operator=(const BerExperiment&) = delete;
    // Point identity is its original request index. Budget must be 1..chunk_bits,
    // and divisible by bits_per_symbol. Never rounds a QPSK bit budget.
    void advance(std::size_t point_index, std::uint64_t bit_budget);
    void run();
    std::vector<BerPoint> results() const; // Owned snapshots, original request order.
    bool complete() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace openece::communications
