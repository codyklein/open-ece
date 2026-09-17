#pragma once
#include <openece/digital/timed_circuit.hpp>

namespace openece::digital::timing {
struct Transition {
    Time at;
    LogicValue value;
    bool operator==(const Transition&) const = default;
};
struct SignalTrace {
    NodeId source;
    // Initial value at zero, then changes only; strictly increasing timestamps.
    std::vector<Transition> transitions;
    bool operator==(const SignalTrace&) const = default;
};
enum class StepStatus { Ready, Complete, Failed };
struct SimulationSnapshot {
    Time reached;
    StepStatus status;
    std::vector<LogicValue> inputs;   // Declaration order.
    std::vector<LogicValue> elements; // Externally visible outputs, declaration order.
    std::vector<LogicValue> outputs;
    std::vector<SignalTrace> traces; // Request observation order.
};
} // namespace openece::digital::timing
