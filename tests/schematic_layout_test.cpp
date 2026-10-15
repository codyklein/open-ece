#include "schematic_layout_cases.hpp"
#include <algorithm>
#include <functional>
#include <gtest/gtest.h>
#include <map>
#include <random>

namespace s = openece::schematic;
namespace p = openece::project;
namespace cases = schematic_cases;
namespace {
s::LayoutDiagram accepted(const s::SchematicGraph& g, const s::LayoutMetrics& m = {}) {
    auto result = s::layout_schematic(g, m);
    if (const auto* failure = std::get_if<s::LayoutFailure>(&result)) {
        ADD_FAILURE() << failure->message;
        throw std::runtime_error(failure->message);
    }
    auto d = std::get<s::LayoutDiagram>(std::move(result));
    const auto verified = s::verify_layout(g, d, m);
    EXPECT_TRUE(verified.valid()) << (verified.failure ? verified.failure->message : "");
    EXPECT_LE(d.operations, s::layout_limits::operations);
    EXPECT_LE(d.working_bytes_bound, s::layout_limits::working_bytes);
    return d;
}
void exact(const s::SchematicGraph& g, const s::LayoutDiagram& d) {
    ASSERT_EQ(g.components().size(), d.components.size());
    ASSERT_EQ(g.nodes().size(), d.nets.size());
    ASSERT_EQ(g.components().size() * 2, d.terminals.size());
    for (const auto& c : g.components()) {
        const auto found = std::find_if(d.components.begin(), d.components.end(),
                                        [&](const auto& x) { return x.source.key == c.key; });
        ASSERT_NE(found, d.components.end());
        EXPECT_EQ(found->source, c);
        for (auto side : {s::TerminalSide::positive, s::TerminalSide::negative}) {
            const auto t = std::find_if(d.terminals.begin(), d.terminals.end(), [&](const auto& x) {
                return x.key == s::TerminalKey{c.key, side};
            });
            ASSERT_NE(t, d.terminals.end());
            EXPECT_EQ(t->reference, side == s::TerminalSide::positive ? c.positive : c.negative);
            std::size_t count = 0;
            for (const auto& wire : d.wires)
                if (wire.terminal == t->key) {
                    ++count;
                    ASSERT_TRUE(std::holds_alternative<s::DeclaredNode>(t->reference));
                    EXPECT_EQ(wire.net, std::get<s::DeclaredNode>(t->reference).key);
                    EXPECT_EQ(wire.points.front(), t->anchor);
                }
            EXPECT_EQ(count, std::holds_alternative<s::DeclaredNode>(t->reference) ? 1U : 0U);
        }
    }
}
struct ExampleCase {
    const char* name;
    s::Analysis analysis;
};
class SchematicLayoutExample : public testing::TestWithParam<ExampleCase> {};
TEST_P(SchematicLayoutExample, ShippedCircuitsAreStructuredAndPreserveEveryTerminal) {
    const auto input = cases::example(GetParam().name);
    const auto before = p::encode_project(input);
    const auto g = [&] {
        switch (GetParam().analysis) {
        case s::Analysis::dc:
            return s::project_schematic(input.circuits.dc);
        case s::Analysis::ac:
            return s::project_schematic(input.circuits.ac);
        default:
            return s::project_schematic(input.circuits.transient);
        }
    }();
    const auto original = g;
    const auto d = accepted(g);
    EXPECT_EQ(d.strategy, s::LayoutStrategy::structured);
    EXPECT_TRUE(d.crossings.empty());
    EXPECT_FALSE(d.junctions.empty());
    exact(g, d);
    EXPECT_EQ(d, accepted(g));
    EXPECT_EQ(g, original);
    EXPECT_EQ(before, p::encode_project(input));
}
INSTANTIATE_TEST_SUITE_P(
    Shipped, SchematicLayoutExample,
    testing::Values(ExampleCase{"dc-divider", s::Analysis::dc},
                    ExampleCase{"rc-lowpass", s::Analysis::ac},
                    ExampleCase{"series-rlc", s::Analysis::ac},
                    ExampleCase{"transient-rc-step", s::Analysis::transient},
                    ExampleCase{"transient-rl-response", s::Analysis::transient},
                    ExampleCase{"transient-rlc-damping", s::Analysis::transient},
                    ExampleCase{"transient-source-breakpoints", s::Analysis::transient}));

TEST(SchematicLayout, SingleResistorSourceAndParallelBranches) {
    auto draft = cases::divider();
    draft.nodes.resize(2);
    draft.components.resize(2);
    for (auto& c : draft.components) {
        c.positive = draft.nodes[1].id;
        c.negative = draft.nodes[0].id;
    }
    auto extra = draft.components.back();
    extra.id = {200};
    extra.kind = "current_source";
    draft.components.push_back(extra);
    const auto g = s::project_schematic(draft);
    const auto d = accepted(g);
    exact(g, d);
    EXPECT_EQ(d.strategy, s::LayoutStrategy::structured);
    EXPECT_EQ(d.components.size(), 3U);
    EXPECT_EQ(d.components[2].symbol, s::SymbolKind::current_source);
}
TEST(SchematicLayout, SimpleOpenSeriesChainNeedsNoSolverOrGround) {
    auto draft = cases::divider();
    draft.components.erase(draft.components.begin());
    draft.ground.reset();
    const auto g = s::project_schematic(draft);
    const auto d = accepted(g);
    EXPECT_EQ(d.strategy, s::LayoutStrategy::structured);
    EXPECT_FALSE(d.ground);
    exact(g, d);
}
TEST(SchematicLayout, ReversedPolarityHasDistinctAnchorsAndUpwardCurrentSense) {
    auto draft = cases::divider();
    std::swap(draft.components.back().positive, draft.components.back().negative);
    const auto g = s::project_schematic(draft);
    const auto d = accepted(g);
    exact(g, d);
    const auto part = std::find_if(d.components.begin(), d.components.end(), [&](const auto& x) {
        return x.source.key.value == draft.components.back().id.value;
    });
    ASSERT_NE(part, d.components.end());
    EXPECT_EQ(part->direction, s::Direction::up);
    EXPECT_GT(part->positive.y, part->negative.y);
}
TEST(SchematicLayout, WheatstoneBridgeUsesExplicitNetRailsAndCrossings) {
    const auto g = s::project_schematic(cases::wheatstone());
    const auto d = accepted(g);
    exact(g, d);
    EXPECT_EQ(d.strategy, s::LayoutStrategy::net_rails);
    EXPECT_FALSE(d.crossings.empty());
    for (const auto& crossing : d.crossings)
        EXPECT_NE(d.wires[crossing.first_wire].net, d.wires[crossing.second_wire].net);
    for (const auto& junction : d.junctions)
        for (auto wire : junction.wires)
            EXPECT_EQ(junction.net, d.wires[wire].net);
}
TEST(SchematicLayout, NonplanarK33AndDisconnectedBranchesFallBackCompletely) {
    auto draft = cases::nonplanar();
    const auto first = s::project_schematic(draft);
    EXPECT_EQ(accepted(first).strategy, s::LayoutStrategy::net_rails);
    draft.nodes.push_back({{99999}, "island +"});
    draft.nodes.push_back({{99998}, "island -"});
    draft.components.push_back(
        {{33333}, "Island current", "current_source", p::Id{99999}, p::Id{99998}, {"1", "mA"}});
    const auto g = s::project_schematic(draft);
    const auto d = accepted(g);
    exact(g, d);
    EXPECT_EQ(d.strategy, s::LayoutStrategy::net_rails);
}
TEST(SchematicLayout, IncompleteAndUnknownDraftsRetainAllInspectionData) {
    auto draft = cases::divider();
    draft.ground = p::Id{7654321};
    draft.nodes.push_back({{99}, "Unused π"});
    draft.components[0].positive.reset();
    draft.components[0].negative = p::Id{7654321};
    draft.components[0].name = " <b>源 π</b> \t";
    draft.components[0].kind = "not-a-component";
    draft.components[0].value = {" 1e- ", "not-a-unit"};
    draft.components[1].name.clear();
    draft.components[2].name.clear();
    const auto g = s::project_schematic(draft);
    const auto d = accepted(g);
    exact(g, d);
    EXPECT_EQ(d.strategy, s::LayoutStrategy::net_rails);
    EXPECT_FALSE(d.ground);
    EXPECT_EQ(d.components[0].symbol, s::SymbolKind::generic);
    EXPECT_EQ(d.components[0].source.name, draft.components[0].name);
    EXPECT_EQ(d.components[0].source.value.text, " 1e- ");
    EXPECT_TRUE(std::holds_alternative<s::Unselected>(d.terminals[0].reference));
    EXPECT_TRUE(std::holds_alternative<s::DanglingNode>(d.terminals[1].reference));
}
TEST(SchematicLayout, ZeroIsAnOrdinaryNodeAndGroundCanBeAnIntermediateNode) {
    auto draft = cases::divider();
    draft.ground = draft.nodes.back().id;
    const auto g = s::project_schematic(draft);
    const auto d = accepted(g);
    ASSERT_TRUE(d.ground);
    EXPECT_EQ(d.ground->net.value, draft.ground->value);
    exact(g, d);
    draft.ground.reset();
    const auto no_ground = accepted(s::project_schematic(draft));
    EXPECT_FALSE(no_ground.ground);
}
TEST(SchematicLayout, EmptyAndUnusedNodesAreRepresentedWithoutFabrication) {
    p::DcDraft draft;
    const auto empty = accepted(s::project_schematic(draft));
    EXPECT_TRUE(empty.nets.empty());
    EXPECT_EQ(empty.bounds, s::Rectangle{});
    draft.nodes = {{{0}, "Not implicitly ground"}, {{200}, "unused"}};
    const auto g = s::project_schematic(draft);
    const auto d = accepted(g);
    EXPECT_EQ(d.nets.size(), 2U);
    EXPECT_EQ(d.labels.size(), 2U);
    EXPECT_FALSE(d.ground);
    exact(g, d);
}
TEST(SchematicLayout, MaximumSparseIdsAndDenseSameNodeTerminalPairsAreBounded) {
    for (bool dense : {false, true}) {
        const auto input = cases::maximum(dense);
        const auto g = s::project_schematic(input);
        const auto d = accepted(g);
        exact(g, d);
        EXPECT_EQ(d.strategy, s::LayoutStrategy::net_rails);
        EXPECT_EQ(d.components.size(), 128U);
        EXPECT_EQ(d.terminals.size(), 256U);
        EXPECT_EQ(d.nets.size(), 32U);
        EXPECT_LE(d.crossings.size(), s::layout_limits::crossings);
        if (dense) {
            EXPECT_EQ(d.crossings.size(), 7936U);
        }
    }
}
TEST(SchematicLayout, CallerMeasurementsAccommodateLongUnicodeLabelsWithoutElision) {
    auto draft = cases::divider();
    draft.components[0].name.assign(p::limits::name_bytes, 'x');
    draft.nodes[0].name = "Ground π 電流";
    const auto g = s::project_schematic(draft);
    auto metrics = cases::measured(g);
    metrics.labels[0].extent = {600, 80};
    metrics.labels[3].extent = {40000, 48};
    const auto d = accepted(g, metrics);
    exact(g, d);
    EXPECT_EQ(d.components[0].source.name, draft.components[0].name);
    const auto label = std::find_if(d.labels.begin(), d.labels.end(), [&](const auto& x) {
        return x.owner == s::LabelOwner{g.components()[0].key} &&
               x.role == s::LabelRole::component_name;
    });
    ASSERT_NE(label, d.labels.end());
    EXPECT_EQ(label->text,
              s::component_label_text(g.components()[0], s::LabelRole::component_name));
    EXPECT_EQ(label->bounds.size.width, 40000);
}
TEST(SchematicLayout, FullyMeasuredMaximumDraftFitsDefaultWorkAndMemoryBudgets) {
    const auto g = s::project_schematic(cases::maximum(true));
    const auto m = cases::measured(g);
    const auto d = accepted(g, m);
    exact(g, d);
}
TEST(SchematicLayout, LargestRetainedSourceRecordsAndCompletelyOpenDraftsFitStorageBudgets) {
    const auto g = s::project_schematic(cases::large_sources());
    const auto d = accepted(g);
    exact(g, d);
    EXPECT_LT(d.working_bytes_bound, s::layout_limits::working_bytes);
    p::DcDraft open;
    for (std::uint32_t i = 0; i < p::limits::circuit_components; ++i) {
        p::DcComponent c;
        c.id = {i};
        c.kind = "unknown";
        if (i % 2)
            c.negative = p::Id{std::numeric_limits<std::uint32_t>::max()};
        open.components.push_back(std::move(c));
    }
    const auto og = s::project_schematic(open);
    const auto od = accepted(og);
    exact(og, od);
    EXPECT_TRUE(od.nets.empty());
    EXPECT_TRUE(od.wires.empty());
    EXPECT_EQ(od.terminals.size(), 256U);
}
TEST(SchematicLayout, DeclarationReorderingProducesIdenticalGeometryAcrossAllAnalyses) {
    auto input = cases::example("transient-rc-step");
    auto check = [](auto draft) {
        const auto original = accepted(s::project_schematic(draft));
        std::reverse(draft.nodes.begin(), draft.nodes.end());
        std::reverse(draft.components.begin(), draft.components.end());
        auto reordered = accepted(s::project_schematic(draft));
        // Sorting comparison counts may differ with input order; geometry and all
        // electrical/inspection metadata must still match exactly.
        reordered.operations = original.operations;
        EXPECT_EQ(original, reordered);
    };
    check(input.circuits.dc);
    check(input.circuits.ac);
    check(input.circuits.transient);
    check(cases::wheatstone());
}
TEST(SchematicLayout, AnalysisNamespacesAndOwnedLifetimeRemainIndependent) {
    auto input = cases::example("dc-divider");
    const auto dc = accepted(s::project_schematic(input.circuits.dc));
    const auto ac = accepted(s::project_schematic(input.circuits.ac));
    EXPECT_NE(dc.nets[0].source.key, ac.nets[0].source.key);
    input.circuits.dc = {};
    EXPECT_EQ(dc.components.size(), 3U);
    EXPECT_FALSE(dc.components[0].source.name.empty());
}
TEST(SchematicLayout, TransientSourceRecordsAndAcPhasesAreFrozenWithoutCompanionSubstitution) {
    auto input = cases::example("transient-source-breakpoints");
    input.circuits.transient.components[0].source.points[0].value_text = " 1e- ";
    const auto g = s::project_schematic(input.circuits.transient);
    const auto d = accepted(g);
    exact(g, d);
    for (const auto& c : d.components) {
        if (c.source.kind == "capacitor") {
            EXPECT_EQ(c.symbol, s::SymbolKind::capacitor);
        }
    }
    auto ac = cases::example("rc-lowpass").circuits.ac;
    ac.components[0].phase.text = "invalid π";
    const auto ag = s::project_schematic(ac);
    exact(ag, accepted(ag));
}

struct LimitCase {
    const char* name;
    std::function<void(s::LayoutLimits&)> change;
};
class SchematicLayoutLimit : public testing::TestWithParam<LimitCase> {};
TEST_P(SchematicLayoutLimit, ExcessBudgetReturnsTypedFailureWithoutPartialDiagram) {
    const auto g = s::project_schematic(cases::wheatstone());
    s::LayoutLimits limits;
    GetParam().change(limits);
    const auto out = s::layout_schematic(g, {}, limits);
    ASSERT_TRUE(std::holds_alternative<s::LayoutFailure>(out));
    EXPECT_EQ(std::get<s::LayoutFailure>(out).code, s::LayoutErrorCode::resource_limit);
}
INSTANTIATE_TEST_SUITE_P(
    Budgets, SchematicLayoutLimit,
    testing::Values(LimitCase{"nodes", [](auto& l) { l.nodes = 1; }},
                    LimitCase{"components", [](auto& l) { l.components = 1; }},
                    LimitCase{"terminals", [](auto& l) { l.terminals = 1; }},
                    LimitCase{"segments", [](auto& l) { l.segments = 1; }},
                    LimitCase{"junctions", [](auto& l) { l.junctions = 0; }},
                    LimitCase{"crossings", [](auto& l) { l.crossings = 0; }},
                    LimitCase{"labels", [](auto& l) { l.labels = 1; }},
                    LimitCase{"primitives", [](auto& l) { l.primitives = 1; }},
                    LimitCase{"annotation primitives", [](auto& l) { l.primitives = 90; }},
                    LimitCase{"operations", [](auto& l) { l.operations = 1; }},
                    LimitCase{"memory", [](auto& l) { l.working_bytes = 1; }},
                    LimitCase{"coordinates", [](auto& l) { l.coordinate = 128; }},
                    LimitCase{"hard ceiling", [](auto& l) {
                                  l.segments = std::numeric_limits<std::size_t>::max();
                              }}));

TEST(SchematicLayout, InvalidAndOverflowingMeasurementsAreRejectedBeforeArithmetic) {
    const auto g = s::project_schematic(cases::divider());
    for (auto value :
         {s::Coordinate{-1}, s::Coordinate{0}, std::numeric_limits<s::Coordinate>::max()}) {
        s::LayoutMetrics m;
        m.grid = value;
        const auto result = s::layout_schematic(g, m);
        ASSERT_TRUE(std::holds_alternative<s::LayoutFailure>(result));
    }
    auto m = cases::measured(g);
    m.labels.push_back(m.labels[0]);
    EXPECT_TRUE(std::holds_alternative<s::LayoutFailure>(s::layout_schematic(g, m)));
    m = {};
    m.labels.push_back({s::NodeKey{s::Analysis::dc, 9999}, s::LabelRole::node_name, {20, 20}});
    EXPECT_TRUE(std::holds_alternative<s::LayoutFailure>(s::layout_schematic(g, m)));
    m = {};
    m.component_label = {s::layout_limits::coordinate, s::layout_limits::coordinate};
    EXPECT_TRUE(std::holds_alternative<s::LayoutFailure>(s::layout_schematic(g, m)));
}
TEST(SchematicLayout, BoundedRandomDraftsAreCompleteAndDeterministic) {
    std::mt19937 random(1729);
    for (std::uint32_t trial = 0; trial < 24; ++trial) {
        p::DcDraft draft;
        for (std::uint32_t i = 0; i < 1 + trial % 16; ++i)
            draft.nodes.push_back({{i * 17000}, "same π"});
        if (trial % 3)
            draft.ground = draft.nodes.back().id;
        for (std::uint32_t i = 0; i < 1 + trial; ++i) {
            p::DcComponent c;
            c.id = {i * 31};
            c.kind = i % 4 ? "resistor" : "unknown";
            c.value.text = "1e-";
            c.positive = draft.nodes[random() % draft.nodes.size()].id;
            c.negative = draft.nodes[random() % draft.nodes.size()].id;
            if (i % 9 == 0)
                c.positive.reset();
            if (i % 11 == 0)
                c.negative = p::Id{99999};
            draft.components.push_back(std::move(c));
        }
        const auto g = s::project_schematic(draft);
        const auto before = g;
        const auto d = accepted(g);
        exact(g, d);
        EXPECT_EQ(d, accepted(g));
        EXPECT_EQ(g, before);
    }
}

TEST(SchematicLayout, StructuredCoordinateFailureFallsBackInsteadOfReturningPartialGeometry) {
    auto draft = cases::divider();
    draft.nodes.resize(2);
    draft.components.clear();
    for (std::uint32_t i = 0; i < p::limits::circuit_components; ++i)
        draft.components.push_back(
            {{i}, "branch", "resistor", draft.nodes[1].id, draft.nodes[0].id, {"1", "ohm"}});
    const auto g = s::project_schematic(draft);
    s::LayoutMetrics metrics;
    metrics.component_label = {12000, 20};
    const auto d = accepted(g, metrics);
    exact(g, d);
    EXPECT_EQ(d.strategy, s::LayoutStrategy::net_rails);
    EXPECT_TRUE(std::any_of(d.diagnostics.begin(), d.diagnostics.end(), [](const auto& x) {
        return x.find("Structured geometry rejected") != std::string::npos;
    }));
}
TEST(SchematicLayout, UnknownKindsAndEmptyNamesRemainDeterministicUnderReordering) {
    auto draft = cases::wheatstone();
    for (auto& c : draft.components) {
        c.name.clear();
        c.kind = "unknown π";
    }
    const auto original = accepted(s::project_schematic(draft));
    std::reverse(draft.components.begin(), draft.components.end());
    auto reordered = accepted(s::project_schematic(draft));
    reordered.operations = original.operations;
    EXPECT_EQ(original, reordered);
    for (const auto& part : original.components)
        EXPECT_EQ(part.symbol, s::SymbolKind::generic);
}
TEST(SchematicLayout, LabelFormattingExposesIdsAndPreservesEngineeringMetadata) {
    const auto g = s::project_schematic(cases::divider());
    EXPECT_EQ(s::node_label_text(g.nodes()[0]), "Ground [0]");
    EXPECT_EQ(s::component_label_text(g.components()[0], s::LabelRole::component_name), "V1 [0]");
    EXPECT_EQ(s::component_label_text(g.components()[0], s::LabelRole::component_value), "10 V");
    EXPECT_THROW(s::component_label_text(g.components()[0], s::LabelRole::node_name),
                 std::invalid_argument);
}
TEST(SchematicLayout, BothSchemasAndPinnedInvalidProjectsRemainUnchangedAndOwned) {
    for (const auto* name :
         {"/tests/fixtures/project-v1.openece", "/tests/fixtures/project-v2.openece",
          "/tests/fixtures/v0.9/complete.openece", "/tests/fixtures/v0.9/incomplete.openece"}) {
        std::ifstream file(std::string(OPENECE_SOURCE_DIR) + name, std::ios::binary);
        ASSERT_TRUE(file);
        const auto project =
            p::decode_project(std::string(std::istreambuf_iterator<char>(file), {})).snapshot;
        const auto before = p::encode_project(project);
        for (const auto& graph :
             {s::project_schematic(project.circuits.dc), s::project_schematic(project.circuits.ac),
              s::project_schematic(project.circuits.transient)}) {
            const auto original = graph;
            const auto d = accepted(graph);
            exact(graph, d);
            EXPECT_EQ(graph, original);
        }
        EXPECT_EQ(p::encode_project(project), before);
        EXPECT_EQ(p::decode_project(before).snapshot, project);
    }
}
TEST(SchematicLayout, VerifierDistinguishesDisconnectedNetAndForeignNetContact) {
    const auto graph = s::project_schematic(cases::wheatstone());
    auto d = accepted(graph);
    d.wires.push_back(
        {d.nets[0].source.key, s::WireRole::backbone, {}, {{10000, 10000}, {10000, 10080}}});
    auto report = s::verify_layout(graph, d, {});
    ASSERT_TRUE(report.failure);
    EXPECT_EQ(report.failure->code, s::LayoutErrorCode::connectivity);
    EXPECT_NE(report.failure->message.find("disconnected"), std::string::npos);
    p::DcDraft empty;
    empty.nodes = {{{0}, "N"}, {{1}, "N"}};
    const auto unused = s::project_schematic(empty);
    auto rails = accepted(unused);
    rails.wires.push_back({rails.nets[0].source.key,
                           s::WireRole::backbone,
                           {},
                           {rails.nets[0].anchor, rails.nets[1].anchor}});
    report = s::verify_layout(unused, rails, {});
    ASSERT_TRUE(report.failure);
    EXPECT_EQ(report.failure->code, s::LayoutErrorCode::connectivity);
    EXPECT_NE(report.failure->message.find("another net"), std::string::npos);
}
TEST(SchematicLayout, VerifierRejectsOverlappingAnchorsEvenWithConsistentPlacementMetadata) {
    const auto graph = s::project_schematic(cases::wheatstone());
    auto d = accepted(graph);
    auto& part = d.components[1];
    const auto& first = d.components[0];
    part.center = first.center;
    part.body = first.body;
    part.positive = first.positive;
    part.negative = first.negative;
    for (auto& t : d.terminals)
        if (t.key.component == part.source.key)
            t.anchor = t.key.side == s::TerminalSide::positive ? part.positive : part.negative;
    const auto report = s::verify_layout(graph, d, {});
    ASSERT_TRUE(report.failure);
    EXPECT_EQ(report.failure->code, s::LayoutErrorCode::invalid_geometry);
    EXPECT_NE(report.failure->message.find("anchors"), std::string::npos);
}

struct Corruption {
    const char* name;
    std::function<void(s::LayoutDiagram&)> change;
};
class SchematicLayoutCorruption : public testing::TestWithParam<Corruption> {};
TEST_P(SchematicLayoutCorruption, IndependentVerifierRejectsFalseCompleteDiagrams) {
    const auto g = s::project_schematic(cases::wheatstone());
    auto d = accepted(g);
    GetParam().change(d);
    const auto report = s::verify_layout(g, d, {});
    EXPECT_FALSE(report.valid()) << GetParam().name;
    ASSERT_TRUE(report.failure);
    EXPECT_FALSE(report.failure->message.empty());
}
INSTANTIATE_TEST_SUITE_P(
    IndependentVerification, SchematicLayoutCorruption,
    testing::Values(
        Corruption{"missing component", [](auto& d) { d.components.pop_back(); }},
        Corruption{"duplicate component", [](auto& d) { d.components[1] = d.components[0]; }},
        Corruption{"missing node", [](auto& d) { d.nets.pop_back(); }},
        Corruption{"overlapping net anchors", [](auto& d) { d.nets[1].anchor = d.nets[0].anchor; }},
        Corruption{"missing terminal", [](auto& d) { d.terminals.pop_back(); }},
        Corruption{"wrong terminal ref",
                   [](auto& d) { d.terminals[0].reference = s::Unselected{}; }},
        Corruption{"duplicate terminal", [](auto& d) { d.terminals[1] = d.terminals[0]; }},
        Corruption{"wrong wire net", [](auto& d) { d.wires.back().net = d.nets[1].source.key; }},
        Corruption{"wrong ground", [](auto& d) { d.ground->net = d.nets[1].source.key; }},
        Corruption{"missing junction", [](auto& d) { d.junctions.pop_back(); }},
        Corruption{"wrong junction net", [](auto& d) { d.junctions[0].net.value = 123; }},
        Corruption{"wrong junction members", [](auto& d) { d.junctions[0].wires.clear(); }},
        Corruption{"unmarked crossing", [](auto& d) { d.crossings.pop_back(); }},
        Corruption{"wrong crossing members",
                   [](auto& d) { d.crossings[0].first_wire = d.crossings[0].second_wire; }},
        Corruption{"extra crossing", [](auto& d) { d.crossings.push_back(d.crossings[0]); }},
        Corruption{"disconnected net segment",
                   [](auto& d) {
                       d.wires.push_back({d.nets[0].source.key,
                                          s::WireRole::backbone,
                                          {},
                                          {{10000, 10000}, {10000, 10080}}});
                   }},
        Corruption{"diagonal", [](auto& d) { d.wires[0].points[1].x += 1; }},
        Corruption{"zero segment", [](auto& d) { d.wires[0].points[1] = d.wires[0].points[0]; }},
        Corruption{"wrong symbol",
                   [](auto& d) { d.components[0].symbol = s::SymbolKind::capacitor; }},
        Corruption{"cross-analysis identity",
                   [](auto& d) { d.components[0].source.key.analysis = s::Analysis::ac; }},
        Corruption{"wrong direction",
                   [](auto& d) { d.components[0].direction = s::Direction::left; }},
        Corruption{"wrong label", [](auto& d) { d.labels[0].text = "repaired"; }},
        Corruption{"overlap label",
                   [](auto& d) { d.labels[0].bounds.origin = d.components[0].center; }},
        Corruption{"bounds", [](auto& d) { d.bounds.size.height -= 8; }},
        Corruption{"marker extent", [](auto& d) { d.marker_radius = 0; }},
        Corruption{"coordinate overflow", [](auto& d) {
                       d.wires[0].points[1].x = std::numeric_limits<s::Coordinate>::max();
                   }}));
} // namespace
