#pragma once
#include <openece/circuits/ac/circuit.hpp>
namespace openece::circuits::ac::detail {
// Internal row-major assembly owned by OpenECE, not Eigen.
struct MnaSystem {
    std::vector<NodeId> voltage_nodes;
    std::vector<ComponentId> voltage_sources;
    std::vector<Phasor> matrix, rhs;
    std::size_t size() const { return rhs.size(); }
};
MnaSystem assemble(const Circuit& circuit, double frequency_hz);
Phasor admittance(const Resistor& c, double omega);
Phasor admittance(const Capacitor& c, double omega);
Phasor admittance(const Inductor& c, double omega);
void check_topology(const Circuit& circuit);
} // namespace openece::circuits::ac::detail
