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

TEST(Timing, InertialPulseBoundaryAndStaleReplacement) {
    for (std::uint64_t width : {4U, 5U, 6U}) {
        auto r = request();
        r.changes = {{{10}, {1}, high}, {{10 + width}, {1}, low}};
        const auto expected =
            width < 5 ? changes({{0, low}}) : changes({{0, low}, {15, high}, {15 + width, low}});
        EXPECT_EQ(simulate(buffer(), r).traces[1].transitions, expected);
    }
    auto r = request();
    r.changes = {{{1}, {1}, high}, {{3}, {1}, low}, {{4}, {1}, high}};
    EXPECT_EQ(simulate(buffer(), r).traces[1].transitions, changes({{0, low}, {9, high}}));
}
TEST(Timing, UnchangedTargetPreservesPendingDeadlineAndBatchAvoidsFalseGlitch) {
    auto def = buffer();
    def.inputs.push_back({{3}, "B"});
    auto& gate = std::get<t::DelayedGate>(def.elements[0]).gate;
    gate.kind = d::GateKind::Or;
    gate.inputs = {{1}, {3}};
    auto r = request();
    r.initial_inputs = {low, low};
    r.changes = {{{2}, {1}, high}, {{4}, {3}, high}, {{5}, {1}, low}};
    EXPECT_EQ(simulate(def, r).traces[1].transitions, changes({{0, low}, {7, high}}));
    gate.kind = d::GateKind::Xor;
    r.changes = {{{2}, {1}, high}, {{2}, {3}, high}};
    EXPECT_EQ(simulate(def, r).traces[1].transitions, changes({{0, low}}));
    auto first = simulate(def, r);
    std::reverse(r.changes.begin(), r.changes.end());
    EXPECT_EQ(simulate(def, r).traces, first.traces);
}
TEST(Timing, TracesIncludeInitialValuesAndHorizonButSuppressRepeatedValues) {
    auto r = request(10);
    r.observed = {{2}, {1}};
    r.changes = {{{1}, {1}, low}, {{5}, {1}, high}, {{6}, {1}, high}, {{10}, {1}, low}};
    const auto s = simulate(buffer(), r);
    EXPECT_EQ(s.traces[0].source, d::NodeId{2});
    EXPECT_EQ(s.traces[0].transitions, changes({{0, low}, {10, high}}));
    EXPECT_EQ(s.traces[1].transitions, changes({{0, low}, {5, high}, {10, low}}));
    EXPECT_EQ(s.reached, t::Time{10});
    r = request(t::limits::time_ticks);
    r.changes = {{{t::limits::time_ticks}, {1}, high}};
    EXPECT_EQ(simulate(buffer(t::limits::time_ticks), r).traces[1].transitions,
              changes({{0, low}}));
    auto def = buffer();
    def.elements.clear();
    def.outputs = {{"direct", {1}}, {"alias", {1}}};
    r.observed = {{1}};
    EXPECT_EQ(simulate(def, r).outputs, (std::vector{high, high}));
}
TEST(Timing, WorkLimitsCountStaleEventsAndFailuresAreTerminal) {
    auto r = request();
    r.changes = {{{1}, {1}, high}, {{2}, {1}, low}};
    r.work.processed_events = 2; // Cancelled delivery at 6 still consumes work.
    t::Simulation s(t::TimedCircuit(buffer()), r);
    EXPECT_THROW(s.run(), t::SimulationError);
    EXPECT_EQ(s.current_time(), t::Time{6});
    EXPECT_EQ(s.status(), t::StepStatus::Failed);
    EXPECT_THROW(s.step(), std::logic_error);
    EXPECT_THROW(s.snapshot(), std::logic_error);
    r.work.processed_events = 3;
    EXPECT_NO_THROW(simulate(buffer(), r));
    r.work.queued_events = 1;
    EXPECT_THROW((t::Simulation{t::TimedCircuit(buffer()), r}), t::SimulationError);
    r = request();
    r.changes = {{{1}, {1}, high}};
    r.work.pin_visits = 1; // Initial settling consumed one visit.
    EXPECT_THROW(simulate(buffer(), r), t::SimulationError);
    r.work.pin_visits = 2;
    r.work.recorded_values = 3; // Two initial values plus input, but no output capacity.
    EXPECT_THROW(simulate(buffer(), r), t::SimulationError);
    r.work.recorded_values = 4;
    EXPECT_NO_THROW(simulate(buffer(), r));
}
TEST(Timing, AgreesWithIndependentDiscreteTickReference) {
    // No event queue or generation tokens: scan each tick, with optional gate deadlines.
    // Fixed deterministic pseudo-random DAGs include tied pins and reconvergent fan-out.
    std::uint32_t seed = 2345;
    auto next = [&] {
        seed = seed * 1664525U + 1013904223U;
        return seed;
    };
    constexpr std::size_t inputs = 3, gates = 8, nodes = inputs + gates;
    constexpr std::uint64_t horizon = 80;
    for (int trial = 0; trial < 30; ++trial) {
        t::TimedCircuitDefinition def;
        t::SimulationRequest r;
        r.horizon = {horizon};
        for (std::size_t i = 0; i < inputs; ++i) {
            def.inputs.push_back({{static_cast<std::uint32_t>(i)}, "I" + std::to_string(i)});
            r.initial_inputs.push_back(next() % 2 ? high : low);
        }
        for (std::size_t i = inputs; i < nodes; ++i) {
            auto kind = static_cast<d::GateKind>(next() % 7);
            std::vector<d::NodeId> pins;
            const auto count = kind == d::GateKind::Not ? 1U : 1U + next() % 4;
            for (unsigned p = 0; p < count; ++p)
                pins.push_back({static_cast<std::uint32_t>(next() % i)});
            def.elements.push_back(
                t::DelayedGate{{{static_cast<std::uint32_t>(i)}, kind, pins}, {1 + next() % 7}});
        }
        def.outputs = {{"Y", {nodes - 1}}};
        for (std::size_t i = 0; i < nodes; ++i)
            r.observed.push_back({static_cast<std::uint32_t>(i)});
        for (std::uint64_t at = 1; at <= horizon; ++at)
            for (std::size_t i = 0; i < inputs; ++i)
                if (next() % 4 == 0)
                    r.changes.push_back(
                        {{at}, {static_cast<std::uint32_t>(i)}, next() % 2 ? high : low});
        std::vector<LogicValue> values = r.initial_inputs;
        values.resize(nodes, low);
        auto truth = [&](const d::Gate& gate) {
            unsigned ones = 0;
            for (auto pin : gate.inputs)
                ones += values[pin.value] == high ? 1U : 0U;
            bool result = false;
            switch (gate.kind) {
            case d::GateKind::Not:
                result = ones == 0;
                break;
            case d::GateKind::And:
                result = ones == gate.inputs.size();
                break;
            case d::GateKind::Or:
                result = ones != 0;
                break;
            case d::GateKind::Nand:
                result = ones != gate.inputs.size();
                break;
            case d::GateKind::Nor:
                result = ones == 0;
                break;
            case d::GateKind::Xor:
                result = ones % 2 != 0;
                break;
            case d::GateKind::Xnor:
                result = ones % 2 == 0;
                break;
            }
            return result ? high : low;
        };
        for (const auto& element : def.elements) {
            const auto& g = std::get<t::DelayedGate>(element);
            values[g.gate.id.value] = truth(g.gate);
        }
        std::vector<t::SignalTrace> expected;
        for (auto id : r.observed)
            expected.push_back({id, {{{0}, values[id.value]}}});
        std::vector<std::optional<std::pair<std::uint64_t, LogicValue>>> pending(gates);
        for (std::uint64_t at = 1; at <= horizon; ++at) {
            auto before = values;
            for (const auto& change : r.changes)
                if (change.at.ticks == at)
                    values[change.input.value] = change.value;
            for (std::size_t i = 0; i < gates; ++i)
                if (pending[i] && pending[i]->first == at) {
                    values[inputs + i] = pending[i]->second;
                    pending[i].reset();
                }
            for (std::size_t i = 0; i < gates; ++i) {
                const auto& g = std::get<t::DelayedGate>(def.elements[i]);
                bool affected = false;
                for (auto pin : g.gate.inputs)
                    affected |= before[pin.value] != values[pin.value];
                if (!affected)
                    continue;
                const auto target = truth(g.gate);
                if (target == values[inputs + i])
                    pending[i].reset();
                else if (!pending[i] || pending[i]->second != target)
                    pending[i] = {{at + g.propagation.ticks, target}};
            }
            for (std::size_t i = 0; i < nodes; ++i)
                if (before[i] != values[i])
                    expected[i].transitions.push_back({{at}, values[i]});
        }
        // Reverse declaration order to exercise forward references independently of reference
        // topology.
        std::reverse(def.elements.begin(), def.elements.end());
        EXPECT_EQ(simulate(def, r).traces, expected) << "trial " << trial;
    }
}

namespace {
t::TimedCircuitDefinition storage(t::Element element) {
    return {{{{1}, "D_or_S"}, {{2}, "clock_enable_or_R"}}, {element}, {{"Q", {3}}}};
}
t::SimulationRequest storage_request(std::uint64_t horizon = 30) {
    auto r = request(horizon);
    r.initial_inputs = {low, low};
    r.initial_storage = {{{3}, low}};
    r.observed = {{1}, {2}, {3}};
    return r;
}
} // namespace
TEST(TimingStorage, FlipFlopSamplesPreBatchDataAndPreservesCapturedPulses) {
    const auto def = storage(t::DFlipFlop{{3}, {1}, {2}, t::Edge::Rising, {5}});
    auto r = storage_request();
    r.changes = {{{1}, {1}, high}, {{2}, {2}, high}, {{3}, {1}, low},   {{3}, {2}, low},
                 {{4}, {2}, high}, {{8}, {2}, low},  {{10}, {1}, high}, {{10}, {2}, high}};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}, {7, high}, {9, low}}));
    // No invented initial edge, even when clock is initially high.
    r = storage_request();
    r.initial_inputs = {high, high};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}}));
    auto falling = storage(t::DFlipFlop{{3}, {1}, {2}, t::Edge::Falling, {2}});
    r.changes = {{{5}, {2}, low}};
    EXPECT_EQ(simulate(falling, r).traces[2].transitions, changes({{0, low}, {7, high}}));
}
TEST(TimingStorage, LatchTracksOpenHoldsClosedAndSamplesBeforeClosingBatch) {
    const auto def = storage(t::DLatch{{3}, {1}, {2}, {5}});
    auto r = storage_request();
    r.changes = {{{1}, {2}, high}, {{2}, {1}, high}, {{3}, {1}, low},
                 {{4}, {1}, high}, {{4}, {2}, low},  {{10}, {1}, low}};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}, {7, high}, {8, low}}));
    r = storage_request();
    r.initial_inputs = {high, high};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}, {5, high}}));
    r.initial_inputs = {high, low};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}}));
}
TEST(TimingStorage, SrSetResetHoldAndForbiddenStateAreExplicit) {
    const auto def = storage(t::SrLatch{{3}, {1}, {2}, {2}});
    auto r = storage_request();
    r.changes = {{{1}, {1}, high}, {{2}, {1}, low}, {{8}, {2}, high}, {{9}, {2}, low}};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}, {3, high}, {10, low}}));
    r.initial_inputs = {high, high};
    EXPECT_THROW(simulate(def, r), t::SimulationError);
    r = storage_request();
    r.changes = {{{5}, {1}, high}, {{5}, {2}, high}};
    t::Simulation s(t::TimedCircuit(def), r);
    try {
        s.run();
        FAIL();
    } catch (const t::SimulationError& e) {
        EXPECT_EQ(e.at(), t::Time{5});
        EXPECT_EQ(e.node(), std::optional<d::NodeId>{{3}});
    }
    EXPECT_EQ(s.status(), t::StepStatus::Failed);
    EXPECT_THROW(s.snapshot(), std::logic_error);
    r = storage_request();
    r.initial_inputs = {high, low};
    // Simultaneous switch from set to reset never passes through forbidden 11.
    r.changes = {{{4}, {1}, low}, {{4}, {2}, high}};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}, {2, high}, {6, low}}));
}
TEST(TimingStorage, ClockContractAndLazyScheduling) {
    for (auto initial : {low, high}) {
        auto def = buffer();
        def.elements.clear();
        def.outputs = {{"clock", {1}}};
        auto r = request(12);
        r.observed = {{1}};
        r.initial_inputs = {initial};
        r.clocks = {{{1}, {2}, {3}, {2}}};
        r.work.queued_events = 1;
        auto expected =
            initial == low
                ? changes({{0, low}, {2, high}, {5, low}, {7, high}, {10, low}, {12, high}})
                : changes({{0, high}, {2, low}, {4, high}, {7, low}, {9, high}, {12, low}});
        EXPECT_EQ(simulate(def, r).traces[0].transitions, expected);
    }
    auto r = request();
    r.clocks = {{{1}, {1}, {1}, {1}}, {{1}, {2}, {1}, {1}}};
    EXPECT_THROW(simulate(buffer(), r), std::length_error);
    r.clocks.resize(1);
    r.changes = {{{2}, {1}, high}};
    EXPECT_THROW(simulate(buffer(), r), std::invalid_argument);
    r.changes.clear();
    for (auto invalid :
         {t::Clock{{2}, {1}, {1}, {1}}, t::Clock{{1}, {0}, {1}, {1}}, t::Clock{{1}, {31}, {1}, {1}},
          t::Clock{{1}, {1}, {0}, {1}}, t::Clock{{1}, {1}, {1}, {0}},
          t::Clock{{1}, {1}, {t::limits::time_ticks + 1}, {1}}}) {
        r.clocks = {invalid};
        EXPECT_THROW(simulate(buffer(), r), std::invalid_argument);
    }
    auto def = buffer();
    def.inputs.push_back({{4}, "B"});
    r.initial_inputs = {low, low};
    r.clocks = {{{1}, {1}, {1}, {1}}, {{1}, {2}, {1}, {1}}};
    EXPECT_THROW(simulate(def, r), std::invalid_argument);
    r.clocks = {{{1}, {1}, {1}, {1}}, {{4}, {1}, {1}, {1}}};
    r.work.queued_events = 2;
    r.horizon = {1};
    EXPECT_NO_THROW(simulate(def, r)); // Both due entries are removed before new events.
}
TEST(TimingStorage, FeedbackThroughStorageAndSimultaneousShiftRegister) {
    auto def = storage(t::DFlipFlop{{3}, {4}, {2}, t::Edge::Rising, {1}});
    def.elements.insert(def.elements.begin(), t::DelayedGate{{{4}, d::GateKind::Not, {{3}}}, {1}});
    auto r = storage_request(22);
    r.clocks = {{{2}, {5}, {5}, {5}}};
    EXPECT_EQ(simulate(def, r).traces[2].transitions, changes({{0, low}, {6, high}, {16, low}}));
    def.elements.push_back(t::DelayedGate{{{5}, d::GateKind::Not, {{5}}}, {1}});
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = storage(t::DFlipFlop{{3}, {1}, {2}, t::Edge::Rising, {5}});
    def.elements.push_back(t::DFlipFlop{{4}, {3}, {2}, t::Edge::Rising, {1}});
    r = storage_request(20);
    r.initial_storage.push_back({{4}, low});
    r.observed = {{3}, {4}};
    r.initial_inputs = {high, low};
    r.clocks = {{{2}, {5}, {2}, {3}}};
    auto first = simulate(def, r);
    EXPECT_EQ(first.traces[0].transitions, changes({{0, low}, {10, high}}));
    // Q1 arrives at same time as second rising edge: stage 2 sees old Q1.
    EXPECT_EQ(first.traces[1].transitions, changes({{0, low}, {16, high}}));
    std::reverse(def.elements.begin(), def.elements.end());
    EXPECT_EQ(simulate(def, r).traces, first.traces);
}
TEST(TimingStorage, ValidatesSharedNamespaceInitialStatesAndFeedbackWorkLimit) {
    auto def = storage(t::DLatch{{3}, {1}, {2}, {1}});
    auto r = storage_request();
    r.initial_storage.clear();
    EXPECT_THROW(simulate(def, r), std::invalid_argument);
    for (auto id : {1U, 99U}) {
        r.initial_storage = {{{id}, low}};
        EXPECT_THROW(simulate(def, r), std::invalid_argument);
    }
    r.initial_storage = {{{3}, static_cast<LogicValue>(9)}};
    EXPECT_THROW(simulate(def, r), std::invalid_argument);
    def.elements.push_back(t::DelayedGate{{{4}, d::GateKind::Not, {{3}}}, {1}});
    r.initial_storage = {{{4}, low}};
    EXPECT_THROW(simulate(def, r), std::invalid_argument);
    r.initial_storage = {{{3}, low}, {{3}, low}};
    EXPECT_THROW(simulate(def, r), std::invalid_argument);
    std::get<t::DLatch>(def.elements[0]).data = {4};
    r = storage_request();
    r.initial_inputs = {low, high};
    r.work.processed_events = 5;
    EXPECT_THROW(simulate(def, r),
                 t::SimulationError); // Legitimate storage feedback may oscillate.
    std::get<t::DLatch>(def.elements[0]).id = {1};
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
    def = storage(t::DFlipFlop{{3}, {1}, {2}, static_cast<t::Edge>(9), {1}});
    EXPECT_THROW(t::TimedCircuit{def}, std::invalid_argument);
}
TEST(TimedCircuit, CentralizedResourceBoundaries) {
    auto def = buffer();
    for (std::size_t i = 1; i < t::limits::inputs; ++i)
        def.inputs.push_back({{static_cast<std::uint32_t>(10 + i)}, "I" + std::to_string(i)});
    EXPECT_NO_THROW(t::TimedCircuit{def});
    def.inputs.push_back({{999}, "excess"});
    EXPECT_THROW(t::TimedCircuit{def}, std::length_error);
    def = buffer();
    auto& pins = std::get<t::DelayedGate>(def.elements[0]).gate.inputs;
    pins.assign(t::limits::connections - 1, {1});
    EXPECT_NO_THROW(t::TimedCircuit{def});
    pins.push_back({1});
    EXPECT_THROW(t::TimedCircuit{def}, std::length_error);
    auto r = request();
    r.changes.resize(t::limits::stimuli + 1);
    EXPECT_THROW(simulate(buffer(), r), std::length_error);
    r = request();
    r.observed.resize(t::limits::observed_nodes + 1);
    EXPECT_THROW(simulate(buffer(), r), std::length_error);
}
