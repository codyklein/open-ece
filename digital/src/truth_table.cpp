#include <openece/digital/truth_table.hpp>
#include <stdexcept>
#include <utility>

namespace openece::digital {
TruthTable truth_table(const Circuit& circuit) {
    const auto& definition = circuit.definition();
    const auto count = definition.inputs.size();
    if (count > limits::truth_table_inputs) {
        throw std::length_error("Truth table exceeds primary-input limit");
    }
    TruthTable result;
    for (const auto& input : definition.inputs)
        result.input_names.push_back(input.name);
    for (const auto& output : definition.outputs)
        result.output_names.push_back(output.name);
    const auto row_count = std::size_t{1} << count;
    result.rows.reserve(row_count);
    for (std::size_t row = 0; row < row_count; ++row) {
        std::vector<LogicValue> inputs(count);
        for (std::size_t column = 0; column < count; ++column) {
            inputs[column] =
                ((row >> (count - column - 1)) & 1) ? LogicValue::one : LogicValue::zero;
        }
        auto evaluation = circuit.evaluate(inputs);
        result.rows.push_back({std::move(inputs), std::move(evaluation.outputs)});
    }
    return result;
}
} // namespace openece::digital
