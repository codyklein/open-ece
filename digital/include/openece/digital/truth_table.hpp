#pragma once
#include <openece/digital/circuit.hpp>

namespace openece::digital {
struct TruthTableRow {
    std::vector<LogicValue> inputs;
    std::vector<LogicValue> outputs;
};
struct TruthTable {
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<TruthTableRow> rows;
};
// Columns follow declaration order; rows count upward in binary, first input MSB.
// Throws length_error above limits::truth_table_inputs before exponential arithmetic.
TruthTable truth_table(const Circuit& circuit);
} // namespace openece::digital
