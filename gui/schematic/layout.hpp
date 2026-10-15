#pragma once
#include "model.hpp"
#include <cstddef>
#include <cstdint>

namespace openece::schematic {
// Integer diagram units, NOT screen pixels. Rendering supplies its own transform.
using Coordinate = std::int64_t;
struct Point {
    Coordinate x = 0, y = 0;
    bool operator==(const Point&) const = default;
};
struct Extent {
    Coordinate width = 0, height = 0;
    bool operator==(const Extent&) const = default;
};
struct Rectangle {
    Point origin;
    Extent size;
    bool operator==(const Rectangle&) const = default;
};
enum class TerminalSide { positive, negative };
struct TerminalKey {
    ComponentKey component;
    TerminalSide side;
    bool operator==(const TerminalKey&) const = default;
};
enum class LabelRole { node_name, component_name, component_value, component_detail };
using LabelOwner = std::variant<NodeKey, ComponentKey>;
struct LabelMeasurement {
    LabelOwner owner;
    LabelRole role;
    Extent extent;
    bool operator==(const LabelMeasurement&) const = default;
};
struct LayoutMetrics {
    Coordinate grid = 8, symbol_length = 80, symbol_width = 32, clearance = 24, ground_size = 24,
               marker_radius = 4;
    Extent node_label{96, 20}, component_label{160, 20};
    // Optional per-label extents supplied by the caller's text measurer. No font dependency.
    std::vector<LabelMeasurement> labels;
    bool operator==(const LayoutMetrics&) const = default;
};
namespace layout_limits {
inline constexpr std::size_t nodes = 32, components = 128, terminals = 256, segments = 1024,
                             junctions = 1024, crossings = 16384, labels = 416, primitives = 20000,
                             operations = 2000000, working_bytes = 32 * 1024 * 1024;
inline constexpr Coordinate coordinate = 1000000;
} // namespace layout_limits
// Callers may tighten these ceilings, not remove the hard bounds.
struct LayoutLimits {
    std::size_t nodes = layout_limits::nodes, components = layout_limits::components,
                terminals = layout_limits::terminals, segments = layout_limits::segments,
                junctions = layout_limits::junctions, crossings = layout_limits::crossings,
                labels = layout_limits::labels, primitives = layout_limits::primitives,
                operations = layout_limits::operations,
                working_bytes = layout_limits::working_bytes;
    Coordinate coordinate = layout_limits::coordinate;
};
enum class LayoutErrorCode { invalid_metrics, resource_limit, invalid_geometry, connectivity };
struct LayoutFailure {
    LayoutErrorCode code;
    std::string message;
    bool operator==(const LayoutFailure&) const = default;
};
enum class LayoutStrategy { structured, net_rails };
enum class SymbolKind { resistor, capacitor, inductor, voltage_source, current_source, generic };
// Direction from the positive terminal TO the negative terminal, including branch-current sense.
enum class Direction { right, down, left, up };
struct ComponentPlacement {
    Component source; // Frozen owned inspection data, including inactive/raw source information.
    SymbolKind symbol;
    Direction direction;
    Point center, positive, negative;
    Rectangle body;
    bool operator==(const ComponentPlacement&) const = default;
};
struct TerminalPlacement {
    TerminalKey key;
    NodeReference
        reference; // Declared => connected; Unselected => open; Dangling => missing-ID mark.
    Point anchor;
    bool operator==(const TerminalPlacement&) const = default;
};
struct NetPlacement {
    Node source;
    Point anchor;
    bool operator==(const NetPlacement&) const = default;
};
enum class WireRole { backbone, terminal, ground };
struct WirePath {
    NodeKey net;
    WireRole role;
    std::optional<TerminalKey> terminal;
    std::vector<Point> points; // Ordered, orthogonal, nonzero segments.
    bool operator==(const WirePath&) const = default;
};
struct Junction {
    NodeKey net;
    Point at;
    std::vector<std::size_t> wires; // Sorted layout-local wire indices, never electrical IDs.
    bool operator==(const Junction&) const = default;
};
struct Crossing {
    Point at;
    // DIFFERENT-net nonconnection. The future renderer marks second_wire crossing over first_wire.
    std::size_t first_wire, second_wire;
    bool operator==(const Crossing&) const = default;
};
struct GroundPlacement {
    NodeKey net;
    Point anchor;
    Rectangle body;
    bool operator==(const GroundPlacement&) const = default;
};
struct LabelPlacement {
    LabelOwner owner;
    LabelRole role;
    std::string text; // Exact original fields remain in source metadata; never elided here.
    Rectangle bounds;
    bool operator==(const LabelPlacement&) const = default;
};
struct LayoutDiagram {
    Analysis analysis = Analysis::dc;
    NodeReference ground_selection = Unselected{};
    LayoutStrategy strategy = LayoutStrategy::net_rails;
    Coordinate marker_radius = 0; // Frozen extent for terminal/net/junction/crossing marks.
    std::vector<NetPlacement> nets;
    std::vector<ComponentPlacement> components;
    std::vector<TerminalPlacement> terminals;
    std::vector<WirePath> wires;
    std::vector<Junction> junctions;
    std::vector<Crossing> crossings;
    std::optional<GroundPlacement> ground;
    std::vector<LabelPlacement> labels;
    Rectangle bounds;
    std::vector<std::string> diagnostics;
    std::size_t operations = 0, working_bytes_bound = 0;
    bool operator==(const LayoutDiagram&) const = default;
};
using LayoutResult = std::variant<LayoutDiagram, LayoutFailure>;
struct VerificationResult {
    std::optional<LayoutFailure> failure;
    std::size_t operations = 0;
    bool valid() const noexcept { return !failure; }
};
// Measure these exact strings when populating LayoutMetrics::labels. Original raw
// fields are retained separately; display labels append stable IDs for disambiguation.
std::string node_label_text(const Node&);
std::string component_label_text(const Component&, LabelRole);
// Independent of placement/recognition. Also useful to verify future renderer geometry changes.
VerificationResult verify_layout(const SchematicGraph&, const LayoutDiagram&, const LayoutMetrics&,
                                 const LayoutLimits& = {});
LayoutResult layout_schematic(const SchematicGraph&, const LayoutMetrics& = {},
                              const LayoutLimits& = {});
} // namespace openece::schematic
