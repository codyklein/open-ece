#pragma once
#include <fstream>
#include <iterator>
#include <limits>
#include <openece/project/model.hpp>
#include <schematic/layout.hpp>

namespace schematic_cases {
namespace p = openece::project;
namespace s = openece::schematic;
inline p::ProjectSnapshot example(const std::string& name) {
    std::ifstream file(std::string(OPENECE_SOURCE_DIR) + "/examples/" + name + ".openece",
                       std::ios::binary);
    if (!file)
        throw std::runtime_error("Missing shipped schematic example: " + name);
    return p::decode_project(std::string(std::istreambuf_iterator<char>(file), {})).snapshot;
}
inline p::DcDraft divider() { return example("dc-divider").circuits.dc; }
inline p::DcDraft wheatstone() {
    p::DcDraft d;
    d.nodes = {{{0}, "Ground"}, {{1}, "Supply"}, {{2}, "Left"}, {{3}, "Right"}};
    d.ground = p::Id{0};
    d.next_node = {4};
    d.next_component = {16};
    d.components = {{{10}, "V", "voltage_source", p::Id{1}, p::Id{0}, {"10", "V"}},
                    {{11}, "R1", "resistor", p::Id{1}, p::Id{2}, {"1", "kohm"}},
                    {{12}, "R2", "resistor", p::Id{2}, p::Id{0}, {"1", "kohm"}},
                    {{13}, "R3", "resistor", p::Id{1}, p::Id{3}, {"1", "kohm"}},
                    {{14}, "R4", "resistor", p::Id{3}, p::Id{0}, {"1", "kohm"}},
                    {{15}, "Bridge", "resistor", p::Id{2}, p::Id{3}, {"1", "kohm"}}};
    return d;
}
inline p::DcDraft nonplanar() {
    p::DcDraft d;
    for (std::uint32_t i = 0; i < 6; ++i)
        d.nodes.push_back({{i * 1000}, "N" + std::to_string(i)});
    d.ground = d.nodes[5].id;
    for (std::uint32_t a = 0; a < 3; ++a)
        for (std::uint32_t z = 3; z < 6; ++z) {
            const auto id = static_cast<std::uint32_t>(d.components.size() + 10);
            d.components.push_back(
                {{id}, "K3,3 branch", "resistor", d.nodes[a].id, d.nodes[z].id, {"1", "kohm"}});
        }
    d.next_node = {5001};
    d.next_component = {19};
    return d;
}
inline p::DcDraft maximum(bool dense = false) {
    p::DcDraft d;
    for (std::uint32_t i = 0; i < p::limits::circuit_nodes; ++i)
        d.nodes.push_back({{dense ? i : std::numeric_limits<std::uint32_t>::max() - i * 1000},
                           "Node " + std::to_string(i)});
    d.ground = d.nodes[17].id;
    for (std::uint32_t i = 0; i < p::limits::circuit_components; ++i)
        d.components.push_back({{std::numeric_limits<std::uint32_t>::max() - i},
                                "Part " + std::to_string(i),
                                i % 5 == 0 ? "current_source" : "resistor",
                                d.nodes[dense ? 0 : i % 32].id,
                                d.nodes[dense ? 0 : (i * 7 + 1) % 32].id,
                                {"1e-", "ohm"}});
    d.next_node = d.next_component = {p::limits::exhausted_id};
    return d;
}
inline p::TransientDraft large_sources() {
    p::TransientDraft d;
    d.nodes = {{{0}, "Ground"}, {{1}, "Supply"}};
    d.ground = p::Id{0};
    d.next_node = {2};
    d.next_component = {4};
    for (std::uint32_t i = 0; i < 4; ++i) {
        p::TransientComponent c;
        c.id = {i};
        c.kind = "current_source";
        c.positive = p::Id{1};
        c.negative = p::Id{0};
        c.source.mode = "constant";
        c.source.points.resize(p::limits::transient_source_points);
        for (auto& point : c.source.points) {
            point.time.text.assign(500, 't');
            point.value_text.assign(500, 'v');
        }
        d.components.push_back(std::move(c));
    }
    return d;
}
inline s::LayoutMetrics measured(const s::SchematicGraph& g) {
    s::LayoutMetrics m;
    for (const auto& n : g.nodes())
        m.labels.push_back({n.key, s::LabelRole::node_name, {120, 32}});
    for (const auto& c : g.components())
        for (auto role : {s::LabelRole::component_name, s::LabelRole::component_value,
                          s::LabelRole::component_detail})
            m.labels.push_back({c.key, role, {192, 32}});
    return m;
}
} // namespace schematic_cases
