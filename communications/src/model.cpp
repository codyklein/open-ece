#include <algorithm>
#include <cmath>
#include <numbers>
#include <openece/communications/model.hpp>
#include <utility>
namespace openece::communications {
std::size_t bits_per_symbol(Modulation modulation) {
    switch (modulation) {
    case Modulation::bpsk:
        return 1;
    case Modulation::qpsk:
        return 2;
    }
    throw Error(ErrorCode::invalid_value, "Unsupported modulation.");
}
BitSequence::BitSequence(std::vector<std::uint8_t> bits) : bits_(std::move(bits)) {
    if (bits_.empty() || bits_.size() > limits::frame_bits)
        throw Error(ErrorCode::resource_limit, "Bit record length is outside the supported range.");
    if (std::ranges::any_of(bits_, [](auto b) { return b > 1; }))
        throw Error(ErrorCode::invalid_bits, "Bits must be exactly 0 or 1.");
}
BasebandSignal::BasebandSignal(std::vector<Sample> samples, double rate)
    : samples_(std::move(samples)), rate_(rate) {
    if (samples_.empty() || samples_.size() > limits::waveform_samples)
        throw Error(ErrorCode::resource_limit,
                    "Baseband record length is outside the supported range.");
    if (!std::isfinite(rate_) || rate_ <= 0 || !std::isfinite(duration_seconds()) ||
        duration_seconds() <= 0 || !std::isfinite(1 / rate_) || 1 / rate_ <= 0)
        throw Error(ErrorCode::invalid_value,
                    "Sample rate must define a finite positive time axis.");
    if (std::ranges::any_of(
            samples_, [](auto s) { return !std::isfinite(s.real()) || !std::isfinite(s.imag()); }))
        throw Error(ErrorCode::invalid_value, "Baseband samples must be finite.");
}
double BasebandSignal::duration_seconds() const noexcept {
    return static_cast<double>(size()) / rate_;
}
double BasebandSignal::time_seconds(std::size_t index) const {
    if (index >= size())
        throw std::out_of_range("Baseband sample index");
    return static_cast<double>(index) / rate_;
}
std::size_t validate_frame(std::size_t count, const FrameSpec& spec) {
    const auto k = bits_per_symbol(spec.modulation);
    if (!count || count > limits::frame_bits)
        throw Error(ErrorCode::resource_limit, "Bit record length is outside the supported range.");
    if (count % k)
        throw Error(ErrorCode::incompatible_length,
                    "QPSK requires an even bit count; no padding is applied.");
    if (!spec.samples_per_symbol || spec.samples_per_symbol > limits::samples_per_symbol ||
        count / k > limits::waveform_samples / spec.samples_per_symbol)
        throw Error(ErrorCode::resource_limit,
                    "Waveform exceeds samples-per-symbol or record limit.");
    if (!std::isfinite(spec.symbol_rate_hz) || spec.symbol_rate_hz < limits::symbol_rate_min ||
        spec.symbol_rate_hz > limits::symbol_rate_max)
        throw Error(ErrorCode::invalid_value, "Symbol rate must be between 1 and 1e9 symbols/s.");
    const auto samples = count / k * spec.samples_per_symbol;
    const double rate = spec.symbol_rate_hz * static_cast<double>(spec.samples_per_symbol);
    const double duration = static_cast<double>(samples) / rate;
    if (!std::isfinite(rate) || !std::isfinite(duration) || duration <= 0 || 1 / rate <= 0)
        throw Error(ErrorCode::invalid_value, "Waveform time axis is not representable.");
    return samples;
}
std::vector<Sample> map_bits(const BitSequence& bits, Modulation modulation) {
    const auto k = bits_per_symbol(modulation);
    if (bits.size() % k)
        throw Error(ErrorCode::incompatible_length,
                    "QPSK requires an even bit count; no padding is applied.");
    std::vector<Sample> result;
    result.reserve(bits.size() / k);
    const auto data = bits.bits();
    for (std::size_t i = 0; i < data.size(); i += k) {
        const double real = data[i] == 0 ? 1 : -1;
        if (k == 1)
            result.emplace_back(real, 0);
        else
            result.emplace_back(real / std::numbers::sqrt2,
                                (data[i + 1] == 0 ? 1 : -1) / std::numbers::sqrt2);
    }
    return result;
}
BitSequence hard_demodulate(std::span<const Sample> symbols, Modulation modulation) {
    const auto k = bits_per_symbol(modulation);
    if (symbols.empty() || symbols.size() > limits::frame_bits / k)
        throw Error(ErrorCode::resource_limit, "Decision record exceeds bit limit.");
    if (std::ranges::any_of(
            symbols, [](auto s) { return !std::isfinite(s.real()) || !std::isfinite(s.imag()); }))
        throw Error(ErrorCode::invalid_value, "Decision samples must be finite.");
    std::vector<std::uint8_t> bits;
    bits.reserve(symbols.size() * k);
    for (auto s : symbols) {
        bits.push_back(static_cast<std::uint8_t>(s.real() < 0));
        if (k == 2)
            bits.push_back(static_cast<std::uint8_t>(s.imag() < 0));
    }
    return BitSequence(std::move(bits));
}
} // namespace openece::communications
