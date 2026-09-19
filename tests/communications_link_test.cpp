#include "random.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <openece/communications/link.hpp>
#include <random>
using namespace openece::communications;
namespace {
TEST(CommunicationsRandom, IntegerSeedAndOpenUniformContract) {
    EXPECT_EQ(detail::mix(0), 0xe220a8397b1dcdafULL);
    EXPECT_EQ(detail::mix(1), 0x910a2dec89025cc1ULL);
    std::mt19937_64 engine(5489);
    EXPECT_EQ(engine(), 14514284786278117030ULL);
    EXPECT_DOUBLE_EQ(detail::Random::open_uniform(0), 0x1p-53);
    EXPECT_DOUBLE_EQ(detail::Random::open_uniform(UINT64_MAX), 1 - 0x1p-53);
    detail::Random random(123, detail::link_noise, 7);
    std::mt19937_64 reference(detail::mix(123 ^ detail::link_noise ^ detail::mix(7)));
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(random.word(), reference());
    EXPECT_NE(detail::derive_seed(1, detail::link_bits, 0),
              detail::derive_seed(1, detail::link_noise, 0));
    EXPECT_NE(detail::derive_seed(1, detail::ber_bits, 0),
              detail::derive_seed(1, detail::ber_bits, 1));
}
TEST(CommunicationsRandom, BoxMullerOrderAndCachedStateSurviveChunkBoundaries) {
    detail::Random actual(321, detail::link_noise), uniforms(321, detail::link_noise);
    const double u1 = detail::Random::open_uniform(uniforms.word()),
                 u2 = detail::Random::open_uniform(uniforms.word());
    const double radius = std::sqrt(-2 * std::log(u1)), angle = 2 * std::numbers::pi * u2;
    EXPECT_DOUBLE_EQ(actual.gaussian(), radius * std::cos(angle));
    auto resumed = actual; // Includes the cached sine value and engine position.
    EXPECT_DOUBLE_EQ(actual.gaussian(), radius * std::sin(angle));
    EXPECT_DOUBLE_EQ(resumed.gaussian(), radius * std::sin(angle));
    for (int i = 0; i < 101; ++i)
        EXPECT_DOUBLE_EQ(actual.gaussian(), resumed.gaussian());
    detail::Random whole(7, detail::ber_noise, 9), chunked(7, detail::ber_noise, 9);
    std::vector<double> values(301);
    for (auto& v : values)
        v = whole.gaussian();
    std::size_t position = 0;
    for (auto chunk : {1, 3, 17, 29, 251})
        for (int i = 0; i < chunk; ++i)
            EXPECT_DOUBLE_EQ(values[position++], chunked.gaussian());
}
TEST(CommunicationsRandom, BitGenerationIsOwnedRepeatableAndBounded) {
    auto first = generate_bits(100, 5), second = generate_bits(100, 5),
         changed = generate_bits(100, 6);
    EXPECT_TRUE(std::equal(first.bits().begin(), first.bits().end(), second.bits().begin()));
    EXPECT_FALSE(std::equal(first.bits().begin(), first.bits().end(), changed.bits().begin()));
    detail::Random reference(5, detail::link_bits);
    for (auto bit : first.bits())
        EXPECT_EQ(bit, reference.word() >> 63);
    EXPECT_THROW(generate_bits(0, 1), Error);
    EXPECT_THROW(generate_bits(std::numeric_limits<std::size_t>::max(), 1), Error);
}
TEST(CommunicationsLink, RectangularPulsesHaveUnitEnergyAndCorrectTimeAxis) {
    for (auto m : {Modulation::bpsk, Modulation::qpsk})
        for (std::size_t l : {1U, 2U, 16U, 256U}) {
            auto result = simulate_link(
                {BitSequence({0, 0, 0, 1, 1, 1, 1, 0}), {m, 2000, l}, std::nullopt, 3});
            EXPECT_EQ(result.bit_errors, 0U);
            EXPECT_EQ(result.bits_tested, 8U);
            ASSERT_EQ(result.transmitted.size(), result.transmitted_symbols.size() * l);
            EXPECT_DOUBLE_EQ(result.transmitted.sample_rate_hz(), 2000 * static_cast<double>(l));
            for (std::size_t i = 0; i < result.transmitted_symbols.size(); ++i) {
                double energy = 0;
                for (std::size_t n = 0; n < l; ++n)
                    energy += std::norm(result.transmitted.samples()[i * l + n]);
                EXPECT_NEAR(energy, 1, 1e-13);
                EXPECT_NEAR(std::abs(result.decisions[i] - result.transmitted_symbols[i]), 0,
                            1e-13);
                EXPECT_DOUBLE_EQ(result.decision_time_seconds(i),
                                 static_cast<double>(i + 1) / 2000);
            }
            EXPECT_DOUBLE_EQ(result.decision_time_seconds(result.decisions.size() - 1),
                             result.transmitted.duration_seconds());
            EXPECT_THROW(result.decision_time_seconds(result.decisions.size()), std::out_of_range);
        }
}
TEST(CommunicationsLink, SuppliedNoiseMatchesIndependentBlockCalculation) {
    // L=4 gives exact weight 1/2. Independent arithmetic fixes both decisions.
    BasebandSignal received({{.5, .5},
                             {1.5, -.5},
                             {-.5, 1.5},
                             {.5, .5},
                             {-.5, -.5},
                             {-.5, -.5},
                             {-.5, -.5},
                             {1.5, 1.5}},
                            4000);
    auto decisions = matched_filter(received, 4);
    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_EQ(decisions[0], Sample(1, 1));
    EXPECT_EQ(decisions[1], Sample(0, 0));
    auto bits = hard_demodulate(decisions, Modulation::qpsk);
    for (auto b : bits.bits())
        EXPECT_EQ(b, 0);
}
TEST(CommunicationsLink, NoiseNormalizationAndRealImaginaryDrawOrder) {
    EXPECT_NEAR(noise_standard_deviation(Modulation::bpsk, 0), 1 / std::sqrt(2.0), 1e-15);
    EXPECT_DOUBLE_EQ(noise_standard_deviation(Modulation::qpsk, 0), .5);
    BasebandSignal zero(std::vector<Sample>(5), 100);
    const auto noisy = apply_awgn(zero, Modulation::qpsk, 0, 99);
    detail::Random random(99, detail::link_noise);
    for (auto sample : noisy.samples()) {
        const auto re = random.gaussian(), im = random.gaussian();
        EXPECT_EQ(sample, Sample(re / 2, im / 2));
    }
    for (auto sample : zero.samples())
        EXPECT_EQ(sample, Sample{});
}
TEST(CommunicationsLink, CompleteLinkReproducesSamplesAndDecisions) {
    LinkRequest request{generate_bits(100, 12), {Modulation::qpsk, 500, 8}, 2, 18};
    auto a = simulate_link(request), b = simulate_link(request);
    EXPECT_TRUE(std::equal(a.received.samples().begin(), a.received.samples().end(),
                           b.received.samples().begin()));
    EXPECT_EQ(a.decisions, b.decisions);
    EXPECT_EQ(a.bit_errors, b.bit_errors);
    std::uint64_t errors = 0;
    for (std::size_t i = 0; i < request.bits.size(); ++i)
        errors += request.bits.bits()[i] != a.recovered_bits.bits()[i];
    EXPECT_EQ(a.bit_errors, errors);
    request.bits = BitSequence({0, 0});
    EXPECT_EQ(a.bits_tested, 100U);
}
TEST(CommunicationsLink, InvalidChannelAndIncompleteFramesAreRejected) {
    BasebandSignal signal({{1, 0}, {1, 0}, {1, 0}}, 100);
    EXPECT_THROW(matched_filter(signal, 2), Error);
    EXPECT_THROW(matched_filter(signal, 0), Error);
    for (double eb : std::vector<double>{-21, 21, INFINITY, NAN}) {
        EXPECT_THROW(apply_awgn(signal, Modulation::bpsk, eb, 1), Error);
        EXPECT_THROW(simulate_link((LinkRequest{BitSequence({0, 0}), {}, eb, 1})), Error);
    }
    EXPECT_THROW(simulate_link(
                     (LinkRequest{BitSequence({0}), {Modulation::qpsk, 1000, 1}, std::nullopt, 1})),
                 Error);
    EXPECT_THROW(rectangular_waveform({}, 1000, 16), Error);
    std::vector<Sample> bad{{INFINITY, 0}};
    EXPECT_THROW(rectangular_waveform(bad, 1000, 1), Error);
    BasebandSignal huge(std::vector<Sample>(4, {std::numeric_limits<double>::max(), 0}), 4);
    EXPECT_THROW(matched_filter(huge, 4), Error);
}
} // namespace
