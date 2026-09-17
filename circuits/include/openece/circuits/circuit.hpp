#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
namespace openece::circuits {
namespace limits {
inline constexpr std::size_t nodes = 128, components = 512, voltage_sources = 64;
inline constexpr std::size_t unknowns = nodes - 1 + voltage_sources, name_bytes = 128;
inline constexpr double resistance_min = 1e-9, resistance_max = 1e12;
inline constexpr double source_nonzero_min = 1e-12, source_max = 1e9;
} // namespace limits
struct NodeId {
    std::uint32_t value;
    bool operator==(const NodeId&) const = default;
};
struct ComponentId {
    std::uint32_t value;
    bool operator==(const ComponentId&) const = default;
};
struct Node {
    NodeId id;
    std::string name;
};
struct Resistor {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    double resistance_ohms;
};
struct CurrentSource {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    double current_amperes;
};
struct VoltageSource {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    double voltage_volts;
};
using Component = std::variant<Resistor, CurrentSource, VoltageSource>;
// Exact nonempty names, unique within nodes/components; no normalization.
// No reserved node ID. Every terminal/current orientation is positive -> negative.
struct CircuitDefinition {
    std::vector<Node> nodes;
    std::optional<NodeId> ground;
    std::vector<Component> components;
};
enum class ErrorCode {
    resource_limit,
    invalid_name,
    duplicate_id,
    missing_ground,
    invalid_terminal,
    invalid_value,
    floating_reference,
    redundant_voltage_constraint,
    contradictory_voltage_constraint,
    rank_deficient,
    ill_conditioned,
    numerical_failure
};
class CircuitError : public std::runtime_error {
  public:
    CircuitError(ErrorCode code, std::string message, std::vector<NodeId> nodes = {},
                 std::vector<ComponentId> components = {});
    ErrorCode code() const noexcept { return code_; }
    const std::vector<NodeId>& nodes() const noexcept { return nodes_; }
    const std::vector<ComponentId>& components() const noexcept { return components_; }

  private:
    ErrorCode code_;
    std::vector<NodeId> nodes_;
    std::vector<ComponentId> components_;
};
class Circuit {
  public:
    // Owns a structurally validated snapshot; solve_dc checks electrical/numerical solvability.
    explicit Circuit(CircuitDefinition definition);
    const CircuitDefinition& definition() const& noexcept { return definition_; }
    const CircuitDefinition& definition() const&& = delete;

  private:
    CircuitDefinition definition_;
};
} // namespace openece::circuits
