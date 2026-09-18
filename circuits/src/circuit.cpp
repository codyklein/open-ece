#include "validation.hpp"
#include <openece/circuits/circuit.hpp>
#include <type_traits>
#include <utility>
namespace openece::circuits {
CircuitError::CircuitError(ErrorCode code, std::string message, std::vector<NodeId> nodes,
                           std::vector<ComponentId> components)
    : std::runtime_error(std::move(message)), code_(code), nodes_(std::move(nodes)),
      components_(std::move(components)) {}
Circuit::Circuit(CircuitDefinition definition) : definition_(std::move(definition)) {
    detail::validate_definition(
        definition_,
        [](const auto& p) { return std::is_same_v<std::decay_t<decltype(p)>, VoltageSource>; },
        [](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, Resistor>)
                return detail::valid_passive(p.resistance_ohms, limits::resistance_min,
                                             limits::resistance_max);
            else if constexpr (std::is_same_v<T, VoltageSource>)
                return detail::valid_source_magnitude(std::abs(p.voltage_volts));
            else
                return detail::valid_source_magnitude(std::abs(p.current_amperes));
        });
}
} // namespace openece::circuits
