#pragma once

#include <openece/digital/logic.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace openece::digital {
// Bound construction, evaluation and table generation independently of any GUI.
namespace limits {
inline constexpr std::size_t primary_inputs = 64;
inline constexpr std::size_t gates = 4096;
inline constexpr std::size_t primary_outputs = 256;
inline constexpr std::size_t connections = 16384; // Gate pins plus primary-output references.
inline constexpr std::size_t name_bytes = 128;
inline constexpr std::size_t truth_table_inputs = 10;
inline constexpr std::size_t truth_table_rows = std::size_t{1} << truth_table_inputs;
} // namespace limits

struct NodeId {
    std::uint32_t value;
    bool operator==(const NodeId&) const = default;
};
struct PrimaryInput {
    NodeId id;
    std::string name;
};
struct Gate {
    NodeId id;
    GateKind kind;
    std::vector<NodeId> inputs;
};
struct PrimaryOutput {
    std::string name;
    NodeId source;
};
// Editable owning draft. Names are exact byte strings, nonempty, at most name_bytes,
// and unique within their respective input/output list; no trimming/normalization.
struct CircuitDefinition {
    std::vector<PrimaryInput> inputs;
    std::vector<Gate> gates;
    std::vector<PrimaryOutput> outputs;
};
struct Evaluation {
    std::vector<LogicValue> gate_values; // Gate declaration order, not topological order.
    std::vector<LogicValue> outputs;     // Output declaration order.
};

class Circuit {
  public:
    // Owns a validated snapshot. Requires at least one input and output.
    // Invalid definitions/cycles: invalid_argument; resource limits: length_error.
    explicit Circuit(CircuitDefinition definition);
    const CircuitDefinition& definition() const& noexcept { return definition_; }
    const CircuitDefinition& definition() const&& = delete;
    // Assignment order is primary-input declaration order. No retained borrowed storage.
    Evaluation evaluate(std::span<const LogicValue> inputs) const;

  private:
    CircuitDefinition definition_;
    std::vector<std::vector<std::size_t>> sources_;
    std::vector<std::size_t> output_sources_;
    std::vector<std::size_t> order_;
};
} // namespace openece::digital
