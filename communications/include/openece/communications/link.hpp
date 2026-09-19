#pragma once
#include <openece/communications/model.hpp>
#include <optional>
namespace openece::communications {
BitSequence generate_bits(std::size_t count, std::uint64_t seed);
BasebandSignal rectangular_waveform(std::span<const Sample> symbols, double symbol_rate_hz,
                                    std::size_t samples_per_symbol);
std::vector<Sample> matched_filter(const BasebandSignal&, std::size_t samples_per_symbol);
// Variance per real coordinate is N0/2, with Es=1 and Eb=1/bits_per_symbol.
double noise_standard_deviation(Modulation, double eb_n0_db);
BasebandSignal apply_awgn(const BasebandSignal&, Modulation, double eb_n0_db, std::uint64_t seed);
struct LinkRequest {
    BitSequence bits;
    FrameSpec frame;
    std::optional<double> eb_n0_db = 6; // nullopt explicitly disables noise.
    std::uint64_t noise_seed = 1;
};
struct LinkResult {
    FrameSpec frame;
    std::vector<Sample> transmitted_symbols;
    BasebandSignal transmitted, received;
    std::vector<Sample> decisions;
    BitSequence recovered_bits;
    std::uint64_t bit_errors, bits_tested;
    double decision_time_seconds(std::size_t index) const;
};
LinkResult simulate_link(const LinkRequest&);
} // namespace openece::communications
