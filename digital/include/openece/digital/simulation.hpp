#pragma once
#include <openece/digital/timing_trace.hpp>

#include <optional>
#include <queue>
#include <stdexcept>

namespace openece::digital::timing {
struct InputChange {
    Time at;
    NodeId input;
    LogicValue value;
};
struct InitialState {
    NodeId element;
    LogicValue q;
};
// Initial input level determines the first transition. Durations describe the
// resulting level; both are positive. One clock per input, no manual co-driver.
struct Clock {
    NodeId input;
    Time first_edge;
    Delay high;
    Delay low;
};
struct WorkLimits {
    std::size_t queued_events = limits::queued_events;
    std::size_t processed_events = limits::processed_events;
    std::size_t pin_visits = limits::pin_visits;
    std::size_t recorded_values = limits::recorded_values;
};
struct SimulationRequest {
    Time horizon;
    std::vector<LogicValue> initial_inputs;
    std::vector<InitialState> initial_storage;
    std::vector<InputChange> changes;
    std::vector<Clock> clocks;
    std::vector<NodeId> observed;
    WorkLimits work; // May tighten, never exceed, central core limits.
};
class SimulationError : public std::runtime_error {
  public:
    SimulationError(Time at, std::optional<NodeId> node, const std::string& reason);
    Time at() const noexcept { return at_; }
    std::optional<NodeId> node() const noexcept { return node_; }

  private:
    Time at_;
    std::optional<NodeId> node_;
};
class Simulation {
  public:
    Simulation(TimedCircuit circuit, SimulationRequest request);
    StepStatus step(); // All deliveries at the next timestamp, or advance to horizon.
    void run();
    Time current_time() const noexcept { return now_; }
    StepStatus status() const noexcept { return status_; }
    SimulationSnapshot snapshot() const; // Failed sessions cannot yield normal results.
  private:
    struct Event {
        Time at;
        std::uint64_t sequence;
        std::size_t node;
        LogicValue value;
        std::uint64_t generation;
        std::optional<std::size_t> clock;
        bool inertial;
    };
    struct Later {
        bool operator()(const Event& a, const Event& b) const noexcept {
            return a.at != b.at ? a.at > b.at : a.sequence > b.sequence;
        }
    };
    struct Pending {
        LogicValue target;
        std::uint64_t generation;
    };
    TimedCircuit circuit_;
    SimulationRequest request_;
    Time now_;
    StepStatus status_ = StepStatus::Ready;
    std::vector<LogicValue> visible_;
    std::vector<LogicValue> stored_; // Never used as a substitute for visible delayed Q.
    std::vector<std::optional<Pending>> pending_;
    std::vector<std::uint64_t> generations_;
    std::priority_queue<Event, std::vector<Event>, Later> queue_;
    std::vector<std::size_t> observed_;
    std::vector<SignalTrace> traces_;
    std::uint64_t sequence_ = 0;
    std::size_t processed_ = 0, pins_ = 0, recorded_ = 0;
    void enqueue(Time at, std::size_t node, LogicValue value, bool inertial = false,
                 std::uint64_t generation = 0, std::optional<std::size_t> clock = {});
    std::optional<Time> delivery_time(Delay delay) const;
    void react(std::size_t element, const std::vector<LogicValue>& before, bool initial);
    LogicValue gate_value(std::size_t element);
    void charge_pins(std::size_t count);
    void record();
};
} // namespace openece::digital::timing
