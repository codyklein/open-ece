#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <openece/circuits/ac/sweep.hpp>
using namespace openece::circuits;
namespace ac = openece::circuits::ac;
namespace {
ac::CircuitDefinition rc() {
    return {{{{0}, "g"}, {{1}, "in"}, {{2}, "out"}},
            NodeId{0},
            {ac::VoltageSource{{1}, "V", {1}, {0}, {3, 4}}, Resistor{{2}, "R", {1}, {2}, 1000},
             ac::Capacitor{{3}, "C", {2}, {0}, 1e-6}}};
}
template <class F> void error(F&& action, ErrorCode code) {
    try {
        action();
        FAIL() << "Expected CircuitError";
    } catch (const CircuitError& e) {
        EXPECT_EQ(e.code(), code) << e.what();
    }
}
} // namespace
TEST(AcSweep, InclusiveDeterministicLinearAndLogarithmicGrids) {
    const ac::Circuit c(rc());
    auto linear = ac::frequency_grid(c, {1, 5, 5, ac::FrequencySpacing::linear});
    EXPECT_EQ(linear, (std::vector<double>{1, 2, 3, 4, 5}));
    auto log = ac::frequency_grid(c, {1, 10000, 5});
    for (std::size_t i = 0; i < log.size(); ++i)
        EXPECT_NEAR(log[i], std::pow(10., static_cast<double>(i)), log[i] * 1e-14);
    EXPECT_EQ(log, ac::frequency_grid(c, {1, 10000, 5}));
    for (auto spacing : {ac::FrequencySpacing::linear, ac::FrequencySpacing::logarithmic}) {
        const auto endpoints = ac::frequency_grid(c, {.123456789123, 123456.789123, 2, spacing});
        EXPECT_EQ(endpoints.front(), .123456789123);
        EXPECT_EQ(endpoints.back(), 123456.789123);
        const auto grid =
            ac::frequency_grid(c, {ac::limits::frequency_min, ac::limits::frequency_max,
                                   ac::limits::sweep_points, spacing});
        EXPECT_EQ(grid.size(), ac::limits::sweep_points);
        EXPECT_EQ(grid.front(), ac::limits::frequency_min);
        EXPECT_EQ(grid.back(), ac::limits::frequency_max);
        for (std::size_t i = 1; i < grid.size(); ++i)
            EXPECT_GT(grid[i], grid[i - 1]);
    }
}
TEST(AcSweep, RejectsMalformedOrUnrepresentableGrids) {
    const ac::Circuit c(rc());
    for (auto count : {std::size_t{0}, std::size_t{1}, ac::limits::sweep_points + 1,
                       std::numeric_limits<std::size_t>::max()})
        error([&] { (void)ac::frequency_grid(c, {1, 100, count}); }, ErrorCode::resource_limit);
    for (double f :
         {0., -1., ac::limits::frequency_min / 2, std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN()})
        error([&] { (void)ac::frequency_grid(c, {f, 100, 3}); }, ErrorCode::invalid_value);
    for (double f :
         {1., .5, ac::limits::frequency_max * 2, std::numeric_limits<double>::quiet_NaN()})
        error([&] { (void)ac::frequency_grid(c, {1, f, 3}); }, ErrorCode::invalid_value);
    error([&] { (void)ac::frequency_grid(c, {1, 2, 3, static_cast<ac::FrequencySpacing>(42)}); },
          ErrorCode::invalid_value);
    for (auto spacing : {ac::FrequencySpacing::linear, ac::FrequencySpacing::logarithmic})
        error([&] { (void)ac::frequency_grid(c, {1, std::nextafter(1., 2.), 4, spacing}); },
              ErrorCode::invalid_value);
}
TEST(AcSweep, AggregateWorkLimitIsCheckedBeforeAllocatingTheGrid) {
    ac::CircuitDefinition d{{{{0}, "g"}}, NodeId{0}, {}};
    for (std::size_t i = 1; i < ac::limits::nodes; ++i)
        d.nodes.push_back({{static_cast<std::uint32_t>(i)}, "n" + std::to_string(i)});
    for (std::size_t i = 0; i < ac::limits::voltage_sources; ++i)
        d.components.push_back(ac::VoltageSource{
            {static_cast<std::uint32_t>(i)}, "V" + std::to_string(i), {1}, {0}, {1, 0}});
    const ac::Circuit c(d);
    const auto k = static_cast<std::uint64_t>(ac::limits::unknowns);
    const auto maximum = static_cast<std::size_t>(ac::limits::sweep_work / (k * k * k));
    EXPECT_EQ(ac::frequency_grid(c, {1, 100, maximum}).size(), maximum);
    error([&] { (void)ac::frequency_grid(c, {1, 100, maximum + 1}); }, ErrorCode::resource_limit);
    const ac::Circuit ground({{{{0}, "g"}}, NodeId{0}, {}});
    EXPECT_EQ(ac::frequency_grid(ground, {1, 100, ac::limits::sweep_points}).size(),
              ac::limits::sweep_points);
}
TEST(AcSweep, EveryRequestedFrequencyRetainsItsSolutionOrStructuredError) {
    constexpr double resonant = 1;
    constexpr double reactive = 1 / (2 * std::numbers::pi);
    // The grid contains exactly 1 Hz; omega*C and omega*L are exactly 1 here.
    const ac::Circuit c(
        {{{{0}, "g"}, {{1}, "v"}},
         NodeId{0},
         {ac::Capacitor{{1}, "C", {1}, {0}, reactive}, ac::Inductor{{2}, "L", {1}, {0}, reactive},
          ac::CurrentSource{{3}, "I", {0}, {1}, {1, 0}}}});
    const ac::SweepSpec spec{resonant / 2, resonant * 1.5, 3, ac::FrequencySpacing::linear};
    const auto grid = ac::frequency_grid(c, spec);
    const auto sweep = ac::sweep_ac(c, spec);
    ASSERT_EQ(sweep.points.size(), grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        EXPECT_EQ(sweep.points[i].frequency_hz, grid[i]);
        if (i == 1) {
            ASSERT_TRUE(std::holds_alternative<CircuitError>(sweep.points[i].result));
            EXPECT_EQ(std::get<CircuitError>(sweep.points[i].result).code(),
                      ErrorCode::rank_deficient);
        } else {
            ASSERT_TRUE(std::holds_alternative<ac::AcSolution>(sweep.points[i].result));
            EXPECT_EQ(std::get<ac::AcSolution>(sweep.points[i].result).frequency_hz, grid[i]);
        }
    }
    auto d = rc();
    d.nodes.push_back({{9}, "unused"});
    const auto failed = ac::sweep_ac(ac::Circuit(d), {1, 100, 4});
    ASSERT_EQ(failed.points.size(), 4u);
    for (const auto& p : failed.points) {
        const auto& e = std::get<CircuitError>(p.result);
        EXPECT_EQ(e.code(), ErrorCode::floating_reference);
        EXPECT_EQ(e.nodes(), (std::vector<NodeId>{{9}}));
        EXPECT_NE(std::string(e.what()).find("Floating"), std::string::npos);
    }
    const auto invalid = ac::solve_ac_point(c, -3);
    EXPECT_EQ(invalid.frequency_hz, -3);
    EXPECT_EQ(std::get<CircuitError>(invalid.result).code(), ErrorCode::invalid_value);
}
TEST(AcSweep, SweepEqualsIndividualSolvesAndOwnsResults) {
    auto d = rc();
    const ac::Circuit c(d);
    auto sweep = ac::sweep_ac(c, {1, 10000, 17});
    d.nodes.clear();
    d.components.clear();
    for (const auto& point : sweep.points) {
        const auto single = ac::solve_ac(c, point.frequency_hz);
        const auto& result = std::get<ac::AcSolution>(point.result);
        EXPECT_EQ(result.node_voltages[2].voltage_volts_rms,
                  single.node_voltages[2].voltage_volts_rms);
        EXPECT_EQ(result.quality.backward_error, single.quality.backward_error);
    }
}
TEST(AcTransfer, NormalizesMagnitudeAndPhaseWithoutChangingSources) {
    const auto d = rc();
    const ac::Circuit c(d);
    const double f = 1 / (2 * std::numbers::pi * 1000 * 1e-6);
    const auto s = ac::solve_ac(c, f);
    const auto h = ac::voltage_transfer(c, s, {{{2}, {0}}, {1}});
    EXPECT_NEAR(h.real(), .5, 1e-12);
    EXPECT_NEAR(h.imag(), -.5, 1e-12);
    EXPECT_NEAR(ac::gain_magnitude_db(h), -3.010299956639812, 1e-12);
    ASSERT_TRUE(ac::wrapped_phase_degrees(h));
    EXPECT_NEAR(*ac::wrapped_phase_degrees(h), -45, 1e-12);
    const auto v = ac::probe_voltage(s, {{2}, {0}});
    EXPECT_NEAR(std::abs(v), 5 / std::sqrt(2.), 1e-12);
    EXPECT_EQ(std::get<ac::VoltageSource>(c.definition().components[0]).voltage_volts_rms,
              ac::Phasor(3, 4));
    EXPECT_EQ(ac::probe_voltage(s, {{0}, {2}}), -v);
    EXPECT_EQ(ac::probe_voltage(s, {{2}, {2}}), ac::Phasor{});
}
TEST(AcTransfer, RejectsNonzeroOtherVoltageAndCurrentSources) {
    auto d = rc();
    d.nodes.push_back({{3}, "extra"});
    d.components.push_back(ac::VoltageSource{{4}, "otherV", {3}, {0}, {}});
    d.components.push_back(ac::CurrentSource{{5}, "otherI", {0}, {2}, {}});
    const ac::VoltageTransfer request{{{2}, {0}}, {1}};
    EXPECT_NO_THROW(ac::validate_voltage_transfer(ac::Circuit(d), request));
    std::get<ac::VoltageSource>(d.components[3]).voltage_volts_rms = {0, 1e-12};
    error([&] { ac::validate_voltage_transfer(ac::Circuit(d), request); },
          ErrorCode::invalid_value);
    std::get<ac::VoltageSource>(d.components[3]).voltage_volts_rms = {};
    std::get<ac::CurrentSource>(d.components[4]).current_amperes_rms = {-1e-12, 0};
    error([&] { ac::validate_voltage_transfer(ac::Circuit(d), request); },
          ErrorCode::invalid_value);
    std::get<ac::CurrentSource>(d.components[4]).current_amperes_rms = {};
    std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = {};
    error([&] { ac::validate_voltage_transfer(ac::Circuit(d), request); },
          ErrorCode::invalid_value);
}
TEST(AcTransfer, ExplicitReferenceAndProbeIdsMustResolve) {
    const ac::Circuit c(rc());
    for (ComponentId id : {ComponentId{2}, ComponentId{99}})
        error([&] { ac::validate_voltage_transfer(c, {{{2}, {0}}, id}); },
              ErrorCode::invalid_value);
    error([&] { ac::validate_voltage_transfer(c, {{{99}, {0}}, {1}}); },
          ErrorCode::invalid_terminal);
    const auto s = ac::solve_ac(c, 1000);
    error([&] { (void)ac::probe_voltage(s, {{2}, {99}}); }, ErrorCode::invalid_terminal);
    EXPECT_NO_THROW(ac::validate_voltage_transfer(c, {{{2}, {2}}, {1}}));
}
TEST(AcPresentation, WrappedPhaseUsesPositive180AndZeroIsUndefined) {
    EXPECT_FALSE(ac::wrapped_phase_degrees({}));
    EXPECT_FALSE(ac::wrapped_phase_degrees({-0., -0.}));
    EXPECT_EQ(ac::wrapped_phase_degrees({-1, 0}), 180);
    EXPECT_EQ(ac::wrapped_phase_degrees({-1, -0.}), 180);
    EXPECT_EQ(ac::wrapped_phase_degrees({0, 1}), 90);
    EXPECT_EQ(ac::wrapped_phase_degrees({0, -1}), -90);
    for (int degree = -720; degree <= 720; ++degree) {
        const auto phase =
            ac::wrapped_phase_degrees(std::polar(1., degree * std::numbers::pi / 180));
        ASSERT_TRUE(phase);
        EXPECT_GT(*phase, -180);
        EXPECT_LE(*phase, 180);
    }
    EXPECT_TRUE(ac::wrapped_phase_degrees({1e-300, 0})); // Small is not mathematically zero.
    error([] { (void)ac::wrapped_phase_degrees({NAN, 0}); }, ErrorCode::invalid_value);
}
TEST(AcPresentation, GainFloorIsFiniteAndDoesNotChangeComplexResults) {
    EXPECT_EQ(ac::gain_magnitude_db({}), ac::gain_display_floor_db);
    EXPECT_EQ(ac::gain_magnitude_db({1e-300, 0}), ac::gain_display_floor_db);
    EXPECT_EQ(ac::gain_magnitude_db({1e-12, 0}), ac::gain_display_floor_db);
    EXPECT_EQ(ac::gain_magnitude_db({1, 0}), 0);
    EXPECT_NEAR(ac::gain_magnitude_db({0, 10}), 20, 1e-12);
    error([] { (void)ac::gain_magnitude_db({INFINITY, 0}); }, ErrorCode::invalid_value);
}
