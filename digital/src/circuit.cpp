#include <openece/digital/circuit.hpp>

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace openece::digital {
namespace {
template <typename Nodes> void validate_names(const Nodes& nodes) {
    std::unordered_set<std::string> names;
    for (const auto& node : nodes) {
        if (node.name.size() > limits::name_bytes) {
            throw std::length_error("Digital name exceeds byte limit");
        }
        if (node.name.empty() || !names.insert(node.name).second) {
            throw std::invalid_argument("Names must be nonempty and unique within inputs/outputs");
        }
    }
}
} // namespace
Circuit::Circuit(CircuitDefinition definition) : definition_(std::move(definition)) {
    const auto& d = definition_;
    if (d.inputs.size() > limits::primary_inputs || d.gates.size() > limits::gates ||
        d.outputs.size() > limits::primary_outputs) {
        throw std::length_error("Digital circuit node limit exceeded");
    }
    if (d.inputs.empty() || d.outputs.empty()) {
        throw std::invalid_argument("A circuit requires at least one primary input and output");
    }
    std::size_t connection_count = d.outputs.size();
    for (const auto& gate : d.gates) {
        if (gate.inputs.size() > limits::connections - connection_count) {
            throw std::length_error("Digital circuit connection limit exceeded");
        }
        connection_count += gate.inputs.size();
        // Validate kind/arity without evaluating real data or allocating pin buffers.
        const LogicValue zero = LogicValue::zero;
        evaluate_gate(gate.kind, std::span(&zero, 1));
        if (gate.inputs.empty() || (gate.kind == GateKind::Not && gate.inputs.size() != 1)) {
            throw std::invalid_argument("Invalid pin count for gate " +
                                        std::to_string(gate.id.value));
        }
    }
    validate_names(d.inputs);
    validate_names(d.outputs);
    std::unordered_map<std::uint32_t, std::size_t> indices;
    auto add = [&](NodeId id, std::size_t index) {
        if (!indices.emplace(id.value, index).second) {
            throw std::invalid_argument("Duplicate node ID " + std::to_string(id.value));
        }
    };
    for (std::size_t i = 0; i < d.inputs.size(); ++i)
        add(d.inputs[i].id, i);
    for (std::size_t i = 0; i < d.gates.size(); ++i)
        add(d.gates[i].id, d.inputs.size() + i);
    auto resolve = [&](NodeId id) {
        const auto found = indices.find(id.value);
        if (found == indices.end()) {
            throw std::invalid_argument("Missing source node " + std::to_string(id.value));
        }
        return found->second;
    };
    sources_.resize(d.gates.size());
    std::vector<std::vector<std::size_t>> dependents(d.gates.size());
    std::vector<std::size_t> indegrees(d.gates.size(), 0);
    for (std::size_t i = 0; i < d.gates.size(); ++i) {
        for (auto id : d.gates[i].inputs) {
            const auto source = resolve(id);
            sources_[i].push_back(source);
            if (source >= d.inputs.size()) {
                // Count each pin, including repeated connections, on both sides.
                dependents[source - d.inputs.size()].push_back(i);
                ++indegrees[i];
            }
        }
    }
    for (const auto& output : d.outputs)
        output_sources_.push_back(resolve(output.source));
    for (std::size_t i = 0; i < indegrees.size(); ++i) {
        if (indegrees[i] == 0)
            order_.push_back(i);
    }
    for (std::size_t cursor = 0; cursor < order_.size(); ++cursor) {
        for (auto dependent : dependents[order_[cursor]]) {
            if (--indegrees[dependent] == 0)
                order_.push_back(dependent);
        }
    }
    if (order_.size() != d.gates.size()) {
        std::string message = "Combinational cycle detected; unresolved/blocked gate IDs:";
        for (std::size_t i = 0; i < indegrees.size(); ++i) {
            if (indegrees[i] != 0)
                message += " " + std::to_string(d.gates[i].id.value);
        }
        throw std::invalid_argument(message);
    }
}

Evaluation Circuit::evaluate(std::span<const LogicValue> inputs) const {
    if (inputs.size() != definition_.inputs.size()) {
        throw std::invalid_argument("Assignment count must match primary inputs");
    }
    for (auto input : inputs) {
        if (input != LogicValue::zero && input != LogicValue::one) {
            throw std::invalid_argument("Logic value must be zero or one");
        }
    }
    std::vector<LogicValue> values(inputs.begin(), inputs.end());
    values.resize(inputs.size() + definition_.gates.size());
    std::vector<LogicValue> pins;
    for (auto index : order_) {
        pins.clear();
        for (auto source : sources_[index])
            pins.push_back(values[source]);
        values[inputs.size() + index] = evaluate_gate(definition_.gates[index].kind, pins);
    }
    Evaluation result;
    result.gate_values.assign(values.begin() + static_cast<std::ptrdiff_t>(inputs.size()),
                              values.end());
    for (auto source : output_sources_)
        result.outputs.push_back(values[source]);
    return result;
}
} // namespace openece::digital
