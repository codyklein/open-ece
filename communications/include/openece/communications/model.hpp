#pragma once
#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace openece::communications {
namespace limits {
inline constexpr std::size_t frame_bits = 1U << 20, waveform_samples = 1U << 20;
inline constexpr std::size_t samples_per_symbol = 256;
inline constexpr double symbol_rate_min = 1, symbol_rate_max = 1e9;
inline constexpr double eb_n0_min_db = -20, eb_n0_max_db = 20;
} // namespace limits
enum class ErrorCode {
    invalid_bits,
    incompatible_length,
    invalid_value,
    resource_limit,
    numerical_failure
};
class Error : public std::runtime_error {
  public:
    Error(ErrorCode code, const char* message) : std::runtime_error(message), code_(code) {}
    ErrorCode code() const noexcept { return code_; }

  private:
    ErrorCode code_;
};
enum class Modulation { bpsk, qpsk };
std::size_t bits_per_symbol(Modulation);
class BitSequence {
  public:
    explicit BitSequence(std::vector<std::uint8_t>);
    std::span<const std::uint8_t> bits() const& noexcept { return bits_; }
    std::span<const std::uint8_t> bits() const&& = delete;
    std::size_t size() const noexcept { return bits_.size(); }

  private:
    std::vector<std::uint8_t> bits_;
};
using Sample = std::complex<double>;
class BasebandSignal {
  public:
    BasebandSignal(std::vector<Sample>, double sample_rate_hz);
    std::span<const Sample> samples() const& noexcept { return samples_; }
    std::span<const Sample> samples() const&& = delete;
    std::size_t size() const noexcept { return samples_.size(); }
    double sample_rate_hz() const noexcept { return rate_; }
    double duration_seconds() const noexcept;
    double time_seconds(std::size_t index) const;

  private:
    std::vector<Sample> samples_;
    double rate_;
};
struct FrameSpec {
    Modulation modulation = Modulation::bpsk;
    double symbol_rate_hz = 1000;
    std::size_t samples_per_symbol = 16;
};
// Validates counts, products and time axes before waveform allocation; returns sample count.
std::size_t validate_frame(std::size_t bit_count, const FrameSpec&);
std::vector<Sample> map_bits(const BitSequence&, Modulation);
BitSequence hard_demodulate(std::span<const Sample>, Modulation);
} // namespace openece::communications
