#include "schematic_layout_cases.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string_view>
#ifdef _WIN32
#include <windows.h>

#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace s = openece::schematic;
namespace cases = schematic_cases;
namespace {
const char* strategy(s::LayoutStrategy v) {
    return v == s::LayoutStrategy::structured ? "structured" : "net_rails";
}
const char* direction(s::Direction v) {
    switch (v) {
    case s::Direction::down:
        return "down";
    case s::Direction::up:
        return "up";
    case s::Direction::left:
        return "left";
    default:
        return "right";
    }
}
std::string point(s::Point p) {
    return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")";
}
std::string reference(const s::NodeReference& ref) {
    if (const auto* n = std::get_if<s::DeclaredNode>(&ref))
        return std::to_string(n->key.value);
    if (const auto* n = std::get_if<s::DanglingNode>(&ref))
        return "missing " + std::to_string(n->key.value);
    return "unselected";
}
struct Case {
    const char* name;
    s::SchematicGraph graph;
    s::LayoutMetrics metrics;
};
std::vector<Case> examples() {
    return {{"DC voltage divider", s::project_schematic(cases::divider()), {}},
            {"RC step",
             s::project_schematic(cases::example("transient-rc-step").circuits.transient),
             {}},
            {"RL response",
             s::project_schematic(cases::example("transient-rl-response").circuits.transient),
             {}},
            {"Series RLC", s::project_schematic(cases::example("series-rlc").circuits.ac), {}},
            {"Wheatstone bridge", s::project_schematic(cases::wheatstone()), {}}};
}
bool dump(const Case& c) {
    const auto result = s::layout_schematic(c.graph, c.metrics);
    if (const auto* error = std::get_if<s::LayoutFailure>(&result)) {
        std::cerr << error->message << '\n';
        return false;
    }
    const auto& d = std::get<s::LayoutDiagram>(result);
    std::cout << "\n## " << c.name << "\n\nStrategy: `" << strategy(d.strategy) << "`; analysis: `"
              << (d.analysis == s::Analysis::dc   ? "DC"
                  : d.analysis == s::Analysis::ac ? "AC"
                                                  : "Transient")
              << "`; bounds: origin " << point(d.bounds.origin) << ", size (" << d.bounds.size.width
              << ',' << d.bounds.size.height << "). Independent verification: **"
              << (s::verify_layout(c.graph, d, c.metrics).valid() ? "PASS" : "FAIL") << "**.\n\n"
              << "| Component ID | Kind | Center | Positive → negative direction | + net / anchor "
                 "| − net / anchor |\n|---|---|---|---|---|---|\n";
    for (const auto& p : d.components)
        std::cout << "| " << p.source.key.value << " | " << p.source.kind << " | "
                  << point(p.center) << " | " << direction(p.direction) << " | "
                  << reference(p.source.positive) << " / " << point(p.positive) << " | "
                  << reference(p.source.negative) << " / " << point(p.negative) << " |\n";
    std::cout << "\n| Net ID | Original name | Inspection anchor |\n|---|---|---|\n";
    for (const auto& n : d.nets)
        std::cout << "| " << n.source.key.value << " | " << n.source.name << " | "
                  << point(n.anchor) << " |\n";
    std::cout << "\n| Wire index | Net | Role / terminal | Ordered route |\n|---|---|---|---|\n";
    for (std::size_t i = 0; i < d.wires.size(); ++i) {
        const auto& w = d.wires[i];
        std::cout << "| " << i << " | " << w.net.value << " | "
                  << (w.role == s::WireRole::terminal ? "terminal"
                      : w.role == s::WireRole::ground ? "ground"
                                                      : "backbone");
        if (w.terminal)
            std::cout << ' ' << w.terminal->component.value
                      << (w.terminal->side == s::TerminalSide::positive ? '+' : '-');
        std::cout << " | ";
        for (std::size_t j = 0; j < w.points.size(); ++j)
            std::cout << (j ? " → " : "") << point(w.points[j]);
        std::cout << " |\n";
    }
    std::cout << "\nElectrical junctions (including degree-two wire joins):\n\n";
    if (d.junctions.empty())
        std::cout << "None.\n";
    for (const auto& j : d.junctions) {
        std::cout << "- Net " << j.net.value << " at " << point(j.at) << ": wires";
        for (auto wire : j.wires)
            std::cout << ' ' << wire;
        std::cout << ".\n";
    }
    std::cout << "\nNonconnecting crossings: " << d.crossings.size() << ".\n\n";
    for (const auto& x : d.crossings)
        std::cout << "- " << point(x.at) << ": wire " << x.first_wire << " (net "
                  << d.wires[x.first_wire].net.value << ") crosses wire " << x.second_wire
                  << " (net " << d.wires[x.second_wire].net.value << "); **no connection**.\n";
    if (d.ground)
        std::cout << "\nExplicit ground: net " << d.ground->net.value << ", symbol anchor "
                  << point(d.ground->anchor) << ".\n";
    std::cout << "\nLabel rectangles (origin; width × height):\n\n";
    for (const auto& label : d.labels)
        std::cout << "- `" << label.text << "`: " << point(label.bounds.origin) << "; "
                  << label.bounds.size.width << " × " << label.bounds.size.height << ".\n";
    return true;
}
std::uint64_t process_peak_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, counters.cb))
        return 0;
    return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0)
        return 0;
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--dump") {
        std::cout << "# Schematic layout geometry review — checkpoint 2\n\n"
                     "Generated by the non-shipping `openece_schematic_benchmark --dump` developer "
                     "tool.\n"
                     "These are integer diagram units using default LayoutMetrics, not pixels or a "
                     "product export format.\n"
                     "Each row preserves the original electrical IDs and positive-to-negative "
                     "convention.\n"
                     "Label extents here are nominal; the future renderer must measure the exact "
                     "label strings.\n"
                     "No GUI or physical-rendering acceptance is claimed.\n";
        for (const auto& c : examples())
            if (!dump(c))
                return 1;
        return 0;
    }
    auto runs = examples();
    runs.push_back({"maximum sparse", s::project_schematic(cases::maximum()), {}});
    runs.push_back({"maximum dense", s::project_schematic(cases::maximum(true)), {}});
    const auto measured_graph = s::project_schematic(cases::maximum(true));
    runs.push_back({"maximum dense measured", measured_graph, cases::measured(measured_graph)});
    runs.push_back(
        {"large retained source metadata", s::project_schematic(cases::large_sources()), {}});
    for (const auto& c : runs) {
        std::vector<double> elapsed;
        for (int i = 0; i < 8; ++i) {
            const auto start = std::chrono::steady_clock::now();
            const auto result = s::layout_schematic(c.graph, c.metrics);
            const auto ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                    .count();
            if (const auto* failure = std::get_if<s::LayoutFailure>(&result)) {
                std::cerr << c.name << ": " << failure->message << '\n';
                return 1;
            }
            if (i)
                elapsed.push_back(
                    ms); // One warm-up, seven measured complete layouts + verification.
        }
        const auto d = std::get<s::LayoutDiagram>(s::layout_schematic(c.graph, c.metrics));
        std::sort(elapsed.begin(), elapsed.end());
        std::size_t segment_count = 0;
        for (const auto& w : d.wires)
            segment_count += w.points.size() - 1;
        std::cout << std::fixed << std::setprecision(3) << c.name
                  << " strategy=" << strategy(d.strategy) << " median_ms=" << elapsed[3]
                  << " max_ms=" << elapsed.back() << " nodes=" << d.nets.size()
                  << " components=" << d.components.size() << " segments=" << segment_count
                  << " junctions=" << d.junctions.size() << " crossings=" << d.crossings.size()
                  << " operations=" << d.operations
                  << " working_bound_bytes=" << d.working_bytes_bound << '\n';
    }
    std::cout
        << "whole_process_peak_bytes=" << process_peak_bytes()
        << " (includes loader, fixtures, runtime and retained test cases; not per-layout memory)\n";
}
