#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <map>
#include <openece/project/model.hpp>
#include <schematic/model.hpp>
#include <type_traits>

namespace p = openece::project;
namespace s = openece::schematic;
namespace {
static_assert(!std::is_convertible_v<s::NodeKey, s::ComponentKey>);
static_assert(std::is_same_v<decltype(std::declval<s::SchematicGraph&>().components()),
                             const std::vector<s::Component>&>);

template <class Draft> constexpr s::Analysis analysis() {
    if constexpr (std::is_same_v<Draft, p::DcDraft>)
        return s::Analysis::dc;
    else if constexpr (std::is_same_v<Draft, p::AcDraft>)
        return s::Analysis::ac;
    else
        return s::Analysis::transient;
}
template <class Draft> Draft circuit() {
    Draft d;
    d.nodes = {{{0}, "Ground"}, {{7}, "Supply"}, {{42}, "Output"}, {{99}, "Unused"}};
    d.ground = p::Id{0};
    d.next_node = {100};
    d.next_component = {100};
    using Part = typename decltype(d.components)::value_type;
    auto add = [&](std::uint32_t id, const char* name, const char* kind, std::uint32_t pos,
                   std::uint32_t neg, p::Quantity value) {
        Part c;
        c.id = {id};
        c.name = name;
        c.kind = kind;
        c.positive = p::Id{pos};
        c.negative = p::Id{neg};
        c.value = std::move(value);
        d.components.push_back(std::move(c));
    };
    add(10, "R", "resistor", 7, 42, {"1", "kohm"});
    add(11, "V", "voltage_source", 7, 0, {"10", "V"});
    add(12, "I", "current_source", 42, 0, {"2", "mA"});
    return d;
}
void expect_reference(const s::NodeReference& actual, p::Reference expected, s::Analysis a,
                      const std::vector<p::Node>& nodes) {
    if (!expected) {
        EXPECT_TRUE(std::holds_alternative<s::Unselected>(actual));
        return;
    }
    // Independent linear membership oracle: names, row indices and ground play no role.
    const bool exists =
        std::any_of(nodes.begin(), nodes.end(), [&](const auto& n) { return n.id == *expected; });
    const s::NodeKey key{a, expected->value};
    if (exists)
        EXPECT_EQ(actual, (s::NodeReference{s::DeclaredNode{key}}));
    else
        EXPECT_EQ(actual, (s::NodeReference{s::DanglingNode{key}}));
}
template <class Draft> void expect_graph(const Draft& d, const s::SchematicGraph& g) {
    EXPECT_EQ(g.analysis(), analysis<Draft>());
    ASSERT_EQ(g.nodes().size(), d.nodes.size());
    ASSERT_EQ(g.components().size(), d.components.size());
    expect_reference(g.ground(), d.ground, g.analysis(), d.nodes);
    for (std::size_t i = 0; i < d.nodes.size(); ++i) {
        EXPECT_EQ(g.nodes()[i].key, (s::NodeKey{g.analysis(), d.nodes[i].id.value}));
        EXPECT_EQ(g.nodes()[i].name, d.nodes[i].name);
    }
    for (std::size_t i = 0; i < d.components.size(); ++i) {
        SCOPED_TRACE(i);
        const auto& in = d.components[i];
        const auto& out = g.components()[i];
        EXPECT_EQ(out.key, (s::ComponentKey{g.analysis(), in.id.value}));
        EXPECT_EQ(out.name, in.name);
        EXPECT_EQ(out.kind, in.kind);
        EXPECT_EQ(out.value, (s::RawQuantity{in.value.text, in.value.unit}));
        expect_reference(out.positive, in.positive, g.analysis(), d.nodes);
        expect_reference(out.negative, in.negative, g.analysis(), d.nodes);
        if constexpr (std::is_same_v<Draft, p::AcDraft>) {
            ASSERT_TRUE(out.phase);
            EXPECT_EQ(*out.phase, (s::RawQuantity{in.phase.text, in.phase.unit}));
        } else
            EXPECT_FALSE(out.phase);
        if constexpr (std::is_same_v<Draft, p::TransientDraft>) {
            ASSERT_TRUE(out.source);
            EXPECT_EQ(out.source->mode, in.source.mode);
            ASSERT_EQ(out.source->points.size(), in.source.points.size());
            for (std::size_t j = 0; j < in.source.points.size(); ++j) {
                EXPECT_EQ(
                    out.source->points[j].time,
                    (s::RawQuantity{in.source.points[j].time.text, in.source.points[j].time.unit}));
                EXPECT_EQ(out.source->points[j].value_text, in.source.points[j].value_text);
            }
        } else
            EXPECT_FALSE(out.source);
    }
}
template <class F> void reject(F&& f, s::ProjectionErrorCode code) {
    try {
        f();
        FAIL() << "Accepted ambiguous or excessive projection input";
    } catch (const s::ProjectionError& e) {
        EXPECT_EQ(e.code(), code) << e.what();
    }
}
template <class T> class SchematicProjection : public testing::Test {};
using Drafts = testing::Types<p::DcDraft, p::AcDraft, p::TransientDraft>;
TYPED_TEST_SUITE(SchematicProjection, Drafts);

TYPED_TEST(SchematicProjection, EveryTerminalAndCurrentSourceIsPreserved) {
    const auto d = circuit<TypeParam>();
    const auto before = d;
    const auto g = s::project_schematic(d);
    expect_graph(d, g);
    EXPECT_EQ(d, before);
    EXPECT_EQ(g.components()[2].kind, "current_source");
    EXPECT_EQ(g.nodes()[3].key.value, 99U); // Unused node is not omitted.
}
TYPED_TEST(SchematicProjection, ParallelAndSeriesConnectionsRetainDistinctBranches) {
    auto d = circuit<TypeParam>();
    auto parallel = d.components[0];
    parallel.id = {20};
    parallel.positive = p::Id{42};
    parallel.negative = p::Id{0};
    d.components.push_back(parallel);
    const auto g = s::project_schematic(d);
    expect_graph(d, g);
    EXPECT_EQ(g.components()[2].positive, g.components()[3].positive);
    EXPECT_EQ(g.components()[2].negative, g.components()[3].negative);
    EXPECT_EQ(g.components()[0].negative, g.components()[3].positive);
    EXPECT_NE(g.components()[2].key, g.components()[3].key);
}
TYPED_TEST(SchematicProjection, GroundIsExplicitAndNeverInferredFromZeroOrName) {
    auto d = circuit<TypeParam>();
    d.nodes[1].name = "Ground";
    d.ground = p::Id{7};
    const auto selected = s::project_schematic(d);
    EXPECT_EQ(selected.ground(), (s::NodeReference{s::DeclaredNode{{analysis<TypeParam>(), 7}}}));
    d.ground.reset();
    const auto unselected = s::project_schematic(d);
    EXPECT_TRUE(std::holds_alternative<s::Unselected>(unselected.ground()));
    EXPECT_EQ(unselected.nodes().front().key.value, 0U);
}
TYPED_TEST(SchematicProjection, UnselectedAndDanglingReferencesNeverInventNodes) {
    auto d = circuit<TypeParam>();
    d.components[0].positive.reset();
    d.components[0].negative = p::Id{88};
    d.components[1].positive = p::Id{88};
    d.ground = p::Id{123};
    const auto before = d;
    const auto g = s::project_schematic(d);
    expect_graph(d, g);
    EXPECT_TRUE(std::holds_alternative<s::Unselected>(g.components()[0].positive));
    EXPECT_TRUE(std::holds_alternative<s::DanglingNode>(g.components()[0].negative));
    EXPECT_EQ(g.components()[0].negative, g.components()[1].positive);
    EXPECT_EQ(g.nodes().size(), 4U);
    EXPECT_EQ(d, before);
}
TYPED_TEST(SchematicProjection, SparseMaximumIdsAndDuplicateNamesRemainDistinct) {
    auto d = circuit<TypeParam>();
    constexpr auto high = std::numeric_limits<std::uint32_t>::max();
    d.nodes[2].id = {high};
    d.nodes[0].name = d.nodes[2].name = "same π name";
    d.components[0].id = {high}; // Separate node/component ID namespaces.
    d.components[0].negative = p::Id{high};
    d.components[2].positive = p::Id{high};
    d.components[0].name = d.components[1].name = "same component";
    d.next_node = d.next_component = {p::limits::exhausted_id};
    const auto before = d;
    const auto g = s::project_schematic(d);
    expect_graph(d, g);
    EXPECT_NE(g.nodes()[0].key, g.nodes()[2].key);
    EXPECT_EQ(d, before); // Projection neither allocates nor adjusts counters.
}
TYPED_TEST(SchematicProjection, DeclarationReorderingDoesNotChangeIdentityOrConnections) {
    auto d = circuit<TypeParam>();
    const auto original = s::project_schematic(d);
    std::reverse(d.nodes.begin(), d.nodes.end());
    std::rotate(d.components.begin(), d.components.begin() + 1, d.components.end());
    const auto reordered = s::project_schematic(d);
    expect_graph(d, reordered);
    EXPECT_EQ(reordered.components().front().key.value, 11U);
    std::map<std::uint32_t, s::Component> by_id;
    for (const auto& c : original.components())
        by_id.emplace(c.key.value, c);
    for (const auto& c : reordered.components())
        EXPECT_EQ(c, by_id.at(c.key.value));
}
TYPED_TEST(SchematicProjection, InvalidValuesTiedTerminalsAndFloatingNodesAreNotValidated) {
    auto d = circuit<TypeParam>();
    d.ground.reset();
    d.components[0].value.text = " 1e-\t";
    d.components[0].positive = d.components[0].negative;
    d.components[1].value.text = "";
    d.components[2].value.text = "NaN";
    // A current-source-only connection is retained even though it cannot ground a DC island.
    d.components[2].positive = p::Id{99};
    const auto g = s::project_schematic(d);
    expect_graph(d, g);
    EXPECT_EQ(g.components()[0].positive, g.components()[0].negative);
    EXPECT_EQ(g.components()[0].value.text, " 1e-\t");
}
TYPED_TEST(SchematicProjection, SnapshotOwnsAllDataIndependentlyOfDraftLifetime) {
    const auto g = [] {
        auto d = circuit<TypeParam>();
        auto out = s::project_schematic(d);
        d.nodes.clear();
        d.components[0].name = "Changed";
        d.components.clear();
        return out;
    }();
    const auto copy = g;
    EXPECT_EQ(g, copy);
    EXPECT_EQ(g.nodes()[1].name, "Supply");
    EXPECT_EQ(g.components()[0].name, "R");
    expect_graph(circuit<TypeParam>(), g);
}
TYPED_TEST(SchematicProjection, DuplicateDeclaredIdsAreRejectedWithoutRepair) {
    auto d = circuit<TypeParam>();
    d.nodes.back().id = d.nodes.front().id;
    const auto before = d;
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::duplicate_node_id);
    EXPECT_EQ(d, before);
    d = circuit<TypeParam>();
    d.components.back().id = d.components.front().id;
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::duplicate_component_id);
}
TYPED_TEST(SchematicProjection, StorageLimitsAreCheckedWithoutPhysicsValidation) {
    auto d = circuit<TypeParam>();
    d.nodes.resize(p::limits::circuit_nodes + 1);
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::resource_limit);
    d = circuit<TypeParam>();
    d.components.resize(p::limits::circuit_components + 1);
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::resource_limit);
    d = circuit<TypeParam>();
    d.components[0].name = std::string(p::limits::name_bytes, 'x');
    d.components[1].name = "源 π <b>voltage</b>\n  ";
    d.components[2].name.clear();
    d.nodes[0].name = "<b>Ground π 電流</b>\n  ";
    const auto g = s::project_schematic(d);
    expect_graph(d, g); // Exceeds engineering name limits, but not draft storage limits.
    d.components[0].name += 'x';
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::resource_limit);
}

TEST(SchematicProjection, AnalysisScopesDoNotCollide) {
    const auto dc = s::project_schematic(circuit<p::DcDraft>());
    const auto ac = s::project_schematic(circuit<p::AcDraft>());
    const auto tr = s::project_schematic(circuit<p::TransientDraft>());
    EXPECT_NE(dc.nodes()[0].key, ac.nodes()[0].key);
    EXPECT_NE(ac.nodes()[0].key, tr.nodes()[0].key);
    EXPECT_NE(dc.components()[0].key, ac.components()[0].key);
    EXPECT_NE(dc.components()[0].key, tr.components()[0].key);
    EXPECT_NE(ac.components()[0].key, tr.components()[0].key);
}
TEST(SchematicProjection, AcRawPhasesAreRetainedEvenForInactivePassiveFields) {
    auto d = circuit<p::AcDraft>();
    d.components[0].phase = {" 1e- π ", "deg"};
    d.components[1].phase = {"-450.0000", "deg"};
    d.components[1].value = {" -2.0 ", "mV"};
    const auto g = s::project_schematic(d);
    expect_graph(d, g);
    EXPECT_EQ(g.components()[1].phase->text, "-450.0000");
    EXPECT_EQ(g.components()[1].value.text, " -2.0 ");
}
TEST(SchematicProjection, TransientModesKnotsAndInactiveInvalidDataAreOwnedAndOrdered) {
    auto d = circuit<p::TransientDraft>();
    d.components[0].source = {"constant", {{{"", "ms"}, " 1e- "}}};
    d.components[1].source = {"hold", {{{"2", "ms"}, " 3 "}, {{"1", "us"}, ""}}};
    d.components[2].source = {"linear", {{{"NaN", "s"}, "-2"}, {{"NaN", "s"}, "π"}}};
    d.components[1].value.unit = "mV";
    const auto before = d;
    const auto g = s::project_schematic(d);
    expect_graph(d, g);
    EXPECT_EQ(d, before);
    d.components[1].source.points[0].time.text = "Changed";
    d.components.clear();
    EXPECT_EQ(g.components()[1].source->points[0].time, (s::RawQuantity{"2", "ms"}));
    EXPECT_EQ(g.components()[1].source->points[1].time, (s::RawQuantity{"1", "us"}));
    EXPECT_EQ(g.components()[0].source->points[0].value_text, " 1e- ");
}
TEST(SchematicProjection, EmptyDraftAndDanglingGroundRemainEmpty) {
    const auto defaults = p::default_project();
    const auto g = s::project_schematic(defaults.circuits.transient);
    EXPECT_TRUE(g.nodes().empty());
    EXPECT_TRUE(g.components().empty());
    EXPECT_TRUE(std::holds_alternative<s::Unselected>(g.ground()));
    auto d = defaults.circuits.transient;
    d.ground = p::Id{0};
    const auto missing = s::project_schematic(d);
    EXPECT_TRUE(missing.nodes().empty());
    EXPECT_EQ(missing.ground(), (s::NodeReference{s::DanglingNode{{s::Analysis::transient, 0}}}));
}
TEST(SchematicProjection, SourcePointBudgetsIncludeInactiveRecords) {
    auto d = circuit<p::TransientDraft>();
    d.components[0].source.points.resize(p::limits::transient_source_points);
    expect_graph(d, s::project_schematic(d));
    d.components[0].source.points.emplace_back();
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::resource_limit);
    d = circuit<p::TransientDraft>();
    const auto prototype = d.components[0];
    d.components.clear();
    const auto count =
        p::limits::transient_total_source_points / p::limits::transient_source_points;
    for (std::size_t i = 0; i <= count; ++i) {
        auto c = prototype;
        c.id = {static_cast<std::uint32_t>(i)};
        c.source.points.resize(p::limits::transient_source_points);
        d.components.push_back(std::move(c));
    }
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::resource_limit);
    d.components.pop_back();
    expect_graph(d, s::project_schematic(d));
}
TEST(SchematicProjection, AggregateTextBudgetIsCheckedBeforeGraphMaterialization) {
    auto d = circuit<p::TransientDraft>();
    const auto half = p::limits::cell_bytes / 2;
    d.components[0].source.points.resize(p::limits::transient_source_points);
    for (auto& point : d.components[0].source.points) {
        point.time.text.assign(half, 'x');
        point.value_text.assign(half, 'y');
    }
    reject([&] { s::project_schematic(d); }, s::ProjectionErrorCode::resource_limit);
}

class SchematicFixture : public testing::TestWithParam<const char*> {};
TEST_P(SchematicFixture, CompleteConnectivityAndProjectSerializationArePreserved) {
    const auto path = std::string(OPENECE_SOURCE_DIR) + GetParam();
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file) << path;
    const std::string bytes((std::istreambuf_iterator<char>(file)), {});
    const auto decoded = p::decode_project(bytes);
    const auto& original = decoded.snapshot;
    const auto encoded = p::encode_project(original);
    expect_graph(original.circuits.dc, s::project_schematic(original.circuits.dc));
    expect_graph(original.circuits.ac, s::project_schematic(original.circuits.ac));
    expect_graph(original.circuits.transient, s::project_schematic(original.circuits.transient));
    EXPECT_EQ(p::encode_project(original), encoded);
    EXPECT_EQ(p::decode_project(encoded).snapshot, original);
}
INSTANTIATE_TEST_SUITE_P(
    ShippedCircuitsAndCompatibility, SchematicFixture,
    testing::Values("/examples/dc-divider.openece", "/examples/rc-lowpass.openece",
                    "/examples/series-rlc.openece", "/examples/transient-rc-step.openece",
                    "/examples/transient-rl-response.openece",
                    "/examples/transient-rlc-damping.openece",
                    "/examples/transient-source-breakpoints.openece",
                    "/examples/intentionally-incomplete.openece",
                    "/tests/fixtures/project-v1.openece", "/tests/fixtures/project-v2.openece",
                    "/tests/fixtures/v0.9/complete.openece",
                    "/tests/fixtures/v0.9/incomplete.openece"));
} // namespace
