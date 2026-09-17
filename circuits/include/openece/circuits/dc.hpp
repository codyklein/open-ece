#pragma once
#include <algorithm>
#include <limits>
#include <openece/circuits/circuit.hpp>
namespace openece::circuits {
// Numerical acceptance policies, not physical component tolerances.
namespace policy {
inline constexpr int equilibration_passes = 4;
inline constexpr double reciprocal_condition_min = 1e-12;
inline constexpr double kcl_absolute_amperes = 1e-15, constraint_absolute_volts = 1e-12,
                        physical_relative = 1e-10;
inline constexpr double constraint_loop_factor = 64;
inline constexpr double rank_threshold(std::size_t n) {
    return 64 * static_cast<double>(n) * std::numeric_limits<double>::epsilon();
}
inline constexpr double backward_threshold(std::size_t n) {
    return std::max(1e-12, 256 * static_cast<double>(n) * std::numeric_limits<double>::epsilon());
}
} // namespace policy
struct NodeVoltage {
    NodeId node;
    double voltage_volts;
};
struct VoltageSourceCurrent {
    ComponentId source;
    double current_amperes;
};
struct NumericalQuality {
    double scaled_reciprocal_condition = 1, backward_error = 0, max_kcl_error_amperes = 0,
           max_constraint_error_volts = 0;
};
struct DcSolution {
    std::vector<NodeVoltage> node_voltages; // Declaration order, including exact-zero ground.
    std::vector<VoltageSourceCurrent>
        voltage_source_currents; // Source declaration order; positive -> negative.
    NumericalQuality quality;
};
// No mutation, regularization, least-squares fallback, or partial solution on failure.
DcSolution solve_dc(const Circuit& circuit);
} // namespace openece::circuits
