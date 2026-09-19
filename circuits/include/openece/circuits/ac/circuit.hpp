#pragma once
#include <complex>
#include <openece/circuits/circuit.hpp>
namespace openece::circuits::ac {
// RMS cosine-reference phasor: x(t) = sqrt(2) * real(X * exp(j*2*pi*f*t)).
using Phasor = std::complex<double>;
namespace limits {
using openece::circuits::limits::components;
using openece::circuits::limits::name_bytes;
using openece::circuits::limits::nodes;
using openece::circuits::limits::resistance_max;
using openece::circuits::limits::resistance_min;
using openece::circuits::limits::source_max;
using openece::circuits::limits::source_nonzero_min;
using openece::circuits::limits::unknowns;
using openece::circuits::limits::voltage_sources;
inline constexpr double capacitance_min = 1e-15, capacitance_max = 1;
inline constexpr double inductance_min = 1e-12, inductance_max = 1e6;
inline constexpr double frequency_min = 1e-6, frequency_max = 1e12;
} // namespace limits
struct Capacitor {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    double capacitance_farads;
};
struct Inductor {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    double inductance_henries;
};
struct VoltageSource {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    Phasor voltage_volts_rms;
};
struct CurrentSource {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    Phasor current_amperes_rms;
};
using Component = std::variant<Resistor, Capacitor, Inductor, VoltageSource, CurrentSource>;
struct CircuitDefinition {
    std::vector<Node> nodes;
    std::optional<NodeId> ground;
    std::vector<Component> components;
};
class Circuit {
  public:
    // Structural validation only; electrical solvability depends on frequency.
    explicit Circuit(CircuitDefinition definition);
    const CircuitDefinition& definition() const& noexcept { return definition_; }
    const CircuitDefinition& definition() const&& = delete;

  private:
    CircuitDefinition definition_;
};
} // namespace openece::circuits::ac
