#include <gtest/gtest.h>
#include <limits>
#include <openece/circuits/ac/circuit.hpp>
using namespace openece::circuits;
namespace ac = openece::circuits::ac;
namespace {
ac::CircuitDefinition example() {
    return {{{{42}, "supply"}, {{7}, "ground"}},
            NodeId{7},
            {ac::VoltageSource{{1}, "V", {42}, {7}, {3, 4}}, Resistor{{2}, "R", {42}, {7}, 1000},
             ac::Capacitor{{3}, "C", {42}, {7}, 1e-6}, ac::Inductor{{4}, "L", {42}, {7}, 1e-3},
             ac::CurrentSource{{5}, "I", {7}, {42}, {-0.001, 0.002}}}};
}
void expect_error(const ac::CircuitDefinition& d, ErrorCode code) {
    try {
        ac::Circuit circuit(d);
        FAIL() << "Expected structural failure";
    } catch (const CircuitError& e) {
        EXPECT_EQ(e.code(), code);
    }
}
} // namespace
TEST(AcModel, OwnsSnapshotPreservesPhasorsAndDeclarationOrder) {
    auto draft = example();
    const ac::Circuit c(draft);
    auto copy = c;
    draft.nodes[0].name = "edited";
    std::get<ac::VoltageSource>(draft.components[0]).voltage_volts_rms = {0, 0};
    EXPECT_EQ(c.definition().nodes[0].id, NodeId{42});
    EXPECT_EQ(c.definition().nodes[0].name, "supply");
    EXPECT_EQ(c.definition().ground, NodeId{7});
    EXPECT_EQ(std::get<ac::VoltageSource>(copy.definition().components[0]).voltage_volts_rms,
              ac::Phasor(3, 4));
    EXPECT_EQ(std::get<ac::CurrentSource>(c.definition().components[4]).current_amperes_rms,
              ac::Phasor(-0.001, 0.002));
    EXPECT_EQ(c.definition().components.size(), 5u);
}
TEST(AcModel, StableIdsExplicitGroundAndForwardReferences) {
    auto d = example();
    d.nodes = {{{7}, "ground"}, {{42}, "supply"}};
    std::get<ac::Inductor>(d.components[3]).id = {std::numeric_limits<std::uint32_t>::max()};
    EXPECT_NO_THROW(ac::Circuit{d});
    EXPECT_NO_THROW(ac::Circuit(ac::CircuitDefinition{{{{0}, "GND"}}, NodeId{0}, {}}));
    d.ground.reset();
    expect_error(d, ErrorCode::missing_ground);
    d.ground = NodeId{88};
    expect_error(d, ErrorCode::missing_ground);
    expect_error({}, ErrorCode::missing_ground);
}
TEST(AcModel, RejectsDuplicateIdsAcrossAllComponentKinds) {
    auto d = example();
    d.nodes[1].id = d.nodes[0].id;
    expect_error(d, ErrorCode::duplicate_id);
    for (std::size_t i = 1; i < example().components.size(); ++i) {
        d = example();
        std::visit([](auto& c) { c.id = {1}; }, d.components[i]);
        expect_error(d, ErrorCode::duplicate_id);
    }
}
TEST(AcModel, NamesAreExactBoundedAndUniqueWithinEachList) {
    auto d = example();
    d.nodes[0].name = "R"; // Node and component names have separate namespaces.
    d.nodes[1].name = " R ";
    EXPECT_NO_THROW(ac::Circuit{d});
    for (const auto& bad :
         {std::string{}, std::string("ground"), std::string(ac::limits::name_bytes + 1, 'x')}) {
        d = example();
        d.nodes[0].name = bad;
        expect_error(d, ErrorCode::invalid_name);
    }
    for (const auto& bad :
         {std::string{}, std::string("V"), std::string(ac::limits::name_bytes + 1, 'x')}) {
        d = example();
        std::get<ac::Capacitor>(d.components[2]).name = bad;
        expect_error(d, ErrorCode::invalid_name);
    }
    d = example();
    d.nodes[0].name.assign(ac::limits::name_bytes, 'n');
    std::get<ac::Capacitor>(d.components[2]).name.assign(ac::limits::name_bytes, 'c');
    EXPECT_NO_THROW(ac::Circuit{d});
}
TEST(AcModel, InvalidTerminalsCarryComponentAndNodeIds) {
    for (std::size_t i = 0; i < example().components.size(); ++i) {
        auto d = example();
        std::visit([](auto& c) { c.positive = {99}; }, d.components[i]);
        try {
            ac::Circuit c(d);
            FAIL();
        } catch (const CircuitError& e) {
            EXPECT_EQ(e.code(), ErrorCode::invalid_terminal);
            ASSERT_EQ(e.components().size(), 1u);
            EXPECT_EQ(e.components()[0], ComponentId{static_cast<std::uint32_t>(i + 1)});
            ASSERT_EQ(e.nodes().size(), 2u);
            EXPECT_EQ(e.nodes()[0], NodeId{99});
        }
        std::visit([](auto& c) { c.positive = c.negative; }, d.components[i]);
        expect_error(d, ErrorCode::invalid_terminal);
    }
}
TEST(AcModel, PassiveValueBoundaries) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    auto check = [&](double low, double high, auto make_part) {
        for (double value : {0.0, -1.0, low / 2, high * 2, nan, inf, -inf}) {
            auto d = example();
            d.components[1] = make_part(value);
            expect_error(d, ErrorCode::invalid_value);
        }
        for (double value : {low, high}) {
            auto d = example();
            d.components[1] = make_part(value);
            EXPECT_NO_THROW(ac::Circuit{d});
        }
    };
    check(ac::limits::resistance_min, ac::limits::resistance_max,
          [](double x) { return Resistor{{2}, "R", {42}, {7}, x}; });
    check(ac::limits::capacitance_min, ac::limits::capacitance_max,
          [](double x) { return ac::Capacitor{{2}, "C2", {42}, {7}, x}; });
    check(ac::limits::inductance_min, ac::limits::inductance_max,
          [](double x) { return ac::Inductor{{2}, "L2", {42}, {7}, x}; });
}
TEST(AcModel, SourceLimitsApplyToMagnitudeNotRectangularComponents) {
    const double low = ac::limits::source_nonzero_min, high = ac::limits::source_max;
    for (ac::Phasor value : {ac::Phasor{},
                             {low, 0},
                             {0, -low},
                             {-high, 0},
                             {0, high},
                             {0.8 * low, 0.8 * low},
                             {0.3 * high, 0.4 * high}}) {
        auto d = example();
        std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = value;
        std::get<ac::CurrentSource>(d.components[4]).current_amperes_rms = value;
        EXPECT_NO_THROW(ac::Circuit{d});
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (ac::Phasor value :
         {ac::Phasor(low / 2, 0),
          {high, high},
          {nan, 0},
          {0, nan},
          {inf, 0},
          {0, -inf},
          {inf, nan},
          {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()}}) {
        auto d = example();
        std::get<ac::VoltageSource>(d.components[0]).voltage_volts_rms = value;
        expect_error(d, ErrorCode::invalid_value);
        d = example();
        std::get<ac::CurrentSource>(d.components[4]).current_amperes_rms = value;
        expect_error(d, ErrorCode::invalid_value);
    }
}
TEST(AcModel, StructuralValidationDefersFrequencyDependentSolvability) {
    auto d = example();
    d.components.clear();
    EXPECT_NO_THROW(ac::Circuit{d}); // Floating reference is a solve error.
    d = example();
    d.components.push_back(ac::VoltageSource{{6}, "conflict", {42}, {7}, {3, -4}});
    EXPECT_NO_THROW(ac::Circuit{d});
}
TEST(AcModel, ResourceLimitsAndLargestStructurallyValidDefinition) {
    auto d = example();
    d.nodes.resize(ac::limits::nodes + 1);
    expect_error(d, ErrorCode::resource_limit);
    d = example();
    d.components.resize(ac::limits::components + 1);
    expect_error(d, ErrorCode::resource_limit);
    d = example();
    d.components.clear();
    for (std::size_t i = 0; i <= ac::limits::voltage_sources; ++i)
        d.components.push_back(ac::VoltageSource{
            {static_cast<std::uint32_t>(i)}, "V" + std::to_string(i), {42}, {7}, {1, 0}});
    expect_error(d, ErrorCode::resource_limit);
    d = {};
    d.ground = NodeId{0};
    for (std::size_t i = 0; i < ac::limits::nodes; ++i)
        d.nodes.push_back({{static_cast<std::uint32_t>(i)}, "N" + std::to_string(i)});
    for (std::size_t i = 0; i < ac::limits::components; ++i) {
        const ComponentId id{static_cast<std::uint32_t>(i)};
        const auto name = "P" + std::to_string(i);
        if (i < ac::limits::voltage_sources)
            d.components.push_back(ac::VoltageSource{id, name, {1}, {0}, {1, 0}});
        else
            d.components.push_back(Resistor{id, name, {1}, {0}, 1});
    }
    EXPECT_NO_THROW(ac::Circuit{d});
}
