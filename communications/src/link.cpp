#include "random.hpp"
#include <algorithm>
#include <cmath>
#include <openece/communications/link.hpp>
namespace openece::communications {
BitSequence generate_bits(std::size_t count, std::uint64_t seed) {
    if (!count || count > limits::frame_bits)
        throw Error(ErrorCode::resource_limit, "Random bit record exceeds supported length.");
    detail::Random random(seed, detail::link_bits);
    std::vector<std::uint8_t> bits(count);
    for (auto& bit : bits)
        bit = random.bit();
    return BitSequence(std::move(bits));
}
BasebandSignal rectangular_waveform(std::span<const Sample> symbols, double rate, std::size_t l) {
    // One symbol corresponds to one BPSK bit for count/rate validation, independent of mapping.
    const auto count = validate_frame(symbols.size(), {Modulation::bpsk, rate, l});
    if (std::ranges::any_of(
            symbols, [](auto s) { return !std::isfinite(s.real()) || !std::isfinite(s.imag()); }))
        throw Error(ErrorCode::invalid_value, "Symbols must be finite.");
    const double weight = 1 / std::sqrt(static_cast<double>(l));
    std::vector<Sample> samples(count);
    for (std::size_t i = 0; i < symbols.size(); ++i)
        std::fill_n(samples.begin() + static_cast<std::ptrdiff_t>(i * l), l, symbols[i] * weight);
    return BasebandSignal(std::move(samples), rate * static_cast<double>(l));
}
std::vector<Sample> matched_filter(const BasebandSignal& signal, std::size_t l) {
    if (!l || l > limits::samples_per_symbol)
        throw Error(ErrorCode::resource_limit, "Invalid samples per symbol.");
    if (signal.size() % l)
        throw Error(ErrorCode::incompatible_length, "Receiver requires complete aligned symbols.");
    std::vector<Sample> decisions(signal.size() / l);
    const double weight = 1 / std::sqrt(static_cast<double>(l));
    // Multiply before summation; avoids unnecessary overflow from an unscaled sum.
    for (std::size_t i = 0; i < signal.size(); ++i)
        decisions[i / l] += signal.samples()[i] * weight;
    for (auto value : decisions)
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag()))
            throw Error(ErrorCode::numerical_failure, "Matched-filter accumulation is nonfinite.");
    return decisions;
}
double noise_standard_deviation(Modulation modulation, double eb_n0_db) {
    const auto k = bits_per_symbol(modulation);
    if (!std::isfinite(eb_n0_db) || eb_n0_db < limits::eb_n0_min_db ||
        eb_n0_db > limits::eb_n0_max_db)
        throw Error(ErrorCode::invalid_value, "Eb/N0 must be finite and between -20 and +20 dB.");
    return std::sqrt(1 / (2 * static_cast<double>(k) * std::pow(10.0, eb_n0_db / 10)));
}
BasebandSignal apply_awgn(const BasebandSignal& signal, Modulation modulation, double eb_n0_db,
                          std::uint64_t seed) {
    const double sigma = noise_standard_deviation(modulation, eb_n0_db);
    detail::Random random(seed, detail::link_noise);
    std::vector<Sample> samples(signal.samples().begin(), signal.samples().end());
    for (auto& sample : samples) {
        const double real = random.gaussian(), imag = random.gaussian();
        sample += sigma * Sample(real, imag);
        if (!std::isfinite(sample.real()) || !std::isfinite(sample.imag()))
            throw Error(ErrorCode::numerical_failure, "Noisy sample is nonfinite.");
    }
    return BasebandSignal(std::move(samples), signal.sample_rate_hz());
}
double LinkResult::decision_time_seconds(std::size_t index) const {
    if (index >= decisions.size())
        throw std::out_of_range("Decision index");
    return static_cast<double>(index + 1) / frame.symbol_rate_hz;
}
LinkResult simulate_link(const LinkRequest& request) {
    (void)validate_frame(request.bits.size(), request.frame);
    if (request.eb_n0_db)
        (void)noise_standard_deviation(request.frame.modulation, *request.eb_n0_db);
    auto symbols = map_bits(request.bits, request.frame.modulation);
    auto tx = rectangular_waveform(symbols, request.frame.symbol_rate_hz,
                                   request.frame.samples_per_symbol);
    auto rx = request.eb_n0_db
                  ? apply_awgn(tx, request.frame.modulation, *request.eb_n0_db, request.noise_seed)
                  : tx;
    auto decisions = matched_filter(rx, request.frame.samples_per_symbol);
    auto decoded = hard_demodulate(decisions, request.frame.modulation);
    std::uint64_t errors = 0;
    for (std::size_t i = 0; i < request.bits.size(); ++i)
        errors += request.bits.bits()[i] != decoded.bits()[i];
    return {request.frame,        std::move(symbols), std::move(tx), std::move(rx),
            std::move(decisions), std::move(decoded), errors,        request.bits.size()};
}
} // namespace openece::communications
