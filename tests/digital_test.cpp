#include <gtest/gtest.h>
#include <openece/digital/truth_table.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

using namespace openece::digital;
namespace {
LogicValue bit(bool value) { return value ? LogicValue::one : LogicValue::zero; }
CircuitDefinition half_adder() {
    return {{{{10}, "A"}, {{20}, "B"}},
            {{{40}, GateKind::And, {{10}, {20}}}, {{30}, GateKind::Xor, {{10}, {20}}}},
            {{"Sum", {30}}, {"Carry", {40}}}};
}
CircuitDefinition pass_through(std::size_t count = 1) {
    CircuitDefinition d;
    for (std::size_t i = 0; i < count; ++i) {
        d.inputs.push_back({{static_cast<std::uint32_t>(i)}, "I" + std::to_string(i)});
    }
    d.outputs.push_back({"Y", {0}});
    return d;
}
void invalid(const CircuitDefinition& d) { EXPECT_THROW((Circuit{d}), std::invalid_argument); }
} // namespace

TEST(DigitalGate, ExhaustiveOneThroughFivePinTruthTables) {
    for (auto kind : {GateKind::Not, GateKind::And, GateKind::Or, GateKind::Nand, GateKind::Nor,
                      GateKind::Xor, GateKind::Xnor}) {
        for (unsigned count = 1; count <= (kind == GateKind::Not ? 1U : 5U); ++count) {
            for (unsigned row = 0; row < (1U << count); ++row) {
                std::vector<LogicValue> inputs;
                for (unsigned i = 0; i < count; ++i)
                    inputs.push_back(bit((row & (1U << i)) != 0));
                const auto ones = static_cast<unsigned>(std::popcount(row));
                bool expected = false;
                switch (kind) {
                case GateKind::Not:
                    expected = ones == 0;
                    break;
                case GateKind::And:
                    expected = ones == count;
                    break;
                case GateKind::Or:
                    expected = ones > 0;
                    break;
                case GateKind::Nand:
                    expected = ones != count;
                    break;
                case GateKind::Nor:
                    expected = ones == 0;
                    break;
                case GateKind::Xor:
                    expected = ones % 2 == 1;
                    break;
                case GateKind::Xnor:
                    expected = ones % 2 == 0;
                    break;
                }
                EXPECT_EQ(evaluate_gate(kind, inputs), bit(expected));
            }
        }
    }
}
TEST(DigitalGate, RejectsInvalidKindsValuesAndArities) {
    const std::array one{LogicValue::one};
    const std::array two{LogicValue::one, LogicValue::zero};
    const std::array bad{static_cast<LogicValue>(2)};
    for (auto kind : {GateKind::Not, GateKind::And, GateKind::Or, GateKind::Nand, GateKind::Nor,
                      GateKind::Xor, GateKind::Xnor}) {
        EXPECT_THROW(evaluate_gate(kind, {}), std::invalid_argument);
        EXPECT_THROW(evaluate_gate(kind, bad), std::invalid_argument);
    }
    EXPECT_THROW(evaluate_gate(GateKind::Not, two), std::invalid_argument);
    EXPECT_THROW(evaluate_gate(static_cast<GateKind>(99), one), std::invalid_argument);
}
TEST(DigitalCircuit, HalfAdderDeclarationOrderAndFanOut) {
    const Circuit circuit(half_adder());
    for (unsigned a = 0; a < 2; ++a) {
        for (unsigned b = 0; b < 2; ++b) {
            const std::array inputs{bit(a != 0), bit(b != 0)};
            const auto result = circuit.evaluate(inputs);
            EXPECT_EQ(result.outputs, (std::vector{bit((a + b) % 2 != 0), bit(a + b == 2)}));
            EXPECT_EQ(result.gate_values, (std::vector{bit(a + b == 2), bit((a + b) % 2 != 0)}));
            EXPECT_EQ(circuit.evaluate(inputs).outputs, result.outputs);
        }
    }
}
TEST(DigitalCircuit, FullAdderWithForwardReferencesAgainstIntegerAddition) {
    Circuit circuit({{{{1}, "A"}, {{2}, "B"}, {{3}, "Cin"}},
                     {{{7}, GateKind::Or, {{5}, {6}}},
                      {{4}, GateKind::Xor, {{1}, {2}}},
                      {{8}, GateKind::Xor, {{4}, {3}}},
                      {{6}, GateKind::And, {{4}, {3}}},
                      {{5}, GateKind::And, {{1}, {2}}}},
                     {{"Sum", {8}}, {"Carry", {7}}}});
    const auto table = truth_table(circuit);
    for (unsigned row = 0; row < 8; ++row) {
        const auto sum = std::popcount(row);
        EXPECT_EQ(table.rows[row].outputs, (std::vector{bit(sum % 2 != 0), bit(sum >= 2)}));
        EXPECT_EQ(circuit.evaluate(table.rows[row].inputs).gate_values.front(), bit(sum >= 2));
    }
}
TEST(DigitalCircuit, MultiplexerAndGateOrderIndependence) {
    CircuitDefinition d{{{{1}, "S"}, {{2}, "A"}, {{3}, "B"}},
                        {{{4}, GateKind::Not, {{1}}},
                         {{5}, GateKind::And, {{4}, {2}}},
                         {{6}, GateKind::And, {{1}, {3}}},
                         {{7}, GateKind::Or, {{5}, {6}}}},
                        {{"Y", {7}}}};
    const auto table = truth_table(Circuit(d));
    std::reverse(d.gates.begin(), d.gates.end());
    const auto reversed = truth_table(Circuit(d));
    for (std::size_t row = 0; row < table.rows.size(); ++row) {
        const auto& inputs = table.rows[row].inputs;
        EXPECT_EQ(table.rows[row].outputs[0], inputs[inputs[0] == LogicValue::one ? 2 : 1]);
        EXPECT_EQ(table.rows[row].outputs, reversed.rows[row].outputs);
    }
}
TEST(DigitalCircuit, TiedGatePinsAndPassThroughOutputs) {
    Circuit circuit({{{{1}, "A"}},
                     {{{3}, GateKind::Xor, {{2}, {2}}}, {{2}, GateKind::Not, {{1}}}},
                     {{"Y", {3}}, {"A", {1}}, {"Again", {3}}}});
    for (auto value : {LogicValue::zero, LogicValue::one}) {
        EXPECT_EQ(circuit.evaluate(std::array{value}).outputs,
                  (std::vector{LogicValue::zero, value, LogicValue::zero}));
    }
    EXPECT_EQ(truth_table(Circuit(pass_through())).rows.size(), 2U);
}
TEST(DigitalCircuit, OwnsSnapshotAndEvaluationResults) {
    auto d = half_adder();
    Circuit circuit(d);
    d.inputs.clear();
    d.gates.clear();
    d.outputs.clear();
    auto inputs = std::array{LogicValue::one, LogicValue::one};
    const auto result = circuit.evaluate(inputs);
    inputs.fill(LogicValue::zero);
    auto copy = circuit;
    EXPECT_EQ(copy.evaluate(inputs).outputs, (std::vector{LogicValue::zero, LogicValue::zero}));
    EXPECT_EQ(result.outputs, (std::vector{LogicValue::zero, LogicValue::one}));
    EXPECT_EQ(circuit.definition().inputs[0].name, "A");
}
TEST(DigitalCircuit, RejectsMalformedDefinitionsAndAssignments) {
    auto d = half_adder();
    d.inputs.clear();
    invalid(d);
    d = half_adder();
    d.outputs.clear();
    invalid(d);
    d = half_adder();
    d.inputs[1].id = d.inputs[0].id;
    invalid(d);
    d = half_adder();
    d.gates[1].id = d.gates[0].id;
    invalid(d);
    d = half_adder();
    d.gates[0].id = d.inputs[0].id;
    invalid(d);
    d = half_adder();
    d.gates[0].inputs[0] = {999};
    invalid(d);
    d = half_adder();
    d.outputs[0].source = {999};
    invalid(d);
    d = half_adder();
    d.gates[0].inputs.clear();
    invalid(d);
    d = half_adder();
    d.gates[0].kind = GateKind::Not;
    invalid(d);
    d = half_adder();
    d.gates[0].kind = static_cast<GateKind>(-1);
    invalid(d);
    Circuit circuit(half_adder());
    EXPECT_THROW(circuit.evaluate({}), std::invalid_argument);
    EXPECT_THROW(circuit.evaluate(std::array{LogicValue::one}), std::invalid_argument);
    EXPECT_THROW(circuit.evaluate(std::array{LogicValue::one, static_cast<LogicValue>(255)}),
                 std::invalid_argument);
    // Invalid values on unused primary inputs are still rejected.
    EXPECT_THROW(
        Circuit(pass_through(2)).evaluate(std::array{LogicValue::one, static_cast<LogicValue>(2)}),
        std::invalid_argument);
}
TEST(DigitalCircuit, NamesAreExactNonemptyAndUniqueWithinEachList) {
    auto d = half_adder();
    d.inputs[0].name.clear();
    invalid(d);
    d = half_adder();
    d.outputs[0].name.clear();
    invalid(d);
    d = half_adder();
    d.inputs[1].name = "A";
    invalid(d);
    d = half_adder();
    d.outputs[1].name = "Sum";
    invalid(d);
    d = half_adder();
    d.inputs[1].name = " A";
    d.outputs[0].name = "A";
    EXPECT_NO_THROW((Circuit{d})); // No normalization; input/output namespaces are separate.
    d.inputs[0].name = " ";
    EXPECT_NO_THROW((Circuit{d}));
    d.inputs[0].name = std::string(limits::name_bytes, 'x');
    EXPECT_NO_THROW((Circuit{d}));
    d.inputs[0].name += 'x';
    EXPECT_THROW((Circuit{d}), std::length_error);
    d = half_adder();
    d.outputs[0].name = std::string(limits::name_bytes + 1, 'x');
    EXPECT_THROW((Circuit{d}), std::length_error);
}
TEST(DigitalCircuit, RejectsSelfCyclesDisconnectedCyclesAndBlockedDownstreamGates) {
    auto d = half_adder();
    d.gates[0].inputs[0] = d.gates[0].id;
    invalid(d);
    d = half_adder();
    d.gates.push_back({{50}, GateKind::Not, {{60}}});
    d.gates.push_back({{60}, GateKind::Not, {{50}}});
    d.gates.push_back({{70}, GateKind::Not, {{60}}});
    try {
        Circuit circuit(d);
        FAIL() << "Cycle accepted";
    } catch (const std::invalid_argument& e) {
        EXPECT_EQ(std::string(e.what()),
                  "Combinational cycle detected; unresolved/blocked gate IDs: 50 60 70");
    }
}
TEST(DigitalCircuit, NodeAndConnectionLimitsAreInclusive) {
    EXPECT_NO_THROW((Circuit{pass_through(limits::primary_inputs)}));
    EXPECT_THROW((Circuit{pass_through(limits::primary_inputs + 1)}), std::length_error);
    auto d = pass_through();
    for (std::size_t i = 0; i < limits::gates; ++i) {
        d.gates.push_back({{static_cast<std::uint32_t>(i + 1)},
                           GateKind::Not,
                           {{static_cast<std::uint32_t>(i)}}});
    }
    d.outputs[0].source = d.gates.back().id;
    EXPECT_EQ(Circuit(d).evaluate(std::array{LogicValue::one}).outputs[0],
              bit(limits::gates % 2 == 0));
    d.gates.push_back({{999999}, GateKind::Not, {{0}}});
    EXPECT_THROW((Circuit{d}), std::length_error);
    d = pass_through();
    d.outputs.clear();
    for (std::size_t i = 0; i < limits::primary_outputs; ++i) {
        d.outputs.push_back({"O" + std::to_string(i), {0}});
    }
    EXPECT_NO_THROW((Circuit{d}));
    d.outputs.push_back({"extra", {0}});
    EXPECT_THROW((Circuit{d}), std::length_error);
    d = pass_through();
    d.gates.push_back({{std::numeric_limits<std::uint32_t>::max()},
                       GateKind::And,
                       std::vector<NodeId>(limits::connections - d.outputs.size(), NodeId{0})});
    EXPECT_NO_THROW((Circuit{d}));
    d.gates[0].inputs.push_back({0});
    EXPECT_THROW((Circuit{d}), std::length_error);
}
TEST(DigitalTruthTable, ColumnsAndRowsHaveDeterministicOrderIncludingUnusedInputs) {
    auto d = half_adder();
    d.inputs.push_back({{100}, "Unused"});
    const auto table = truth_table(Circuit(d));
    EXPECT_EQ(table.input_names, (std::vector<std::string>{"A", "B", "Unused"}));
    EXPECT_EQ(table.output_names, (std::vector<std::string>{"Sum", "Carry"}));
    ASSERT_EQ(table.rows.size(), 8U);
    for (unsigned row = 0; row < 8; ++row) {
        EXPECT_EQ(table.rows[row].inputs,
                  (std::vector{bit((row & 4) != 0), bit((row & 2) != 0), bit((row & 1) != 0)}));
        const unsigned a = (row >> 2) & 1, b = (row >> 1) & 1;
        EXPECT_EQ(table.rows[row].outputs, (std::vector{bit(a != b), bit(a && b)}));
    }
}
TEST(DigitalTruthTable, ChecksLimitBeforeExponentialArithmetic) {
    EXPECT_EQ(truth_table(Circuit(pass_through(limits::truth_table_inputs))).rows.size(),
              limits::truth_table_rows);
    EXPECT_THROW(truth_table(Circuit(pass_through(limits::truth_table_inputs + 1))),
                 std::length_error);
    // In particular, do not shift a 64-bit integer by the maximum circuit input count.
    EXPECT_THROW(truth_table(Circuit(pass_through(limits::primary_inputs))), std::length_error);
}
