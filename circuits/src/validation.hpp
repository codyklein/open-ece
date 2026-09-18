#pragma once
#include <cmath>
#include <openece/circuits/circuit.hpp>
#include <set>
namespace openece::circuits::detail {
inline bool valid_passive(double value, double minimum, double maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
inline bool valid_source_magnitude(double magnitude) {
    return std::isfinite(magnitude) &&
           (magnitude == 0 ||
            (magnitude >= limits::source_nonzero_min && magnitude <= limits::source_max));
}
// Common structural rules for the separate DC and AC owning definitions.
template <class Definition, class IsVoltage, class ValidValue>
void validate_definition(const Definition& d, IsVoltage is_voltage, ValidValue valid_value) {
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
                if (!component_ids.insert(p.id.value).second)
                    throw CircuitError(ErrorCode::duplicate_id, "Duplicate component ID.", {},
                                       {p.id});
                name(p.name, component_names);
                if (!node_ids.contains(p.positive.value) || !node_ids.contains(p.negative.value) ||
                    p.positive == p.negative)
                    throw CircuitError(ErrorCode::invalid_terminal,
                                       "Component needs two distinct existing terminals.",
                                       {p.positive, p.negative}, {p.id});
                if (is_voltage(p))
                    ++voltage_count;
                const bool valid = valid_value(p);
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
} // namespace openece::circuits::detail
