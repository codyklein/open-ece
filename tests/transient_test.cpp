#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <numeric>
#include <openece/circuits/transient/analysis.hpp>
using namespace openece::circuits;
namespace tr = openece::circuits::transient;
namespace {
tr::CircuitDefinition rc(double source = 10, double initial_cap = 1e-6) {
    return {{{{0}, "ground"}, {{1}, "supply"}, {{2}, "output"}},
            NodeId{0},
            {tr::VoltageSource{{10}, "V", {1}, {0}, {source, {}}},
             Resistor{{11}, "R", {1}, {2}, 1000}, tr::Capacitor{{12}, "C", {2}, {0}, initial_cap}}};
}
tr::Request specified(double stop, double step, std::vector<tr::CapacitorVoltage> c = {},
                      std::vector<tr::InductorCurrent> l = {}) {
    tr::Request r;
    r.stop_seconds = stop;
    r.maximum_step_seconds = step;
    r.initial = {tr::Initialization::specified_storage, std::move(c), std::move(l)};
    return r;
}
tr::Request rc_request(double stop = .005, double step = .0001, double initial = 0) {
    auto r = specified(stop, step, {{{12}, initial}});
    r.voltages = {{{2}, {0}}, {{0}, {2}}};
    r.currents = {{{10}}, {{11}}, {{12}}};
    return r;
}
tr::Result run(tr::CircuitDefinition d, tr::Request r) {
    tr::Simulation s(tr::Circuit(std::move(d)), std::move(r));
    const auto status = s.run();
    const auto result = s.snapshot();
    EXPECT_EQ(status, tr::Status::complete)
        << (result.failure ? result.failure->error.what() : "") << " at "
        << (result.failure ? result.failure->time_seconds : 0);
    return s.snapshot();
}
void error(const std::function<void()>& f, tr::ErrorCode code) {
    try {
        f();
        FAIL() << "Expected transient error";
    } catch (const tr::Error& e) {
        EXPECT_EQ(e.code(), code) << e.what();
    }
}
double node(const tr::State& s, NodeId id) {
    return std::find_if(s.node_voltages.begin(), s.node_voltages.end(),
                        [&](auto v) { return v.node == id; })
        ->voltage_volts;
}
double current(const tr::State& s, ComponentId id) {
    return std::find_if(s.branch_currents.begin(), s.branch_currents.end(),
                        [&](auto v) { return v.component == id; })
        ->current_amperes;
}
tr::CircuitDefinition rlc(double r) {
    return {{{{0}, "g"}, {{1}, "supply"}, {{2}, "a"}, {{3}, "output"}},
            NodeId{0},
            {tr::VoltageSource{{10}, "V", {1}, {0}, {1, {}}}, Resistor{{11}, "R", {1}, {2}, r},
             tr::Inductor{{12}, "L", {2}, {3}, .01}, tr::Capacitor{{13}, "C", {3}, {0}, 1e-6}}};
}
std::pair<double, double> rlc_exact(double resistance, double t) {
    const double w = 10000, alpha = resistance / .02;
    if (alpha < w) {
        double d = std::sqrt(w * w - alpha * alpha);
        return {1 - std::exp(-alpha * t) * (std::cos(d * t) + alpha / d * std::sin(d * t)),
                1e-6 * w * w / d * std::exp(-alpha * t) * std::sin(d * t)};
    }
    if (alpha == w)
        return {1 - std::exp(-w * t) * (1 + w * t), 1e-6 * w * w * t * std::exp(-w * t)};
    double a = -alpha + std::sqrt(alpha * alpha - w * w),
           b = -alpha - std::sqrt(alpha * alpha - w * w);
    return {1 - (b * std::exp(a * t) - a * std::exp(b * t)) / (b - a),
            1e-6 * a * b * (std::exp(b * t) - std::exp(a * t)) / (b - a)};
}
// Independent tableau: a current unknown for EVERY component, constitutive rows
// v_C - (h/C)i_C = v_old, and nodal KCL; no admittance/history-current stamping.
std::vector<long double> reference_step(const tr::CircuitDefinition& d, const tr::State& previous,
                                        double t, double h) {
    const std::size_t nv = d.nodes.size() - 1, k = nv + d.components.size();
    std::vector<std::vector<long double>> a(k, std::vector<long double>(k + 1));
    auto ni = [&](NodeId id) {
        std::size_t j = 0;
        for (auto n : d.nodes) {
            if (n.id == *d.ground)
                continue;
            if (n.id == id)
                return j;
            ++j;
        }
        return k;
    };
    for (std::size_t i = 0; i < d.components.size(); ++i)
        std::visit(
            [&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                auto pos = ni(p.positive), neg = ni(p.negative), row = nv + i;
                if (pos != k)
                    a[pos][row] += 1;
                if (neg != k)
                    a[neg][row] -= 1;
                if constexpr (std::is_same_v<T, tr::CurrentSource>) {
                    a[row][row] = 1;
                    a[row][k] = tr::source_value(p.current_amperes, t);
                } else {
                    if (pos != k)
                        a[row][pos] = 1;
                    if (neg != k)
                        a[row][neg] = -1;
                    if constexpr (std::is_same_v<T, Resistor>)
                        a[row][row] = -p.resistance_ohms;
                    else if constexpr (std::is_same_v<T, tr::Capacitor>) {
                        a[row][row] = -static_cast<long double>(h) / p.capacitance_farads;
                        a[row][k] = node(previous, p.positive) - node(previous, p.negative);
                    } else if constexpr (std::is_same_v<T, tr::Inductor>) {
                        a[row][row] = -static_cast<long double>(p.inductance_henries) / h;
                        a[row][k] = a[row][row] * current(previous, p.id);
                    } else
                        a[row][k] = tr::source_value(p.voltage_volts, t);
                }
            },
            d.components[i]);
    for (std::size_t j = 0; j < k; ++j) {
        std::size_t pivot = j;
        for (std::size_t i = j + 1; i < k; ++i)
            if (std::abs(a[i][j]) > std::abs(a[pivot][j]))
                pivot = i;
        if (a[pivot][j] == 0)
            throw std::runtime_error("Singular reference tableau");
        std::swap(a[j], a[pivot]);
        auto scale = a[j][j];
        for (std::size_t n = j; n <= k; ++n)
            a[j][n] /= scale;
        for (std::size_t i = 0; i < k; ++i)
            if (i != j) {
                auto factor = a[i][j];
                for (std::size_t n = j; n <= k; ++n)
                    a[i][n] -= factor * a[j][n];
            }
    }
    std::vector<long double> x(k);
    for (std::size_t i = 0; i < k; ++i)
        x[i] = a[i][k];
    return x;
}
} // namespace
TEST(TransientModel, StructuralValidationAndOwnedSnapshots) {
    auto d = rc();
    tr::Circuit c(d);
    d.nodes[1].name = "changed";
    EXPECT_EQ(c.definition().nodes[1].name, "supply");
    auto invalid = rc();
    invalid.ground.reset();
    error([&] { tr::Circuit x(invalid); }, tr::ErrorCode::invalid_definition);
    invalid = rc();
    invalid.nodes[1].id = {0};
    error([&] { tr::Circuit x(invalid); }, tr::ErrorCode::invalid_definition);
    invalid = rc();
    std::get<Resistor>(invalid.components[1]).negative = {99};
    error([&] { tr::Circuit x(invalid); }, tr::ErrorCode::invalid_definition);
    invalid = rc();
    std::get<Resistor>(invalid.components[1]).resistance_ohms = 0;
    error([&] { tr::Circuit x(invalid); }, tr::ErrorCode::invalid_definition);
    invalid = rc();
    std::get<tr::Capacitor>(invalid.components[2]).capacitance_farads =
        std::numeric_limits<double>::infinity();
    error([&] { tr::Circuit x(invalid); }, tr::ErrorCode::invalid_definition);
    invalid = rc();
    invalid.nodes[1].name = "";
    error([&] { tr::Circuit x(invalid); }, tr::ErrorCode::invalid_definition);
    invalid = rc();
    std::get<Resistor>(invalid.components[1]).id = {10};
    error([&] { tr::Circuit x(invalid); }, tr::ErrorCode::invalid_definition);
}
TEST(TransientModel, SourceSemanticsAndValidation) {
    tr::Source s{2, {{1, 4}, {2, -1}}};
    EXPECT_EQ(tr::source_value(s, 0), 2);
    EXPECT_EQ(tr::source_value(s, 1, true), 2);
    EXPECT_EQ(tr::source_value(s, 1), 4);
    EXPECT_EQ(tr::source_value(s, 4), -1);
    s.interpolation = tr::Interpolation::linear;
    EXPECT_EQ(tr::source_value(s, .5), 3);
    EXPECT_EQ(tr::source_value(s, 1, true), 4);
    EXPECT_EQ(tr::source_derivative(s, 0), 2);
    EXPECT_EQ(tr::source_derivative(s, 1), -5);
    EXPECT_EQ(tr::source_derivative(s, 2), 0);
    for (auto points : {std::vector<tr::SourcePoint>{{0, 1}},
                        {{1, 1}, {1, 2}},
                        {{2, 1}, {1, 2}},
                        {{1, std::numeric_limits<double>::quiet_NaN()}}}) {
        auto d = rc();
        std::get<tr::VoltageSource>(d.components[0]).voltage_volts.points = points;
        error([&] { tr::Circuit c(d); }, tr::ErrorCode::invalid_definition);
    }
    const tr::Source small_endpoint{1e9, {{1, 1e-20}}, tr::Interpolation::linear};
    EXPECT_EQ(tr::source_value(small_endpoint, 1, true), 1e-20);
    EXPECT_EQ(tr::source_value(small_endpoint, 1), 1e-20);
    auto d = rc();
    std::get<tr::VoltageSource>(d.components[0]).voltage_volts.interpolation =
        static_cast<tr::Interpolation>(99);
    error([&] { tr::Circuit c(d); }, tr::ErrorCode::invalid_definition);
}
TEST(Transient, RcChargingAndDischargingAnalyticalAndSignConventions) {
    for (auto [supply, initial] : {std::pair{10., 0.}, std::pair{0., 10.}}) {
        auto result = run(rc(supply), rc_request(.005, .00001, initial));
        for (std::size_t i = 0; i < result.times.size(); ++i) {
            double t = result.times[i].seconds,
                   expected = supply + (initial - supply) * std::exp(-t / .001);
            EXPECT_NEAR(result.voltages[0][i], expected, .02);
            EXPECT_DOUBLE_EQ(result.voltages[1][i], -result.voltages[0][i]);
            EXPECT_NEAR(result.currents[0][i], -result.currents[1][i], 1e-12);
            EXPECT_NEAR(result.currents[1][i], result.currents[2][i], 1e-12);
        }
        EXPECT_EQ(result.times.front().seconds, 0);
        EXPECT_EQ(result.times.back().seconds, .005);
        EXPECT_EQ(result.latest.node_voltages.front().voltage_volts, 0);
    }
    auto r = run(rc(), rc_request());
    EXPECT_NEAR(r.currents[0].front(), -.01, 1e-14);
}
TEST(Transient, RcBackwardEulerRecurrenceAndStepConvergence) {
    double previous_error = 1;
    for (double h : {.0001, .00005, .000025}) {
        auto r = run(rc(1), rc_request(.001, h));
        double product = 1;
        for (std::size_t i = 1; i < r.times.size(); ++i) {
            product /= 1 + (r.times[i].seconds - r.times[i - 1].seconds) / .001;
            EXPECT_NEAR(r.voltages[0][i], 1 - product, 1e-12);
        }
        double error_value = std::abs(r.voltages[0].back() - (1 - std::exp(-1.)));
        EXPECT_LT(error_value, previous_error * .6);
        previous_error = error_value;
    }
}
TEST(Transient, RlAnalyticalAndNonzeroEnergy) {
    tr::CircuitDefinition d{{{{0}, "g"}, {{1}, "supply"}, {{2}, "out"}},
                            NodeId{0},
                            {tr::VoltageSource{{1}, "V", {1}, {0}, {10, {}}},
                             Resistor{{2}, "R", {1}, {2}, 1000},
                             tr::Inductor{{3}, "L", {2}, {0}, 1}}};
    for (double initial : {0., .02}) {
        auto req = specified(.005, .00001, {}, {{{3}, initial}});
        req.currents = {{{3}}};
        req.voltages = {{{2}, {0}}};
        auto r = run(d, req);
        for (std::size_t i = 0; i < r.times.size(); ++i) {
            double expected = .01 + (initial - .01) * std::exp(-r.times[i].seconds / .001);
            EXPECT_NEAR(r.currents[0][i], expected, .00002);
            EXPECT_NEAR(r.voltages[0][i], 10 - 1000 * r.currents[0][i], 1e-10);
        }
    }
}
TEST(Transient, RlcAllThreeDampingRegimesAndConvergence) {
    for (double resistance : {20., 200., 400.}) {
        double previous_error = 1;
        for (double h : {2e-6, 1e-6, 5e-7}) {
            auto req = specified(.001, h, {{{13}, 0}}, {{{12}, 0}});
            req.voltages = {{{3}, {0}}};
            req.currents = {{{12}}};
            auto r = run(rlc(resistance), req);
            double max_error = 0;
            for (std::size_t i = 0; i < r.times.size(); ++i) {
                auto expected = rlc_exact(resistance, r.times[i].seconds);
                max_error = std::max(max_error, std::abs(r.voltages[0][i] - expected.first));
                EXPECT_NEAR(r.currents[0][i], expected.second, .002);
            }
            EXPECT_LT(max_error, .09);
            EXPECT_LT(max_error, previous_error * .65);
            previous_error = max_error;
        }
    }
}
TEST(Transient, LosslessLcShowsDocumentedNumericalDamping) {
    tr::CircuitDefinition d{
        {{{0}, "g"}, {{1}, "node"}},
        NodeId{0},
        {tr::Capacitor{{1}, "C", {1}, {0}, 1e-6}, tr::Inductor{{2}, "L", {1}, {0}, .01}}};
    auto req = specified(.001, 1e-6, {{{1}, 1}}, {{{2}, 0}});
    req.voltages = {{{1}, {0}}};
    req.currents = {{{2}}};
    auto r = run(d, req);
    double energy = .5e-6;
    for (std::size_t i = 1; i < r.times.size(); ++i) {
        double next = .5e-6 * r.voltages[0][i] * r.voltages[0][i] +
                      .005 * r.currents[0][i] * r.currents[0][i];
        EXPECT_LT(next, energy + 1e-18);
        energy = next;
    }
    EXPECT_LT(energy, .5e-6);
    EXPECT_GT(energy, .4e-6);
}
TEST(Transient, OperatingPointAndStrictSpecifiedStateContract) {
    tr::Request r;
    r.stop_seconds = .001;
    r.maximum_step_seconds = .0001;
    r.voltages = {{{2}, {0}}};
    auto out = run(rc(), r);
    for (double v : out.voltages[0])
        EXPECT_NEAR(v, 10, 1e-11);
    auto invalid = rc_request();
    invalid.initial.capacitor_voltages.clear();
    error([&] { tr::Simulation s(tr::Circuit(rc()), invalid); }, tr::ErrorCode::invalid_request);
    invalid = rc_request();
    invalid.initial.capacitor_voltages.push_back({{12}, 0});
    error([&] { tr::Simulation s(tr::Circuit(rc()), invalid); }, tr::ErrorCode::invalid_request);
    invalid = rc_request();
    invalid.initial.capacitor_voltages[0].capacitor = {11};
    error([&] { tr::Simulation s(tr::Circuit(rc()), invalid); }, tr::ErrorCode::invalid_request);
    invalid = rc_request();
    invalid.initial.capacitor_voltages[0].voltage_volts = std::numeric_limits<double>::quiet_NaN();
    error([&] { tr::Simulation s(tr::Circuit(rc()), invalid); }, tr::ErrorCode::invalid_request);
    r.initial.capacitor_voltages = {{{12}, 0}};
    error([&] { tr::Simulation s(tr::Circuit(rc()), r); }, tr::ErrorCode::invalid_request);
}
TEST(Transient, ParallelCapacitorsHaveUniquePhysicalCurrents) {
    auto d = rc(1);
    d.components.push_back(tr::Capacitor{{13}, "C2", {2}, {0}, 2e-6});
    auto req = rc_request(.003, .00001);
    req.initial.capacitor_voltages.push_back({{13}, 0});
    req.currents = {{{12}}, {{13}}};
    auto r = run(d, req);
    for (std::size_t i = 0; i < r.times.size(); ++i) {
        EXPECT_NEAR(r.currents[1][i], 2 * r.currents[0][i], 1e-12);
        EXPECT_NEAR(r.voltages[0][i], 1 - std::exp(-r.times[i].seconds / .003), .001);
    }
}
TEST(Transient, LinearVoltageSourceClampedCapacitorAndInitialDerivative) {
    tr::CircuitDefinition d{
        {{{0}, "g"}, {{1}, "out"}},
        NodeId{0},
        {tr::VoltageSource{{1}, "ramp", {1}, {0}, {0, {{.001, 1}}, tr::Interpolation::linear}},
         tr::Capacitor{{2}, "C", {1}, {0}, 1e-6}}};
    auto req = specified(.001, .00007, {{{2}, 0}});
    req.voltages = {{{1}, {0}}};
    req.currents = {{{1}}, {{2}}};
    auto r = run(d, req);
    EXPECT_NEAR(r.currents[1].front(), .001, 1e-14);
    for (std::size_t i = 0; i < r.times.size(); ++i) {
        EXPECT_NEAR(r.voltages[0][i], r.times[i].seconds * 1000, 1e-13);
        if (r.times[i].side != tr::SampleSide::after_breakpoint) {
            EXPECT_NEAR(r.currents[1][i], .001, 1e-12);
        }
    }
    EXPECT_EQ(r.times.back().side, tr::SampleSide::after_breakpoint);
    EXPECT_NEAR(r.currents[1].back(), 0, 1e-14);
}
TEST(Transient, StepSourcesHaveBothSidesAndContinuousStorage) {
    auto d = rc(0);
    std::get<tr::VoltageSource>(d.components[0]).voltage_volts.points = {{.001, 10}, {.002, 0}};
    auto r = run(d, rc_request(.003, .00013));
    for (double time : {.001, .002}) {
        auto found =
            std::find_if(r.times.begin(), r.times.end(), [&](auto p) { return p.seconds == time; });
        auto i = static_cast<std::size_t>(found - r.times.begin());
        ASSERT_LT(i + 1, r.times.size());
        EXPECT_EQ(r.times[i].side, tr::SampleSide::before_breakpoint);
        EXPECT_EQ(r.times[i + 1].side, tr::SampleSide::after_breakpoint);
        EXPECT_EQ(r.times[i + 1].seconds, time);
        EXPECT_NEAR(r.voltages[0][i], r.voltages[0][i + 1], 1e-13);
    }
    // No new source excitation is applied to the interval preceding the first step.
    for (std::size_t i = 0; i < r.times.size() && r.times[i].seconds <= .001; ++i)
        EXPECT_NEAR(r.voltages[0][i], 0, 1e-14);
}
TEST(Transient, SimultaneousChangesAreReconciledAsOneBatch) {
    tr::CircuitDefinition d{{{{0}, "g"}, {{1}, "a"}, {{2}, "b"}},
                            NodeId{0},
                            {tr::VoltageSource{{1}, "a", {1}, {0}, {0, {{.001, 1}}}},
                             tr::VoltageSource{{2}, "b", {2}, {0}, {0, {{.001, 1}}}},
                             tr::Capacitor{{3}, "C", {1}, {2}, 1e-6}}};
    auto req = specified(.002, .0002, {{{3}, 0}});
    req.voltages = {{{1}, {2}}, {{1}, {0}}};
    auto r = run(d, req);
    EXPECT_EQ(r.voltages[1].back(), 1);
    for (double v : r.voltages[0])
        EXPECT_EQ(v, 0);
}
TEST(Transient, CurrentSourceBreakpointPreservesInductorCurrent) {
    tr::CircuitDefinition d{{{{0}, "g"}, {{1}, "out"}},
                            NodeId{0},
                            {tr::CurrentSource{{1}, "I", {0}, {1}, {0, {{.001, .002}}}},
                             Resistor{{2}, "R", {1}, {0}, 1000},
                             tr::Inductor{{3}, "L", {1}, {0}, 1}}};
    auto req = specified(.003, .00007, {}, {{{3}, 0}});
    req.voltages = {{{1}, {0}}};
    req.currents = {{{3}}};
    auto r = run(d, req);
    auto it =
        std::find_if(r.times.begin(), r.times.end(), [](auto p) { return p.seconds == .001; });
    auto i = static_cast<std::size_t>(it - r.times.begin());
    EXPECT_DOUBLE_EQ(r.currents[0][i], r.currents[0][i + 1]);
    EXPECT_NEAR(r.voltages[0][i + 1], 2, 1e-12);
    EXPECT_NEAR(r.currents[0].back(), .002 * (1 - std::exp(-2.)), .000025);
}
TEST(Transient, ImpossibleImpulseFailsWithoutAppendingOrAdvancing) {
    tr::CircuitDefinition d{{{{0}, "g"}, {{1}, "out"}},
                            NodeId{0},
                            {tr::VoltageSource{{1}, "V", {1}, {0}, {0, {{.05, 1}}}},
                             tr::Capacitor{{2}, "C", {1}, {0}, 1e-6}}};
    auto req = specified(.1, .04, {{{2}, 0}});
    req.voltages = {{{1}, {0}}};
    tr::Simulation s(tr::Circuit(d), req);
    EXPECT_EQ(s.step(), tr::Status::running);
    auto old = s.snapshot();
    EXPECT_EQ(s.step(), tr::Status::failed);
    auto failed = s.snapshot();
    ASSERT_TRUE(failed.failure);
    EXPECT_EQ(failed.failure->error.code(), tr::ErrorCode::contradictory_constraint);
    EXPECT_EQ(failed.failure->time_seconds, .05);
    EXPECT_EQ(failed.current_time_seconds, old.current_time_seconds);
    EXPECT_EQ(failed.voltages, old.voltages);
    EXPECT_EQ(failed.times.size(), old.times.size());
    EXPECT_EQ(s.run(), tr::Status::failed);
}
TEST(Transient, ConstraintsFloatingAndHigherIndexDiagnostics) {
    auto d = rc();
    d.nodes.push_back({{9}, "island"});
    d.components.push_back(tr::CurrentSource{{19}, "I", {0}, {9}, {0, {}}});
    error([&] { tr::Simulation s(tr::Circuit(d), rc_request()); },
          tr::ErrorCode::floating_reference);
    d = rc();
    d.components.push_back(tr::VoltageSource{{19}, "V2", {1}, {0}, {10, {}}});
    error([&] { tr::Simulation s(tr::Circuit(d), rc_request()); },
          tr::ErrorCode::nonunique_source_current);
    std::get<tr::VoltageSource>(d.components.back()).voltage_volts.initial = 9;
    error([&] { tr::Simulation s(tr::Circuit(d), rc_request()); },
          tr::ErrorCode::contradictory_constraint);
    tr::CircuitDefinition cutset{
        {{{0}, "g"}, {{1}, "out"}},
        NodeId{0},
        {tr::Inductor{{1}, "L", {1}, {0}, 1}, tr::CurrentSource{{2}, "I", {0}, {1}, {1, {}}}}};
    error([&] { tr::Simulation s(tr::Circuit(cutset), specified(.1, .01, {}, {{{1}, 1}})); },
          tr::ErrorCode::unsupported_initialization);
    d = rc();
    std::get<tr::Capacitor>(d.components.back()).positive = {1};
    error([&] { tr::Simulation s(tr::Circuit(d), rc_request()); },
          tr::ErrorCode::contradictory_constraint);
}
TEST(Transient, IndependentBranchTableauForMixedNetworks) {
    for (int variant = 0; variant < 12; ++variant) {
        auto d = rlc(20 + static_cast<double>(variant) * 20);
        d.components.push_back(Resistor{{21}, "parallel", {3}, {0}, 2000 + variant * 100.});
        d.components.push_back(tr::CurrentSource{{22}, "extra", {0}, {2}, {.001, {}}});
        auto req = specified(.0004, .00001, {{{13}, .2}}, {{{12}, .003}});
        tr::Simulation s(tr::Circuit(d), req);
        while (s.snapshot().status != tr::Status::complete) {
            auto old = s.snapshot();
            ASSERT_NE(s.step(), tr::Status::failed);
            auto next = s.snapshot();
            auto ref = reference_step(d, old.latest, next.current_time_seconds,
                                      next.current_time_seconds - old.current_time_seconds);
            std::size_t j = 0;
            for (auto n : d.nodes) {
                if (n.id != *d.ground) {
                    EXPECT_NEAR(node(next.latest, n.id), static_cast<double>(ref[j++]), 1e-10);
                }
            }
            for (std::size_t i = 0; i < d.components.size(); ++i)
                EXPECT_NEAR(next.latest.branch_currents[i].current_amperes,
                            static_cast<double>(ref[d.nodes.size() - 1 + i]), 1e-11);
            EXPECT_LT(next.latest.quality.max_kcl_error_amperes, 1e-12);
        }
    }
}
TEST(Transient, DeclarationOrderProbesOrientationAndOwnedResults) {
    auto d = rc();
    auto req = rc_request();
    auto first = run(d, req);
    std::reverse(d.nodes.begin(), d.nodes.end());
    std::reverse(d.components.begin(), d.components.end());
    auto second = run(d, req);
    ASSERT_EQ(first.times.size(), second.times.size());
    for (std::size_t i = 0; i < first.times.size(); ++i)
        EXPECT_NEAR(first.voltages[0][i], second.voltages[0][i], 1e-12);
    EXPECT_EQ(second.latest.node_voltages[0].node, NodeId{2});
    EXPECT_EQ(second.latest.branch_currents[0].component, ComponentId{12});
    std::get<Resistor>(d.components[1]).positive = {2};
    std::get<Resistor>(d.components[1]).negative = {1};
    auto reversed = run(d, req);
    for (std::size_t i = 0; i < first.times.size(); ++i)
        EXPECT_NEAR(reversed.currents[1][i], -first.currents[1][i], 1e-12);
    tr::Simulation s(tr::Circuit(rc()), req);
    auto copy = s.snapshot();
    copy.latest.node_voltages[1].voltage_volts = 123;
    EXPECT_NE(s.snapshot().latest.node_voltages[1].voltage_volts, 123);
}
TEST(Transient, IncrementalRunCancellationAndTerminalBehavior) {
    tr::Simulation a(tr::Circuit(rc()), rc_request()), b(tr::Circuit(rc()), rc_request());
    while (b.step() == tr::Status::running) {
    };
    ASSERT_EQ(a.run(), tr::Status::complete);
    auto x = a.snapshot(), y = b.snapshot();
    EXPECT_EQ(x.voltages, y.voltages);
    EXPECT_EQ(x.currents, y.currents);
    ASSERT_EQ(x.times.size(), y.times.size());
    for (std::size_t i = 0; i < x.times.size(); ++i)
        EXPECT_EQ(x.times[i].seconds, y.times[i].seconds);
    EXPECT_EQ(a.step(), tr::Status::complete);
    a.cancel();
    EXPECT_EQ(a.snapshot().status, tr::Status::complete);
    tr::Simulation c(tr::Circuit(rc()), rc_request());
    c.step();
    auto prefix = c.snapshot();
    c.cancel();
    EXPECT_EQ(c.run(), tr::Status::cancelled);
    EXPECT_EQ(c.snapshot().voltages, prefix.voltages);
    EXPECT_EQ(c.snapshot().current_time_seconds, prefix.current_time_seconds);
    tr::Simulation immediate(tr::Circuit(rc()), rc_request());
    immediate.cancel();
    EXPECT_EQ(immediate.snapshot().times.size(), 1u);
    EXPECT_EQ(immediate.step(), tr::Status::cancelled);
}
TEST(Transient, TimeGridExactBoundariesAndResourceGuards) {
    auto d = rc();
    auto& source = std::get<tr::VoltageSource>(d.components[0]).voltage_volts;
    source.points = {{.00035, 1}, {.001, 2}, {.002, 3}};
    tr::Circuit c(d);
    auto req = rc_request(.001, .0001);
    auto grid = tr::time_grid(c, req);
    EXPECT_EQ(grid.front(), 0);
    EXPECT_EQ(grid.back(), .001);
    EXPECT_NE(std::find(grid.begin(), grid.end(), .00035), grid.end());
    for (std::size_t i = 1; i < grid.size(); ++i) {
        EXPECT_GT(grid[i], grid[i - 1]);
        EXPECT_LE(grid[i] - grid[i - 1], req.maximum_step_seconds);
    }
    req.maximum_step_seconds = 0;
    error([&] { tr::time_grid(c, req); }, tr::ErrorCode::invalid_request);
    req = rc_request();
    req.stop_seconds = std::numeric_limits<double>::infinity();
    error([&] { tr::time_grid(c, req); }, tr::ErrorCode::invalid_request);
    req = rc_request();
    req.maximum_step_seconds = std::numeric_limits<double>::denorm_min();
    error([&] { tr::time_grid(c, req); }, tr::ErrorCode::resource_limit);
    req = rc_request();
    req.voltages = {{{99}, {0}}};
    error([&] { tr::time_grid(c, req); }, tr::ErrorCode::invalid_request);
    req = rc_request();
    req.currents = {{{99}}};
    error([&] { tr::time_grid(c, req); }, tr::ErrorCode::invalid_request);
    req = rc_request();
    req.voltages.resize(tr::limits::probes + 1, {{1}, {0}});
    error([&] { tr::time_grid(c, req); }, tr::ErrorCode::resource_limit);
    req = rc_request(1, .00002);
    req.voltages.resize(tr::limits::probes, {{1}, {0}});
    req.currents.clear();
    error([&] { tr::time_grid(c, req); }, tr::ErrorCode::resource_limit);
    source.points.resize(tr::limits::source_points + 1);
    error([&] { tr::Circuit invalid(d); }, tr::ErrorCode::resource_limit);
}
TEST(Transient, FiniteValueAndConditioningFailuresAreNotRegularized) {
    auto d = rc();
    d.nodes.push_back({{4}, "weak"});
    d.nodes.push_back({{5}, "weaker"});
    d.components.push_back(Resistor{{20}, "weak ground", {4}, {0}, 1e12});
    d.components.push_back(Resistor{{21}, "link", {4}, {5}, 1});
    error([&] { tr::Simulation s(tr::Circuit(d), rc_request()); }, tr::ErrorCode::ill_conditioned);
    d = rc();
    std::get<tr::VoltageSource>(d.components[0]).voltage_volts.points = {
        {std::numeric_limits<double>::denorm_min(), 1}};
    auto req = rc_request();
    tr::Simulation s(tr::Circuit(d), req);
    EXPECT_EQ(s.step(), tr::Status::failed);
    ASSERT_TRUE(s.snapshot().failure);
    EXPECT_EQ(s.snapshot().failure->error.code(), tr::ErrorCode::numerical_failure);
    EXPECT_EQ(s.snapshot().current_time_seconds, 0);
}
TEST(Transient, CapacitorConstraintLoopsAndInductorOperatingPoint) {
    auto d = rc();
    d.components.push_back(tr::Capacitor{{13}, "series C", {1}, {2}, 2e-6});
    auto req = rc_request(.001, .0001);
    req.initial.capacitor_voltages.push_back({{13}, 10});
    auto r = run(d, req);
    EXPECT_EQ(r.voltages[0].front(), 0);
    req.initial.capacitor_voltages.back().voltage_volts = 9;
    error([&] { tr::Simulation s(tr::Circuit(d), req); }, tr::ErrorCode::contradictory_constraint);
    d = rc();
    d.components.back() = tr::Inductor{{12}, "L", {2}, {0}, 1};
    tr::Request op;
    op.stop_seconds = .001;
    op.maximum_step_seconds = .0001;
    op.currents = {{{12}}, {{10}}};
    r = run(d, op);
    for (std::size_t i = 0; i < r.times.size(); ++i) {
        EXPECT_NEAR(r.currents[0][i], .01, 1e-13);
        EXPECT_NEAR(r.currents[1][i], -.01, 1e-13);
    }
    d.components.push_back(tr::VoltageSource{{20}, "clamp", {2}, {0}, {1, {}}});
    error([&] { tr::Simulation s(tr::Circuit(d), op); }, tr::ErrorCode::contradictory_constraint);
    std::get<tr::VoltageSource>(d.components.back()).voltage_volts.initial = 0;
    error([&] { tr::Simulation s(tr::Circuit(d), op); }, tr::ErrorCode::rank_deficient);
}
TEST(Transient, ResistiveLimitAndReversedSourceOrientation) {
    tr::CircuitDefinition d{{{{0}, "g"}, {{1}, "out"}},
                            NodeId{0},
                            {tr::VoltageSource{{1}, "V", {0}, {1}, {-10, {{.05, -5}}}},
                             Resistor{{2}, "R", {1}, {0}, 1000},
                             tr::CurrentSource{{3}, "I", {0}, {1}, {.001, {}}}}};
    auto req = specified(.1, .02);
    req.voltages = {{{1}, {0}}};
    req.currents = {{{1}}, {{2}}, {{3}}};
    auto r = run(d, req);
    for (std::size_t i = 0; i < r.times.size(); ++i) {
        const bool before =
            r.times[i].seconds < .05 || r.times[i].side == tr::SampleSide::before_breakpoint;
        EXPECT_EQ(r.voltages[0][i], before ? 10 : 5);
        EXPECT_NEAR(r.currents[0][i], before ? .009 : .004, 1e-14);
        EXPECT_NEAR(r.currents[1][i], before ? .01 : .005, 1e-14);
        EXPECT_EQ(r.currents[2][i], .001);
    }
    tr::CircuitDefinition ground_only{{{{0}, "g"}}, NodeId{0}, {}};
    req.voltages = {{{0}, {0}}};
    req.currents.clear();
    r = run(ground_only, req);
    EXPECT_EQ(r.voltages[0].back(), 0);
}
TEST(Transient, BreakpointsAtFinalTimeBeyondHorizonAndLinearCorners) {
    auto d = rc(0);
    auto& v = std::get<tr::VoltageSource>(d.components[0]).voltage_volts;
    v.points = {{.001, 10}, {.002, -10}};
    auto r = run(d, rc_request(.001, .00013));
    EXPECT_EQ(r.times.back().seconds, .001);
    EXPECT_EQ(r.times.back().side, tr::SampleSide::after_breakpoint);
    EXPECT_EQ(r.voltages[0].back(), 0);
    EXPECT_NEAR(r.currents[1].back(), .01, 1e-14);
    EXPECT_EQ(std::count_if(r.times.begin(), r.times.end(),
                            [](auto t) { return t.side == tr::SampleSide::after_breakpoint; }),
              1);
    v.interpolation = tr::Interpolation::linear;
    r = run(d, rc_request(.003, .00002));
    for (double t : {.001, .002}) {
        const auto i = static_cast<std::size_t>(
            std::find_if(r.times.begin(), r.times.end(), [&](auto x) { return x.seconds == t; }) -
            r.times.begin());
        ASSERT_LT(i + 1, r.times.size());
        EXPECT_NEAR(r.voltages[0][i], r.voltages[0][i + 1], 1e-13);
        EXPECT_NEAR(r.currents[2][i], r.currents[2][i + 1], 1e-12);
    }
}
TEST(Transient, TinyTimeGridAndStepProgressAreExplicit) {
    auto tiny = std::numeric_limits<double>::denorm_min();
    tr::CircuitDefinition d{
        {{{0}, "g"}, {{1}, "out"}}, NodeId{0}, {Resistor{{1}, "R", {1}, {0}, 1000}}};
    auto req = specified(tiny, 1e9);
    req.voltages = {{{1}, {0}}};
    const auto grid = tr::time_grid(tr::Circuit(d), req);
    ASSERT_EQ(grid.size(), 2u);
    EXPECT_EQ(grid.back(), tiny);
    EXPECT_EQ(run(d, req).current_time_seconds, tiny);
    // Large absolute times still retain requested short endpoint intervals.
    auto normal = rc();
    std::get<tr::VoltageSource>(normal.components[0]).voltage_volts.points = {{1e9 - .001, 10},
                                                                              {1e9, 10}};
    req = rc_request(1e9, 1e9);
    const auto g = tr::time_grid(tr::Circuit(normal), req);
    EXPECT_EQ(g.back(), 1e9);
    EXPECT_EQ(g[g.size() - 2], 1e9 - .001);
}
TEST(Transient, DefinitionAndWorkResourceBoundaries) {
    tr::CircuitDefinition d{{{{0}, "g"}, {{1}, "out"}}, NodeId{0}, {}};
    for (std::size_t i = 0; i < tr::limits::inductors; ++i)
        d.components.push_back(
            tr::Inductor{{static_cast<std::uint32_t>(i)}, "L" + std::to_string(i), {1}, {0}, 1});
    EXPECT_NO_THROW(tr::Circuit{d});
    d.components.push_back(tr::Inductor{{1000}, "extra", {1}, {0}, 1});
    error([&] { tr::Circuit c(d); }, tr::ErrorCode::resource_limit);
    d.components.clear();
    for (std::size_t i = 0; i < 5; ++i) {
        tr::Source source;
        for (std::size_t j = 1; j <= tr::limits::source_points; ++j)
            source.points.push_back({static_cast<double>(j), 0});
        d.components.push_back(tr::CurrentSource{
            {static_cast<std::uint32_t>(i)}, "I" + std::to_string(i), {1}, {0}, source});
        if (i == 3) {
            EXPECT_NO_THROW(tr::Circuit{d});
        }
    }
    error([&] { tr::Circuit c(d); }, tr::ErrorCode::resource_limit);
    d = rc();
    for (std::uint32_t i = 3; i < 63; ++i) {
        d.nodes.push_back({{i}, "n" + std::to_string(i)});
        d.components.push_back(Resistor{{100 + i}, "R" + std::to_string(i), {i}, {0}, 1});
    }
    auto req = rc_request(1, .0001);
    error([&] { tr::Simulation s(tr::Circuit(d), req); }, tr::ErrorCode::resource_limit);
    req = rc_request();
    req.initial.mode = static_cast<tr::Initialization>(9);
    error([&] { tr::Simulation s(tr::Circuit(rc()), req); }, tr::ErrorCode::invalid_request);
}
TEST(Transient, MovedExecutionOwnsInputsAndTraceShape) {
    auto d = rc();
    auto req = rc_request();
    tr::Simulation original(tr::Circuit(d), req);
    original.step();
    tr::Simulation moved(std::move(original));
    d.nodes.clear();
    req.voltages.clear();
    EXPECT_EQ(moved.run(), tr::Status::complete);
    auto r = moved.snapshot();
    for (const auto& v : r.voltages)
        EXPECT_EQ(v.size(), r.times.size());
    for (const auto& v : r.currents)
        EXPECT_EQ(v.size(), r.times.size());
    tr::Simulation assigned(tr::Circuit(rc()), rc_request());
    assigned = std::move(moved);
    EXPECT_EQ(assigned.snapshot().voltages, r.voltages);
}
TEST(Transient, ReversedStorageTerminalsAndInitialCurrents) {
    auto d = rc();
    auto req = rc_request();
    auto expected = run(d, req);
    auto& c = std::get<tr::Capacitor>(d.components.back());
    std::swap(c.positive, c.negative);
    auto reversed = run(d, req);
    ASSERT_EQ(expected.times.size(), reversed.times.size());
    for (std::size_t i = 0; i < expected.times.size(); ++i) {
        EXPECT_NEAR(expected.voltages[0][i], reversed.voltages[0][i], 1e-12);
        EXPECT_NEAR(expected.currents[2][i], -reversed.currents[2][i], 1e-12);
    }
    d = rlc(100);
    req = specified(.001, .00001, {{{13}, .25}}, {{{12}, .002}});
    req.voltages = {{{3}, {0}}};
    req.currents = {{{12}}};
    expected = run(d, req);
    auto& l = std::get<tr::Inductor>(d.components[2]);
    std::swap(l.positive, l.negative);
    req.initial.inductor_currents.front().current_amperes = -.002;
    reversed = run(d, req);
    ASSERT_EQ(expected.times.size(), reversed.times.size());
    for (std::size_t i = 0; i < expected.times.size(); ++i) {
        EXPECT_NEAR(expected.voltages[0][i], reversed.voltages[0][i], 1e-12);
        EXPECT_NEAR(expected.currents[0][i], -reversed.currents[0][i], 1e-12);
    }
    req.initial.inductor_currents.clear();
    error([&] { tr::Simulation s(tr::Circuit(d), req); }, tr::ErrorCode::invalid_request);
    req.initial.inductor_currents = {{{999}, 0}};
    error([&] { tr::Simulation s(tr::Circuit(d), req); }, tr::ErrorCode::invalid_request);
}
