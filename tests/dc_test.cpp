#include "mna.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <openece/circuits/dc.hpp>
#include <random>
#include <type_traits>
using namespace openece::circuits;
namespace {
CircuitDefinition divider() {
    return {{{{0}, "ground"}, {{1}, "supply"}, {{2}, "middle"}},
            NodeId{0},
            {VoltageSource{{10}, "V", {1}, {0}, 10}, Resistor{{11}, "R1", {1}, {2}, 1000},
             Resistor{{12}, "R2", {2}, {0}, 1000}}};
}
double voltage(const DcSolution& s, NodeId id) {
    for (auto v : s.node_voltages)
        if (v.node == id)
            return v.voltage_volts;
    throw std::runtime_error("missing node");
}
double current(const DcSolution& s, ComponentId id) {
    for (auto v : s.voltage_source_currents)
        if (v.source == id)
            return v.current_amperes;
    throw std::runtime_error("missing source");
}
void error(const CircuitDefinition& d, ErrorCode code) {
    Circuit c(d);
    try {
        (void)solve_dc(c);
        FAIL() << "Expected solve error";
    } catch (const CircuitError& e) {
        EXPECT_EQ(e.code(), code) << e.what();
    }
}
// Independent reference: one current unknown for EVERY branch; no conductance stamps.
// KCL plus Vp-Vn=R*i / i=I / Vp-Vn=E, solved by test-only Gauss-Jordan elimination.
std::vector<double> branch_reference(const CircuitDefinition& d) {
    std::vector<NodeId> nodes;
    for (auto n : d.nodes)
        if (n.id != *d.ground)
            nodes.push_back(n.id);
    auto nv = nodes.size(), size = nv + d.components.size();
    std::vector<std::vector<double>> a(size, std::vector<double>(size + 1));
    auto ni = [&](NodeId id) {
        return static_cast<std::size_t>(std::find(nodes.begin(), nodes.end(), id) - nodes.begin());
    };
    for (std::size_t j = 0; j < d.components.size(); ++j)
        std::visit(
            [&](const auto& c) {
                auto p = ni(c.positive), n = ni(c.negative), row = nv + j;
                if (p < nv)
                    a[p][row] += 1;
                if (n < nv)
                    a[n][row] -= 1;
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, CurrentSource>) {
                    a[row][row] = 1;
                    a[row][size] = c.current_amperes;
                } else {
                    if (p < nv)
                        a[row][p] += 1;
                    if (n < nv)
                        a[row][n] -= 1;
                    if constexpr (std::is_same_v<T, Resistor>)
                        a[row][row] = -c.resistance_ohms;
                    else
                        a[row][size] = c.voltage_volts;
                }
            },
            d.components[j]);
    for (std::size_t j = 0; j < size; ++j) {
        auto pivot = j;
        for (auto i = j + 1; i < size; ++i)
            if (std::abs(a[i][j]) > std::abs(a[pivot][j]))
                pivot = i;
        if (a[pivot][j] == 0)
            throw std::runtime_error("Singular reference");
        std::swap(a[j], a[pivot]);
        double v = a[j][j];
        for (auto k = j; k <= size; ++k)
            a[j][k] /= v;
        for (std::size_t i = 0; i < size; ++i)
            if (i != j) {
                double factor = a[i][j];
                for (auto k = j; k <= size; ++k)
                    a[i][k] -= factor * a[j][k];
            }
    }
    std::vector<double> result;
    for (auto& row : a)
        result.push_back(row[size]);
    return result;
}
} // namespace
TEST(Dc, ExactStampConventions) {
    auto d = divider();
    d.components.push_back(CurrentSource{{13}, "I", {2}, {1}, 0.002});
    auto s = detail::assemble(Circuit(d));
    EXPECT_EQ(s.voltage_nodes, (std::vector<NodeId>{{1}, {2}}));
    EXPECT_EQ(s.voltage_sources, (std::vector<ComponentId>{{10}}));
    EXPECT_EQ(s.matrix, (std::vector<double>{.001, -.001, 1, -.001, .002, 0, 1, 0, 0}));
    EXPECT_EQ(s.rhs, (std::vector<double>{.002, -.002, 10}));
}
TEST(Dc, TenVoltOneKilohmSignRegression) {
    auto d = divider();
    d.nodes.pop_back();
    d.components.resize(2);
    std::get<Resistor>(d.components[1]).negative = {0};
    auto s = solve_dc(Circuit(d));
    EXPECT_DOUBLE_EQ(voltage(s, {1}), 10);
    EXPECT_NEAR(current(s, {10}), -.01, 1e-15);
    EXPECT_DOUBLE_EQ(voltage(s, {0}), 0);
    EXPECT_EQ(s.node_voltages[0].node, NodeId{0});
}
TEST(Dc, DividerParallelResistorsAndSuperposition) {
    auto d = divider();
    auto s = solve_dc(Circuit(d));
    EXPECT_NEAR(voltage(s, {2}), 5, 1e-12);
    EXPECT_NEAR(current(s, {10}), -.005, 1e-15);
    d.components.push_back(Resistor{{13}, "parallel", {2}, {0}, 1000});
    s = solve_dc(Circuit(d));
    EXPECT_NEAR(voltage(s, {2}), 10.0 / 3, 1e-12);
    d = divider();
    d.components.push_back(CurrentSource{{13}, "I", {0}, {2}, .001});
    auto both = solve_dc(Circuit(d));
    auto v = d;
    std::get<CurrentSource>(v.components.back()).current_amperes = 0;
    auto vs = solve_dc(Circuit(v));
    std::get<VoltageSource>(d.components[0]).voltage_volts = 0;
    auto is = solve_dc(Circuit(d));
    EXPECT_NEAR(voltage(both, {2}), voltage(vs, {2}) + voltage(is, {2}), 1e-12);
}
TEST(Dc, CurrentSourcesAndTerminalReversal) {
    CircuitDefinition d{
        {{{0}, "g"}, {{1}, "n"}},
        NodeId{0},
        {Resistor{{1}, "R", {1}, {0}, 2000}, CurrentSource{{2}, "I", {0}, {1}, .003}}};
    EXPECT_NEAR(voltage(solve_dc(Circuit(d)), {1}), 6, 1e-12);
    auto& i = std::get<CurrentSource>(d.components[1]);
    std::swap(i.positive, i.negative);
    i.current_amperes = -i.current_amperes;
    EXPECT_NEAR(voltage(solve_dc(Circuit(d)), {1}), 6, 1e-12);
    d = divider();
    auto& v = std::get<VoltageSource>(d.components[0]);
    std::swap(v.positive, v.negative);
    v.voltage_volts = -10;
    auto s = solve_dc(Circuit(d));
    EXPECT_NEAR(voltage(s, {2}), 5, 1e-12);
    EXPECT_NEAR(current(s, {10}), .005, 1e-15);
}
TEST(Dc, SupernodeAndZeroVoltCurrentProbe) {
    CircuitDefinition d{{{{0}, "g"}, {{1}, "a"}, {{2}, "b"}},
                        NodeId{0},
                        {VoltageSource{{1}, "V", {1}, {2}, 6}, Resistor{{2}, "R1", {1}, {0}, 1000},
                         Resistor{{3}, "R2", {2}, {0}, 2000}}};
    auto s = solve_dc(Circuit(d));
    EXPECT_NEAR(voltage(s, {1}), 2, 1e-12);
    EXPECT_NEAR(voltage(s, {2}), -4, 1e-12);
    EXPECT_NEAR(current(s, {1}), -.002, 1e-15);
    d = divider();
    d.nodes.push_back({{3}, "probe"});
    std::get<Resistor>(d.components[2]).negative = {3};
    d.components.push_back(VoltageSource{{13}, "probe", {3}, {0}, 0});
    s = solve_dc(Circuit(d));
    EXPECT_NEAR(current(s, {13}), .005, 1e-15);
}
TEST(Dc, FloatingIslandsIgnoreCurrentSources) {
    auto d = divider();
    d.components[0] = CurrentSource{{10}, "I", {1}, {0}, .001};
    d.components.pop_back();
    error(d, ErrorCode::floating_reference);
    d.components.push_back(CurrentSource{{13}, "return", {0}, {2}, .001});
    error(d, ErrorCode::floating_reference);
    d = divider();
    d.nodes.push_back({{99}, "unused"});
    try {
        (void)solve_dc(Circuit(d));
        FAIL();
    } catch (const CircuitError& e) {
        EXPECT_EQ(e.nodes(), (std::vector<NodeId>{{99}}));
    }
}
TEST(Dc, SeparateGroundedBranchesAreValid) {
    auto d = divider();
    d.nodes.push_back({{3}, "other"});
    d.components.push_back(Resistor{{13}, "R3", {3}, {0}, 100});
    d.components.push_back(CurrentSource{{14}, "I", {0}, {3}, .02});
    auto s = solve_dc(Circuit(d));
    EXPECT_NEAR(voltage(s, {3}), 2, 1e-12);
    EXPECT_NEAR(voltage(s, {2}), 5, 1e-12);
    auto trivial = solve_dc(Circuit({{{{8}, "g"}}, NodeId{8}, {}}));
    EXPECT_EQ(trivial.node_voltages.size(), 1u);
    EXPECT_EQ(trivial.node_voltages[0].voltage_volts, 0);
}
TEST(Dc, VoltageSourceLoopsAreRejectedWithDistinctDiagnostics) {
    auto d = divider();
    d.components.push_back(VoltageSource{{13}, "duplicate", {1}, {0}, 10});
    error(d, ErrorCode::redundant_voltage_constraint);
    std::get<VoltageSource>(d.components.back()).voltage_volts = 11;
    error(d, ErrorCode::contradictory_voltage_constraint);
    d = divider();
    d.components.push_back(VoltageSource{{13}, "V2", {2}, {1}, -5});
    d.components.push_back(VoltageSource{{14}, "V3", {2}, {0}, 5});
    error(d, ErrorCode::redundant_voltage_constraint);
    std::get<VoltageSource>(d.components.back()).voltage_volts = 4;
    error(d, ErrorCode::contradictory_voltage_constraint);
    d = divider();
    d.components.push_back(VoltageSource{{13}, "reverse", {0}, {1}, -10});
    error(d, ErrorCode::redundant_voltage_constraint);
}
TEST(Dc, DeclarationOrderingAndInputPreservation) {
    auto d = divider();
    d.nodes.push_back({{3}, "other"});
    d.components.push_back(VoltageSource{{13}, "V2", {3}, {0}, 3});
    auto original = solve_dc(Circuit(d));
    std::reverse(d.nodes.begin(), d.nodes.end());
    std::reverse(d.components.begin(), d.components.end());
    Circuit circuit(d);
    auto s = solve_dc(circuit);
    for (std::size_t i = 0; i < d.nodes.size(); ++i) {
        EXPECT_EQ(s.node_voltages[i].node, d.nodes[i].id);
        EXPECT_NEAR(s.node_voltages[i].voltage_volts, voltage(original, d.nodes[i].id), 1e-12);
    }
    EXPECT_EQ(s.voltage_source_currents[0].source, ComponentId{13});
    EXPECT_EQ(s.voltage_source_currents[1].source, ComponentId{10});
    EXPECT_EQ(circuit.definition().components.size(), d.components.size());
    auto repeated = solve_dc(circuit);
    EXPECT_DOUBLE_EQ(repeated.quality.backward_error, s.quality.backward_error);
}
TEST(Dc, SupportedResistanceExtremesScaleWithoutChangingPhysics) {
    for (double r : {limits::resistance_min, 1.0, limits::resistance_max}) {
        auto d = divider();
        std::get<Resistor>(d.components[1]).resistance_ohms = r;
        std::get<Resistor>(d.components[2]).resistance_ohms = r;
        auto s = solve_dc(Circuit(d));
        EXPECT_NEAR(voltage(s, {2}), 5, 1e-11);
        EXPECT_NEAR(current(s, {10}), -5 / r, std::abs(5 / r) * 1e-12);
    }
}
TEST(Dc, RejectsNumericallyUnreliableWeakGroundReference) {
    CircuitDefinition d{{{{0}, "g"}, {{1}, "a"}, {{2}, "b"}},
                        NodeId{0},
                        {Resistor{{1}, "link", {1}, {2}, 1}, Resistor{{2}, "weak", {2}, {0}, 1e12},
                         CurrentSource{{3}, "I", {0}, {1}, .001}}};
    error(d, ErrorCode::ill_conditioned);
    std::get<Resistor>(d.components[0]).resistance_ohms = 1e-9;
    error(d, ErrorCode::rank_deficient);
}
TEST(Dc, IndependentBranchReferenceForDeterministicNetworks) {
    std::mt19937 random(601);
    for (int trial = 0; trial < 40; ++trial) {
        auto d = divider();
        d.nodes.push_back({{3}, "third"});
        std::get<Resistor>(d.components[1]).resistance_ohms =
            static_cast<double>(10 + random() % 1000);
        std::get<Resistor>(d.components[2]).resistance_ohms =
            static_cast<double>(10 + random() % 1000);
        d.components.push_back(
            Resistor{{13}, "bridge", {1}, {3}, static_cast<double>(10 + random() % 1000)});
        d.components.push_back(
            Resistor{{14}, "cross", {2}, {3}, static_cast<double>(10 + random() % 1000)});
        d.components.push_back(CurrentSource{{15}, "I", {3}, {0}, .003});
        if (trial % 2)
            d.components.push_back(VoltageSource{{16}, "V2", {3}, {2}, 2});
        auto ref = branch_reference(d);
        auto s = solve_dc(Circuit(d));
        for (std::size_t i = 1; i < d.nodes.size(); ++i)
            EXPECT_NEAR(s.node_voltages[i].voltage_volts, ref[i - 1], 1e-10);
        for (std::size_t i = 0; i < d.components.size(); ++i)
            if (const auto* v = std::get_if<VoltageSource>(&d.components[i])) {
                EXPECT_NEAR(current(s, v->id), ref[d.nodes.size() - 1 + i], 1e-12);
            }
        EXPECT_LE(
            s.quality.backward_error,
            policy::backward_threshold(d.nodes.size() - 1 + s.voltage_source_currents.size()));
    }
}
TEST(Dc, MaximumBoundedNetwork) {
    CircuitDefinition d{{{{0}, "g"}}, NodeId{0}, {}};
    for (std::size_t i = 1; i < limits::nodes; ++i) {
        auto id = static_cast<std::uint32_t>(i);
        d.nodes.push_back({{id}, "n" + std::to_string(i)});
        d.components.push_back(Resistor{{id}, "R" + std::to_string(i), {id}, {0}, 1000});
        if (i <= limits::voltage_sources)
            d.components.push_back(
                VoltageSource{{id + 1000}, "V" + std::to_string(i), {id}, {0}, 1});
    }
    auto s = solve_dc(Circuit(d));
    EXPECT_EQ(s.node_voltages.size(), limits::nodes);
    EXPECT_EQ(s.voltage_source_currents.size(), limits::voltage_sources);
}
