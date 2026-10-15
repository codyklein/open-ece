#include "model.hpp"
#include <openece/project/model.hpp>
#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace openece::schematic {
namespace {
using Ids = std::unordered_set<std::uint32_t>;
[[noreturn]] void resource(const char* message) {
    throw ProjectionError(ProjectionErrorCode::resource_limit, message);
}
struct Budget {
    std::size_t text_left = project::limits::text_bytes;
    std::size_t points_left = project::limits::transient_total_source_points;
    void text(std::string_view s, std::size_t maximum = project::limits::cell_bytes) {
        if (s.size() > maximum || s.size() > text_left)
            resource("Schematic projection exceeds draft text storage limits.");
        text_left -= s.size();
    }
    void quantity(const project::Quantity& q) {
        text(q.text);
        text(q.unit);
    }
    void source(const project::TransientSource& s) {
        if (s.points.size() > project::limits::transient_source_points ||
            s.points.size() > points_left)
            resource("Schematic projection exceeds draft source-point limits.");
        points_left -= s.points.size();
        text(s.mode);
        for (const auto& p : s.points) {
            quantity(p.time);
            text(p.value_text);
        }
    }
};
template <class Draft> Ids check_bounds_and_ids(const Draft& d) {
    if (d.nodes.size() > project::limits::circuit_nodes ||
        d.components.size() > project::limits::circuit_components)
        resource("Schematic projection exceeds draft node/component limits.");
    Budget budget;
    Ids nodes, parts;
    for (const auto& n : d.nodes) {
        if (!nodes.insert(n.id.value).second)
            throw ProjectionError(ProjectionErrorCode::duplicate_node_id,
                                  "Duplicate declared node ID: " + std::to_string(n.id.value));
        budget.text(n.name, project::limits::name_bytes);
    }
    for (const auto& c : d.components) {
        if (!parts.insert(c.id.value).second)
            throw ProjectionError(ProjectionErrorCode::duplicate_component_id,
                                  "Duplicate declared component ID: " + std::to_string(c.id.value));
        budget.text(c.name, project::limits::name_bytes);
        budget.text(c.kind);
        budget.quantity(c.value);
        if constexpr (std::is_same_v<Draft, project::AcDraft>)
            budget.quantity(c.phase);
        if constexpr (std::is_same_v<Draft, project::TransientDraft>)
            budget.source(c.source);
    }
    return nodes;
}
NodeReference reference(project::Reference ref, Analysis analysis, const Ids& declared) {
    if (!ref)
        return Unselected{};
    const NodeKey key{analysis, ref->value};
    if (declared.contains(ref->value))
        return DeclaredNode{key};
    return DanglingNode{key};
}
RawQuantity quantity(const project::Quantity& q) { return {q.text, q.unit}; }
Source source(const project::TransientSource& s) {
    Source out{s.mode, {}};
    out.points.reserve(s.points.size());
    for (const auto& p : s.points)
        out.points.push_back({quantity(p.time), p.value_text});
    return out;
}
struct Projection {
    std::vector<Node> nodes;
    NodeReference ground;
    std::vector<Component> components;
};
template <class Draft> Projection project_draft(const Draft& d, Analysis analysis) {
    // Finish the storage/identity scan before copying potentially large raw strings/points.
    const auto declared = check_bounds_and_ids(d);
    Projection out;
    out.nodes.reserve(d.nodes.size());
    for (const auto& n : d.nodes)
        out.nodes.push_back({{analysis, n.id.value}, n.name});
    out.ground = reference(d.ground, analysis, declared);
    out.components.reserve(d.components.size());
    for (const auto& c : d.components) {
        Component part{{analysis, c.id.value},
                       c.name,
                       c.kind,
                       reference(c.positive, analysis, declared),
                       reference(c.negative, analysis, declared),
                       quantity(c.value),
                       std::nullopt,
                       std::nullopt};
        if constexpr (std::is_same_v<Draft, project::AcDraft>)
            part.phase = quantity(c.phase);
        if constexpr (std::is_same_v<Draft, project::TransientDraft>)
            part.source = source(c.source);
        out.components.push_back(std::move(part));
    }
    return out;
}
} // namespace
SchematicGraph project_schematic(const project::DcDraft& d) {
    auto p = project_draft(d, Analysis::dc);
    return {Analysis::dc, std::move(p.nodes), std::move(p.ground), std::move(p.components)};
}
SchematicGraph project_schematic(const project::AcDraft& d) {
    auto p = project_draft(d, Analysis::ac);
    return {Analysis::ac, std::move(p.nodes), std::move(p.ground), std::move(p.components)};
}
SchematicGraph project_schematic(const project::TransientDraft& d) {
    auto p = project_draft(d, Analysis::transient);
    return {Analysis::transient, std::move(p.nodes), std::move(p.ground), std::move(p.components)};
}
} // namespace openece::schematic
