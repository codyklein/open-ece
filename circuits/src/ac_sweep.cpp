#include "validation.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <openece/circuits/ac/sweep.hpp>
#include <type_traits>
namespace openece::circuits::ac {
namespace {
void finite(Phasor value) {
    if (!std::isfinite(value.real()) || !std::isfinite(value.imag()) ||
        !std::isfinite(std::abs(value)))
        throw CircuitError(ErrorCode::invalid_value,
                           "Response must have a finite complex magnitude.");
}
Phasor reference_source(const Circuit& circuit, VoltageTransfer transfer) {
    const auto& d = circuit.definition();
    for (NodeId node : {transfer.output.positive, transfer.output.negative})
        if (std::none_of(d.nodes.begin(), d.nodes.end(),
                         [&](const auto& n) { return n.id == node; }))
            throw CircuitError(ErrorCode::invalid_terminal,
                               "Output probe references a missing node.", {node});
    std::optional<Phasor> reference;
    for (const auto& component : d.components)
        std::visit(
            [&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, VoltageSource>) {
                    if (c.id == transfer.source)
                        reference = c.voltage_volts_rms;
                    else if (c.voltage_volts_rms != Phasor{})
                        throw CircuitError(ErrorCode::invalid_value,
                                           "Voltage transfer requires every other independent "
                                           "voltage and current source to be zero.",
                                           {}, {c.id});
                } else if constexpr (std::is_same_v<T, CurrentSource>) {
                    if (c.current_amperes_rms != Phasor{})
                        throw CircuitError(ErrorCode::invalid_value,
                                           "Voltage transfer requires every other independent "
                                           "voltage and current source to be zero.",
                                           {}, {c.id});
                }
            },
            component);
    if (!reference || *reference == Phasor{})
        throw CircuitError(ErrorCode::invalid_value,
                           "Select a nonzero AC voltage source for voltage-transfer normalization.",
                           {}, {transfer.source});
    return *reference;
}
} // namespace
std::vector<double> frequency_grid(const Circuit& circuit, const SweepSpec& spec) {
    if (spec.point_count < 2 || spec.point_count > limits::sweep_points)
        throw CircuitError(ErrorCode::resource_limit, "AC sweep needs 2 through 4096 points.");
    if (!openece::circuits::detail::valid_passive(spec.start_hz, limits::frequency_min,
                                                  limits::frequency_max) ||
        !openece::circuits::detail::valid_passive(spec.stop_hz, limits::frequency_min,
                                                  limits::frequency_max) ||
        spec.start_hz >= spec.stop_hz)
        throw CircuitError(
            ErrorCode::invalid_value,
            "Sweep endpoints must be supported positive frequencies with start < stop.");
    if (spec.spacing != FrequencySpacing::linear && spec.spacing != FrequencySpacing::logarithmic)
        throw CircuitError(ErrorCode::invalid_value, "Unknown frequency spacing.");
    const auto& d = circuit.definition();
    std::uint64_t k = static_cast<std::uint64_t>(d.nodes.size() - 1);
    for (const auto& c : d.components)
        if (std::holds_alternative<VoltageSource>(c))
            ++k;
    // A validated circuit has k<=191. Division avoids forming point_count*k^3.
    k = std::max<std::uint64_t>(1, k);
    const auto per_point = k * k * k;
    if (spec.point_count > limits::sweep_work / per_point)
        throw CircuitError(
            ErrorCode::resource_limit,
            "Sweep exceeds the aggregate dense-work limit; reduce circuit size or point count.");
    std::vector<double> result(spec.point_count);
    result.front() = spec.start_hz;
    result.back() = spec.stop_hz;
    const double log_start = std::log(spec.start_hz), log_stop = std::log(spec.stop_hz);
    for (std::size_t i = 1; i + 1 < spec.point_count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(spec.point_count - 1);
        result[i] = spec.spacing == FrequencySpacing::linear
                        ? std::lerp(spec.start_hz, spec.stop_hz, t)
                        : std::exp(std::lerp(log_start, log_stop, t));
    }
    for (std::size_t i = 1; i < result.size(); ++i)
        if (!std::isfinite(result[i]) || result[i] <= result[i - 1])
            throw CircuitError(
                ErrorCode::invalid_value,
                "Requested frequency grid cannot be represented as strictly increasing doubles.");
    return result;
}
SweepPoint solve_ac_point(const Circuit& circuit, double frequency_hz) {
    try {
        return {frequency_hz, solve_ac(circuit, frequency_hz)};
    } catch (const CircuitError& error) {
        return {frequency_hz, error};
    }
}
SweepResult sweep_ac(const Circuit& circuit, const SweepSpec& spec) {
    const auto frequencies = frequency_grid(circuit, spec);
    SweepResult result;
    result.points.reserve(frequencies.size());
    for (double frequency : frequencies)
        result.points.push_back(solve_ac_point(circuit, frequency));
    return result;
}
void validate_voltage_transfer(const Circuit& circuit, VoltageTransfer transfer) {
    (void)reference_source(circuit, transfer);
}
Phasor probe_voltage(const AcSolution& solution, VoltageProbe probe) {
    auto value = [&](NodeId id) {
        for (const auto& node : solution.node_voltages)
            if (node.node == id)
                return node.voltage_volts_rms;
        throw CircuitError(ErrorCode::invalid_terminal,
                           "Output probe references a missing result node.", {id});
    };
    const Phasor result = value(probe.positive) - value(probe.negative);
    finite(result);
    return result;
}
Phasor voltage_transfer(const Circuit& circuit, const AcSolution& solution,
                        VoltageTransfer transfer) {
    const auto reference = reference_source(circuit, transfer);
    const auto result = probe_voltage(solution, transfer.output) / reference;
    finite(result);
    return result;
}
std::optional<double> wrapped_phase_degrees(Phasor value) {
    finite(value);
    if (value == Phasor{})
        return std::nullopt;
    double degrees = std::arg(value) * 180 / std::numbers::pi;
    if (degrees <= -180)
        degrees = 180;
    return degrees;
}
double gain_magnitude_db(Phasor value) {
    finite(value);
    const double magnitude = std::abs(value);
    if (magnitude == 0)
        return gain_display_floor_db;
    return std::max(gain_display_floor_db, 20 * std::log10(magnitude));
}
} // namespace openece::circuits::ac
