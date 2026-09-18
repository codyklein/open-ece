#include "ac_mna.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <openece/circuits/ac/analysis.hpp>
#include <type_traits>
namespace openece::circuits::ac::detail {
namespace {
std::size_t node_index(const CircuitDefinition& d, NodeId id) {
    return static_cast<std::size_t>(
        std::find_if(d.nodes.begin(), d.nodes.end(), [&](const Node& n) { return n.id == id; }) -
        d.nodes.begin());
}
} // namespace
Phasor admittance(const Resistor& c, double) { return {1 / c.resistance_ohms, 0}; }
Phasor admittance(const Capacitor& c, double omega) { return {0, omega * c.capacitance_farads}; }
Phasor admittance(const Inductor& c, double omega) {
    return {0, -1 / (omega * c.inductance_henries)};
}
void check_topology(const Circuit& circuit) {
    const auto& d = circuit.definition();
    const auto count = d.nodes.size();
    std::vector<std::vector<std::size_t>> edges(count);
    for (const auto& part : d.components)
        std::visit(
            [&](const auto& c) {
                if constexpr (!std::is_same_v<std::decay_t<decltype(c)>, CurrentSource>) {
                    auto p = node_index(d, c.positive), n = node_index(d, c.negative);
                    edges[p].push_back(n);
                    edges[n].push_back(p);
                }
            },
            part);
    std::vector<bool> reached(count);
    std::vector<std::size_t> queue{node_index(d, *d.ground)};
    reached[queue[0]] = true;
    for (std::size_t i = 0; i < queue.size(); ++i)
        for (auto n : edges[queue[i]])
            if (!reached[n]) {
                reached[n] = true;
                queue.push_back(n);
            }
    std::vector<NodeId> floating;
    for (std::size_t i = 0; i < count; ++i)
        if (!reached[i])
            floating.push_back(d.nodes[i].id);
    if (!floating.empty())
        throw CircuitError(ErrorCode::floating_reference,
                           "Floating nodes: current sources do not establish a voltage reference.",
                           floating);
    // Each non-tree voltage-source edge closes a constraint loop in this forest.
    struct Edge {
        std::size_t node;
        Phasor drop;
        ComponentId source;
    };
    std::vector<std::vector<Edge>> forest(count);
    std::vector<ComponentId> redundant;
    for (const auto& part : d.components)
        if (const auto* v = std::get_if<VoltageSource>(&part)) {
            auto p = node_index(d, v->positive), n = node_index(d, v->negative);
            std::vector<std::size_t> parent(count, count);
            std::vector<ComponentId> source(count);
            std::vector<Phasor> drop(count);
            std::vector<double> magnitude(count);
            queue = {p};
            parent[p] = p;
            for (std::size_t i = 0; i < queue.size(); ++i)
                for (const auto& e : forest[queue[i]])
                    if (parent[e.node] == count) {
                        parent[e.node] = queue[i];
                        source[e.node] = e.source;
                        drop[e.node] = drop[queue[i]] + e.drop;
                        magnitude[e.node] = magnitude[queue[i]] + std::abs(e.drop);
                        queue.push_back(e.node);
                    }
            if (parent[n] != count) {
                std::vector<ComponentId> loop{v->id};
                for (auto k = n; k != p; k = parent[k])
                    loop.push_back(source[k]);
                const double tolerance = policy::constraint_loop_factor *
                                         static_cast<double>(loop.size()) *
                                         std::numeric_limits<double>::epsilon() *
                                         (magnitude[n] + std::abs(v->voltage_volts_rms));
                if (std::abs(drop[n] - v->voltage_volts_rms) > tolerance)
                    throw CircuitError(ErrorCode::contradictory_voltage_constraint,
                                       "Contradictory ideal-voltage-source loop.", {}, loop);
                if (redundant.empty())
                    redundant = loop;
            } else {
                forest[p].push_back({n, v->voltage_volts_rms, v->id});
                forest[n].push_back({p, -v->voltage_volts_rms, v->id});
            }
        }
    if (!redundant.empty())
        throw CircuitError(ErrorCode::redundant_voltage_constraint,
                           "Redundant ideal-voltage constraints (within numerical tolerance): "
                           "individual source currents are not unique.",
                           {}, redundant);
}
MnaSystem assemble(const Circuit& circuit, double frequency_hz) {
    const double omega = 2 * std::numbers::pi * frequency_hz;
    const auto& d = circuit.definition();
    MnaSystem s;
    for (const auto& n : d.nodes)
        if (n.id != *d.ground)
            s.voltage_nodes.push_back(n.id);
    for (const auto& c : d.components)
        if (const auto* v = std::get_if<VoltageSource>(&c))
            s.voltage_sources.push_back(v->id);
    const auto k = s.voltage_nodes.size() + s.voltage_sources.size();
    if (k > limits::unknowns)
        throw CircuitError(ErrorCode::resource_limit, "Too many MNA unknowns.");
    s.rhs.assign(k, 0);
    s.matrix.assign(k * k, 0);
    auto index = [&](NodeId id) {
        return static_cast<std::size_t>(
            std::find(s.voltage_nodes.begin(), s.voltage_nodes.end(), id) -
            s.voltage_nodes.begin());
    };
    auto add = [&](std::size_t i, std::size_t j, Phasor value) { s.matrix[i * k + j] += value; };
    // Stable ID order stabilizes accumulation across component declaration permutations.
    std::vector<std::size_t> order(d.components.size());
    std::iota(order.begin(), order.end(), 0);
    auto id = [&](std::size_t i) {
        return std::visit([](const auto& c) { return c.id.value; }, d.components[i]);
    };
    std::sort(order.begin(), order.end(), [&](auto a, auto b) { return id(a) < id(b); });
    for (auto i : order)
        std::visit(
            [&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                auto p = index(c.positive), n = index(c.negative);
                bool pg = c.positive == *d.ground, ng = c.negative == *d.ground;
                if constexpr (std::is_same_v<T, Resistor> || std::is_same_v<T, Capacitor> ||
                              std::is_same_v<T, Inductor>) {
                    const Phasor g = admittance(c, omega);
                    if (!pg)
                        add(p, p, g);
                    if (!ng)
                        add(n, n, g);
                    if (!pg && !ng) {
                        add(p, n, -g);
                        add(n, p, -g);
                    }
                } else if constexpr (std::is_same_v<T, CurrentSource>) {
                    if (!pg)
                        s.rhs[p] -= c.current_amperes_rms;
                    if (!ng)
                        s.rhs[n] += c.current_amperes_rms;
                } else {
                    auto j = s.voltage_nodes.size() +
                             static_cast<std::size_t>(std::find(s.voltage_sources.begin(),
                                                                s.voltage_sources.end(), c.id) -
                                                      s.voltage_sources.begin());
                    if (!pg) {
                        add(p, j, 1);
                        add(j, p, 1);
                    }
                    if (!ng) {
                        add(n, j, -1);
                        add(j, n, -1);
                    }
                    s.rhs[j] = c.voltage_volts_rms;
                }
            },
            d.components[i]);
    return s;
}
} // namespace openece::circuits::ac::detail
