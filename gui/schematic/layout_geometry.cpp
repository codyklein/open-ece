#include "layout_detail.hpp"
#include <openece/project/model.hpp>

namespace openece::schematic {
std::string node_label_text(const Node& n) {
    return n.name + " [" + std::to_string(n.key.value) + "]";
}
std::string component_label_text(const Component& c, LabelRole role) {
    if (role != LabelRole::component_name && role != LabelRole::component_value &&
        role != LabelRole::component_detail)
        throw std::invalid_argument("Expected a component label role.");
    return detail::label_text(c, role);
}
} // namespace openece::schematic

namespace openece::schematic::detail {
static_assert(layout_limits::nodes == project::limits::circuit_nodes);
static_assert(layout_limits::components == project::limits::circuit_components);
void preflight(const SchematicGraph& g, const LayoutMetrics& m, const LayoutLimits& l, Budget& b) {
    for (const auto [actual, ceiling] : {std::pair{l.nodes, layout_limits::nodes},
                                         {l.components, layout_limits::components},
                                         {l.terminals, layout_limits::terminals},
                                         {l.segments, layout_limits::segments},
                                         {l.junctions, layout_limits::junctions},
                                         {l.crossings, layout_limits::crossings},
                                         {l.labels, layout_limits::labels},
                                         {l.primitives, layout_limits::primitives},
                                         {l.operations, layout_limits::operations},
                                         {l.working_bytes, layout_limits::working_bytes}})
        if (actual > ceiling)
            fail(LayoutErrorCode::resource_limit,
                 "A requested layout limit exceeds its hard safety ceiling.");
    if (l.coordinate <= 0 || l.coordinate > layout_limits::coordinate)
        fail(LayoutErrorCode::invalid_metrics,
             "Coordinate limit must be positive and within the hard layout range.");
    count(g.nodes().size(), l.nodes, "Circuit exceeds the layout node limit.");
    count(g.components().size(), l.components, "Circuit exceeds the layout component limit.");
    count(g.components().size() * 2, l.terminals, "Circuit exceeds the layout terminal limit.");
    count(g.nodes().size() + g.components().size() * 3, l.labels,
          "Circuit exceeds the layout label limit.");
    const auto has_ground = std::holds_alternative<DeclaredNode>(g.ground());
    count(g.nodes().size() * 2 + g.components().size() * 6 + (has_ground ? 1U : 0U), l.primitives,
          "Required node/component/terminal/label geometry exceeds the primitive budget.");
    std::size_t connected = has_ground ? 1 : 0;
    for (const auto& c : g.components()) {
        b.spend();
        connected += std::holds_alternative<DeclaredNode>(c.positive) ? 1U : 0U;
        connected += std::holds_alternative<DeclaredNode>(c.negative) ? 1U : 0U;
    }
    count(connected, l.segments, "Required terminal/ground leads exceed the segment budget.");
    const Math math{l.coordinate};
    for (auto value :
         {m.grid, m.symbol_length, m.symbol_width, m.clearance, m.ground_size, m.marker_radius})
        if (value <= 0 || value > l.coordinate)
            fail(LayoutErrorCode::invalid_metrics,
                 "Layout metrics must be positive diagram units within the coordinate range.");
    const auto twice_grid = math.add(m.grid, m.grid);
    if (m.symbol_length % twice_grid || m.symbol_width % twice_grid || m.clearance < m.grid ||
        m.clearance % m.grid || m.ground_size % m.grid || m.marker_radius > m.clearance / 2)
        fail(LayoutErrorCode::invalid_metrics,
             "Symbols must span whole double-grid units; clearances/ground must align to the grid "
             "and contain markers.");
    auto check_extent = [&](Extent e) {
        if (e.width <= 0 || e.height <= 0)
            fail(LayoutErrorCode::invalid_metrics,
                 "Label extents must be positive and fit the coordinate budget.");
        if (e.width > l.coordinate || e.height > l.coordinate)
            fail(LayoutErrorCode::resource_limit,
                 "Label extents exceed the configured coordinate budget.");
    };
    check_extent(m.node_label);
    check_extent(m.component_label);
    count(m.labels.size(), l.labels, "Too many per-label measurements.");
    std::set<std::tuple<std::size_t, Analysis, std::uint32_t, LabelRole>> measured;
    for (const auto& entry : m.labels) {
        b.spend();
        check_extent(entry.extent);
        const auto [type, analysis, id] = key(entry.owner);
        bool exists = false;
        if (type == 0 && entry.role == LabelRole::node_name)
            for (const auto& n : g.nodes()) {
                b.spend();
                exists |= key(n.key) == std::tuple{analysis, id};
            }
        if (type == 1 &&
            (entry.role == LabelRole::component_name || entry.role == LabelRole::component_value ||
             entry.role == LabelRole::component_detail))
            for (const auto& c : g.components()) {
                b.spend();
                exists |= key(c.key) == std::tuple{analysis, id};
            }
        if (!exists || !measured.emplace(type, analysis, id, entry.role).second)
            fail(LayoutErrorCode::invalid_metrics,
                 "Label measurements contain an unknown owner/role or duplicate entry.");
    }
    count(memory_bound(g, l), l.working_bytes,
          "Layout working-storage bound exceeds the configured memory budget.");
}
std::size_t memory_bound(const SchematicGraph& g, const LayoutLimits& l) {
    std::size_t text = 0, points = 0;
    auto quantity = [&](const RawQuantity& q) { text += q.text.size() + q.unit.size(); };
    for (const auto& n : g.nodes())
        text += n.name.size();
    for (const auto& c : g.components()) {
        text += c.name.size() + c.kind.size();
        quantity(c.value);
        if (c.phase)
            quantity(*c.phase);
        if (c.source) {
            text += c.source->mode.size();
            points += c.source->points.size();
            for (const auto& p : c.source->points) {
                quantity(p.time);
                text += p.value_text.size();
            }
        }
    }
    // Projection bounds text/point counts. Conservative capacities include both annotation
    // and independent verification maps, allocator overhead, frozen text and label copies.
    return 4096 + text * 3 + points * 256 + l.crossings * 256 +
           l.junctions * (512 + 16 * (l.terminals + l.nodes + 1)) + l.segments * 512 +
           l.labels * 512 + l.components * 1024;
}
std::vector<Segment> segments(const LayoutDiagram& d, const LayoutLimits& l, const Math& math,
                              Budget& b) {
    count(d.nets.size(), l.nodes, "Too many net placements.");
    count(d.components.size(), l.components, "Too many component placements.");
    count(d.terminals.size(), l.terminals, "Too many terminal placements.");
    count(d.wires.size(), l.segments, "Too many wire paths.");
    count(d.junctions.size(), l.junctions, "Too many junction records.");
    count(d.crossings.size(), l.crossings, "Too many crossing records.");
    count(d.labels.size(), l.labels, "Too many label placements.");
    count(d.diagnostics.size(), l.components + 3, "Too many layout diagnostics.");
    std::size_t remaining = l.primitives;
    auto consume = [&](std::size_t n) {
        count(n, remaining, "Total layout primitive budget exceeded.");
        remaining -= n;
    };
    for (auto n : {d.nets.size(), d.components.size(), d.terminals.size(), d.junctions.size(),
                   d.crossings.size(), d.labels.size(), d.ground ? std::size_t{1} : std::size_t{0}})
        consume(n);
    std::vector<Segment> out;
    for (std::size_t i = 0; i < d.wires.size(); ++i) {
        const auto& w = d.wires[i];
        if (w.points.size() < 2)
            fail(LayoutErrorCode::invalid_geometry, "A wire has no routing segment.");
        count(w.points.size() - 1, l.segments - out.size(), "Wire segment budget exceeded.");
        consume(w.points.size() - 1);
        for (std::size_t j = 1; j < w.points.size(); ++j) {
            b.spend();
            const auto a = w.points[j - 1], z = w.points[j];
            math.point(a);
            math.point(z);
            if (a == z || (a.x != z.x && a.y != z.y))
                fail(LayoutErrorCode::invalid_geometry,
                     "Wire segments must be nonzero and orthogonal.");
            out.push_back({a, z, w.net, i, j - 1});
        }
    }
    for (const auto& j : d.junctions)
        count(j.wires.size(), d.wires.size(), "Junction incident-wire list exceeds wire count.");
    return out;
}
std::size_t annotation_capacity(const LayoutDiagram& d, std::size_t lines, const LayoutLimits& l) {
    std::size_t remaining = l.primitives;
    for (auto n : {d.nets.size(), d.components.size(), d.terminals.size(), d.labels.size(), lines,
                   d.ground ? std::size_t{1} : std::size_t{0}}) {
        count(n, remaining, "Base geometry exhausts the total primitive budget.");
        remaining -= n;
    }
    return remaining;
}
Rectangle diagram_bounds(const LayoutDiagram& d, const LayoutMetrics& m, const Math& math) {
    std::optional<Point> lo, hi;
    auto point = [&](Point p) {
        math.point(p);
        if (!lo) {
            lo = hi = p;
            return;
        }
        lo->x = std::min(lo->x, p.x);
        lo->y = std::min(lo->y, p.y);
        hi->x = std::max(hi->x, p.x);
        hi->y = std::max(hi->y, p.y);
    };
    auto rect = [&](Rectangle r) {
        math.rectangle(r);
        point(r.origin);
        point({math.add(r.origin.x, r.size.width), math.add(r.origin.y, r.size.height)});
    };
    for (const auto& n : d.nets)
        rect(marker(n.anchor, m.marker_radius, math));
    for (const auto& c : d.components)
        rect(c.body);
    for (const auto& t : d.terminals)
        rect(marker(t.anchor, m.marker_radius, math));
    for (const auto& w : d.wires)
        for (auto p : w.points)
            point(p);
    for (const auto& j : d.junctions)
        rect(marker(j.at, m.marker_radius, math));
    for (const auto& c : d.crossings)
        rect(marker(c.at, m.marker_radius, math));
    for (const auto& l : d.labels)
        rect(l.bounds);
    if (d.ground)
        rect(d.ground->body);
    if (!lo)
        return {};
    return {{math.add(lo->x, -m.clearance), math.add(lo->y, -m.clearance)},
            {math.add(math.add(hi->x, -lo->x), math.add(m.clearance, m.clearance)),
             math.add(math.add(hi->y, -lo->y), math.add(m.clearance, m.clearance))}};
}
} // namespace openece::schematic::detail
