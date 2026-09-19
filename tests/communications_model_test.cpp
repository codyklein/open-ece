#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <openece/communications/model.hpp>
using namespace openece::communications;
namespace {
template <class F> void expect_error(F&& f, ErrorCode code) {
    try {
        f();
        FAIL() << "Expected communications error";
    } catch (const Error& e) {
        EXPECT_EQ(e.code(), code);
    }
}
TEST(CommunicationsModel, BitSequenceOwnsAndValidatesBytes) {
    std::vector<std::uint8_t> v{0, 1, 0};
    BitSequence b(v);
    v[0] = 1;
    EXPECT_EQ(b.bits()[0], 0);
    auto copy = b;
    EXPECT_EQ(copy.size(), 3U);
    expect_error([] { BitSequence invalid({0, 2}); }, ErrorCode::invalid_bits);
    expect_error([] { BitSequence invalid({255}); }, ErrorCode::invalid_bits);
    expect_error([] { BitSequence invalid({}); }, ErrorCode::resource_limit);
    expect_error([] { BitSequence invalid(std::vector<std::uint8_t>(limits::frame_bits + 1)); },
                 ErrorCode::resource_limit);
    EXPECT_EQ(BitSequence(std::vector<std::uint8_t>(limits::frame_bits)).size(),
              limits::frame_bits);
}
TEST(CommunicationsModel, BpskExactMappingAndRoundTrip) {
    BitSequence b({0, 1, 1, 0});
    auto s = map_bits(b, Modulation::bpsk);
    EXPECT_EQ(s, (std::vector<Sample>{{1, 0}, {-1, 0}, {-1, 0}, {1, 0}}));
    auto decoded = hard_demodulate(s, Modulation::bpsk);
    EXPECT_EQ(std::vector(decoded.bits().begin(), decoded.bits().end()),
              (std::vector<std::uint8_t>{0, 1, 1, 0}));
}
TEST(CommunicationsModel, QpskGrayMappingBitOrderAndUnitEnergy) {
    auto s = map_bits(BitSequence({0, 0, 0, 1, 1, 1, 1, 0}), Modulation::qpsk);
    const double a = 1 / std::sqrt(2.0);
    std::vector<Sample> expected{{a, a}, {a, -a}, {-a, -a}, {-a, a}};
    ASSERT_EQ(s.size(), expected.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        EXPECT_NEAR(std::abs(s[i] - expected[i]), 0, 1e-15);
        EXPECT_NEAR(std::norm(s[i]), 1, 3e-16);
    }
    auto b = hard_demodulate(s, Modulation::qpsk);
    EXPECT_EQ(std::vector(b.bits().begin(), b.bits().end()),
              (std::vector<std::uint8_t>{0, 0, 0, 1, 1, 1, 1, 0}));
}
TEST(CommunicationsModel, RejectsOddQpskRatherThanPadding) {
    for (auto count : {1U, 3U, 17U}) {
        expect_error(
            [&] {
                (void)map_bits(BitSequence(std::vector<std::uint8_t>(count)), Modulation::qpsk);
            },
            ErrorCode::incompatible_length);
        expect_error([&] { (void)validate_frame(count, {Modulation::qpsk, 1000, 16}); },
                     ErrorCode::incompatible_length);
    }
}
TEST(CommunicationsModel, DecisionsUseSignsAndAssignSignedZerosToZero) {
    std::vector<Sample> s{{0, -0.0}, {-0.0, 0}, {-1e-300, 1e-300}, {1e-300, -1e-300}};
    auto q = hard_demodulate(s, Modulation::qpsk);
    EXPECT_EQ(std::vector(q.bits().begin(), q.bits().end()),
              (std::vector<std::uint8_t>{0, 0, 0, 0, 1, 0, 0, 1}));
    auto b = hard_demodulate(s, Modulation::bpsk);
    EXPECT_EQ(std::vector(b.bits().begin(), b.bits().end()),
              (std::vector<std::uint8_t>{0, 0, 1, 0}));
}
TEST(CommunicationsModel, ExhaustiveShortBitRoundTrips) {
    for (unsigned word = 0; word < 256; ++word) {
        std::vector<std::uint8_t> bits;
        for (unsigned bit = 0; bit < 8; ++bit)
            bits.push_back(static_cast<std::uint8_t>((word >> bit) & 1));
        for (auto m : {Modulation::bpsk, Modulation::qpsk}) {
            auto result = hard_demodulate(map_bits(BitSequence(bits), m), m);
            EXPECT_EQ(std::vector(result.bits().begin(), result.bits().end()), bits);
        }
    }
}
TEST(CommunicationsModel, UnknownModulationsAndNonfiniteDecisionsAreRejected) {
    const auto bad = static_cast<Modulation>(42);
    expect_error([&] { (void)bits_per_symbol(bad); }, ErrorCode::invalid_value);
    expect_error([&] { (void)map_bits(BitSequence({0}), bad); }, ErrorCode::invalid_value);
    for (double value :
         {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        for (auto s : {Sample(value, 0), Sample(0, value)}) {
            std::vector<Sample> input{s};
            expect_error([&] { (void)hard_demodulate(input, Modulation::bpsk); },
                         ErrorCode::invalid_value);
        }
    }
    expect_error([] { (void)hard_demodulate({}, Modulation::bpsk); }, ErrorCode::resource_limit);
    std::vector<Sample> large(limits::frame_bits / 2 + 1);
    expect_error([&] { (void)hard_demodulate(large, Modulation::qpsk); },
                 ErrorCode::resource_limit);
}
TEST(CommunicationsModel, BasebandOwnershipAndTimeAxis) {
    std::vector<Sample> s{{1, 2}, {3, 4}, {5, 6}};
    BasebandSignal signal(s, 4);
    s[0] = {};
    EXPECT_EQ(signal.samples()[0], Sample(1, 2));
    EXPECT_DOUBLE_EQ(signal.duration_seconds(), .75);
    EXPECT_DOUBLE_EQ(signal.time_seconds(0), 0);
    EXPECT_DOUBLE_EQ(signal.time_seconds(2), .5);
    EXPECT_THROW(signal.time_seconds(3), std::out_of_range);
    auto copy = signal;
    EXPECT_EQ(copy.samples()[2], Sample(5, 6));
}
TEST(CommunicationsModel, BasebandRejectsInvalidRecordsAndRates) {
    expect_error([] { BasebandSignal s({}, 1); }, ErrorCode::resource_limit);
    expect_error([] { BasebandSignal s(std::vector<Sample>(limits::waveform_samples + 1), 1); },
                 ErrorCode::resource_limit);
    for (double rate :
         {0.0, -1.0, std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::denorm_min()})
        expect_error([&] { BasebandSignal s({{1, 0}}, rate); }, ErrorCode::invalid_value);
    for (auto sample : {Sample(INFINITY, 0), Sample(0, NAN)})
        expect_error([&] { BasebandSignal s({sample}, 1); }, ErrorCode::invalid_value);
}
TEST(CommunicationsModel, FrameLimitsCheckProductsBeforeAllocation) {
    EXPECT_EQ(validate_frame(limits::frame_bits, {Modulation::bpsk, 1, 1}),
              limits::waveform_samples);
    EXPECT_EQ(validate_frame(limits::frame_bits, {Modulation::qpsk, limits::symbol_rate_max, 2}),
              limits::waveform_samples);
    expect_error([] { (void)validate_frame(limits::frame_bits, {Modulation::bpsk, 1000, 2}); },
                 ErrorCode::resource_limit);
    expect_error([] { (void)validate_frame(std::numeric_limits<std::size_t>::max(), {}); },
                 ErrorCode::resource_limit);
    for (auto l :
         {std::size_t{0}, limits::samples_per_symbol + 1, std::numeric_limits<std::size_t>::max()})
        expect_error([&] { (void)validate_frame(2, {Modulation::qpsk, 1000, l}); },
                     ErrorCode::resource_limit);
    for (double rate : std::vector<double>{0.0, -1.0, .5, 1e10, INFINITY, NAN})
        expect_error([&] { (void)validate_frame(2, {Modulation::qpsk, rate, 1}); },
                     ErrorCode::invalid_value);
}
} // namespace
