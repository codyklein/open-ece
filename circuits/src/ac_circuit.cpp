#include "validation.hpp"
#include <openece/circuits/ac/circuit.hpp>
#include <type_traits>
#include <utility>
namespace openece::circuits::ac {
Circuit::Circuit(CircuitDefinition definition) : definition_(std::move(definition)) {
    detail::validate_definition(
        definition_,
        [](const auto& p) { return std::is_same_v<std::decay_t<decltype(p)>, VoltageSource>; },
        [](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, Resistor>)
                return detail::valid_passive(p.resistance_ohms, limits::resistance_min,
                                             limits::resistance_max);
            else if constexpr (std::is_same_v<T, Capacitor>)
                return detail::valid_passive(p.capacitance_farads, limits::capacitance_min,
                                             limits::capacitance_max);
            else if constexpr (std::is_same_v<T, Inductor>)
                return detail::valid_passive(p.inductance_henries, limits::inductance_min,
                                             limits::inductance_max);
            else {
                Phasor value;
                if constexpr (std::is_same_v<T, VoltageSource>)
                    value = p.voltage_volts_rms;
                else
                    value = p.current_amperes_rms;
                return std::isfinite(value.real()) && std::isfinite(value.imag()) &&
                       detail::valid_source_magnitude(std::abs(value));
            }
        });
}
} // namespace openece::circuits::ac
