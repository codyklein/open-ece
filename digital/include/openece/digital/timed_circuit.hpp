#pragma once
#include <openece/digital/circuit.hpp>

#include <compare>
#include <unordered_map>
#include <variant>

namespace openece::digital::timing {
// One tick is exactly one picosecond. Time and delay are distinct quantities.
struct Time {
    std::uint64_t ticks = 0;
    auto operator<=>(const Time&) const = default;
};
struct Delay {
    std::uint64_t ticks = 0;
};
enum class Edge { Rising, Falling };
struct DelayedGate {
    Gate gate;
    Delay propagation;
};
struct SrLatch {
    NodeId id;
    NodeId set;
    NodeId reset;
    Delay output_delay;
};
struct DLatch {
    NodeId id;
    NodeId data;
    NodeId enable;
    Delay output_delay;
};
struct DFlipFlop {
    NodeId id;
    NodeId data;
    NodeId clock;
    Edge edge;
    Delay clock_to_q;
};
using Element = std::variant<DelayedGate, SrLatch, DLatch, DFlipFlop>;
NodeId element_id(const Element& element);
Delay element_delay(const Element& element);
std::vector<NodeId> element_inputs(const Element& element);
bool is_storage(const Element& element) noexcept;

namespace limits {
inline constexpr std::size_t inputs = digital::limits::primary_inputs;
inline constexpr std::size_t elements = digital::limits::gates;
inline constexpr std::size_t outputs = digital::limits::primary_outputs;
inline constexpr std::size_t connections = digital::limits::connections;
inline constexpr std::uint64_t time_ticks = 1'000'000'000'000; // One second.
inline constexpr std::size_t stimuli = 100'000;
inline constexpr std::size_t queued_events = 100'000;
inline constexpr std::size_t processed_events = 1'000'000;
inline constexpr std::size_t pin_visits = 64'000'000;
inline constexpr std::size_t observed_nodes = 64;
inline constexpr std::size_t recorded_values = 1'000'000;
} // namespace limits

struct TimedCircuitDefinition {
    std::vector<PrimaryInput> inputs;
    std::vector<Element> elements;
    std::vector<PrimaryOutput> outputs;
};
class Simulation;
class TimedCircuit {
  public:
    explicit TimedCircuit(TimedCircuitDefinition definition);
    const TimedCircuitDefinition& definition() const& noexcept { return definition_; }
    const TimedCircuitDefinition& definition() const&& = delete;

  private:
    friend class Simulation;
    TimedCircuitDefinition definition_;
    std::vector<std::vector<std::size_t>> sources_;
    std::vector<std::vector<std::size_t>> dependents_;
    std::vector<std::size_t> gate_order_;
    std::vector<std::size_t> output_sources_;
    std::unordered_map<std::uint32_t, std::size_t> indices_;
    std::size_t resolve(NodeId id) const;
};
// Explicit one-way conversion, preserving IDs, names and declaration order.
TimedCircuitDefinition with_delay(const Circuit& circuit, Delay delay);
} // namespace openece::digital::timing
