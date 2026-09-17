#include <cmath>
#include <openece/circuits/circuit.hpp>
#include <set>
#include <type_traits>
#include <utility>
namespace openece::circuits {
CircuitError::CircuitError(ErrorCode code, std::string message, std::vector<NodeId> nodes,
                           std::vector<ComponentId> components)
    : std::runtime_error(std::move(message)), code_(code), nodes_(std::move(nodes)),
      components_(std::move(components)) {}
Circuit::Circuit(CircuitDefinition definition) : definition_(std::move(definition)) {
    const auto& d = definition_;
    if (d.nodes.size() > limits::nodes || d.components.size() > limits::components)
        throw CircuitError(ErrorCode::resource_limit, "Circuit exceeds node/component limits.");
    std::set<std::uint32_t> node_ids, component_ids;
    std::set<std::string> node_names, component_names;
    auto name = [](const std::string& v, std::set<std::string>& names) {
        if (v.empty() || v.size() > limits::name_bytes || !names.insert(v).second)
            throw CircuitError(ErrorCode::invalid_name,
                               "Names must be nonempty, bounded and unique within their list.");
    };
    for (const auto& n : d.nodes) {
        if (!node_ids.insert(n.id.value).second)
            throw CircuitError(ErrorCode::duplicate_id, "Duplicate node ID.", {n.id});
        name(n.name, node_names);
    }
    if (!d.ground || !node_ids.contains(d.ground->value))
        throw CircuitError(ErrorCode::missing_ground, "Select an existing node as ground.");
    std::size_t voltage_count = 0;
    for (const auto& c : d.components)
        std::visit(
            [&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if (!component_ids.insert(p.id.value).second)
                    throw CircuitError(ErrorCode::duplicate_id, "Duplicate component ID.", {},
                                       {p.id});
                name(p.name, component_names);
                if (!node_ids.contains(p.positive.value) || !node_ids.contains(p.negative.value) ||
                    p.positive == p.negative)
                    throw CircuitError(ErrorCode::invalid_terminal,
                                       "Component needs two distinct existing terminals.",
                                       {p.positive, p.negative}, {p.id});
                bool valid;
                if constexpr (std::is_same_v<T, Resistor>)
                    valid = std::isfinite(p.resistance_ohms) &&
                            p.resistance_ohms >= limits::resistance_min &&
                            p.resistance_ohms <= limits::resistance_max;
                else {
                    double value;
                    if constexpr (std::is_same_v<T, VoltageSource>) {
                        value = p.voltage_volts;
                        ++voltage_count;
                    } else
                        value = p.current_amperes;
                    valid = std::isfinite(value) &&
                            (value == 0 || (std::abs(value) >= limits::source_nonzero_min &&
                                            std::abs(value) <= limits::source_max));
                }
                if (!valid)
                    throw CircuitError(ErrorCode::invalid_value,
                                       "Component value is outside the supported SI range.", {},
                                       {p.id});
            },
            c);
    if (voltage_count > limits::voltage_sources ||
        d.nodes.size() - 1 + voltage_count > limits::unknowns)
        throw CircuitError(ErrorCode::resource_limit,
                           "Circuit exceeds voltage-source/unknown limits.");
}
} // namespace openece::circuits
