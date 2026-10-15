#include <Eigen/LU>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <openece/circuits/transient/analysis.hpp>
#include <type_traits>
namespace openece::circuits::transient {
namespace {
enum class Kind { resistor, capacitor, inductor, voltage, current };
struct Part {
    ComponentId id;
    std::size_t p, n, slot = 0;
    Kind kind;
    double value = 0;
    const Source* source = nullptr;
};
struct System {
    std::size_t count;
    std::vector<double> a, b;
    explicit System(std::size_t k) : count(k), a(k * k), b(k) {}
    void add(std::size_t i, std::size_t j, double x) {
        if (i < count && j < count)
            a[i * count + j] += x;
    }
    void inject(std::size_t p, std::size_t n, double current) {
        if (p < count)
            b[p] -= current;
        if (n < count)
            b[n] += current;
    }
    void conductance(std::size_t p, std::size_t n, double g) {
        add(p, p, g);
        add(n, n, g);
        add(p, n, -g);
        add(n, p, -g);
    }
    void incidence(std::size_t p, std::size_t n, std::size_t j) {
        add(p, j, 1);
        add(j, p, 1);
        add(n, j, -1);
        add(j, n, -1);
    }
};
[[noreturn]] void numerical() {
    throw Error(ErrorCode::numerical_failure,
                "Transient solution failed finite-value, branch or residual checks.");
}
void check_constraint(double actual, double expected, NumericalQuality& quality) {
    const double error = std::abs(actual - expected), scale = std::abs(actual) + std::abs(expected);
    quality.max_constraint_error_volts = std::max(quality.max_constraint_error_volts, error);
    if (!std::isfinite(error) || !std::isfinite(scale) ||
        error > policy::constraint_absolute_volts + policy::physical_relative * scale)
        numerical();
}
void check_backward(const System& s, const std::vector<double>& result, NumericalQuality& quality) {
    double backward_error = 0;
    for (std::size_t i = 0; i < s.count; ++i) {
        double residual = -s.b[i], scale = std::abs(s.b[i]);
        for (std::size_t j = 0; j < s.count; ++j) {
            double term = s.a[i * s.count + j] * result[j];
            residual += term;
            scale += std::abs(term);
        }
        if (!std::isfinite(residual) || !std::isfinite(scale))
            numerical();
        double error = scale == 0 ? (residual == 0 ? 0 : std::numeric_limits<double>::infinity())
                                  : std::abs(residual) / scale;
        backward_error = std::max(backward_error, error);
    }
    quality.backward_error = std::max(quality.backward_error, backward_error);
    if (backward_error > policy::backward_threshold(s.count))
        numerical();
}
std::vector<double> solve(const System& s, NumericalQuality& quality) {
    if (!s.count)
        return {};
    auto k = static_cast<Eigen::Index>(s.count);
    Eigen::MatrixXd a(k, k);
    Eigen::VectorXd b(k), columns = Eigen::VectorXd::Ones(k);
    for (std::size_t i = 0; i < s.count; ++i) {
        b[static_cast<Eigen::Index>(i)] = s.b[i];
        for (std::size_t j = 0; j < s.count; ++j)
            a(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) = s.a[i * s.count + j];
    }
    if (!a.allFinite() || !b.allFinite())
        numerical();
    for (int pass = 0; pass < policy::equilibration_passes; ++pass) {
        for (Eigen::Index i = 0; i < k; ++i) {
            double scale = a.row(i).cwiseAbs().maxCoeff();
            if (scale == 0)
                throw Error(ErrorCode::rank_deficient, "Zero transient equation.");
            a.row(i) /= scale;
            b[i] /= scale;
        }
        for (Eigen::Index j = 0; j < k; ++j) {
            double scale = a.col(j).cwiseAbs().maxCoeff();
            if (scale == 0)
                throw Error(ErrorCode::rank_deficient, "Zero transient column.");
            a.col(j) /= scale;
            columns[j] /= scale;
        }
    }
    if (!a.allFinite() || !b.allFinite() || !columns.allFinite())
        numerical();
    Eigen::FullPivLU<Eigen::MatrixXd> lu(a);
    lu.setThreshold(policy::rank_threshold(s.count));
    if (lu.rank() != k)
        throw Error(ErrorCode::rank_deficient,
                    "Transient MNA matrix is numerically rank deficient.");
    double condition = lu.rcond();
    if (!std::isfinite(condition) || condition < policy::reciprocal_condition_min)
        throw Error(ErrorCode::ill_conditioned,
                    "Transient MNA matrix exceeds the scaled conditioning policy.");
    quality.scaled_reciprocal_condition = std::min(quality.scaled_reciprocal_condition, condition);
    Eigen::VectorXd x = columns.cwiseProduct(lu.solve(b));
    if (!x.allFinite())
        numerical();
    std::vector<double> result(s.count);
    for (std::size_t i = 0; i < s.count; ++i)
        result[i] = x[static_cast<Eigen::Index>(i)];
    check_backward(s, result, quality);
    return result;
}
struct Edge {
    std::size_t node;
    double offset;
    ComponentId component;
};
} // namespace
struct Simulation::Impl {
    Circuit circuit;
    Request request;
    std::vector<double> grid, breakpoints;
    std::vector<Part> parts;
    std::vector<std::size_t> order, voltage_probes_positive, voltage_probes_negative,
        current_probes;
    std::size_t ground, node_unknowns, unknowns, cursor = 0;
    std::vector<std::size_t> node_columns;
    std::vector<double>
        stored; // Capacitor voltage / inductor current; component declaration order.
    Result result{Status::ready, 0, {}, {}, {}, {}, {}};
    Impl(Circuit c, Request r)
        : circuit(std::move(c)), request(std::move(r)), grid(time_grid(circuit, request)) {
        const auto& d = circuit.definition();
        ground = index(*d.ground);
        node_unknowns = d.nodes.size() - 1;
        unknowns = node_unknowns;
        node_columns.resize(d.nodes.size());
        std::size_t j = 0;
        for (std::size_t i = 0; i < d.nodes.size(); ++i)
            node_columns[i] = i == ground ? limits::unknowns : j++;
        for (const auto& component : d.components)
            std::visit(
                [&](const auto& p) {
                    using T = std::decay_t<decltype(p)>;
                    Part q{p.id, index(p.positive), index(p.negative), 0, Kind::resistor};
                    if constexpr (std::is_same_v<T, Resistor>)
                        q.value = p.resistance_ohms;
                    else if constexpr (std::is_same_v<T, Capacitor>) {
                        q.kind = Kind::capacitor;
                        q.value = p.capacitance_farads;
                    } else if constexpr (std::is_same_v<T, Inductor>) {
                        q.kind = Kind::inductor;
                        q.value = p.inductance_henries;
                        q.slot = unknowns++;
                    } else if constexpr (std::is_same_v<T, VoltageSource>) {
                        q.kind = Kind::voltage;
                        q.source = &p.voltage_volts;
                        q.slot = unknowns++;
                    } else {
                        q.kind = Kind::current;
                        q.source = &p.current_amperes;
                    }
                    parts.push_back(q);
                },
                component);
        order.resize(parts.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](auto a, auto b) { return parts[a].id.value < parts[b].id.value; });
        for (auto p : request.voltages) {
            voltage_probes_positive.push_back(index(p.positive));
            voltage_probes_negative.push_back(index(p.negative));
        }
        for (auto p : request.currents)
            current_probes.push_back(part_index(p.component));
        topology();
        stored.assign(parts.size(), 0);
        if (request.initial.mode == Initialization::operating_point) {
            if (!request.initial.capacitor_voltages.empty() ||
                !request.initial.inductor_currents.empty())
                throw Error(ErrorCode::invalid_request,
                            "Operating-point mode cannot also supply storage values.");
            check_voltage_sources(0);
            check_voltage_sources(0, false, true);
            System s(unknowns);
            stamp(s, 0, false, 0, stored, true);
            NumericalQuality quality;
            auto x = solve(s, quality);
            for (std::size_t i = 0; i < parts.size(); ++i) {
                const auto& p = parts[i];
                if (p.kind == Kind::capacitor)
                    stored[i] = voltage(x, p.p) - voltage(x, p.n);
                if (p.kind == Kind::inductor)
                    stored[i] = x[p.slot];
            }
        } else if (request.initial.mode == Initialization::specified_storage) {
            if (request.initial.capacitor_voltages.size() > parts.size() ||
                request.initial.inductor_currents.size() > parts.size())
                throw Error(ErrorCode::invalid_request, "Too many initial storage entries.");
            std::vector<bool> seen(parts.size());
            auto entry = [&](ComponentId id, double value, Kind kind) {
                auto i = part_index(id);
                if (i == parts.size() || parts[i].kind != kind || seen[i] ||
                    !std::isfinite(value) || std::abs(value) > limits::state_max)
                    throw Error(ErrorCode::invalid_request,
                                "Invalid, duplicate or wrong-kind initial storage entry.", {},
                                {id});
                seen[i] = true;
                stored[i] = value;
            };
            for (auto p : request.initial.capacitor_voltages)
                entry(p.capacitor, p.voltage_volts, Kind::capacitor);
            for (auto p : request.initial.inductor_currents)
                entry(p.inductor, p.current_amperes, Kind::inductor);
            for (std::size_t i = 0; i < parts.size(); ++i)
                if ((parts[i].kind == Kind::capacitor || parts[i].kind == Kind::inductor) &&
                    !seen[i])
                    throw Error(ErrorCode::invalid_request, "Missing initial storage entry.", {},
                                {parts[i].id});
        } else
            throw Error(ErrorCode::invalid_request, "Unknown initialization policy.");
        result.latest = reconcile(0, stored);
        std::vector<double> boundaries;
        for (const auto& p : parts)
            if (p.source)
                for (auto point : p.source->points)
                    if (point.time_seconds <= request.stop_seconds)
                        boundaries.push_back(point.time_seconds);
        std::sort(boundaries.begin(), boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
        breakpoints = std::move(boundaries);
        const auto capacity = grid.size() + breakpoints.size();
        result.times.reserve(capacity);
        result.voltages.resize(request.voltages.size());
        result.currents.resize(request.currents.size());
        for (auto& v : result.voltages)
            v.reserve(capacity);
        for (auto& v : result.currents)
            v.reserve(capacity);
        append(0, SampleSide::regular, result.latest);
    }
    std::size_t index(NodeId id) const {
        const auto& ns = circuit.definition().nodes;
        return static_cast<std::size_t>(
            std::find_if(ns.begin(), ns.end(), [&](const auto& n) { return n.id == id; }) -
            ns.begin());
    }
    std::size_t part_index(ComponentId id) const {
        return static_cast<std::size_t>(
            std::find_if(parts.begin(), parts.end(), [&](auto p) { return p.id == id; }) -
            parts.begin());
    }
    double voltage(const std::vector<double>& x, std::size_t node) const {
        return node == ground ? 0 : x[node_columns[node]];
    }
    void topology() const {
        const auto count = node_columns.size();
        std::vector<std::vector<std::size_t>> edges(count);
        for (auto p : parts)
            if (p.kind != Kind::current) {
                edges[p.p].push_back(p.n);
                edges[p.n].push_back(p.p);
            }
        std::vector<bool> seen(count);
        std::vector<std::size_t> queue{ground};
        seen[ground] = true;
        for (std::size_t i = 0; i < queue.size(); ++i)
            for (auto n : edges[queue[i]])
                if (!seen[n]) {
                    seen[n] = true;
                    queue.push_back(n);
                }
        std::vector<NodeId> floating;
        for (std::size_t i = 0; i < count; ++i)
            if (!seen[i])
                floating.push_back(circuit.definition().nodes[i].id);
        if (!floating.empty())
            throw Error(ErrorCode::floating_reference,
                        "Floating transient island: current sources do not establish a reference.",
                        floating);
    }
    void check_voltage_sources(double t, bool left = false, bool operating = false) const {
        std::vector<std::vector<Edge>> forest(node_columns.size());
        std::vector<ComponentId> redundant;
        for (auto i : order) {
            const auto& p = parts[i];
            if (p.kind != Kind::voltage && !(operating && p.kind == Kind::inductor))
                continue;
            std::vector<bool> seen(forest.size());
            std::vector<double> offset(forest.size()), magnitude(forest.size());
            std::vector<std::size_t> queue{p.p};
            seen[p.p] = true;
            for (std::size_t j = 0; j < queue.size(); ++j)
                for (auto e : forest[queue[j]])
                    if (!seen[e.node]) {
                        seen[e.node] = true;
                        offset[e.node] = offset[queue[j]] + e.offset;
                        magnitude[e.node] = magnitude[queue[j]] + std::abs(e.offset);
                        queue.push_back(e.node);
                    }
            double value = p.kind == Kind::inductor ? 0 : source_value(*p.source, t, left);
            if (seen[p.n]) {
                const double tolerance =
                    policy::constraint_loop_factor * static_cast<double>(parts.size()) *
                    std::numeric_limits<double>::epsilon() * (magnitude[p.n] + std::abs(value));
                if (std::abs(offset[p.n] + value) > tolerance)
                    throw Error(ErrorCode::contradictory_constraint,
                                "Contradictory ideal voltage constraints.", {}, {p.id});
                redundant.push_back(p.id);
            } else {
                forest[p.p].push_back({p.n, -value, p.id});
                forest[p.n].push_back({p.p, value, p.id});
            }
        }
        if (!redundant.empty())
            throw Error(operating ? ErrorCode::rank_deficient : ErrorCode::nonunique_source_current,
                        operating
                            ? "Nonunique operating-point source/inductor currents."
                            : "Redundant voltage-source loop: individual currents are nonunique.",
                        {}, redundant);
    }
    void stamp(System& s, double t, bool left, double h, const std::vector<double>& history,
               bool operating = false, bool centered = true) const {
        for (auto i : order) {
            const auto& p = parts[i];
            auto a = node_columns[p.p], b = node_columns[p.n];
            // Dynamic unknowns are increments about the last accepted state.
            // Center each component before accumulation, never subtract A*x_old
            // from an already rounded, potentially huge history RHS.
            const bool incremental = !operating && centered;
            const double old_voltage = incremental
                                           ? result.latest.node_voltages[p.p].voltage_volts -
                                                 result.latest.node_voltages[p.n].voltage_volts
                                           : 0;
            switch (p.kind) {
            case Kind::resistor:
                s.conductance(a, b, 1 / p.value);
                if (incremental)
                    s.inject(a, b, old_voltage / p.value);
                break;
            case Kind::capacitor:
                if (!operating) {
                    double g = p.value / h;
                    s.conductance(a, b, g);
                    s.inject(a, b, g * (old_voltage - history[i]));
                }
                break;
            case Kind::inductor:
                s.incidence(a, b, p.slot);
                if (!operating) {
                    double g = p.value / h;
                    s.add(p.slot, p.slot, -g);
                    s.b[p.slot] = incremental ? -old_voltage : -g * history[i];
                    if (incremental)
                        s.inject(a, b, history[i]);
                }
                break;
            case Kind::voltage:
                s.incidence(a, b, p.slot);
                s.b[p.slot] = source_value(*p.source, t, left) - old_voltage;
                if (incremental)
                    s.inject(a, b, result.latest.branch_currents[i].current_amperes);
                break;
            case Kind::current:
                s.inject(a, b, source_value(*p.source, t, left));
                break;
            }
        }
    }
    State state(const std::vector<double>& v, const std::vector<double>& currents,
                NumericalQuality quality, double t, bool left) const {
        State out;
        out.quality = quality;
        std::vector<double> balance(v.size()), magnitude(v.size());
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (!std::isfinite(v[i]))
                numerical();
            out.node_voltages.push_back({circuit.definition().nodes[i].id, v[i]});
        }
        for (std::size_t i = 0; i < parts.size(); ++i)
            out.branch_currents.push_back({parts[i].id, currents[i]});
        for (auto i : order) {
            const auto& p = parts[i];
            double current = currents[i];
            if (!std::isfinite(current))
                numerical();
            balance[p.p] += current;
            balance[p.n] -= current;
            magnitude[p.p] += std::abs(current);
            magnitude[p.n] += std::abs(current);
            if (p.kind == Kind::voltage)
                check_constraint(v[p.p] - v[p.n], source_value(*p.source, t, left), out.quality);
        }
        for (std::size_t i = 0; i < v.size(); ++i) {
            double error = std::abs(balance[i]);
            out.quality.max_kcl_error_amperes = std::max(out.quality.max_kcl_error_amperes, error);
            if (!std::isfinite(error) || !std::isfinite(magnitude[i]) ||
                error > policy::kcl_absolute_amperes + policy::physical_relative * magnitude[i])
                numerical();
        }
        return out;
    }
    State reconcile(double t, const std::vector<double>& history) const {
        check_voltage_sources(t);
        const auto count = node_columns.size();
        std::vector<std::vector<Edge>> edges(count);
        for (auto i : order) {
            const auto& p = parts[i];
            if (p.kind != Kind::capacitor && p.kind != Kind::voltage)
                continue;
            double value = p.kind == Kind::capacitor ? history[i] : source_value(*p.source, t);
            edges[p.p].push_back({p.n, -value, p.id});
            edges[p.n].push_back({p.p, value, p.id});
        }
        std::vector<std::size_t> group(count, count), roots;
        std::vector<double> offset(count), path_magnitude(count);
        auto visit = [&](std::size_t root) {
            const auto g = roots.size();
            roots.push_back(root);
            group[root] = g;
            std::vector<std::size_t> queue{root};
            for (std::size_t j = 0; j < queue.size(); ++j)
                for (auto e : edges[queue[j]]) {
                    auto n = queue[j];
                    double value = offset[n] + e.offset;
                    if (!std::isfinite(value))
                        numerical();
                    if (group[e.node] == count) {
                        group[e.node] = g;
                        offset[e.node] = value;
                        path_magnitude[e.node] = path_magnitude[n] + std::abs(e.offset);
                        queue.push_back(e.node);
                    } else {
                        const double tolerance =
                            policy::constraint_loop_factor * static_cast<double>(count) *
                            std::numeric_limits<double>::epsilon() *
                            (path_magnitude[n] + path_magnitude[e.node] + std::abs(e.offset));
                        if (std::abs(offset[e.node] - value) > tolerance)
                            throw Error(ErrorCode::contradictory_constraint,
                                        "Storage voltages conflict with ideal constraints "
                                        "(impulsive changes are unsupported).",
                                        {}, {e.component});
                    }
                }
        };
        visit(ground);
        for (std::size_t i = 0; i < count; ++i)
            if (group[i] == count)
                visit(i);
        // Constraint-group potentials are actual physical voltages. No physical grounding
        // is introduced for a floating group; resistor KCL must uniquely determine it.
        System potentials(roots.size() - 1);
        auto column = [&](std::size_t n) {
            return group[n] == 0 ? potentials.count : group[n] - 1;
        };
        for (auto i : order) {
            const auto& p = parts[i];
            auto a = column(p.p), b = column(p.n);
            if (p.kind == Kind::resistor) {
                double g = 1 / p.value;
                potentials.conductance(a, b, g);
                potentials.inject(a, b, g * (offset[p.p] - offset[p.n]));
            }
            if (p.kind == Kind::current || p.kind == Kind::inductor)
                potentials.inject(
                    a, b, p.kind == Kind::current ? source_value(*p.source, t) : history[i]);
        }
        NumericalQuality quality;
        std::vector<double> common;
        try {
            common = solve(potentials, quality);
        } catch (const Error& e) {
            if (e.code() == ErrorCode::rank_deficient)
                throw Error(ErrorCode::unsupported_initialization,
                            "Storage/source cutset leaves initialization underdetermined; "
                            "higher-index initialization is unsupported.");
            throw;
        }
        std::vector<double> v(count), currents(parts.size());
        for (std::size_t i = 0; i < count; ++i)
            v[i] = offset[i] + (group[i] == 0 ? 0 : common[group[i] - 1]);
        // Relative derivatives inside capacitor/voltage-source groups determine observable
        // capacitor and source currents. The common group derivative cancels from every
        // such branch, so eliminate it algebraically, rather than assign a physical ground.
        std::vector<std::size_t> derivative_column(count, limits::unknowns),
            source_slot(parts.size());
        std::size_t k = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (i != roots[group[i]])
                derivative_column[i] = k++;
        for (std::size_t i = 0; i < parts.size(); ++i)
            if (parts[i].kind == Kind::voltage)
                source_slot[i] = k++;
        System derivatives(k);
        for (auto i : order) {
            const auto& p = parts[i];
            auto a = derivative_column[p.p], b = derivative_column[p.n];
            if (p.kind == Kind::capacitor)
                derivatives.conductance(a, b, p.value);
            else if (p.kind == Kind::voltage) {
                derivatives.incidence(a, b, source_slot[i]);
                derivatives.b[source_slot[i]] = source_derivative(*p.source, t);
            } else {
                double current = p.kind == Kind::resistor   ? (v[p.p] - v[p.n]) / p.value
                                 : p.kind == Kind::inductor ? history[i]
                                                            : source_value(*p.source, t);
                currents[i] = current;
                derivatives.inject(a, b, current);
            }
        }
        auto dx = solve(derivatives, quality);
        auto dv = [&](std::size_t n) {
            auto j = derivative_column[n];
            return j == limits::unknowns ? 0 : dx[j];
        };
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto& p = parts[i];
            if (p.kind == Kind::capacitor)
                currents[i] = p.value * (dv(p.p) - dv(p.n));
            if (p.kind == Kind::voltage)
                currents[i] = dx[source_slot[i]];
        }
        for (auto i : order) {
            const auto& p = parts[i];
            if (p.kind == Kind::capacitor)
                check_constraint(v[p.p] - v[p.n], history[i], quality);
        }
        return state(v, currents, quality, t, false);
    }
    State integrate(double t, double h, bool left, const std::vector<double>& history) const {
        check_voltage_sources(t, left);
        System s(unknowns);
        // Solve absolute values at source knots, preserving their exact constraints
        // even when a large prior value falls to a very small specified endpoint.
        const bool centered = !left;
        stamp(s, t, left, h, history, false, centered);
        NumericalQuality quality;
        auto x = solve(s, quality);
        std::vector<double> v(node_columns.size()), currents(parts.size());
        for (std::size_t i = 0; i < v.size(); ++i)
            v[i] = (centered ? result.latest.node_voltages[i].voltage_volts : 0) + voltage(x, i);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto& p = parts[i];
            double branch = v[p.p] - v[p.n];
            switch (p.kind) {
            case Kind::resistor:
                currents[i] = branch / p.value;
                break;
            case Kind::capacitor:
                currents[i] = (p.value / h) * (branch - history[i]);
                break;
            case Kind::inductor: {
                currents[i] = (centered ? history[i] : 0) + x[p.slot];
                const double expected = (p.value / h) * (currents[i] - history[i]);
                check_constraint(branch, expected, quality);
                break;
            }
            case Kind::voltage:
                currents[i] =
                    (centered ? result.latest.branch_currents[i].current_amperes : 0) + x[p.slot];
                break;
            case Kind::current:
                currents[i] = source_value(*p.source, t, left);
                break;
            }
        }
        if (centered) {
            System original(unknowns);
            stamp(original, t, left, h, history, false, false);
            std::vector<double> recovered(unknowns);
            for (std::size_t i = 0; i < v.size(); ++i)
                if (i != ground)
                    recovered[node_columns[i]] = v[i];
            for (std::size_t i = 0; i < parts.size(); ++i)
                if (parts[i].kind == Kind::voltage || parts[i].kind == Kind::inductor)
                    recovered[parts[i].slot] = currents[i];
            check_backward(original, recovered, quality);
        }
        return state(v, currents, quality, t, left);
    }
    bool breakpoint(double t) const {
        return std::binary_search(breakpoints.begin(), breakpoints.end(), t);
    }
    void append(double t, SampleSide side, const State& s) {
        result.times.push_back({t, side});
        for (std::size_t i = 0; i < result.voltages.size(); ++i)
            result.voltages[i].push_back(s.node_voltages[voltage_probes_positive[i]].voltage_volts -
                                         s.node_voltages[voltage_probes_negative[i]].voltage_volts);
        for (std::size_t i = 0; i < result.currents.size(); ++i)
            result.currents[i].push_back(s.branch_currents[current_probes[i]].current_amperes);
    }
    Status step() {
        if (result.status == Status::complete || result.status == Status::cancelled ||
            result.status == Status::failed)
            return result.status;
        double t = grid[cursor + 1], h = t - grid[cursor];
        bool boundary = breakpoint(t);
        try {
            State left = integrate(t, h, boundary, stored);
            auto next = stored;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                const auto& p = parts[i];
                if (p.kind == Kind::capacitor)
                    next[i] = left.node_voltages[p.p].voltage_volts -
                              left.node_voltages[p.n].voltage_volts;
                if (p.kind == Kind::inductor)
                    next[i] = left.branch_currents[i].current_amperes;
            }
            State right = boundary ? reconcile(t, next) : State{};
            // Both solves/checks succeed before any accepted history or trace is changed.
            append(t, boundary ? SampleSide::before_breakpoint : SampleSide::regular, left);
            if (boundary)
                append(t, SampleSide::after_breakpoint, right);
            stored = std::move(next);
            result.latest = boundary ? std::move(right) : std::move(left);
            result.current_time_seconds = t;
            ++cursor;
            result.status = cursor + 1 == grid.size() ? Status::complete : Status::running;
        } catch (const Error& e) {
            result.status = Status::failed;
            result.failure.emplace(Failure{t, e});
        }
        return result.status;
    }
};
Simulation::Simulation(Circuit c, Request r)
    : impl_(std::make_unique<Impl>(std::move(c), std::move(r))) {}
Simulation::~Simulation() = default;
Simulation::Simulation(Simulation&&) noexcept = default;
Simulation& Simulation::operator=(Simulation&&) noexcept = default;
Status Simulation::step() { return impl_->step(); }
Status Simulation::run() {
    while (impl_->result.status == Status::ready || impl_->result.status == Status::running)
        step();
    return impl_->result.status;
}
void Simulation::cancel() noexcept {
    if (impl_->result.status == Status::ready || impl_->result.status == Status::running)
        impl_->result.status = Status::cancelled;
}
Result Simulation::snapshot() const { return impl_->result; }
} // namespace openece::circuits::transient
