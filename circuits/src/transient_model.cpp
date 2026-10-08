#include "validation.hpp"
#include <algorithm>
#include <cmath>
#include <openece/circuits/ac/circuit.hpp>
#include <openece/circuits/transient/analysis.hpp>
#include <type_traits>
namespace openece::circuits::transient {
Error::Error(ErrorCode code, std::string message, std::vector<NodeId> nodes,
             std::vector<ComponentId> components)
    : std::runtime_error(std::move(message)), code_(code), nodes_(std::move(nodes)),
      components_(std::move(components)) {}
namespace {
void validate_source(const Source& s) {
    if (s.points.size() > limits::source_points)
        throw Error(ErrorCode::resource_limit, "Too many source points.");
    if (s.interpolation != Interpolation::hold && s.interpolation != Interpolation::linear)
        throw Error(ErrorCode::invalid_definition, "Unknown source interpolation.");
    auto value = [](double x) {
        return std::isfinite(x) && std::abs(x) <= circuits::limits::source_max;
    };
    if (!value(s.initial))
        throw Error(ErrorCode::invalid_definition, "Invalid initial source value.");
    double previous = 0, previous_value = s.initial;
    for (const auto& p : s.points) {
        if (!std::isfinite(p.time_seconds) || p.time_seconds <= previous ||
            p.time_seconds > limits::time_max || !value(p.value))
            throw Error(
                ErrorCode::invalid_definition,
                "Source points need increasing positive times and bounded finite SI values.");
        if (s.interpolation == Interpolation::linear &&
            !std::isfinite((p.value - previous_value) / (p.time_seconds - previous)))
            throw Error(ErrorCode::invalid_definition, "Unrepresentable source slope.");
        previous = p.time_seconds;
        previous_value = p.value;
    }
}
} // namespace
Circuit::Circuit(CircuitDefinition d) : definition_(std::move(d)) {
    try {
        circuits::detail::validate_definition(
            definition_,
            [](const auto& p) { return std::is_same_v<std::decay_t<decltype(p)>, VoltageSource>; },
            [](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, Resistor>)
                    return circuits::detail::valid_passive(p.resistance_ohms,
                                                           circuits::limits::resistance_min,
                                                           circuits::limits::resistance_max);
                else if constexpr (std::is_same_v<T, Capacitor>)
                    return circuits::detail::valid_passive(p.capacitance_farads,
                                                           ac::limits::capacitance_min,
                                                           ac::limits::capacitance_max);
                else if constexpr (std::is_same_v<T, Inductor>)
                    return circuits::detail::valid_passive(p.inductance_henries,
                                                           ac::limits::inductance_min,
                                                           ac::limits::inductance_max);
                else
                    return true;
            });
    } catch (const CircuitError& e) {
        throw Error(e.code() == circuits::ErrorCode::resource_limit ? ErrorCode::resource_limit
                                                                    : ErrorCode::invalid_definition,
                    e.what(), e.nodes(), e.components());
    }
    std::size_t inductors = 0, voltage_sources = 0, points = 0;
    for (const auto& c : definition_.components)
        std::visit(
            [&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, Inductor>)
                    ++inductors;
                if constexpr (std::is_same_v<T, VoltageSource> ||
                              std::is_same_v<T, CurrentSource>) {
                    const Source* s;
                    if constexpr (std::is_same_v<T, VoltageSource>) {
                        ++voltage_sources;
                        s = &p.voltage_volts;
                    } else
                        s = &p.current_amperes;
                    validate_source(*s);
                    points += s->points.size();
                }
            },
            c);
    if (inductors > limits::inductors || points > limits::total_source_points ||
        definition_.nodes.size() - 1 + inductors + voltage_sources > limits::unknowns)
        throw Error(ErrorCode::resource_limit,
                    "Transient circuit exceeds storage, source-point or unknown limits.");
}
double source_value(const Source& s, double t, bool left) {
    if (!std::isfinite(t) || t < 0 || t > limits::time_max)
        throw Error(ErrorCode::invalid_request, "Invalid source time.");
    const auto found =
        left ? std::lower_bound(s.points.begin(), s.points.end(), t,
                                [](const auto& p, double value) { return p.time_seconds < value; })
             : std::upper_bound(s.points.begin(), s.points.end(), t,
                                [](double value, const auto& p) { return value < p.time_seconds; });
    const auto previous_time = found == s.points.begin() ? 0 : (found - 1)->time_seconds;
    const auto previous_value = found == s.points.begin() ? s.initial : (found - 1)->value;
    if (found == s.points.end() || s.interpolation == Interpolation::hold)
        return previous_value;
    return previous_value + (found->value - previous_value) *
                                ((t - previous_time) / (found->time_seconds - previous_time));
}
double source_derivative(const Source& s, double t) {
    if (!std::isfinite(t) || t < 0 || t > limits::time_max)
        throw Error(ErrorCode::invalid_request, "Invalid source time.");
    if (s.interpolation == Interpolation::hold)
        return 0;
    const auto found =
        std::upper_bound(s.points.begin(), s.points.end(), t,
                         [](double value, const auto& p) { return value < p.time_seconds; });
    if (found == s.points.end())
        return 0;
    const auto previous_time = found == s.points.begin() ? 0 : (found - 1)->time_seconds;
    const auto previous_value = found == s.points.begin() ? s.initial : (found - 1)->value;
    return (found->value - previous_value) / (found->time_seconds - previous_time);
}
std::vector<double> time_grid(const Circuit& circuit, const Request& r) {
    if (!std::isfinite(r.stop_seconds) || !std::isfinite(r.maximum_step_seconds) ||
        r.stop_seconds <= 0 || r.maximum_step_seconds <= 0 || r.stop_seconds > limits::time_max ||
        r.maximum_step_seconds > limits::time_max)
        throw Error(
            ErrorCode::invalid_request,
            "Stop time and maximum step must be finite, positive and within the time limit.");
    if (r.voltages.size() > limits::probes || r.currents.size() > limits::probes ||
        r.initial.capacitor_voltages.size() > circuits::limits::components ||
        r.initial.inductor_currents.size() > limits::inductors)
        throw Error(ErrorCode::resource_limit, "Too many probes or initial-state entries.");
    const auto probe_count = r.voltages.size() + r.currents.size();
    if (probe_count > limits::probes ||
        std::ceil(r.stop_seconds / r.maximum_step_seconds) > static_cast<double>(limits::intervals))
        throw Error(ErrorCode::resource_limit, "Transient request exceeds probe/interval limits.");
    const auto& d = circuit.definition();
    auto node = [&](NodeId id) {
        return std::any_of(d.nodes.begin(), d.nodes.end(), [&](auto n) { return n.id == id; });
    };
    for (auto p : r.voltages)
        if (!node(p.positive) || !node(p.negative))
            throw Error(ErrorCode::invalid_request, "Voltage probe references a missing node.",
                        {p.positive, p.negative});
    for (auto p : r.currents)
        if (!std::any_of(d.components.begin(), d.components.end(), [&](const auto& c) {
                return std::visit([&](const auto& x) { return x.id == p.component; }, c);
            }))
            throw Error(ErrorCode::invalid_request, "Current probe references a missing component.",
                        {}, {p.component});
    std::vector<double> boundaries{r.stop_seconds};
    std::size_t unknowns = d.nodes.size() - 1;
    for (const auto& c : d.components)
        std::visit(
            [&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, Inductor> || std::is_same_v<T, VoltageSource>)
                    ++unknowns;
                if constexpr (std::is_same_v<T, CurrentSource> ||
                              std::is_same_v<T, VoltageSource>) {
                    const Source* s;
                    if constexpr (std::is_same_v<T, CurrentSource>)
                        s = &p.current_amperes;
                    else
                        s = &p.voltage_volts;
                    for (auto point : s->points)
                        if (point.time_seconds <= r.stop_seconds)
                            boundaries.push_back(point.time_seconds);
                }
            },
            c);
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
    std::vector<double> grid{0};
    for (double target : boundaries) {
        const double start = grid.back(), span = target - start;
        auto steps =
            static_cast<std::size_t>(std::max(1.0, std::ceil(span / r.maximum_step_seconds)));
        // Subdivide each source interval from its fixed endpoints. Repeated addition
        // can leave a microscopic final interval that has no physical significance.
        // If representational rounding exceeds the strict maximum, use one more
        // subdivision, rather than relaxing the bound or creating a tiny remainder.
        const auto beginning = grid.size();
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (steps > limits::intervals - (beginning - 1))
                throw Error(ErrorCode::resource_limit,
                            "Too many transient intervals after breakpoints.");
            grid.resize(beginning);
            bool exceeds = false;
            for (std::size_t i = 1; i <= steps; ++i) {
                const double next =
                    i == steps ? target
                               : std::lerp(start, target,
                                           static_cast<double>(i) / static_cast<double>(steps));
                if (!std::isfinite(next) || next <= grid.back())
                    throw Error(ErrorCode::invalid_request,
                                "Time step cannot make representable progress.");
                exceeds |= next - grid.back() > r.maximum_step_seconds;
                grid.push_back(next);
            }
            if (!exceeds)
                break;
            if (attempt == 1)
                throw Error(ErrorCode::invalid_request,
                            "Representable grid cannot satisfy the maximum time step.");
            ++steps;
        }
    }
    const auto recorded = grid.size() + boundaries.size(); // Conservative before/after allowance.
    const std::uint64_t k = std::max<std::size_t>(1, unknowns);
    if (recorded > limits::time_points ||
        (probe_count && recorded > limits::trace_values / probe_count) ||
        grid.size() + 2 * boundaries.size() + 3 > limits::dense_work / (k * k * k))
        throw Error(ErrorCode::resource_limit,
                    "Transient request exceeds trace-memory or dense-work limit.");
    return grid;
}
} // namespace openece::circuits::transient
