#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <openece/digital/simulation.hpp>

namespace d = openece::digital;
namespace t = openece::digital::timing;
using d::LogicValue;
constexpr auto low = LogicValue::zero;
constexpr auto high = LogicValue::one;
namespace {
t::TimedCircuitDefinition buffer(std::uint64_t delay = 5) {
    return {{{{1}, "A"}}, {t::DelayedGate{{{2}, d::GateKind::And, {{1}}}, {delay}}}, {{"Y", {2}}}};
}
t::SimulationRequest request(std::uint64_t horizon = 30) {
    t::SimulationRequest r;
    r.horizon = {horizon};
    r.initial_inputs = {low};
    r.observed = {{1}, {2}};
    return r;
}
std::vector<t::Transition>
changes(std::initializer_list<std::pair<std::uint64_t, LogicValue>> values) {
    std::vector<t::Transition> result;
    for (auto [at, value] : values)
        result.push_back({{at}, value});
    return result;
}
t::SimulationSnapshot simulate(t::TimedCircuitDefinition definition, t::SimulationRequest r) {
    t::Simulation simulation(t::TimedCircuit(std::move(definition)), std::move(r));
    simulation.run();
    return simulation.snapshot();
}
} // namespace
TEST(TimedCircuit, ValidatesNamesIdsReferencesKindsAritiesAndDelays) {
    auto def = buffer();
    def.inputs[0].name = "";
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = buffer();
    def.outputs.push_back(def.outputs[0]);
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = buffer();
    def.inputs.push_back({{3}, "A"});
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = buffer();
    std::get<t::DelayedGate>(def.elements[0]).gate.id = {1};
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = buffer();
    def.outputs[0].source = {999};
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = buffer();
    std::get<t::DelayedGate>(def.elements[0]).gate.inputs = {{999}};
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = buffer();
    std::get<t::DelayedGate>(def.elements[0]).gate.kind = static_cast<d::GateKind>(90);
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = buffer();
    std::get<t::DelayedGate>(def.elements[0]).gate.inputs.clear();
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    EXPECT_THROW(t::TimedCircuit{buffer(0)}, std::invalid_argument);
    EXPECT_THROW(t::TimedCircuit{buffer(std::numeric_limits<std::uint64_t>::max())},
                 std::invalid_argument);
    def = buffer();
    def.inputs[0].name = std::string(d::limits::name_bytes + 1, 'x');
    EXPECT_THROW(t::TimedCircuit{def}, std::length_error);
    def = buffer();
    def.outputs.clear();
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
}
TEST(TimedCircuit, GateCyclesAreRejectedEvenWhenDisconnected) {
    auto def = buffer();
    def.elements.push_back(t::DelayedGate{{{4}, d::GateKind::Not, {{5}}}, {1}});
    def.elements.push_back(t::DelayedGate{{{5}, d::GateKind::Not, {{4}}}, {1}});
    def.elements.push_back(t::DelayedGate{{{6}, d::GateKind::Not, {{5}}}, {1}});
    try {
        t::TimedCircuit c(def);
        FAIL();
    } catch (const std::invalid_argument& e) {
        EXPECT_EQ(std::string(e.what()),
                  "Pure combinational cycle; unresolved/blocked gate IDs: 4 5 6");
    }
}
TEST(Timing, InitializesSettledThenPropagatesThroughForwardReferencedGates) {
    auto def = buffer(3);
    def.elements.insert(def.elements.begin(), t::DelayedGate{{{3}, d::GateKind::Not, {{2}}}, {2}});
    def.outputs = {{"inverted", {3}}, {"original", {1}}};
    auto r = request(20);
    r.observed = {{3}, {2}, {1}};
    r.changes = {{{10}, {1}, high}};
    t::Simulation s(t::TimedCircuit(def), r);
    auto initial = s.snapshot();
    EXPECT_EQ(initial.elements, (std::vector{high, low}));
    EXPECT_EQ(s.step(), t::StepStatus::Ready);
    EXPECT_EQ(s.current_time().ticks, 10U);
    EXPECT_EQ(s.snapshot().outputs, (std::vector{high, high}));
    s.run();
    auto end = s.snapshot();
    EXPECT_EQ(end.reached.ticks, 20U);
    EXPECT_EQ(end.traces[0].transitions, changes({{0, high}, {15, low}}));
    EXPECT_EQ(end.traces[1].transitions, changes({{0, low}, {13, high}}));
}
TEST(Timing, ValidatesRequestsBeforeRunning) {
    auto r = request();
    r.initial_inputs.clear();
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    r = request();
    r.initial_inputs[0] = static_cast<LogicValue>(2);
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    r = request();
    r.observed = {{999}};
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    r = request();
    r.observed = {{1}, {1}};
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    for (auto at : {0ULL, 31ULL, std::numeric_limits<unsigned long long>::max()}) {
        r = request();
        r.changes = {{{at}, {1}, high}};
        EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    }
    r = request();
    r.changes = {{{1}, {2}, high}};
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    r = request();
    r.changes = {{{1}, {1}, high}, {{1}, {1}, high}};
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    r = request();
    r.horizon = {t::limits::time_ticks + 1};
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::length_error);
    r = request();
    r.work.processed_events = t::limits::processed_events + 1;
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
    r = request();
    r.work.queued_events = 0;
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), std::invalid_argument);
}
TEST(Timing, OwnsCircuitRequestAndSnapshotWithIndependentSessions) {
    auto def = buffer();
    auto r = request();
    r.changes = {{{1}, {1}, high}};
    t::Simulation s(t::TimedCircuit(def), r), other(t::TimedCircuit(def), r);
    auto before = s.snapshot();
    def.elements.clear();
    r.changes.clear();
    s.run();
    EXPECT_EQ(before.elements[0], low);
    EXPECT_EQ(other.snapshot().elements[0], low);
    other.run();
    EXPECT_EQ(s.snapshot().traces, other.snapshot().traces);
}
TEST(Timing, ExplicitConversionPreservesCombinationalBehavior) {
    d::Circuit c(
        {{{{1}, "A"}, {{2}, "B"}}, {{{3}, d::GateKind::Xnor, {{1}, {2}, {1}}}}, {{"Y", {3}}}});
    auto def = t::with_delay(c, {5});
    for (auto a : {low, high})
        for (auto b : {low, high}) {
            auto r = request(0);
            r.initial_inputs = {a, b};
            r.observed = {{3}};
            EXPECT_EQ(simulate(def, r).outputs, c.evaluate(std::array{a, b}).outputs);
        }
    EXPECT_THROW(t::with_delay(c, {0}), std::invalid_argument);
}
