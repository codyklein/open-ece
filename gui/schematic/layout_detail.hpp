#pragma once
#include "layout.hpp"
#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <tuple>

namespace openece::schematic::detail {
struct Failure : std::runtime_error {
    LayoutErrorCode code;
    Failure(LayoutErrorCode c, const std::string& message) : std::runtime_error(message), code(c) {}
};
[[noreturn]] inline void fail(LayoutErrorCode code, const char* message) {
    throw Failure(code, message);
}
struct Budget {
    std::size_t used = 0, maximum;
    explicit Budget(std::size_t m) : maximum(m) {}
    void spend(std::size_t n = 1) {
        if (n > maximum - used)
            fail(LayoutErrorCode::resource_limit,
                 "Layout operation budget exhausted; reduce the circuit or label measurements.");
        used += n;
    }
};
struct Math {
    Coordinate maximum;
    Coordinate add(Coordinate a, Coordinate b) const {
        if (a < -maximum || a > maximum || b < -maximum || b > maximum ||
            (b > 0 && a > maximum - b) || (b < 0 && a < -maximum - b))
            fail(LayoutErrorCode::resource_limit, "Diagram exceeds the configured coordinate "
                                                  "range; reduce label sizes or circuit size.");
        return a + b;
    }
    Coordinate multiply(Coordinate a, std::size_t b) const {
        if (a < 0 || a > maximum || (a != 0 && b > static_cast<std::size_t>(maximum / a)))
            fail(LayoutErrorCode::resource_limit, "Diagram spacing exceeds the coordinate budget.");
        return a * static_cast<Coordinate>(b);
    }
    Coordinate snap(Coordinate a, Coordinate grid) const {
        if (a < 0)
            fail(LayoutErrorCode::invalid_metrics, "Cannot align a negative spacing to the grid.");
        const auto remainder = a % grid;
        return remainder ? add(a, grid - remainder) : a;
    }
    void point(Point p) const {
        if (p.x < -maximum || p.x > maximum || p.y < -maximum || p.y > maximum)
            fail(LayoutErrorCode::resource_limit,
                 "Geometry coordinate is outside the configured range.");
    }
    void rectangle(const Rectangle& r, bool empty = false) const {
        point(r.origin);
        if (r.size.width < (empty ? 0 : 1) || r.size.height < (empty ? 0 : 1))
            fail(LayoutErrorCode::invalid_geometry, "A geometry rectangle has invalid dimensions.");
        add(r.origin.x, r.size.width);
        add(r.origin.y, r.size.height);
    }
};
inline auto key(NodeKey k) { return std::tuple{k.analysis, k.value}; }
inline auto key(ComponentKey k) { return std::tuple{k.analysis, k.value}; }
inline auto key(TerminalKey k) {
    return std::tuple{k.component.analysis, k.component.value, k.side};
}
inline auto key(Point p) { return std::tuple{p.x, p.y}; }
inline auto key(const LabelOwner& owner) {
    return std::visit([&](auto k) { return std::tuple{owner.index(), k.analysis, k.value}; },
                      owner);
}
inline bool between(Coordinate a, Coordinate b, Coordinate p) {
    return std::min(a, b) <= p && p <= std::max(a, b);
}
struct Segment {
    Point a, b;
    NodeKey net;
    std::size_t wire, index;
    bool horizontal() const { return a.y == b.y; }
};
inline bool on(const Segment& s, Point p) {
    return between(s.a.x, s.b.x, p.x) && between(s.a.y, s.b.y, p.y);
}
struct Intersection {
    bool present = false, overlap = false;
    Point at;
};
inline Intersection intersect(const Segment& a, const Segment& b) {
    if (a.horizontal() == b.horizontal()) {
        if ((a.horizontal() && a.a.y != b.a.y) || (!a.horizontal() && a.a.x != b.a.x))
            return {};
        const auto a0 = a.horizontal() ? a.a.x : a.a.y;
        const auto a1 = a.horizontal() ? a.b.x : a.b.y;
        const auto b0 = b.horizontal() ? b.a.x : b.a.y;
        const auto b1 = b.horizontal() ? b.b.x : b.b.y;
        const auto lo = std::max(std::min(a0, a1), std::min(b0, b1));
        const auto hi = std::min(std::max(a0, a1), std::max(b0, b1));
        return {lo <= hi, lo < hi, a.horizontal() ? Point{lo, a.a.y} : Point{a.a.x, lo}};
    }
    const auto& h = a.horizontal() ? a : b;
    const auto& v = a.horizontal() ? b : a;
    const Point p{v.a.x, h.a.y};
    return {on(h, p) && on(v, p), false, p};
}
inline bool overlaps(const Rectangle& a, const Rectangle& b) {
    return a.origin.x < b.origin.x + b.size.width && b.origin.x < a.origin.x + a.size.width &&
           a.origin.y < b.origin.y + b.size.height && b.origin.y < a.origin.y + a.size.height;
}
inline Intersection rectangle_contact(const Segment& s, const Rectangle& r) {
    const auto fixed = s.horizontal() ? s.a.y : s.a.x;
    const auto edge = s.horizontal() ? r.origin.y : r.origin.x;
    const auto width = s.horizontal() ? r.size.height : r.size.width;
    if (fixed < edge || fixed > edge + width)
        return {};
    const auto origin = s.horizontal() ? r.origin.x : r.origin.y;
    const auto length = s.horizontal() ? r.size.width : r.size.height;
    const auto a = s.horizontal() ? s.a.x : s.a.y;
    const auto b = s.horizontal() ? s.b.x : s.b.y;
    const auto lo = std::max(std::min(a, b), origin);
    const auto hi = std::min(std::max(a, b), origin + length);
    return {lo <= hi, lo < hi, s.horizontal() ? Point{lo, fixed} : Point{fixed, lo}};
}
inline SymbolKind symbol(const std::string& token) {
    if (token == "resistor")
        return SymbolKind::resistor;
    if (token == "capacitor")
        return SymbolKind::capacitor;
    if (token == "inductor")
        return SymbolKind::inductor;
    if (token == "voltage_source")
        return SymbolKind::voltage_source;
    if (token == "current_source")
        return SymbolKind::current_source;
    return SymbolKind::generic;
}
inline std::string label_text(const Component& c, LabelRole role) {
    if (role == LabelRole::component_name)
        return c.name + " [" + std::to_string(c.key.value) + "]";
    if (role == LabelRole::component_value) {
        auto text = c.value.text + " " + c.value.unit;
        if (c.phase && (c.kind == "voltage_source" || c.kind == "current_source"))
            text += " ∠ " + c.phase->text + " " + c.phase->unit;
        return text;
    }
    auto text = c.kind;
    if (c.source)
        text += " | " + c.source->mode + " (" + std::to_string(c.source->points.size()) +
                " retained points)";
    for (auto side : {TerminalSide::positive, TerminalSide::negative}) {
        const auto& ref = side == TerminalSide::positive ? c.positive : c.negative;
        const auto prefix = side == TerminalSide::positive ? " | + " : " | - ";
        if (std::holds_alternative<Unselected>(ref))
            text += std::string(prefix) + "unselected";
        if (const auto* missing = std::get_if<DanglingNode>(&ref))
            text += std::string(prefix) + "missing node " + std::to_string(missing->key.value);
    }
    return text;
}
inline Extent extent(const LayoutMetrics& m, LabelOwner owner, LabelRole role, Budget& budget) {
    for (const auto& entry : m.labels) {
        budget.spend();
        if (entry.owner == owner && entry.role == role)
            return entry.extent;
    }
    return role == LabelRole::node_name ? m.node_label : m.component_label;
}
void preflight(const SchematicGraph&, const LayoutMetrics&, const LayoutLimits&, Budget&);
std::size_t memory_bound(const SchematicGraph&, const LayoutLimits&);
std::vector<Segment> segments(const LayoutDiagram&, const LayoutLimits&, const Math&, Budget&);
Rectangle diagram_bounds(const LayoutDiagram&, const LayoutMetrics&, const Math&);
std::size_t annotation_capacity(const LayoutDiagram&, std::size_t segments, const LayoutLimits&);

inline void count(std::size_t actual, std::size_t maximum, const char* message) {
    if (actual > maximum)
        fail(LayoutErrorCode::resource_limit, message);
}
inline Rectangle marker(Point p, Coordinate radius, const Math& math) {
    return {{math.add(p.x, -radius), math.add(p.y, -radius)},
            {math.add(radius, radius), math.add(radius, radius)}};
}
} // namespace openece::schematic::detail
