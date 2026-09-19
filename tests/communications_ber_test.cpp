#include "random.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <openece/communications/ber.hpp>
#include <openece/communications/link.hpp>
using namespace openece::communications;
namespace {
void same(const std::vector<BerPoint>& a, const std::vector<BerPoint>& b) {
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].point_index, b[i].point_index);
        EXPECT_EQ(a[i].eb_n0_db, b[i].eb_n0_db);
        EXPECT_EQ(a[i].bit_errors, b[i].bit_errors);
        EXPECT_EQ(a[i].bits_tested, b[i].bits_tested);
        EXPECT_EQ(a[i].requested_bits, b[i].requested_bits);
    }
}
TEST(CommunicationsBer, RunChunkSizeAndPointProcessingOrderDoNotChangeResults) {
    for (auto modulation : {Modulation::bpsk, Modulation::qpsk}) {
        BerRequest request{modulation, {-2, 0, 4}, 10000, 14, 27};
        BerExperiment whole(request), stepped(request);
        whole.run();
        while (!stepped.complete())
            for (auto index : {2U, 0U, 1U})
                stepped.advance(index, 46);
        same(whole.results(), stepped.results());
        auto before = whole.results();
        whole.run();
        whole.advance(0, 2);
        same(before, whole.results());
    }
}
TEST(CommunicationsBer, PointStreamsAreIndependentOfOtherPointsAndOwned) {
    BerRequest request{Modulation::qpsk, {0, 2, 4}, 1000, 12, 42};
    BerExperiment first(request);
    first.advance(1, 42);
    auto snapshot = first.results();
    request.eb_n0_db[0] = -4;
    request.eb_n0_db.push_back(7);
    BerExperiment second(request);
    first.run();
    second.run();
    auto a = first.results(), b = second.results();
    EXPECT_EQ(a[1].bit_errors, b[1].bit_errors);
    EXPECT_EQ(a[2].bit_errors, b[2].bit_errors);
    EXPECT_EQ(snapshot[1].bits_tested, 42U);
    EXPECT_FALSE(snapshot[1].complete());
    EXPECT_EQ(snapshot[0].bits_tested, 0U);
    EXPECT_FALSE(measured_ber(snapshot[0]));
    EXPECT_EQ(a[0].eb_n0_db, 0);
    EXPECT_EQ(a[0].requested_bits, 1000U);
}
TEST(CommunicationsBer, TheoryKnownValuesAndBothModulationsAgree) {
    EXPECT_NEAR(theoretical_ber(Modulation::bpsk, 0), 0.078649603525142565, 1e-15);
    EXPECT_NEAR(theoretical_ber(Modulation::qpsk, 10), 3.872108215522037e-6, 1e-18);
    for (double eb : {-20, -4, 0, 4, 8, 20}) {
        EXPECT_DOUBLE_EQ(theoretical_ber(Modulation::bpsk, eb),
                         theoretical_ber(Modulation::qpsk, eb));
        EXPECT_GT(theoretical_ber(Modulation::bpsk, eb), 0);
    }
}
TEST(CommunicationsBer, FixedSeedCountsAgreeWithBinomialTheory) {
    // Predeclared two-sided Bernstein bound: failure probability <=1e-6 per case
    // for N independent Bernoulli trials. Eight cases => union bound <=8e-6.
    // No equality/monotonicity assertion and no seed selection based on outcomes.
    constexpr double alpha = 1e-6;
    constexpr std::uint64_t count = 200000;
    const double t = std::log(2 / alpha);
    for (auto modulation : {Modulation::bpsk, Modulation::qpsk}) {
        BerExperiment run({modulation, {-4, 0, 4, 8}, count, 0x12345678, 0xabcdef01});
        run.run();
        for (const auto& point : run.results()) {
            const double p = theoretical_ber(modulation, point.eb_n0_db), mean = count * p;
            const double bound = std::sqrt(2 * count * p * (1 - p) * t) + 2 * t / 3;
            EXPECT_LE(std::abs(static_cast<double>(point.bit_errors) - mean), bound);
        }
    }
}
TEST(CommunicationsBer, ZeroErrorReportsExactCountsAndStableUpperBound) {
    BerPoint p{0, 20, 0, 100000, 100000};
    EXPECT_EQ(measured_ber(p), 0);
    EXPECT_NEAR(zero_error_upper_bound95(100000), 2.995687401942795e-5, 1e-16);
    EXPECT_DOUBLE_EQ(zero_error_upper_bound95(1), .95);
    EXPECT_GT(zero_error_upper_bound95(UINT64_MAX), 0);
    EXPECT_THROW(zero_error_upper_bound95(0), Error);
    EXPECT_THROW(measured_ber((BerPoint{0, 0, 2, 1, 1})), Error);
    EXPECT_THROW(measured_ber((BerPoint{0, 0, 0, 2, 1})), Error);
    BerExperiment run({Modulation::qpsk, {20}, 10000, 7, 9});
    run.run();
    EXPECT_EQ(run.results()[0].bit_errors, 0U);
    EXPECT_EQ(run.results()[0].bits_tested, 10000U);
}
TEST(CommunicationsBer, RejectsOddQpskBudgetsAndInvalidChunksWithoutProgress) {
    EXPECT_THROW(BerExperiment((BerRequest{Modulation::qpsk, {0}, 3, 1, 2})), Error);
    BerExperiment run({Modulation::qpsk, {0}, 100, 1, 2});
    EXPECT_THROW(run.advance(0, 1), Error);
    EXPECT_THROW(run.advance(0, 0), Error);
    EXPECT_THROW(run.advance(0, limits::chunk_bits + 2), Error);
    EXPECT_THROW(run.advance(1, 2), std::out_of_range);
    EXPECT_EQ(run.results()[0].bits_tested, 0U);
    run.advance(0, 98);
    run.advance(0, 4);
    EXPECT_EQ(run.results()[0].bits_tested, 100U);
}
TEST(CommunicationsBer, BoundsValidatedBeforeExperimentAllocation) {
    EXPECT_THROW(BerExperiment((BerRequest{Modulation::bpsk, {}, 100, 1, 2})), Error);
    EXPECT_THROW(BerExperiment((BerRequest{Modulation::bpsk,
                                           std::vector<double>(limits::ber_points + 1), 2, 1, 2})),
                 Error);
    for (auto count : {std::uint64_t{0}, limits::bits_per_ber_point + 1, UINT64_MAX})
        EXPECT_THROW(BerExperiment((BerRequest{Modulation::bpsk, {0}, count, 1, 2})), Error);
    EXPECT_THROW(BerExperiment((BerRequest{Modulation::bpsk, std::vector<double>(6),
                                           limits::bits_per_ber_point, 1, 2})),
                 Error);
    EXPECT_NO_THROW(BerExperiment(
        (BerRequest{Modulation::qpsk, std::vector<double>(5), limits::bits_per_ber_point, 1, 2})));
    for (double eb : std::vector<double>{-21, 21, INFINITY, NAN})
        EXPECT_THROW(BerExperiment((BerRequest{Modulation::bpsk, {0, eb}, 2, 1, 2})), Error);
}
TEST(CommunicationsBer, GaussianMomentsAndMatchedFilterVarianceAreNormalized) {
    // Gaussian mean tail and Laurent-Massart chi-square energy bounds, t=log(4/alpha).
    // Bound second moment and mean separately; no fitted tolerance or sample renormalization.
    const double t = std::log(4 / 1e-6);
    constexpr std::size_t n = 32768;
    for (auto l : {1U, 16U}) {
        BasebandSignal zero(std::vector<Sample>(n * l), 1000 * l);
        auto samples = matched_filter(apply_awgn(zero, Modulation::qpsk, 0, 12345), l);
        for (bool imaginary : {false, true}) {
            double sum = 0, squares = 0;
            for (auto s : samples) {
                const double z = (imaginary ? s.imag() : s.real()) / .5;
                sum += z;
                squares += z * z;
            }
            EXPECT_LE(std::abs(sum / n), std::sqrt(2 * t / n));
            EXPECT_LE(std::abs(squares / n - 1), 2 * std::sqrt(t / n) + 2 * t / n);
        }
    }
}
TEST(CommunicationsBer, ScalarReferenceChecksBitAndNoiseConsumption) {
    BerExperiment run({Modulation::qpsk, {0}, 200, 31, 47});
    run.run();
    detail::Random bits(31, detail::ber_bits, 0), noise(47, detail::ber_noise, 0);
    std::uint64_t errors = 0;
    for (int i = 0; i < 100; ++i) {
        std::vector<std::uint8_t> pair{bits.bit(), bits.bit()};
        auto symbols = map_bits(BitSequence(pair), Modulation::qpsk);
        const double re = noise.gaussian(), im = noise.gaussian();
        symbols[0] += .5 * Sample(re, im);
        auto decoded = hard_demodulate(symbols, Modulation::qpsk);
        errors += pair[0] != decoded.bits()[0];
        errors += pair[1] != decoded.bits()[1];
    }
    EXPECT_EQ(run.results()[0].bit_errors, errors);
}
} // namespace
