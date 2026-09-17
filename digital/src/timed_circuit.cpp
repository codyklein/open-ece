#include <openece/digital/timed_circuit.hpp>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace openece::digital::timing {
NodeId element_id(const Element& element) {
    return std::visit(
        [](const auto& e) {
            if constexpr (std::is_same_v<std::decay_t<decltype(e)>, DelayedGate>)
                return e.gate.id;
            else
                return e.id;
        },
        element);
}
Delay element_delay(const Element& element) {
    return std::visit(
        [](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, DelayedGate>)
                return e.propagation;
            else if constexpr (std::is_same_v<T, DFlipFlop>)
                return e.clock_to_q;
            else
                return e.output_delay;
        },
        element);
}
std::vector<NodeId> element_inputs(const Element& element) {
    return std::visit(
        [](const auto& e) -> std::vector<NodeId> {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, DelayedGate>)
                return e.gate.inputs;
            else if constexpr (std::is_same_v<T, SrLatch>)
                return {e.set, e.reset};
            else if constexpr (std::is_same_v<T, DLatch>)
                return {e.data, e.enable};
            else
                return {e.data, e.clock};
        },
        element);
}
bool is_storage(const Element& element) noexcept {
    return !std::holds_alternative<DelayedGate>(element);
}
namespace {
template <class T> void names(const std::vector<T>& nodes) {
    std::unordered_set<std::string> used;
    for (const auto& n : nodes) {
        if (n.name.size() > digital::limits::name_bytes)
            throw std::length_error("Name exceeds byte limit");
        if (n.name.empty() || !used.insert(n.name).second)
            throw std::invalid_argument("Names must be nonempty and unique within inputs/outputs");
    }
}
} // namespace
TimedCircuit::TimedCircuit(TimedCircuitDefinition definition) : definition_(std::move(definition)) {
    const auto& d = definition_;
    if (d.inputs.size() > limits::inputs || d.elements.size() > limits::elements ||
        d.outputs.size() > limits::outputs)
        throw std::length_error("Timed circuit node limit exceeded");
    if (d.inputs.empty() || d.outputs.empty())
        throw std::invalid_argument("A timed circuit requires inputs and outputs");
    names(d.inputs);
    names(d.outputs);
    auto add = [&](NodeId id, std::size_t index) {
        if (!indices_.emplace(id.value, index).second)
            throw std::invalid_argument("Duplicate node ID " + std::to_string(id.value));
    };
    for (std::size_t i = 0; i < d.inputs.size(); ++i)
        add(d.inputs[i].id, i);
    std::size_t references = d.outputs.size(), gate_count = 0;
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        const auto& e = d.elements[i];
        add(element_id(e), d.inputs.size() + i);
        const auto delay = element_delay(e).ticks;
        if (delay == 0 || delay > limits::time_ticks)
            throw std::invalid_argument("Element delay must be positive and at most one second");
        const auto* gate = std::get_if<DelayedGate>(&e);
        const auto count = gate ? gate->gate.inputs.size() : 2;
        if (count > limits::connections - references)
            throw std::length_error("Timed circuit connection limit exceeded");
        references += count;
        if (gate) {
            ++gate_count;
            const LogicValue zero = LogicValue::zero;
            evaluate_gate(gate->gate.kind, std::span(&zero, 1));
            if (count == 0 || (gate->gate.kind == GateKind::Not && count != 1))
                throw std::invalid_argument("Invalid gate pin count");
        }
        if (const auto* ff = std::get_if<DFlipFlop>(&e);
            ff && ff->edge != Edge::Rising && ff->edge != Edge::Falling)
            throw std::invalid_argument("Invalid clock edge");
    }
    sources_.resize(d.elements.size());
    dependents_.resize(d.inputs.size() + d.elements.size());
    std::vector<std::size_t> degree(d.elements.size(), 0);
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        for (auto id : element_inputs(d.elements[i])) {
            const auto source = resolve(id);
            sources_[i].push_back(source);
            dependents_[source].push_back(i);
            if (!is_storage(d.elements[i]) && source >= d.inputs.size() &&
                !is_storage(d.elements[source - d.inputs.size()]))
                ++degree[i];
        }
    }
    for (const auto& output : d.outputs)
        output_sources_.push_back(resolve(output.source));
    for (std::size_t i = 0; i < d.elements.size(); ++i)
        if (!is_storage(d.elements[i]) && degree[i] == 0)
            gate_order_.push_back(i);
    for (std::size_t cursor = 0; cursor < gate_order_.size(); ++cursor) {
        for (auto next : dependents_[d.inputs.size() + gate_order_[cursor]]) {
            if (!is_storage(d.elements[next]) && --degree[next] == 0)
                gate_order_.push_back(next);
        }
    }
    if (gate_order_.size() != gate_count) {
        std::string error = "Pure combinational cycle; unresolved/blocked gate IDs:";
        for (std::size_t i = 0; i < degree.size(); ++i)
            if (degree[i])
                error += " " + std::to_string(element_id(d.elements[i]).value);
        throw std::invalid_argument(error);
    }
}
std::size_t TimedCircuit::resolve(NodeId id) const {
    const auto found = indices_.find(id.value);
    if (found == indices_.end())
        throw std::invalid_argument("Missing source node " + std::to_string(id.value));
    return found->second;
}
TimedCircuitDefinition with_delay(const Circuit& circuit, Delay delay) {
    if (delay.ticks == 0 || delay.ticks > limits::time_ticks)
        throw std::invalid_argument("Invalid propagation delay");
    const auto& d = circuit.definition();
    TimedCircuitDefinition result{d.inputs, {}, d.outputs};
    for (const auto& gate : d.gates)
        result.elements.emplace_back(DelayedGate{gate, delay});
    return result;
}
} // namespace openece::digital::timing
