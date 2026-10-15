# Linear transient analysis — v1.1 core contract

This is the transient numerical implementation contract. Configuration uses
[schema-2 persistence](transient-project-format.md). OpenECE 1.1 provides
[owned worker execution and numerical inspection](transient-execution.md). Loading
a project remains inert. See [1.1 release notes and acceptance gates](release-notes-v1.1.md).

## Model and conventions

`openece::circuits::transient` owns a separate validated circuit definition with
nodes, explicit ground, resistors, capacitors, inductors and independent voltage/
current sources. NodeId/ComponentId retain the circuit identifiers, exact names,
declaration ordering and positive-to-negative terminal/current convention.
Values are SI doubles: seconds, ohms, farads, henries, volts and amperes.
Sources are instantaneous physical values, not RMS AC phasors. No Qt, Eigen or
project-codec types occur in public transient APIs. Definitions and requests are
owned; validated snapshots never borrow widget data.

DC/AC APIs, numerical policies and schema-1 field meanings remain unchanged. Only small
internal structural validation utilities are reused. A transient time grid is not
a SampledSignal and the digital event simulator is not a circuit integrator.

## Sources and time grid

A source has an initial value and an ordered list of positive-time points.
An empty list means a constant source. Piecewise-constant sources take each point's
new value on its right; the initial value holds until the first point. Piecewise-
linear sources interpolate from (0, initial) through their points and then hold
the final value. Times must be finite, strictly increasing and within the supported
range; no sorting or deduplication. Signed finite values and zero are permitted.

The engine includes t=0 and the exact requested final time. Each integration
interval is at most the requested maximum step. Subdivide each interval between
fixed source/final endpoints evenly (one extra subdivision when required by
rounding) rather than accumulating a microscopic final remainder. Do not step
across a breakpoint. All simultaneous
source changes are applied together, in stable component-ID accumulation order.
Floating-point grid construction must make strictly positive progress; reject
unrepresentable intervals, excessive work and nonfinite coefficients explicitly.

Integrate to a breakpoint using its left-limit source values. Then reconcile the
right-limit algebraic values with capacitor voltages and inductor currents held
continuous. Record before/after breakpoint samples at the same timestamp in that
order, identified by an explicit sample-side enum. No interpolation conceals a
source discontinuity. A final-time breakpoint is processed too. Sources beyond
the horizon remain in the definition but do not generate execution work.

## Initialization

Two explicit modes:

- Operating point (default): capacitors are open and inductors have zero voltage;
  solve the linear operating point using each source's t=0 value. Reject floating,
  inconsistent or nonunique operating points. Use the accepted capacitor voltages
  and inductor currents as storage state. Initial-state lists must be empty.
- Specified storage: require exactly one finite voltage per capacitor and current
  per inductor, by ComponentId, with no duplicate, missing or wrong-kind entries.
  Determine algebraic node voltages/source currents consistently with those values.
  There is no silent defaulting or least-squares fitting of conflicting states.

Initial reconciliation first groups capacitor-voltage and voltage-source
constraints, checks consistent offsets, and solves resistor-connected group
potentials. It then solves capacitor differential currents and voltage-source
currents from KCL and the source's right-hand derivative. Relative derivative
coordinates within each constraint group remove unobservable common derivatives;
they do not ground a physical node or alter its accepted voltage. This supports
consistent parallel capacitors and capacitor/source constraints without assigning
arbitrary observable currents. Inductor derivatives follow v=L*di/dt.

Independent voltage-source cycles are rejected: individual source currents would
be nonunique even if voltages were consistent. Distinguish contradictory loops
where possible. Operating-point checks also include the zero-voltage inductor
constraints; redundant inductor/source loops have nonunique initial branch currents. A source jump that would require an instantaneous capacitor-
voltage or inductor-current jump is unsupported (impulses are not simulated).
Certain higher-index source/inductor cutsets cannot be initialized by this bounded
formulation; report unsupported initialization, not a fabricated solution.

## Backward-Euler MNA

Unknowns are non-ground node voltages in declaration order, followed by currents
of voltage sources and inductors in component declaration order. Stamp resistors
and independent sources with the existing positive-to-negative sign convention.
Ground is omitted from the unknown vector and returned as exactly zero.

For step h and previous capacitor voltage v_old:

    i_C = (C/h) * (v_new - v_old)

Stamp conductance C/h and the corresponding history current. For an inductor:

    v_new - (L/h)*i_new = -(L/h)*i_old

Between source knots, solve for increments about the last accepted state,
centering each component's RHS before accumulation. This avoids subtracting large
already-rounded history terms near a settled value. At knots use the absolute
form to preserve exact small source endpoints after large preceding values.
These are algebraically equivalent backward-Euler equations with the same matrix,
variable ordering and acceptance thresholds. Check the backward error of both the
centered solve and the recovered original equations, then physical branch/KCL
residuals. The source-incidence block uses an ordinary transpose. No regularization, added
resistance, hidden ground, pseudoinverse or nonlinear iteration is permitted.
OpenECE owns all indexing/stamps and physical checks; private Eigen FullPivLU
performs the dense solve. Four matrix-only equilibration passes and the existing
rank/rcond/backward-error policies are reused, without changing DC/AC acceptance.
Independently check branch equations and KCL, including omitted ground KCL.
Voltage constraints (sources, initialized/held capacitor voltages, and inductor
step equations) use 1e-12 V absolute plus 1e-10 times the sum of actual/expected
branch-voltage magnitudes. Do not use large common-mode node voltages to hide a
lost differential storage value. KCL uses 1e-15 A absolute plus 1e-10 times the
sum of incident current magnitudes. These are numerical acceptance policies,
not physical tolerances or an integration-accuracy estimate.

Backward Euler is first order and introduces numerical damping. A successful
linear solve does not establish integration accuracy. Verify step refinement and
analytical responses; users will choose a step small relative to relevant time
constants/oscillation periods. No adaptive accuracy controller or trapezoidal
method is included in this milestone.

## Execution, traces and errors

A move-only Simulation owns its circuit/request, storage state and accepted trace.
One step processes one interval and its endpoint source changes without recursive
execution. Run uses the same operation. Cancellation is terminal for that instance;
it retains the accepted prefix, while pausing is simply not calling step. Completion,
cancellation and failure are distinct statuses. Failed intervals append nothing
and leave the last accepted state/time intact. Structured failure includes time,
error code, message and implicated IDs where known. Constructor validation or
initialization fails before any normal result is exposed.

Requests select bounded ordered differential-voltage and component-current probes.
Node voltage is a probe relative to ground. Returned traces retain request order;
latest full node/branch results retain declaration order. All records are owned,
with explicit timestamps/sides. No normal complete result is returned for a partial
or failed run. Allocation/system exceptions are not mislabeled circuit failures.

Diagnostics distinguish malformed definitions/requests, floating topology,
contradictory storage/source constraints, nonunique source currents, unsupported
initialization, numerical rank/conditioning/residual failures and resource limits.

## Execution resource policies

Centralize and test limits before allocation: 128 nodes, 512 components, 64 voltage
sources, 64 inductors, 255 dynamic MNA unknowns; 4096 points per source and 16384
total source points; 64 total probes; 100000 integration intervals and 200001
recorded time points; 2000000 recorded probe values. Maximum source/initial-value
magnitude is 1e9; supported positive time/step requests extend through 1e9 seconds.
Reuse existing R/C/L ranges and 128-byte exact nonempty unique names. Actual
representable grid progress and finite arithmetic are additional requirements.

Bound worst-case dense work by (grid points + twice the source/final boundary
count + 3) times max(1,unknowns)^3 <= 1e9, checked before a simulation begins.
This conservatively includes the two right-limit reconciliation solves at each
breakpoint and up to three initialization solves. These are the v1.1 execution
policies, not accuracy/latency guarantees. [Fedora and Windows benchmarks](transient-execution.md)
justify worker scheduling; they do not justify relaxing numerical acceptance.
Resource failure never truncates a request. GUI/storage limits remain separate.

## Tests and compatibility

Analytical RC/RL and all RLC damping regimes, nonzero energy, convergence, damping,
480+ independent branch-formulated reference steps, source boundaries, continuity, signs,
KCL/KVL, invalid/floating/singular constraints, ordering, ownership, bounds and
incremental/Run equivalence precede GUI use. Retain the complete existing matrix.

Schema 2 now stores transient draft text/units/IDs/probes without changing
schema-1 meanings or release-pinned v0.9 fixture bytes. Restoration never
executes this core; execution requires an explicit Run/Step action. Nonlinear devices,
adaptive stepping, SPICE integration and schematic editing remain out of scope.

## Public entry points and a minimal RC run

The public header is `openece/circuits/transient/analysis.hpp`, linked through
`OpenECE::circuits`. `CircuitDefinition` is an owning editable input; constructing
`Circuit` validates structural rules only. `Source` points are likewise checked
by Circuit; the public source evaluation helpers require a valid Source.
`Request` selects time bounds, initialization and ordered probes. `time_grid()`
validates the execution grid/probe budgets without executing a circuit.
`Simulation` performs initialization, owns that validated circuit/request and
provides `step()`, `run()`, `cancel()` and an owned `snapshot()`.

```cpp
namespace tr = openece::circuits::transient;
using openece::circuits::NodeId;
using openece::circuits::Resistor;
tr::CircuitDefinition draft{
    {{{0}, "ground"}, {{1}, "supply"}, {{2}, "output"}}, NodeId{0},
    {tr::VoltageSource{{10}, "V", {1}, {0}, {10, {}}},
     Resistor{{11}, "R", {1}, {2}, 1000},
     tr::Capacitor{{12}, "C", {2}, {0}, 1e-6}}};
tr::Request request;
request.stop_seconds = 0.005;
request.maximum_step_seconds = 1e-5;
request.initial.mode = tr::Initialization::specified_storage;
request.initial.capacitor_voltages = {{{12}, 0}};
request.voltages = {{{2}, {0}}};
request.currents = {{{10}}, {{12}}};
tr::Simulation simulation(tr::Circuit(draft), request);
simulation.run();
auto result = simulation.snapshot(); // Check status/failure before claiming completion.
```

Here the analytical capacitor response is 10*(1-exp(-t/0.001)) V. Initial source
current is -10 mA; the positive-to-negative capacitor current is +10 mA. The
backward-Euler samples approximate this response with first-order step error.
With default operating-point initialization the same circuit instead starts at
10 V with zero charging current. There is no implicit zero-energy start.

Moved-from Simulation instances may only be destroyed or reassigned.
