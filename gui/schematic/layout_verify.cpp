#include "layout_detail.hpp"

namespace openece::schematic {
namespace {
using namespace detail;
using NodeIndex = std::tuple<Analysis, std::uint32_t>;
using PartIndex = NodeIndex;
using TerminalIndex = std::tuple<Analysis, std::uint32_t, TerminalSide>;
using LabelIndex = std::tuple<std::size_t, Analysis, std::uint32_t, LabelRole>;
using ContactIndex = std::tuple<Analysis, std::uint32_t, Coordinate, Coordinate>;
using CrossingIndex = std::tuple<Coordinate, Coordinate, std::size_t, std::size_t>;
[[noreturn]] void connectivity(const char* message) {
    fail(LayoutErrorCode::connectivity, message);
}
[[noreturn]] void geometry(const char* message) {
    fail(LayoutErrorCode::invalid_geometry, message);
}
struct Union {
    std::vector<std::size_t> parent;
    explicit Union(std::size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    std::size_t root(std::size_t i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    }
    void join(std::size_t a, std::size_t b) { parent[root(a)] = root(b); }
};
void verify(const SchematicGraph& graph, const LayoutDiagram& d, const LayoutMetrics& metrics,
            const LayoutLimits& limits, Budget& budget) {
    preflight(graph, metrics, limits, budget);
    const Math math{limits.coordinate};
    const auto lines = segments(d, limits, math, budget);
    if (d.analysis != graph.analysis() || d.ground_selection != graph.ground())
        connectivity(
            "Diagram analysis or explicit ground selection differs from the source graph.");
    if (d.strategy != LayoutStrategy::structured && d.strategy != LayoutStrategy::net_rails)
        geometry("Unknown layout strategy.");
    if (d.marker_radius != metrics.marker_radius)
        geometry("Frozen marker extent does not match the requested geometry metrics.");
    std::map<NodeIndex, const Node*> expected_nodes;
    std::map<PartIndex, const Component*> expected_parts;
    for (const auto& n : graph.nodes())
        expected_nodes.emplace(key(n.key), &n);
    for (const auto& c : graph.components())
        expected_parts.emplace(key(c.key), &c);
    if (d.nets.size() != expected_nodes.size() || d.components.size() != expected_parts.size() ||
        d.terminals.size() != expected_parts.size() * 2)
        connectivity("Diagram is missing or duplicates a component, node or terminal.");
    std::map<NodeIndex, const NetPlacement*> nodes;
    for (const auto& n : d.nets) {
        budget.spend();
        math.point(n.anchor);
        const auto found = expected_nodes.find(key(n.source.key));
        if (found == expected_nodes.end() || n.source != *found->second ||
            !nodes.emplace(found->first, &n).second)
            connectivity("A node placement changed or duplicated source identity/data.");
    }
    for (std::size_t i = 0; i < d.nets.size(); ++i)
        for (std::size_t j = i + 1; j < d.nets.size(); ++j) {
            budget.spend();
            if (overlaps(marker(d.nets[i].anchor, metrics.marker_radius, math),
                         marker(d.nets[j].anchor, metrics.marker_radius, math)))
                connectivity("Different net inspection anchors overlap.");
        }
    std::map<PartIndex, const ComponentPlacement*> parts;
    for (const auto& p : d.components) {
        budget.spend();
        const auto found = expected_parts.find(key(p.source.key));
        if (found == expected_parts.end() || p.source != *found->second ||
            !parts.emplace(found->first, &p).second)
            connectivity("A component placement changed or duplicated source identity/data.");
        if (p.symbol != symbol(p.source.kind))
            geometry("Symbol kind differs from the original component token.");
        math.point(p.center);
        math.point(p.positive);
        math.point(p.negative);
        math.rectangle(p.body);
        if (p.direction != Direction::right && p.direction != Direction::left &&
            p.direction != Direction::down && p.direction != Direction::up)
            geometry("Unsupported component orientation.");
        const bool vertical = p.direction == Direction::down || p.direction == Direction::up;
        const bool forward = p.direction == Direction::down || p.direction == Direction::right;
        const auto half = metrics.symbol_length / 2, thickness = metrics.symbol_width / 2;
        const Point a = vertical ? Point{p.center.x, math.add(p.center.y, -half)}
                                 : Point{math.add(p.center.x, -half), p.center.y};
        const Point z = vertical ? Point{p.center.x, math.add(p.center.y, half)}
                                 : Point{math.add(p.center.x, half), p.center.y};
        const Rectangle expected_body{vertical ? Point{math.add(p.center.x, -thickness), a.y}
                                               : Point{a.x, math.add(p.center.y, -thickness)},
                                      vertical
                                          ? Extent{metrics.symbol_width, metrics.symbol_length}
                                          : Extent{metrics.symbol_length, metrics.symbol_width}};
        if (p.positive != (forward ? a : z) || p.negative != (forward ? z : a) ||
            p.body != expected_body)
            geometry("Component body/terminal anchors do not match its dimensions and polarity "
                     "direction.");
    }
    std::map<TerminalIndex, const TerminalPlacement*> terminals;
    for (const auto& t : d.terminals) {
        budget.spend();
        math.point(t.anchor);
        const auto part = parts.find(key(t.key.component));
        if (part == parts.end() ||
            (t.key.side != TerminalSide::positive && t.key.side != TerminalSide::negative))
            connectivity("A terminal has an unknown component or polarity.");
        const auto& p = *part->second;
        const auto& ref =
            t.key.side == TerminalSide::positive ? p.source.positive : p.source.negative;
        const auto anchor = t.key.side == TerminalSide::positive ? p.positive : p.negative;
        if (t.reference != ref || t.anchor != anchor || !terminals.emplace(key(t.key), &t).second)
            connectivity("Terminal reference/anchor differs from its source component, or a "
                         "terminal is duplicated.");
    }
    for (std::size_t i = 0; i < d.terminals.size(); ++i)
        for (std::size_t j = i + 1; j < d.terminals.size(); ++j) {
            budget.spend();
            if (overlaps(marker(d.terminals[i].anchor, metrics.marker_radius, math),
                         marker(d.terminals[j].anchor, metrics.marker_radius, math)))
                geometry("Terminal anchors overlap or cannot be distinguished.");
        }
    for (const auto& n : d.nets)
        for (const auto& t : d.terminals) {
            budget.spend();
            if (overlaps(marker(n.anchor, metrics.marker_radius, math),
                         marker(t.anchor, metrics.marker_radius, math))) {
                const auto* ref = std::get_if<DeclaredNode>(&t.reference);
                if (!ref || ref->key != n.source.key)
                    connectivity(
                        "A net anchor overlaps an open, dangling or different-net terminal.");
            }
        }
    for (std::size_t i = 0; i < d.components.size(); ++i)
        for (std::size_t j = i + 1; j < d.components.size(); ++j) {
            budget.spend();
            if (overlaps(d.components[i].body, d.components[j].body))
                geometry("Component bodies overlap.");
        }
    std::map<LabelIndex, std::pair<std::string, Extent>> expected_labels;
    auto expected_label = [&](LabelOwner owner, LabelRole role, std::string text) {
        const auto [type, analysis, id] = key(owner);
        expected_labels.emplace(LabelIndex{type, analysis, id, role},
                                std::pair{std::move(text), extent(metrics, owner, role, budget)});
    };
    for (const auto& n : graph.nodes())
        expected_label(n.key, LabelRole::node_name, node_label_text(n));
    for (const auto& c : graph.components())
        for (auto role :
             {LabelRole::component_name, LabelRole::component_value, LabelRole::component_detail})
            expected_label(c.key, role, component_label_text(c, role));
    if (d.labels.size() != expected_labels.size())
        geometry("Missing or extra inspection labels.");
    for (const auto& label : d.labels) {
        budget.spend();
        math.rectangle(label.bounds);
        const auto [type, analysis, id] = key(label.owner);
        const auto found = expected_labels.find({type, analysis, id, label.role});
        if (found == expected_labels.end() || label.text != found->second.first ||
            label.bounds.size != found->second.second)
            geometry("A label lost its original text, owner or measured extent.");
        expected_labels.erase(found);
        for (const auto& p : d.components) {
            budget.spend();
            if (overlaps(label.bounds, p.body))
                geometry("A label overlaps an electrical symbol.");
        }
    }
    for (std::size_t i = 0; i < d.labels.size(); ++i)
        for (std::size_t j = i + 1; j < d.labels.size(); ++j) {
            budget.spend();
            if (overlaps(d.labels[i].bounds, d.labels[j].bounds))
                geometry("Measured label rectangles overlap.");
        }
    const auto* selected_ground = std::get_if<DeclaredNode>(&graph.ground());
    if (static_cast<bool>(d.ground) != static_cast<bool>(selected_ground))
        connectivity(
            "A ground symbol is missing or was invented without a declared explicit reference.");
    if (d.ground) {
        const auto& ground = *d.ground;
        math.point(ground.anchor);
        math.rectangle(ground.body);
        if (ground.net != selected_ground->key ||
            ground.body !=
                Rectangle{{math.add(ground.anchor.x, -metrics.ground_size / 2), ground.anchor.y},
                          {metrics.ground_size, metrics.ground_size}})
            connectivity("Ground identity or symbol anchor differs from the explicit reference.");
        for (const auto& p : d.components) {
            budget.spend();
            if (overlaps(p.body, ground.body))
                geometry("Ground symbol overlaps a component.");
        }
        for (const auto& label : d.labels) {
            budget.spend();
            if (overlaps(label.bounds, ground.body))
                geometry("Ground symbol overlaps a label.");
        }
    }
    std::map<TerminalIndex, std::size_t> wired;
    std::size_t ground_wires = 0;
    for (const auto& w : d.wires) {
        budget.spend();
        if (!nodes.contains(key(w.net)))
            connectivity("A wire references an invented or analysis-mismatched net.");
        switch (w.role) {
        case WireRole::terminal: {
            if (!w.terminal)
                connectivity("Terminal wire has no terminal identity.");
            const auto found = terminals.find(key(*w.terminal));
            if (found == terminals.end())
                connectivity("Wire references an unknown terminal.");
            const auto& t = *found->second;
            const auto* ref = std::get_if<DeclaredNode>(&t.reference);
            if (!ref || ref->key != w.net || w.points.front() != t.anchor ||
                ++wired[found->first] != 1)
                connectivity("Wire is assigned to the wrong terminal/net, or connects an "
                             "open/dangling terminal.");
            break;
        }
        case WireRole::ground:
            if (!d.ground || w.terminal || w.net != d.ground->net ||
                w.points.back() != d.ground->anchor || ++ground_wires != 1)
                connectivity("Ground lead does not connect to the explicit ground symbol.");
            break;
        case WireRole::backbone:
            if (w.terminal)
                connectivity("A backbone cannot claim terminal ownership.");
            break;
        default:
            geometry("Unknown wire role.");
        }
    }
    if (ground_wires != (d.ground ? 1U : 0U))
        connectivity("Missing ground lead.");
    for (const auto& [id, t] : terminals) {
        budget.spend();
        if (wired.contains(id) != std::holds_alternative<DeclaredNode>(t->reference))
            connectivity(
                "A declared terminal lacks its wire, or an unconnected terminal acquired one.");
    }
    for (const auto& line : lines) {
        for (const auto& p : d.components) {
            budget.spend();
            const auto hit = rectangle_contact(line, p.body);
            if (hit.present) {
                const auto* ref =
                    hit.at == p.positive   ? std::get_if<DeclaredNode>(&p.source.positive)
                    : hit.at == p.negative ? std::get_if<DeclaredNode>(&p.source.negative)
                                           : nullptr;
                if (hit.overlap || !ref || ref->key != line.net)
                    geometry("A wire intersects a component body away from its assigned terminal.");
            }
        }
        for (const auto& label : d.labels) {
            budget.spend();
            if (rectangle_contact(line, label.bounds).present)
                geometry("A wire runs through a measured label rectangle.");
        }
        if (d.ground) {
            const auto hit = rectangle_contact(line, d.ground->body);
            if (hit.present &&
                (hit.overlap || hit.at != d.ground->anchor || line.net != d.ground->net))
                geometry("A wire intersects the ground symbol away from its assigned anchor.");
        }
        for (const auto& t : d.terminals) {
            budget.spend();
            if (on(line, t.anchor)) {
                const auto* ref = std::get_if<DeclaredNode>(&t.reference);
                if (!ref || ref->key != line.net)
                    connectivity(
                        "A wire touches an open, dangling or different-net terminal anchor.");
            }
        }
        for (const auto& n : d.nets) {
            budget.spend();
            if (on(line, n.anchor) && line.net != n.source.key)
                connectivity("A wire touches another net's inspection anchor.");
        }
    }
    // Reconstruct contacts from geometry WITHOUT using router annotations or recognition.
    // Union only same-net contacts; foreign-net intersections must be marked nonconnections.
    Union connected(d.wires.size());
    auto remaining_annotations = annotation_capacity(d, lines.size(), limits);
    auto reserve_annotation = [&] {
        count(1, remaining_annotations, "Verified contacts exhaust the total primitive budget.");
        --remaining_annotations;
    };
    std::map<ContactIndex, std::set<std::size_t>> contacts;
    std::set<CrossingIndex> cross;
    for (std::size_t i = 0; i < lines.size(); ++i)
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            budget.spend();
            const auto& a = lines[i];
            const auto& z = lines[j];
            const auto hit = intersect(a, z);
            if (!hit.present)
                continue;
            if (hit.overlap)
                geometry("Collinear wire overlap is unsupported and ambiguous.");
            if (a.wire == z.wire) {
                if (z.index != a.index + 1)
                    geometry("Wire self-intersection is unsupported.");
            } else if (a.net == z.net) {
                connected.join(a.wire, z.wire);
                if (!contacts.contains({a.net.analysis, a.net.value, hit.at.x, hit.at.y})) {
                    count(contacts.size() + 1, limits.junctions,
                          "Verified junctions exceed the configured budget.");
                    reserve_annotation();
                }
                auto& incident = contacts[{a.net.analysis, a.net.value, hit.at.x, hit.at.y}];
                incident.insert(a.wire);
                incident.insert(z.wire);
            } else {
                if (a.horizontal() == z.horizontal() || hit.at == a.a || hit.at == a.b ||
                    hit.at == z.a || hit.at == z.b)
                    connectivity("Different nets meet at an ambiguous endpoint; a crossing marker "
                                 "cannot repair this.");
                const CrossingIndex id{hit.at.x, hit.at.y, std::min(a.wire, z.wire),
                                       std::max(a.wire, z.wire)};
                if (!cross.contains(id)) {
                    count(cross.size() + 1, limits.crossings,
                          "Verified crossings exceed the configured budget.");
                    reserve_annotation();
                    cross.insert(id);
                }
            }
        }
    for (const auto& j : d.junctions) {
        budget.spend();
        math.point(j.at);
        const auto found = contacts.find({j.net.analysis, j.net.value, j.at.x, j.at.y});
        if (found == contacts.end() ||
            std::vector<std::size_t>(found->second.begin(), found->second.end()) != j.wires)
            connectivity("Junction identity or incident-wire metadata is incorrect or duplicated.");
        contacts.erase(found);
    }
    if (!contacts.empty())
        connectivity("An electrical wire contact has no explicit junction record.");
    for (const auto& c : d.crossings) {
        budget.spend();
        math.point(c.at);
        if (cross.erase({c.at.x, c.at.y, c.first_wire, c.second_wire}) != 1)
            connectivity(
                "Nonconnection marker has incorrect wire identities/position, or is duplicated.");
    }
    if (!cross.empty())
        connectivity("A different-net crossing lacks an explicit nonconnection marker.");
    for (const auto& n : d.nets) {
        std::optional<std::size_t> root;
        bool anchored = false;
        for (std::size_t i = 0; i < d.wires.size(); ++i) {
            budget.spend();
            if (d.wires[i].net == n.source.key) {
                const auto r = connected.root(i);
                if (root && *root != r)
                    connectivity("A represented electrical net contains disconnected wire paths.");
                root = r;
            }
        }
        for (const auto& line : lines) {
            budget.spend();
            anchored |= line.net == n.source.key && on(line, n.anchor);
        }
        if (root && !anchored)
            connectivity("A net anchor is disconnected from its routing.");
    }
    if (d.bounds != diagram_bounds(d, metrics, math))
        geometry("Diagram bounds do not match complete geometry and padding.");
}
} // namespace
VerificationResult verify_layout(const SchematicGraph& g, const LayoutDiagram& d,
                                 const LayoutMetrics& m, const LayoutLimits& limits) {
    Budget budget(limits.operations);
    try {
        verify(g, d, m, limits, budget);
        return {{}, budget.used};
    } catch (const Failure& e) {
        return {LayoutFailure{e.code, e.what()}, budget.used};
    } catch (const std::bad_alloc&) {
        return {LayoutFailure{LayoutErrorCode::resource_limit,
                              "Insufficient memory to verify complete schematic geometry."},
                budget.used};
    }
}
} // namespace openece::schematic
