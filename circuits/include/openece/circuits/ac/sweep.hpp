#pragma once
#include <openece/circuits/ac/analysis.hpp>
namespace openece::circuits::ac {
namespace limits {
inline constexpr std::size_t sweep_points = 4096;
// Aggregate dense-work proxy, not a latency or numerical-accuracy guarantee.
inline constexpr std::uint64_t sweep_work = 1'000'000'000;
} // namespace limits
enum class FrequencySpacing { linear, logarithmic };
struct SweepSpec {
    double start_hz, stop_hz;
    std::size_t point_count;
    FrequencySpacing spacing = FrequencySpacing::logarithmic;
};
struct SweepPoint {
    double frequency_hz; // Always the exact requested grid entry, including on failure.
    std::variant<AcSolution, CircuitError> result;
};
struct SweepResult {
    std::vector<SweepPoint> points; // Every requested frequency in ascending order.
};
// Validates the complete request and work bound BEFORE grid/result allocation.
std::vector<double> frequency_grid(const Circuit&, const SweepSpec&);
// Converts only CircuitError into a point failure. Allocation/system failures propagate.
SweepPoint solve_ac_point(const Circuit&, double frequency_hz);
SweepResult sweep_ac(const Circuit&, const SweepSpec&);
struct VoltageProbe {
    NodeId positive, negative;
}; // Same-node probes explicitly read zero.
struct VoltageTransfer {
    VoltageProbe output;
    ComponentId source;
};
void validate_voltage_transfer(const Circuit&, VoltageTransfer);
Phasor probe_voltage(const AcSolution&, VoltageProbe);
// Circuit and solution must refer to the same validated snapshot.
Phasor voltage_transfer(const Circuit&, const AcSolution&, VoltageTransfer);
// Presentation helpers do not modify complex results. Phase is undefined at zero.
inline constexpr double gain_display_floor_db = -240;
std::optional<double> wrapped_phase_degrees(Phasor);
double gain_magnitude_db(Phasor);
} // namespace openece::circuits::ac
