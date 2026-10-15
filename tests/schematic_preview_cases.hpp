#pragma once
// Developer/test fixtures only; never linked into the application or installed.
#include "schematic_layout_cases.hpp"
#include <QString>
#include <functional>
namespace openece::schematic::qt {
struct PreviewCase {
    QString name;
    SchematicGraph graph;
};
inline std::vector<PreviewCase>
preview_cases(const std::function<openece::project::ProjectSnapshot(const std::string&)>& load =
                  schematic_cases::example) {
    namespace cases = schematic_cases;
    const auto divider = load("dc-divider").circuits.dc;
    auto disconnected = divider;
    disconnected.nodes.push_back({{30}, "Unused island — Ω"});
    auto invalid = divider;
    invalid.ground.reset();
    invalid.components[0].negative.reset();
    invalid.components[1].positive = openece::project::Id{999};
    invalid.components[1].value.text = " 1e- ";
    invalid.components[1].kind = "unknown_draft_token";
    invalid.components[1].name = "<b>literal markup</b> αβ Ω\n" + std::string(300, 'X');
    return {
        {"DC voltage divider", project_schematic(divider)},
        {"Transient RC charging", project_schematic(load("transient-rc-step").circuits.transient)},
        {"Transient RL response",
         project_schematic(load("transient-rl-response").circuits.transient)},
        {"AC series RLC", project_schematic(load("series-rlc").circuits.ac)},
        {"Wheatstone bridge — connectivity fallback", project_schematic(cases::wheatstone())},
        {"Disconnected / unused node", project_schematic(disconnected)},
        {"Incomplete / long Unicode labels / unknown symbol", project_schematic(invalid)},
        {"Maximum density — 7,936 nonconnecting crossings",
         project_schematic(cases::maximum(true))}};
}
} // namespace openece::schematic::qt
