#pragma once
#include <memory>
#include <openece/circuits/dc.hpp>
namespace openece::circuits::transient {
namespace limits {
inline constexpr std::size_t inductors = 64, unknowns = 255, source_points = 4096,
                             total_source_points = 16384, probes = 64, intervals = 100000,
                             time_points = 200001, trace_values = 2000000;
inline constexpr std::uint64_t dense_work = 1000000000;
inline constexpr double time_max = 1e9, state_max = 1e9;
} // namespace limits
enum class Interpolation { hold, linear };
struct SourcePoint {
    double time_seconds, value;
};
struct Source {
    double initial = 0;
    std::vector<SourcePoint> points;
    Interpolation interpolation = Interpolation::hold;
};
// Instantaneous SI values, never RMS phasors. Positive terminal/current: positive -> negative.
struct Capacitor {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    double capacitance_farads;
};
struct Inductor {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    double inductance_henries;
};
struct VoltageSource {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    Source voltage_volts;
};
struct CurrentSource {
    ComponentId id;
    std::string name;
    NodeId positive, negative;
    Source current_amperes;
};
using Component = std::variant<Resistor, Capacitor, Inductor, VoltageSource, CurrentSource>;
struct CircuitDefinition {
    std::vector<Node> nodes;
    std::optional<NodeId> ground;
    std::vector<Component> components;
};
enum class ErrorCode {
    invalid_definition,
    invalid_request,
    resource_limit,
    floating_reference,
    contradictory_constraint,
    nonunique_source_current,
    unsupported_initialization,
    rank_deficient,
    ill_conditioned,
    numerical_failure
};
class Error : public std::runtime_error {
  public:
    Error(ErrorCode code, std::string message, std::vector<NodeId> nodes = {},
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
    explicit Circuit(CircuitDefinition definition);
    const CircuitDefinition& definition() const& noexcept { return definition_; }
    const CircuitDefinition& definition() const&& = delete;

  private:
    CircuitDefinition definition_;
};
enum class SampleSide { regular, before_breakpoint, after_breakpoint };
// Helpers require a structurally valid Source (as checked by Circuit).
// Interpolation is deterministic; left=true means value immediately before a hold point.
double source_value(const Source&, double time_seconds, bool left = false);
double source_derivative(const Source&, double time_seconds);
enum class Initialization { operating_point, specified_storage };
struct CapacitorVoltage {
    ComponentId capacitor;
    double voltage_volts;
};
struct InductorCurrent {
    ComponentId inductor;
    double current_amperes;
};
struct InitialConditions {
    Initialization mode = Initialization::operating_point;
    std::vector<CapacitorVoltage> capacitor_voltages;
    std::vector<InductorCurrent> inductor_currents;
};
struct VoltageProbe {
    NodeId positive, negative;
};
struct CurrentProbe {
    ComponentId component;
};
struct Request {
    double stop_seconds = 1, maximum_step_seconds = 0.001;
    InitialConditions initial;
    std::vector<VoltageProbe> voltages;
    std::vector<CurrentProbe> currents;
};
// Validates time, source/probe counts, grid progress, memory and worst-case work before solving.
std::vector<double> time_grid(const Circuit&, const Request&);
struct BranchCurrent {
    ComponentId component;
    double current_amperes;
};
struct State {
    std::vector<NodeVoltage> node_voltages;
    std::vector<BranchCurrent> branch_currents;
    NumericalQuality quality;
};
enum class Status { ready, running, complete, cancelled, failed };
struct TimePoint {
    double seconds;
    SampleSide side;
};
struct Failure {
    double time_seconds;
    Error error;
};
struct Result {
    Status status;
    double current_time_seconds;
    State latest;
    std::vector<TimePoint> times;
    std::vector<std::vector<double>> voltages, currents; // Probe declaration order.
    std::optional<Failure> failure;
};
class Simulation {
  public:
    Simulation(Circuit circuit, Request request);
    ~Simulation();
    Simulation(Simulation&&) noexcept;
    Simulation& operator=(Simulation&&) noexcept;
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;
    Status step(); // One interval plus endpoint batch; terminal calls are idempotent.
    Status run();
    void cancel() noexcept;  // Terminal, keeps accepted prefix; pausing requires no core operation.
    Result snapshot() const; // Owned, explicitly partial/failed if execution did not complete.
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace openece::circuits::transient
