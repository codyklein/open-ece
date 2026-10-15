#include "layout_detail.hpp"

namespace openece::schematic {
namespace {
using namespace detail;
struct Step {
    const Component* component;
    NodeKey from, to;
};
struct Branches {
    NodeKey top, bottom;
    std::vector<std::vector<Step>> paths;
};
std::optional<Branches> recognize(const SchematicGraph& g, Budget& b) {
    if (g.components().empty() || g.nodes().size() < 2)
        return std::nullopt;
    std::map<std::uint32_t, std::vector<const Component*>> adjacent;
    for (const auto& n : g.nodes())
        adjacent[n.key.value] = {};
    for (const auto& c : g.components()) {
        b.spend();
        const auto* p = std::get_if<DeclaredNode>(&c.positive);
        const auto* n = std::get_if<DeclaredNode>(&c.negative);
        if (!p || !n || p->key == n->key)
            return std::nullopt;
        adjacent.at(p->key.value).push_back(&c);
        adjacent.at(n->key.value).push_back(&c);
    }
    std::vector<std::uint32_t> hubs;
    for (auto& [id, edges] : adjacent) {
        b.spend();
        if (edges.empty())
            return std::nullopt;
        std::sort(edges.begin(), edges.end(), [&](auto a, auto z) {
            b.spend();
            return key(a->key) < key(z->key);
        });
        if (edges.size() != 2)
            hubs.push_back(id);
    }
    if (hubs.empty()) {
        // A simple cycle: prefer the lowest-ID independent voltage source as the first branch.
        const Component* seed = nullptr;
        for (const auto& c : g.components()) {
            b.spend();
            if (!seed || std::tuple{c.kind != "voltage_source", c.key.value} <
                             std::tuple{seed->kind != "voltage_source", seed->key.value})
                seed = &c;
        }
        hubs = {std::get<DeclaredNode>(seed->positive).key.value,
                std::get<DeclaredNode>(seed->negative).key.value};
    }
    if (hubs.size() != 2)
        return std::nullopt;
    if (const auto* ground = std::get_if<DeclaredNode>(&g.ground());
        ground && hubs[0] == ground->key.value)
        std::swap(hubs[0], hubs[1]);
    Branches out{{g.analysis(), hubs[0]}, {g.analysis(), hubs[1]}, {}};
    std::set<std::uint32_t> visited;
    for (const auto* first : adjacent.at(hubs[0])) {
        std::vector<Step> path;
        auto at = hubs[0];
        const Component* edge = first;
        while (true) {
            b.spend();
            if (!visited.insert(edge->key.value).second)
                return std::nullopt;
            const auto p = std::get<DeclaredNode>(edge->positive).key;
            const auto n = std::get<DeclaredNode>(edge->negative).key;
            const auto next = p.value == at ? n : p;
            path.push_back({edge, {g.analysis(), at}, next});
            at = next.value;
            if (at == hubs[1])
                break;
            if (at == hubs[0] || adjacent.at(at).size() != 2)
                return std::nullopt;
            const auto& choices = adjacent.at(at);
            edge = choices[0] == edge ? choices[1] : choices[0];
        }
        out.paths.push_back(std::move(path));
    }
    if (visited.size() != g.components().size())
        return std::nullopt;
    std::sort(out.paths.begin(), out.paths.end(), [&](const auto& a, const auto& z) {
        b.spend();
        return std::lexicographical_compare(
            a.begin(), a.end(), z.begin(), z.end(), [](const Step& x, const Step& y) {
                return key(x.component->key) < key(y.component->key);
            });
    });
    return out;
}
struct Builder {
    const SchematicGraph& graph;
    const LayoutMetrics& metrics;
    const LayoutLimits& limits;
    Budget& budget;
    Math math;
    LayoutDiagram d;
    Coordinate node_width = 0, node_height = 0, label_width = 0, block_height = 0;
    Builder(const SchematicGraph& g, const LayoutMetrics& m, const LayoutLimits& l, Budget& b,
            LayoutStrategy strategy)
        : graph(g), metrics(m), limits(l), budget(b), math{l.coordinate} {
        d.analysis = g.analysis();
        d.ground_selection = g.ground();
        d.strategy = strategy;
        d.marker_radius = m.marker_radius;
        for (const auto& n : g.nodes()) {
            const auto size = extent(m, n.key, LabelRole::node_name, b);
            node_width = std::max(node_width, size.width);
            node_height = std::max(node_height, size.height);
        }
        for (const auto& c : g.components()) {
            Coordinate height = 0;
            for (auto role : {LabelRole::component_name, LabelRole::component_value,
                              LabelRole::component_detail}) {
                const auto size = extent(m, c.key, role, b);
                label_width = std::max(label_width, size.width);
                height = math.add(height, size.height);
            }
            block_height = std::max(block_height, math.add(height, math.add(m.grid, m.grid)));
            if (symbol(c.kind) == SymbolKind::generic)
                d.diagnostics.push_back(
                    "Component " + std::to_string(c.key.value) +
                    " uses an unknown kind; a generic symbol retains its original data.");
        }
        if (!std::holds_alternative<DeclaredNode>(g.ground()))
            d.diagnostics.push_back("Ground is unselected or references a missing node; no ground "
                                    "symbol has been invented.");
    }
    void wire(NodeKey net, WireRole role, std::optional<TerminalKey> terminal,
              std::vector<Point> points) {
        budget.spend();
        count(d.wires.size() + 1, limits.segments, "Wire-path budget exhausted.");
        d.wires.push_back({net, role, terminal, std::move(points)});
    }
    void part(const Component& c, Point center, Direction direction) {
        const bool vertical = direction == Direction::down || direction == Direction::up;
        const auto half = metrics.symbol_length / 2, thickness = metrics.symbol_width / 2;
        const Point start = vertical ? Point{center.x, math.add(center.y, -half)}
                                     : Point{math.add(center.x, -half), center.y};
        const Point end = vertical ? Point{center.x, math.add(center.y, half)}
                                   : Point{math.add(center.x, half), center.y};
        const bool forward = direction == Direction::down || direction == Direction::right;
        const Point positive = forward ? start : end, negative = forward ? end : start;
        const Rectangle body{vertical ? Point{math.add(center.x, -thickness), start.y}
                                      : Point{start.x, math.add(center.y, -thickness)},
                             vertical ? Extent{metrics.symbol_width, metrics.symbol_length}
                                      : Extent{metrics.symbol_length, metrics.symbol_width}};
        d.components.push_back({c, symbol(c.kind), direction, center, positive, negative, body});
        d.terminals.push_back({{c.key, TerminalSide::positive}, c.positive, positive});
        d.terminals.push_back({{c.key, TerminalSide::negative}, c.negative, negative});
    }
    void component_labels(const Component& c, Point origin) {
        for (auto role :
             {LabelRole::component_name, LabelRole::component_value, LabelRole::component_detail}) {
            const auto size = extent(metrics, c.key, role, budget);
            d.labels.push_back({c.key, role, component_label_text(c, role), {origin, size}});
            origin.y = math.add(origin.y, math.add(size.height, metrics.grid));
        }
    }
    Coordinate component_height(const Component& c) {
        Coordinate h = math.add(metrics.grid, metrics.grid);
        for (auto role :
             {LabelRole::component_name, LabelRole::component_value, LabelRole::component_detail})
            h = math.add(h, extent(metrics, c.key, role, budget).height);
        return h;
    }
    void ground(Point start, Point anchor, NodeKey net, bool elbow) {
        const auto size = metrics.ground_size;
        d.ground =
            GroundPlacement{net, anchor, {{math.add(anchor.x, -size / 2), anchor.y}, {size, size}}};
        if (elbow)
            wire(net, WireRole::ground, {}, {start, {anchor.x, start.y}, anchor});
        else
            wire(net, WireRole::ground, {}, {start, anchor});
    }
};
LayoutDiagram rails(const SchematicGraph& g, const LayoutMetrics& m, const LayoutLimits& l,
                    Budget& work) {
    Builder b(g, m, l, work, LayoutStrategy::net_rails);
    b.d.diagnostics.push_back("Connectivity fallback: separate labeled net rails; marked crossings "
                              "never join different nets.");
    std::vector<const Node*> nodes;
    std::vector<const Component*> parts;
    for (const auto& n : g.nodes())
        nodes.push_back(&n);
    for (const auto& c : g.components())
        parts.push_back(&c);
    std::sort(nodes.begin(), nodes.end(), [](auto a, auto z) { return key(a->key) < key(z->key); });
    std::sort(parts.begin(), parts.end(), [](auto a, auto z) { return key(a->key) < key(z->key); });
    const auto clearance = m.clearance;
    const auto rail_step =
        b.math.snap(b.math.add(b.node_width, b.math.add(clearance, clearance)), m.grid);
    const auto rail_top =
        b.math.snap(b.math.add(b.node_height, b.math.multiply(clearance, 2)), m.grid);
    const auto band = b.math.snap(
        b.math.add(std::max(b.block_height, m.symbol_width), b.math.multiply(clearance, 2)),
        b.math.multiply(m.grid, 2));
    const auto pitch = b.math.add(band, b.math.multiply(clearance, 2));
    const auto rail_end =
        b.math.add(b.math.add(rail_top, b.math.multiply(pitch, parts.size())), clearance);
    std::map<std::uint32_t, Coordinate> xs;
    Coordinate last_x = b.math.add(clearance, b.math.snap(b.node_width / 2, m.grid));
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        work.spend();
        const auto x = b.math.add(last_x, b.math.multiply(rail_step, i));
        const auto* n = nodes[i];
        xs[n->key.value] = x;
        b.d.nets.push_back({*n, {x, rail_top}});
        b.wire(n->key, WireRole::backbone, {}, {{x, rail_top}, {x, rail_end}});
        const auto size = extent(m, n->key, LabelRole::node_name, work);
        b.d.labels.push_back({n->key,
                              LabelRole::node_name,
                              node_label_text(*n),
                              {{b.math.add(x, -size.width / 2), clearance}, size}});
    }
    if (!nodes.empty())
        last_x = xs.at(nodes.back()->key.value);
    const auto x =
        b.math.add(last_x, b.math.add(b.math.multiply(clearance, 2), m.symbol_length / 2));
    const auto escape = b.math.add(x, b.math.add(m.symbol_length / 2, clearance));
    for (std::size_t i = 0; i < parts.size(); ++i) {
        work.spend();
        const auto& c = *parts[i];
        const auto y = b.math.add(
            rail_top, b.math.add(b.math.multiply(pitch, i), b.math.add(clearance, band / 2)));
        b.part(c, {x, y}, Direction::right);
        b.component_labels(
            c, {b.math.add(escape, clearance), b.math.add(y, -b.component_height(c) / 2)});
        for (auto side : {TerminalSide::positive, TerminalSide::negative}) {
            const auto& t =
                b.d.terminals[b.d.terminals.size() - (side == TerminalSide::positive ? 2 : 1)];
            if (const auto* ref = std::get_if<DeclaredNode>(&t.reference)) {
                const auto net_x = xs.at(ref->key.value);
                if (side == TerminalSide::positive)
                    b.wire(ref->key, WireRole::terminal, t.key, {t.anchor, {net_x, y}});
                else {
                    const auto return_y = b.math.add(y, b.math.add(band / 2, clearance));
                    b.wire(ref->key, WireRole::terminal, t.key,
                           {t.anchor, {escape, y}, {escape, return_y}, {net_x, return_y}});
                }
            }
        }
    }
    if (const auto* ground = std::get_if<DeclaredNode>(&g.ground())) {
        const auto net_x = xs.at(ground->key.value);
        b.ground({net_x, rail_end}, {net_x, b.math.add(rail_end, clearance)}, ground->key, false);
    }
    return std::move(b.d);
}
LayoutDiagram structured(const SchematicGraph& g, const Branches& paths, const LayoutMetrics& m,
                         const LayoutLimits& l, Budget& work) {
    Builder b(g, m, l, work, LayoutStrategy::structured);
    const auto clear = m.clearance;
    const auto step_x =
        b.math.snap(b.math.add(b.math.add(b.label_width, b.node_width),
                               b.math.add(m.ground_size, b.math.multiply(clear, 6))),
                    m.grid);
    const auto pitch = b.math.snap(
        b.math.add(std::max(m.symbol_length, b.block_height),
                   b.math.add(b.node_height, b.math.add(b.math.multiply(clear, 4),
                                                        b.math.multiply(m.ground_size, 2)))),
        b.math.multiply(m.grid, 2));
    std::size_t depth = 0;
    for (const auto& p : paths.paths)
        depth = std::max(depth, p.size());
    const auto first_x = b.math.add(m.ground_size, b.math.multiply(clear, 4));
    const auto top = b.math.snap(b.math.add(b.node_height, b.math.multiply(clear, 3)), m.grid);
    const auto span = b.math.multiply(pitch, depth + 1);
    const auto bottom = b.math.add(top, span);
    const auto last_x = b.math.add(first_x, b.math.multiply(step_x, paths.paths.size() - 1));
    std::map<std::uint32_t, Point> anchors{{paths.top.value, {first_x, top}},
                                           {paths.bottom.value, {first_x, bottom}}};
    if (paths.paths.size() > 1) {
        b.wire(paths.top, WireRole::backbone, {}, {{first_x, top}, {last_x, top}});
        b.wire(paths.bottom, WireRole::backbone, {}, {{first_x, bottom}, {last_x, bottom}});
    }
    for (std::size_t branch = 0; branch < paths.paths.size(); ++branch) {
        const auto& p = paths.paths[branch];
        const auto x = b.math.add(first_x, b.math.multiply(step_x, branch));
        std::vector<Point> upper, lower;
        for (std::size_t i = 0; i < p.size(); ++i) {
            work.spend();
            // Quotient/remainder avoids multiplying a large span before division.
            const auto divisor = static_cast<Coordinate>(p.size() + 1);
            auto offset = b.math.add(b.math.multiply(span / divisor, i + 1),
                                     b.math.multiply(span % divisor, i + 1) / divisor);
            offset -= offset % m.grid;
            const auto y = b.math.add(top, offset);
            const auto& c = *p[i].component;
            const bool forward = std::get<DeclaredNode>(c.positive).key == p[i].from;
            b.part(c, {x, y}, forward ? Direction::down : Direction::up);
            upper.push_back({x, b.math.add(y, -m.symbol_length / 2)});
            lower.push_back({x, b.math.add(y, m.symbol_length / 2)});
            b.component_labels(c, {b.math.add(x, b.math.add(m.symbol_width / 2, clear)),
                                   b.math.add(y, -b.component_height(c) / 2)});
        }
        for (std::size_t i = 0; i < p.size(); ++i) {
            const Point before = i == 0 ? Point{x, top} : anchors.at(p[i].from.value);
            const Point after =
                i + 1 == p.size()
                    ? Point{x, bottom}
                    : Point{x, b.math.add(lower[i].y, (upper[i + 1].y - lower[i].y) / 2)};
            if (i + 1 != p.size())
                anchors[p[i].to.value] = after;
            const auto& c = *p[i].component;
            const bool forward = std::get<DeclaredNode>(c.positive).key == p[i].from;
            b.wire(p[i].from, WireRole::terminal,
                   TerminalKey{c.key, forward ? TerminalSide::positive : TerminalSide::negative},
                   {upper[i], before});
            b.wire(p[i].to, WireRole::terminal,
                   TerminalKey{c.key, forward ? TerminalSide::negative : TerminalSide::positive},
                   {lower[i], after});
        }
    }
    std::vector<const Node*> nodes;
    for (const auto& n : g.nodes())
        nodes.push_back(&n);
    std::sort(nodes.begin(), nodes.end(), [](auto a, auto z) { return key(a->key) < key(z->key); });
    for (const auto* n : nodes) {
        const auto p = anchors.at(n->key.value);
        b.d.nets.push_back({*n, p});
        const auto size = extent(m, n->key, LabelRole::node_name, work);
        Point origin{b.math.add(p.x, clear), b.math.add(p.y, -size.height / 2)};
        if (n->key == paths.top)
            origin.y = b.math.add(top, -b.math.add(size.height, clear));
        if (n->key == paths.bottom)
            origin.y = b.math.add(bottom, clear);
        b.d.labels.push_back({n->key, LabelRole::node_name, node_label_text(*n), {origin, size}});
    }
    if (const auto* ground = std::get_if<DeclaredNode>(&g.ground())) {
        const auto start = anchors.at(ground->key.value);
        b.ground(start,
                 {b.math.add(start.x, -b.math.multiply(clear, 2)), b.math.add(start.y, clear)},
                 ground->key, true);
    }
    return std::move(b.d);
}
void annotate(LayoutDiagram& d, const LayoutMetrics& m, const LayoutLimits& l, Budget& b) {
    const auto lines = segments(d, l, Math{l.coordinate}, b);
    auto remaining = annotation_capacity(d, lines.size(), l);
    auto reserve_annotation = [&] {
        count(1, remaining, "Contact annotations exhaust the total primitive budget.");
        --remaining;
    };
    std::map<std::tuple<Analysis, std::uint32_t, Coordinate, Coordinate>, std::set<std::size_t>>
        joins;
    for (std::size_t i = 0; i < lines.size(); ++i)
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            b.spend();
            const auto& a = lines[i];
            const auto& z = lines[j];
            const auto hit = intersect(a, z);
            if (!hit.present)
                continue;
            if (hit.overlap)
                fail(LayoutErrorCode::invalid_geometry,
                     "Routing produced overlapping wire segments.");
            if (a.wire == z.wire) {
                if (z.index != a.index + 1)
                    fail(LayoutErrorCode::invalid_geometry,
                         "Routing produced a self-intersecting wire.");
                continue;
            }
            if (a.net == z.net) {
                if (!joins.contains({a.net.analysis, a.net.value, hit.at.x, hit.at.y})) {
                    count(joins.size() + 1, l.junctions, "Junction budget exhausted.");
                    reserve_annotation();
                }
                auto& wires = joins[{a.net.analysis, a.net.value, hit.at.x, hit.at.y}];
                wires.insert(a.wire);
                wires.insert(z.wire);
            } else {
                if (a.horizontal() == z.horizontal() || hit.at == a.a || hit.at == a.b ||
                    hit.at == z.a || hit.at == z.b)
                    fail(LayoutErrorCode::connectivity,
                         "Different nets touch at an ambiguous endpoint.");
                count(d.crossings.size() + 1, l.crossings,
                      "Nonconnecting crossing-marker budget exhausted; reduce the circuit or "
                      "increase the configured budget within its ceiling.");
                reserve_annotation();
                d.crossings.push_back({hit.at, std::min(a.wire, z.wire), std::max(a.wire, z.wire)});
            }
        }
    for (const auto& [k, wires] : joins) {
        const auto [analysis, id, x, y] = k;
        d.junctions.push_back({{analysis, id}, {x, y}, {wires.begin(), wires.end()}});
    }
    std::sort(d.crossings.begin(), d.crossings.end(), [](const auto& a, const auto& z) {
        return std::tuple{a.at.x, a.at.y, a.first_wire, a.second_wire} <
               std::tuple{z.at.x, z.at.y, z.first_wire, z.second_wire};
    });
    d.bounds = diagram_bounds(d, m, Math{l.coordinate});
}
} // namespace
LayoutResult layout_schematic(const SchematicGraph& g, const LayoutMetrics& m,
                              const LayoutLimits& l) {
    Budget budget(l.operations);
    try {
        preflight(g, m, l, budget);
        std::string reason = "No conservative series/parallel recognition matched this draft.";
        auto finish = [&](LayoutDiagram d) -> LayoutResult {
            std::sort(d.diagnostics.begin(), d.diagnostics.end());
            annotate(d, m, l, budget);
            auto verification_limits = l;
            verification_limits.operations -= budget.used;
            const auto checked = verify_layout(g, d, m, verification_limits);
            budget.spend(checked.operations);
            if (checked.failure)
                throw Failure(checked.failure->code, checked.failure->message);
            d.operations = budget.used;
            d.working_bytes_bound = memory_bound(g, l);
            return LayoutResult{std::move(d)};
        };
        if (const auto paths = recognize(g, budget)) {
            try {
                return finish(structured(g, *paths, m, l, budget));
            } catch (const Failure& e) {
                reason = std::string("Structured geometry rejected: ") + e.what();
            }
        }
        auto fallback = rails(g, m, l, budget);
        fallback.diagnostics.push_back(std::move(reason));
        return finish(std::move(fallback));
    } catch (const Failure& e) {
        return LayoutFailure{e.code, e.what()};
    } catch (const std::bad_alloc&) {
        return LayoutFailure{
            LayoutErrorCode::resource_limit,
            "Insufficient memory for a complete schematic; no partial diagram was returned."};
    }
}
} // namespace openece::schematic
