#pragma once
#include <openece/circuits/ac/circuit.hpp>
#include <openece/circuits/dc.hpp>
namespace openece::circuits::ac {
struct NodeVoltage {
    NodeId node;
    Phasor voltage_volts_rms;
};
struct VoltageSourceCurrent {
    ComponentId source;
    Phasor current_amperes_rms;
};
struct AcSolution {
    double frequency_hz;
    std::vector<NodeVoltage> node_voltages; // Declaration order, including exact-zero ground.
    std::vector<VoltageSourceCurrent>
        voltage_source_currents; // Declaration order, positive -> negative.
    NumericalQuality quality;
};
// Positive-frequency RMS steady state; failure throws CircuitError, never a partial solution.
AcSolution solve_ac(const Circuit& circuit, double frequency_hz);
} // namespace openece::circuits::ac
