#include <gtest/gtest.h>
#include <limits>
#include <openece/circuits/circuit.hpp>
using namespace openece::circuits;
namespace {
CircuitDefinition load() {
    return {{{{42}, "supply"}, {{7}, "ground"}},
            NodeId{7},
            {VoltageSource{{1}, "V1", {42}, {7}, 10}, Resistor{{2}, "R1", {42}, {7}, 1000}}};
}
void expect_error(const CircuitDefinition& d, ErrorCode code) {
    try {
        Circuit c(d);
        FAIL() << "Expected validation failure";
    } catch (const CircuitError& e) {
        EXPECT_EQ(e.code(), code);
    }
}
} // namespace
TEST(CircuitModel, OwnsSnapshotAndExplicitGround) {
    auto d = load();
    Circuit c(d);
    d.nodes[0].name = "changed";
    EXPECT_EQ(c.definition().nodes[0].name, "supply");
    EXPECT_EQ(c.definition().ground, NodeId{7});
    EXPECT_NO_THROW(Circuit(CircuitDefinition{{{{9}, "GND"}}, NodeId{9}, {}}));
}
TEST(CircuitModel, RejectsMalformedDefinitions) {
    auto d = load();
    d.nodes[1].id = {42};
    expect_error(d, ErrorCode::duplicate_id);
    d = load();
    std::get<Resistor>(d.components[1]).id = {1};
    expect_error(d, ErrorCode::duplicate_id);
    d = load();
    d.nodes[0].name.clear();
    expect_error(d, ErrorCode::invalid_name);
    d = load();
    d.nodes[0].name = d.nodes[1].name;
    expect_error(d, ErrorCode::invalid_name);
    d = load();
    std::get<Resistor>(d.components[1]).name = "V1";
    expect_error(d, ErrorCode::invalid_name);
    d = load();
    d.nodes[0].name.assign(limits::name_bytes + 1, 'x');
    expect_error(d, ErrorCode::invalid_name);
    d = load();
    d.ground.reset();
    expect_error(d, ErrorCode::missing_ground);
    d = load();
    d.ground = NodeId{99};
    expect_error(d, ErrorCode::missing_ground);
    d = load();
    std::get<Resistor>(d.components[1]).positive = {99};
    expect_error(d, ErrorCode::invalid_terminal);
    d = load();
    std::get<Resistor>(d.components[1]).positive = {7};
    expect_error(d, ErrorCode::invalid_terminal);
    d = load();
    d.nodes[0].name = " ground";
    EXPECT_NO_THROW(Circuit{d});
}
TEST(CircuitModel, FiniteValuesAndBoundaries) {
    for (double v :
         {0.0, -1.0, limits::resistance_min / 2, limits::resistance_max * 2,
          std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        auto d = load();
        std::get<Resistor>(d.components[1]).resistance_ohms = v;
        expect_error(d, ErrorCode::invalid_value);
    }
    for (double v : {limits::resistance_min, limits::resistance_max}) {
        auto d = load();
        std::get<Resistor>(d.components[1]).resistance_ohms = v;
        EXPECT_NO_THROW(Circuit{d});
    }
    for (double v : {0.0, limits::source_nonzero_min, -limits::source_nonzero_min,
                     limits::source_max, -limits::source_max}) {
        auto d = load();
        std::get<VoltageSource>(d.components[0]).voltage_volts = v;
        EXPECT_NO_THROW(Circuit{d});
    }
    for (double v : {limits::source_nonzero_min / 2, limits::source_max * 2,
                     std::numeric_limits<double>::quiet_NaN()}) {
        auto d = load();
        d.components[0] = CurrentSource{{1}, "I1", {42}, {7}, v};
        expect_error(d, ErrorCode::invalid_value);
    }
}
TEST(CircuitModel, DefersElectricalSolvability) {
    auto d = load();
    d.components.clear();
    EXPECT_NO_THROW(Circuit{d});
    d = load();
    d.components.push_back(VoltageSource{{3}, "V2", {42}, {7}, 12});
    EXPECT_NO_THROW(Circuit{d});
}
TEST(CircuitModel, ResourceLimits) {
    auto d = load();
    d.nodes.resize(limits::nodes + 1);
    expect_error(d, ErrorCode::resource_limit);
    d = load();
    d.components.resize(limits::components + 1);
    expect_error(d, ErrorCode::resource_limit);
    d = load();
    d.components.clear();
    for (std::size_t i = 0; i <= limits::voltage_sources; ++i)
        d.components.push_back(
            VoltageSource{{static_cast<std::uint32_t>(i)}, "V" + std::to_string(i), {42}, {7}, 1});
    expect_error(d, ErrorCode::resource_limit);
}
