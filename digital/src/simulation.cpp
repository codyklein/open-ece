#include <algorithm>
#include <limits>
#include <openece/digital/simulation.hpp>
#include <utility>

namespace openece::digital::timing {
namespace {
bool high(LogicValue value) { return value == LogicValue::one; }
void valid_value(LogicValue value) {
    if (value != LogicValue::zero && value != LogicValue::one)
        throw std::invalid_argument("Logic value must be zero or one");
}
void budget(std::size_t requested, std::size_t maximum) {
    if (requested == 0 || requested > maximum)
        throw std::invalid_argument("Work limits must be positive and no greater than core limits");
}
} // namespace
SimulationError::SimulationError(Time at, std::optional<NodeId> node, const std::string& reason)
    : std::runtime_error("At " + std::to_string(at.ticks) + " ps" +
                         (node ? ", node " + std::to_string(node->value) : "") + ": " + reason),
      at_(at), node_(node) {}
Simulation::Simulation(TimedCircuit circuit, SimulationRequest request)
    : circuit_(std::move(circuit)), request_(std::move(request)) {
    const auto& d = circuit_.definition_;
    if (request_.horizon.ticks > limits::time_ticks)
        throw std::length_error("Simulation horizon exceeds one second");
    if (request_.changes.size() > limits::stimuli ||
        request_.observed.size() > limits::observed_nodes ||
        request_.clocks.size() > d.inputs.size() ||
        request_.initial_storage.size() > d.elements.size())
        throw std::length_error("Simulation request count limit exceeded");
    budget(request_.work.queued_events, limits::queued_events);
    budget(request_.work.processed_events, limits::processed_events);
    budget(request_.work.pin_visits, limits::pin_visits);
    budget(request_.work.recorded_values, limits::recorded_values);
    if (request_.initial_inputs.size() != d.inputs.size())
        throw std::invalid_argument("Initial input count must match primary inputs");
    for (auto value : request_.initial_inputs)
        valid_value(value);
    visible_ = request_.initial_inputs;
    visible_.resize(d.inputs.size() + d.elements.size(), LogicValue::zero);
    stored_.resize(d.elements.size(), LogicValue::zero);
    pending_.resize(d.elements.size());
    generations_.resize(d.elements.size(), 0);
    std::vector<bool> initialized(d.elements.size(), false);
    for (const auto& initial : request_.initial_storage) {
        valid_value(initial.q);
        const auto node = circuit_.resolve(initial.element);
        if (node < d.inputs.size() || !is_storage(d.elements[node - d.inputs.size()]))
            throw std::invalid_argument("InitialState must reference a storage element");
        const auto i = node - d.inputs.size();
        if (initialized[i])
            throw std::invalid_argument("Duplicate storage initial state");
        initialized[i] = true;
        stored_[i] = initial.q;
        visible_[node] = initial.q;
    }
    for (std::size_t i = 0; i < d.elements.size(); ++i)
        if (is_storage(d.elements[i]) && !initialized[i])
            throw std::invalid_argument("Missing storage initial state");
    std::vector<bool> observed(visible_.size(), false), clocked(d.inputs.size(), false);
    for (auto id : request_.observed) {
        const auto index = circuit_.resolve(id);
        if (observed[index])
            throw std::invalid_argument("Duplicate observed node");
        observed[index] = true;
        observed_.push_back(index);
        traces_.push_back({id, {}});
    }
    for (const auto& clock : request_.clocks) {
        const auto node = circuit_.resolve(clock.input);
        if (node >= d.inputs.size())
            throw std::invalid_argument("Clock must drive a primary input");
        if (clocked[node])
            throw std::invalid_argument("Multiple clocks on the same input");
        clocked[node] = true;
        if (clock.first_edge.ticks == 0 || clock.first_edge > request_.horizon ||
            clock.high.ticks == 0 || clock.low.ticks == 0 ||
            clock.high.ticks > limits::time_ticks || clock.low.ticks > limits::time_ticks)
            throw std::invalid_argument("Clock needs a positive first edge within the horizon and "
                                        "positive bounded high/low durations");
    }
    std::sort(request_.changes.begin(), request_.changes.end(), [](const auto& a, const auto& b) {
        return a.at != b.at ? a.at < b.at : a.input.value < b.input.value;
    });
    for (std::size_t i = 0; i < request_.changes.size(); ++i) {
        const auto& change = request_.changes[i];
        valid_value(change.value);
        const auto node = circuit_.resolve(change.input);
        if (node >= d.inputs.size())
            throw std::invalid_argument("Manual stimuli must drive primary inputs");
        if (clocked[node])
            throw std::invalid_argument("Input cannot have both clock and manual stimuli");
        if (change.at.ticks == 0 || change.at > request_.horizon)
            throw std::invalid_argument(
                "Input-change time must be positive and within the horizon");
        if (i && change.at == request_.changes[i - 1].at &&
            change.input == request_.changes[i - 1].input)
            throw std::invalid_argument("Duplicate input assignment at one timestamp");
    }
    // Initial combinational values are settled, using explicit storage Q as boundaries.
    for (auto i : circuit_.gate_order_)
        visible_[d.inputs.size() + i] = gate_value(i);
    record();
    for (const auto& change : request_.changes)
        enqueue(change.at, circuit_.resolve(change.input), change.value);
    for (std::size_t i = 0; i < request_.clocks.size(); ++i) {
        const auto& clock = request_.clocks[i];
        const auto node = circuit_.resolve(clock.input);
        enqueue(clock.first_edge, node, high(visible_[node]) ? LogicValue::zero : LogicValue::one,
                false, 0, i);
    }
    const auto before = visible_;
    for (std::size_t i = 0; i < d.elements.size(); ++i)
        if (is_storage(d.elements[i]))
            react(i, before, true);
}
std::optional<Time> Simulation::delivery_time(Delay delay) const {
    // Subtract before addition: no wraparound and no beyond-horizon queue entries.
    if (delay.ticks > request_.horizon.ticks - now_.ticks)
        return {};
    return Time{now_.ticks + delay.ticks};
}
void Simulation::enqueue(Time at, std::size_t node, LogicValue value, bool inertial,
                         std::uint64_t generation, std::optional<std::size_t> clock) {
    if (at <= now_ || at > request_.horizon)
        throw std::logic_error("Delivery must be strictly future and within horizon");
    if (queue_.size() >= request_.work.queued_events ||
        sequence_ == std::numeric_limits<std::uint64_t>::max())
        throw SimulationError(now_, {}, "Queued-event limit exceeded");
    queue_.push({at, sequence_++, node, value, generation, clock, inertial});
}
void Simulation::charge_pins(std::size_t count) {
    if (count > request_.work.pin_visits - pins_)
        throw SimulationError(now_, {}, "Pin-visit work limit exceeded");
    pins_ += count;
}
LogicValue Simulation::gate_value(std::size_t element) {
    charge_pins(circuit_.sources_[element].size());
    std::vector<LogicValue> pins;
    pins.reserve(circuit_.sources_[element].size());
    for (auto node : circuit_.sources_[element])
        pins.push_back(visible_[node]);
    return evaluate_gate(std::get<DelayedGate>(circuit_.definition_.elements[element]).gate.kind,
                         pins);
}
void Simulation::react(std::size_t i, const std::vector<LogicValue>& before, bool initial) {
    const auto& element = circuit_.definition_.elements[i];
    const auto node = circuit_.definition_.inputs.size() + i;
    if (!is_storage(element)) {
        const auto target = gate_value(i);
        if (target == visible_[node]) {
            pending_[i].reset();
            return;
        }
        if (pending_[i] && pending_[i]->target == target)
            return; // Preserve deadline.
        pending_[i].reset();
        if (const auto due = delivery_time(element_delay(element))) {
            if (generations_[i] == std::numeric_limits<std::uint64_t>::max())
                throw SimulationError(now_, element_id(element), "Generation limit exceeded");
            pending_[i] = Pending{target, ++generations_[i]};
            enqueue(*due, node, target, true, generations_[i]);
        }
        return;
    }
    // Implemented in the storage checkpoint; never substitute an invented value.
    (void)before;
    (void)initial;
    throw std::invalid_argument("Storage simulation is not yet enabled");
}
void Simulation::record() {
    for (std::size_t i = 0; i < traces_.size(); ++i) {
        auto& changes = traces_[i].transitions;
        const auto value = visible_[observed_[i]];
        if (changes.empty() || changes.back().value != value) {
            if (recorded_ >= request_.work.recorded_values)
                throw SimulationError(now_, traces_[i].source, "Recorded-value limit exceeded");
            changes.push_back({now_, value});
            ++recorded_;
        }
    }
}
StepStatus Simulation::step() {
    if (status_ == StepStatus::Failed)
        throw std::logic_error("Failed simulation requires a new session");
    if (status_ == StepStatus::Complete)
        return status_;
    try {
        if (queue_.empty()) {
            now_ = request_.horizon;
            return status_ = StepStatus::Complete;
        }
        now_ = queue_.top().at;
        const auto before = visible_;
        // Deliver all preexisting due events before reacting to any changed input.
        while (!queue_.empty() && queue_.top().at == now_) {
            if (processed_ >= request_.work.processed_events)
                throw SimulationError(now_, {}, "Processed-event limit exceeded");
            ++processed_;
            const auto event = queue_.top();
            queue_.pop();
            if (event.inertial) {
                const auto i = event.node - circuit_.definition_.inputs.size();
                if (!pending_[i] || pending_[i]->generation != event.generation)
                    continue;
                pending_[i].reset();
            }
            visible_[event.node] = event.value;
            if (event.clock) {
                const auto& clock = request_.clocks[*event.clock];
                if (const auto next = delivery_time(high(event.value) ? clock.high : clock.low))
                    enqueue(*next, event.node,
                            high(event.value) ? LogicValue::zero : LogicValue::one, false, 0,
                            event.clock);
            }
        }
        std::vector<bool> affected(circuit_.definition_.elements.size(), false);
        for (std::size_t node = 0; node < visible_.size(); ++node)
            if (visible_[node] != before[node])
                for (auto i : circuit_.dependents_[node])
                    affected[i] = true;
        for (std::size_t i = 0; i < affected.size(); ++i)
            if (affected[i])
                react(i, before, false);
        record();
        if (queue_.empty() && now_ == request_.horizon)
            status_ = StepStatus::Complete;
        return status_;
    } catch (...) {
        status_ = StepStatus::Failed;
        throw;
    }
}
void Simulation::run() {
    while (step() != StepStatus::Complete) {
    }
}
SimulationSnapshot Simulation::snapshot() const {
    if (status_ == StepStatus::Failed)
        throw std::logic_error("Failed simulation has no valid snapshot");
    const auto n = circuit_.definition_.inputs.size();
    SimulationSnapshot result{now_,
                              status_,
                              {visible_.begin(), visible_.begin() + static_cast<std::ptrdiff_t>(n)},
                              {visible_.begin() + static_cast<std::ptrdiff_t>(n), visible_.end()},
                              {},
                              traces_};
    for (auto node : circuit_.output_sources_)
        result.outputs.push_back(visible_[node]);
    return result;
}
} // namespace openece::digital::timing
