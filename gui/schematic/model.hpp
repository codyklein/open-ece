#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace openece::project {
struct DcDraft;
struct AcDraft;
struct TransientDraft;
} // namespace openece::project

namespace openece::schematic {
enum class Analysis { dc, ac, transient };
// Keys are scoped by analysis AND by entity type. Numeric IDs never index storage.
struct NodeKey {
    Analysis analysis;
    std::uint32_t value;
    bool operator==(const NodeKey&) const = default;
};
struct ComponentKey {
    Analysis analysis;
    std::uint32_t value;
    bool operator==(const ComponentKey&) const = default;
};
struct Unselected {
    bool operator==(const Unselected&) const = default;
};
struct DeclaredNode {
    NodeKey key;
    bool operator==(const DeclaredNode&) const = default;
};
struct DanglingNode {
    NodeKey key;
    bool operator==(const DanglingNode&) const = default;
};
// Used for terminals and ground alike. ID 0 is an ordinary, selectable node ID.
using NodeReference = std::variant<Unselected, DeclaredNode, DanglingNode>;
struct RawQuantity {
    std::string text, unit;
    bool operator==(const RawQuantity&) const = default;
};
struct SourcePoint {
    RawQuantity time;
    // Amplitude uses the parent component value's unit, exactly as in the draft.
    std::string value_text;
    bool operator==(const SourcePoint&) const = default;
};
struct Source {
    std::string mode;
    std::vector<SourcePoint> points;
    bool operator==(const Source&) const = default;
};
struct Node {
    NodeKey key;
    std::string name;
    bool operator==(const Node&) const = default;
};
struct Component {
    ComponentKey key;
    std::string name, kind; // Exact draft token; no display-name or physics conversion.
    NodeReference positive, negative;
    RawQuantity value;
    // AC retains phase even on passive components; Transient retains inactive points.
    std::optional<RawQuantity> phase;
    std::optional<Source> source;
    bool operator==(const Component&) const = default;
};
enum class ProjectionErrorCode { resource_limit, duplicate_node_id, duplicate_component_id };
class ProjectionError final : public std::runtime_error {
  public:
    ProjectionError(ProjectionErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}
    ProjectionErrorCode code() const noexcept { return code_; }

  private:
    ProjectionErrorCode code_;
};
// Owned, read-only projection, NOT a validated electrical circuit or a persistence model.
// Vectors retain declaration order. Only membership by ID resolves node references.
class SchematicGraph {
  public:
    Analysis analysis() const noexcept { return analysis_; }
    const std::vector<Node>& nodes() const& noexcept { return nodes_; }
    const std::vector<Node>& nodes() const&& = delete;
    const NodeReference& ground() const& noexcept { return ground_; }
    const NodeReference& ground() const&& = delete;
    const std::vector<Component>& components() const& noexcept { return components_; }
    const std::vector<Component>& components() const&& = delete;
    bool operator==(const SchematicGraph&) const = default;

  private:
    friend SchematicGraph project_schematic(const project::DcDraft&);
    friend SchematicGraph project_schematic(const project::AcDraft&);
    friend SchematicGraph project_schematic(const project::TransientDraft&);
    SchematicGraph(Analysis analysis, std::vector<Node> nodes, NodeReference ground,
                   std::vector<Component> components)
        : analysis_(analysis), nodes_(std::move(nodes)), ground_(std::move(ground)),
          components_(std::move(components)) {}
    Analysis analysis_;
    std::vector<Node> nodes_;
    NodeReference ground_;
    std::vector<Component> components_;
};
// No parsing, unit conversion, solver construction, ID allocation, or input mutation.
// Bounded storage and unique declared IDs are checked; engineering invalidity is retained.
SchematicGraph project_schematic(const project::DcDraft&);
SchematicGraph project_schematic(const project::AcDraft&);
SchematicGraph project_schematic(const project::TransientDraft&);
} // namespace openece::schematic
