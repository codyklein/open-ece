#pragma once
#include <openece/circuits/circuit.hpp>
namespace openece::circuits::detail {
// Internal row-major assembly owned by OpenECE, not Eigen.
struct MnaSystem {
    std::vector<NodeId> voltage_nodes;
    std::vector<ComponentId> voltage_sources;
    std::vector<double> matrix, rhs;
    std::size_t size() const { return rhs.size(); }
};
MnaSystem assemble(const Circuit& circuit);
void check_topology(const Circuit& circuit);
} // namespace openece::circuits::detail
