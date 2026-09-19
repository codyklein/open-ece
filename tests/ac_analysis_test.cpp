#include "ac_mna.hpp"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <openece/circuits/ac/analysis.hpp>
#include <random>
#include <type_traits>
using namespace openece::circuits;
namespace ac = openece::circuits::ac;
namespace {
constexpr double unit_frequency = 1 / (2 * std::numbers::pi);
ac::CircuitDefinition rc() {
    return {{{{0}, "ground"}, {{1}, "input"}, {{2}, "output"}},
            NodeId{0},
            {ac::VoltageSource{{10}, "V", {1}, {0}, {1, 0}}, Resistor{{11}, "R", {1}, {2}, 1000},
             ac::Capacitor{{12}, "C", {2}, {0}, 1e-6}}};
}
ac::Phasor voltage(const ac::AcSolution& s, NodeId id) {
    for (auto v : s.node_voltages)
        if (v.node == id)
            return v.voltage_volts_rms;
    throw std::runtime_error("missing node");
}
ac::Phasor current(const ac::AcSolution& s, ComponentId id) {
    for (auto v : s.voltage_source_currents)
        if (v.source == id)
            return v.current_amperes_rms;
    throw std::runtime_error("missing source");
}
void near(ac::Phasor value, ac::Phasor expected, double tolerance = 1e-12) {
    EXPECT_NEAR(value.real(), expected.real(), tolerance);
    EXPECT_NEAR(value.imag(), expected.imag(), tolerance);
}
void error(const ac::CircuitDefinition& d, ErrorCode expected, double frequency = 1000) {
    try {
        (void)ac::solve_ac(ac::Circuit(d), frequency);
        FAIL() << "Expected solve failure";
    } catch (const CircuitError& e) {
        EXPECT_EQ(e.code(), expected) << e.what();
    }
}
// Independent reference uses an extra current unknown for EVERY branch and
// impedance equations, not production admittance/MNA stamps or Eigen.
// Test-only complex long-double Gauss-Jordan elimination; Windows long double
// may have double precision, so agreement never depends on extended precision.
std::vector<std::complex<long double>> reference(const ac::CircuitDefinition& d, double f) {
    using C = std::complex<long double>;
    const long double w = 2 * std::numbers::pi_v<long double> * f;
    std::vector<NodeId> nodes;
    for (const auto& n : d.nodes)
        if (n.id != *d.ground)
            nodes.push_back(n.id);
    const auto nv = nodes.size(), k = nv + d.components.size();
    std::vector<std::vector<C>> a(k, std::vector<C>(k + 1));
    auto ni = [&](NodeId id) {
        return static_cast<std::size_t>(std::find(nodes.begin(), nodes.end(), id) - nodes.begin());
    };
    for (std::size_t j = 0; j < d.components.size(); ++j)
        std::visit(
            [&](const auto& part) {
                const auto p = ni(part.positive), n = ni(part.negative), row = nv + j;
                if (p < nv)
                    a[p][row] += 1;
                if (n < nv)
                    a[n][row] -= 1;
                using T = std::decay_t<decltype(part)>;
                if constexpr (std::is_same_v<T, ac::CurrentSource>) {
                    a[row][row] = 1;
                    a[row][k] = C(part.current_amperes_rms);
                } else {
                    if (p < nv)
                        a[row][p] += 1;
                    if (n < nv)
                        a[row][n] -= 1;
                    if constexpr (std::is_same_v<T, Resistor>)
                        a[row][row] = -part.resistance_ohms;
                    else if constexpr (std::is_same_v<T, ac::Capacitor>)
                        a[row][row] = C(0, 1 / (w * part.capacitance_farads));
                    else if constexpr (std::is_same_v<T, ac::Inductor>)
                        a[row][row] = C(0, -w * part.inductance_henries);
                    else
                        a[row][k] = C(part.voltage_volts_rms);
                }
            },
            d.components[j]);
    for (std::size_t j = 0; j < k; ++j) {
        auto pivot = j;
        for (auto i = j + 1; i < k; ++i)
            if (std::abs(a[i][j]) > std::abs(a[pivot][j]))
                pivot = i;
        if (std::abs(a[pivot][j]) == 0)
            throw std::runtime_error("singular independent reference");
        std::swap(a[pivot], a[j]);
        const auto factor = a[j][j];
        for (auto col = j; col <= k; ++col)
            a[j][col] /= factor;
        for (std::size_t i = 0; i < k; ++i)
            if (i != j) {
                const auto multiplier = a[i][j];
                for (auto col = j; col <= k; ++col)
                    a[i][col] -= multiplier * a[j][col];
            }
    }
    std::vector<C> result;
    for (const auto& row : a)
        result.push_back(row[k]);
    return result;
}
} // namespace
TEST(Ac, ComplexStampsAreSymmetricNotHermitian) {
    auto d = rc();
    std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = {3, 4};
    d.components[1] = Resistor{{11}, "R", {1}, {0}, 2};
    d.components[2] = ac::Capacitor{{12}, "C", {1}, {2}, .25};
    d.components.push_back(ac::Inductor{{13}, "L", {2}, {0}, 2});
    d.components.push_back(ac::CurrentSource{{14}, "I", {2}, {1}, {.01, .02}});
    auto s = ac::detail::assemble(ac::Circuit(d), unit_frequency);
    const std::vector<ac::Phasor> expected{{.5, .25}, {0, -.25}, {1, 0}, {0, -.25}, {0, -.25},
                                           {0, 0},    {1, 0},    {0, 0}, {0, 0}};
    ASSERT_EQ(s.matrix.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        near(s.matrix[i], expected[i]);
    EXPECT_EQ(s.voltage_nodes, (std::vector<NodeId>{{1}, {2}}));
    EXPECT_EQ(s.rhs, (std::vector<ac::Phasor>{{.01, .02}, {-.01, -.02}, {3, 4}}));
    EXPECT_EQ(s.matrix[1], s.matrix[3]);
    EXPECT_NE(s.matrix[1], std::conj(s.matrix[3]));
}
TEST(Ac, TenVoltOneKilohmSourceSignAndDcEquivalence) {
    ac::CircuitDefinition d{
        {{{8}, "supply"}, {{3}, "ground"}},
        NodeId{3},
        {ac::VoltageSource{{1}, "V", {8}, {3}, {10, 0}}, Resistor{{2}, "R", {8}, {3}, 1000}}};
    for (double f : {ac::limits::frequency_min, 1234.5, ac::limits::frequency_max}) {
        auto s = ac::solve_ac(ac::Circuit(d), f);
        near(voltage(s, {8}), {10, 0});
        near(current(s, {1}), {-.01, 0}, 1e-15);
        EXPECT_EQ(voltage(s, {3}), ac::Phasor{});
        EXPECT_EQ(s.frequency_hz, f);
    }
    CircuitDefinition dc{
        {{{8}, "supply"}, {{3}, "ground"}},
        NodeId{3},
        {VoltageSource{{1}, "V", {8}, {3}, 10}, Resistor{{2}, "R", {8}, {3}, 1000}}};
    const auto real = solve_dc(Circuit(dc));
    near(voltage(ac::solve_ac(ac::Circuit(d), 100), {8}), {real.node_voltages[0].voltage_volts, 0});
    auto& source = std::get<ac::VoltageSource>(d.components[0]);
    std::swap(source.positive, source.negative);
    source.voltage_volts_rms = {-10, 0};
    near(current(ac::solve_ac(ac::Circuit(d), 100), {1}), {.01, 0});
}
TEST(Ac, CapacitorCurrentLeadsAndInductorCurrentLagsVoltage) {
    for (bool capacitor : {true, false}) {
        ac::CircuitDefinition d{
            {{{0}, "g"}, {{1}, "v"}}, NodeId{0}, {ac::VoltageSource{{1}, "V", {1}, {0}, {3, 4}}}};
        if (capacitor)
            d.components.push_back(ac::Capacitor{{2}, "C", {1}, {0}, .5});
        else
            d.components.push_back(ac::Inductor{{2}, "L", {1}, {0}, 2});
        auto s = ac::solve_ac(ac::Circuit(d), unit_frequency);
        const ac::Phasor load = capacitor ? ac::Phasor(-2, 1.5) : ac::Phasor(2, -1.5);
        near(current(s, {1}), -load);
    }
}
TEST(Ac, RcLowpassAcrossCapacitorCornerMagnitudeAndPhase) {
    const double corner = 1 / (2 * std::numbers::pi * 1000 * 1e-6);
    const auto s = ac::solve_ac(ac::Circuit(rc()), corner);
    auto h = voltage(s, {2});
    near(h, {.5, -.5});
    EXPECT_NEAR(20 * std::log10(std::abs(h)), -3.010299956639812, 1e-12);
    EXPECT_NEAR(std::arg(h) * 180 / std::numbers::pi, -45, 1e-12);
    for (double ratio : {.01, .1, 1., 10., 100.}) {
        auto value = voltage(ac::solve_ac(ac::Circuit(rc()), corner * ratio), {2});
        near(value, 1.0 / ac::Phasor(1, ratio));
    }
}
TEST(Ac, RlDividerAndSeriesRlcResonance) {
    auto d = rc();
    d.components[2] = ac::Inductor{{12}, "L", {2}, {0}, 1};
    const auto w = 2 * std::numbers::pi * 1000;
    near(voltage(ac::solve_ac(ac::Circuit(d), 1000), {2}), ac::Phasor(0, w) / ac::Phasor(1000, w));
    d.nodes.push_back({{3}, "last"});
    d.components[1] = ac::Inductor{{11}, "L", {1}, {2}, 1};
    d.components[2] = ac::Capacitor{{12}, "C", {2}, {3}, 1};
    d.components.push_back(Resistor{{13}, "R", {3}, {0}, 2});
    const auto s = ac::solve_ac(ac::Circuit(d), unit_frequency);
    near(current(s, {10}), {-.5, 0});
    near(voltage(s, {3}), {1, 0});
    near(voltage(s, {2}), {1, -.5});
}
TEST(Ac, CurrentSourceNetworksAndComplexSuperposition) {
    auto d = rc();
    d.components.push_back(ac::CurrentSource{{13}, "I", {0}, {2}, {.001, .002}});
    auto both = ac::solve_ac(ac::Circuit(d), 1000);
    auto vd = d;
    std::get<ac::CurrentSource>(vd.components.back()).current_amperes_rms = {};
    auto vs = ac::solve_ac(ac::Circuit(vd), 1000);
    std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = {};
    auto is = ac::solve_ac(ac::Circuit(d), 1000);
    near(voltage(both, {2}), voltage(vs, {2}) + voltage(is, {2}));
    auto& source = std::get<ac::CurrentSource>(d.components.back());
    std::swap(source.positive, source.negative);
    source.current_amperes_rms = -source.current_amperes_rms;
    near(voltage(ac::solve_ac(ac::Circuit(d), 1000), {2}), voltage(is, {2}));
}
TEST(Ac, ReferenceConnectivityUsesRclAndVoltageButNeverCurrentSources) {
    ac::CircuitDefinition d{{{{0}, "g"}, {{1}, "a"}, {{2}, "b"}},
                            NodeId{0},
                            {ac::Capacitor{{1}, "C", {1}, {2}, 1e-6},
                             ac::CurrentSource{{2}, "I", {0}, {1}, {.001, 0}}}};
    error(d, ErrorCode::floating_reference);
    d.components.push_back(ac::Inductor{{3}, "L", {2}, {0}, .1});
    EXPECT_NO_THROW(ac::solve_ac(ac::Circuit(d), 1000));
    d.nodes.push_back({{8}, "unused"});
    try {
        (void)ac::solve_ac(ac::Circuit(d), 1000);
        FAIL();
    } catch (const CircuitError& e) {
        EXPECT_EQ(e.nodes(), (std::vector<NodeId>{{8}}));
    }
    d.nodes.pop_back();
    d.nodes.push_back({{8}, "separate"});
    d.components.push_back(Resistor{{4}, "R", {8}, {0}, 100});
    near(voltage(ac::solve_ac(ac::Circuit(d), 1000), {8}), {});
    const auto trivial = ac::solve_ac(ac::Circuit({{{{7}, "g"}}, NodeId{7}, {}}), 1000);
    ASSERT_EQ(trivial.node_voltages.size(), 1u);
    EXPECT_EQ(trivial.node_voltages[0].voltage_volts_rms, ac::Phasor{});
}
TEST(Ac, ComplexVoltageLoopsDistinguishRedundancyFromContradiction) {
    auto d = rc();
    std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = {3, 4};
    d.components.push_back(ac::VoltageSource{{13}, "reverse", {0}, {1}, {-3, -4}});
    error(d, ErrorCode::redundant_voltage_constraint);
    std::get<ac::VoltageSource>(d.components.back()).voltage_volts_rms = {-3, 4};
    error(d, ErrorCode::contradictory_voltage_constraint);
    d.components.back() = ac::VoltageSource{{13}, "V2", {2}, {1}, {-1, 2}};
    d.components.push_back(ac::VoltageSource{{14}, "V3", {2}, {0}, {2, 6}});
    error(d, ErrorCode::redundant_voltage_constraint);
    d.components.push_back(ac::VoltageSource{{15}, "conflict", {1}, {0}, {3, 5}});
    error(d, ErrorCode::contradictory_voltage_constraint);
}
TEST(Ac, SupernodeAndZeroVoltCurrentProbe) {
    ac::CircuitDefinition d{{{{0}, "g"}, {{1}, "a"}, {{2}, "b"}},
                            NodeId{0},
                            {ac::VoltageSource{{1}, "V", {1}, {2}, {6, 3}},
                             Resistor{{2}, "R1", {1}, {0}, 1000},
                             Resistor{{3}, "R2", {2}, {0}, 2000}}};
    auto s = ac::solve_ac(ac::Circuit(d), 1000);
    near(voltage(s, {1}), {2, 1});
    near(voltage(s, {2}), {-4, -2});
    near(current(s, {1}), {-.002, -.001});
    d = rc();
    d.nodes.push_back({{3}, "probe"});
    std::get<ac::Capacitor>(d.components[2]).negative = {3};
    d.components.push_back(ac::VoltageSource{{13}, "probe", {3}, {0}, {}});
    s = ac::solve_ac(ac::Circuit(d), 1000);
    near(current(s, {13}), -current(s, {10}));
}
TEST(Ac, IdealParallelResonanceIsNotRegularized) {
    ac::CircuitDefinition d{{{{0}, "g"}, {{1}, "v"}},
                            NodeId{0},
                            {ac::Capacitor{{1}, "C", {1}, {0}, 1},
                             ac::Inductor{{2}, "L", {1}, {0}, 1},
                             ac::CurrentSource{{3}, "I", {0}, {1}, {1, 0}}}};
    error(d, ErrorCode::rank_deficient, unit_frequency);
    for (double ratio : {.5, 2.}) {
        auto s = ac::solve_ac(ac::Circuit(d), unit_frequency * ratio);
        near(voltage(s, {1}), 1.0 / ac::Phasor(0, ratio - 1 / ratio));
    }
    std::get<ac::CurrentSource>(d.components.back()).current_amperes_rms = {};
    error(d, ErrorCode::rank_deficient,
          unit_frequency); // Non-unique even without excitation.
}
TEST(Ac, FrequencyValidationAndComponentExtremes) {
    for (double f :
         {0., -1., ac::limits::frequency_min / 2, ac::limits::frequency_max * 2,
          std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        error(rc(), ErrorCode::invalid_value, f);
    for (double f : {ac::limits::frequency_min, ac::limits::frequency_max})
        for (bool capacitor : {true, false}) {
            for (bool maximum : {true, false}) {
                ac::CircuitDefinition d{{{{0}, "g"}, {{1}, "v"}},
                                        NodeId{0},
                                        {ac::VoltageSource{{1}, "V", {1}, {0}, {1, 0}}}};
                ac::Phasor expected;
                const double w = 2 * std::numbers::pi * f;
                if (capacitor) {
                    double c = maximum ? ac::limits::capacitance_max : ac::limits::capacitance_min;
                    d.components.push_back(ac::Capacitor{{2}, "C", {1}, {0}, c});
                    expected = {0, -w * c};
                } else {
                    double l = maximum ? ac::limits::inductance_max : ac::limits::inductance_min;
                    d.components.push_back(ac::Inductor{{2}, "L", {1}, {0}, l});
                    expected = {0, 1 / (w * l)};
                }
                near(current(ac::solve_ac(ac::Circuit(d), f), {1}), expected,
                     std::abs(expected) * 1e-12);
            }
        }
}
TEST(Ac, RankConditioningAndPhysicalResidualPoliciesRemainStrict) {
    ac::CircuitDefinition d{{{{0}, "g"}, {{1}, "a"}, {{2}, "b"}},
                            NodeId{0},
                            {Resistor{{1}, "link", {1}, {2}, 1},
                             Resistor{{2}, "weak", {2}, {0}, 1e12},
                             ac::CurrentSource{{3}, "I", {0}, {1}, {.001, .002}}}};
    error(d, ErrorCode::ill_conditioned);
    std::get<Resistor>(d.components[0]).resistance_ohms = 1e-9;
    error(d, ErrorCode::rank_deficient);
    d = rc();
    std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = {1e9, 0};
    std::get<Resistor>(d.components[1]).resistance_ohms = ac::limits::resistance_min;
    d.components[2] = Resistor{{12}, "weak", {2}, {0}, ac::limits::resistance_max};
    error(d, ErrorCode::numerical_failure);
}
TEST(Ac, DeclarationOrderDeterminismAndOwnedInputPreservation) {
    auto d = rc();
    d.nodes.push_back({{3}, "extra"});
    d.components.push_back(ac::VoltageSource{{13}, "V2", {3}, {2}, {2, -1}});
    const auto before = ac::solve_ac(ac::Circuit(d), 1000);
    std::reverse(d.nodes.begin(), d.nodes.end());
    std::reverse(d.components.begin(), d.components.end());
    const ac::Circuit c(d);
    const auto after = ac::solve_ac(c, 1000);
    for (std::size_t i = 0; i < d.nodes.size(); ++i) {
        EXPECT_EQ(after.node_voltages[i].node, d.nodes[i].id);
        near(after.node_voltages[i].voltage_volts_rms, voltage(before, d.nodes[i].id));
    }
    EXPECT_EQ(after.voltage_source_currents[0].source, ComponentId{13});
    EXPECT_EQ(after.voltage_source_currents[1].source, ComponentId{10});
    near(current(after, {13}), current(before, {13}));
    EXPECT_EQ(ac::solve_ac(c, 1000).quality.backward_error, after.quality.backward_error);
    EXPECT_EQ(std::get<ac::VoltageSource>(c.definition().components[0]).voltage_volts_rms,
              ac::Phasor(2, -1));
}
TEST(Ac, IndependentBranchReferenceForMixedNetworks) {
    std::mt19937 random(707);
    for (int trial = 0; trial < 40; ++trial) {
        auto d = rc();
        d.nodes.push_back({{3}, "third"});
        std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = {3, 4};
        std::get<Resistor>(d.components[1]).resistance_ohms =
            static_cast<double>(10 + random() % 1000);
        std::get<ac::Capacitor>(d.components[2]).capacitance_farads =
            static_cast<double>(1 + random() % 100) * 1e-8;
        d.components.push_back(
            ac::Inductor{{13}, "L", {1}, {3}, static_cast<double>(1 + random() % 100) * 1e-3});
        d.components.push_back(
            Resistor{{14}, "cross", {2}, {3}, static_cast<double>(10 + random() % 1000)});
        d.components.push_back(ac::CurrentSource{{15}, "I", {3}, {0}, {.003, -.001}});
        if (trial % 2)
            d.components.push_back(ac::VoltageSource{{16}, "V2", {3}, {2}, {2, -1}});
        for (double f : {3., 159.15494309189535, 3000.}) {
            const auto ref = reference(d, f);
            const auto s = ac::solve_ac(ac::Circuit(d), f);
            for (std::size_t i = 1; i < d.nodes.size(); ++i)
                near(s.node_voltages[i].voltage_volts_rms, ac::Phasor(ref[i - 1]), 1e-10);
            for (std::size_t i = 0; i < d.components.size(); ++i)
                if (const auto* v = std::get_if<ac::VoltageSource>(&d.components[i]))
                    near(current(s, v->id), ac::Phasor(ref[d.nodes.size() - 1 + i]), 1e-11);
        }
    }
}
TEST(Ac, MaximumNetworkAndZeroExcitation) {
    ac::CircuitDefinition d{{{{0}, "g"}}, NodeId{0}, {}};
    for (std::size_t i = 1; i < ac::limits::nodes; ++i) {
        auto id = static_cast<std::uint32_t>(i);
        d.nodes.push_back({{id}, "N" + std::to_string(i)});
        d.components.push_back(ac::Capacitor{{id}, "C" + std::to_string(i), {id}, {0}, 1e-6});
        if (i <= ac::limits::voltage_sources)
            d.components.push_back(
                ac::VoltageSource{{1000 + id}, "V" + std::to_string(i), {id}, {0}, {}});
    }
    const auto s = ac::solve_ac(ac::Circuit(d), 1000);
    EXPECT_EQ(s.node_voltages.size(), ac::limits::nodes);
    EXPECT_EQ(s.voltage_source_currents.size(), ac::limits::voltage_sources);
    for (const auto& v : s.node_voltages)
        EXPECT_EQ(v.voltage_volts_rms, ac::Phasor{});
    EXPECT_EQ(s.quality.backward_error, 0);
}
