# Digital Timing and Sequential Logic (v0.5)

## Public API and ownership

Link `OpenECE::digital`. Public headers under `openece/digital/` are
`timed_circuit.hpp`, `simulation.hpp` and `timing_trace.hpp`, in namespace
`openece::digital::timing`. This layer is Qt-independent and does not use
`SampledSignal`. All v0.4 combinational APIs remain unchanged.

- `Time{uint64_t ticks}` and `Delay{uint64_t ticks}`: one tick = one picosecond.
- `DelayedGate{Gate gate, Delay propagation}` reuses the seven Boolean gate kinds.
- `SrLatch{id, set, reset, output_delay}`: active-high controls.
- `DLatch{id, data, enable, output_delay}`: active-high enable.
- `DFlipFlop{id, data, clock, Edge::Rising/Falling, clock_to_q}`.
- `Element` is a variant of those four types; `element_id`, `element_delay`,
  `element_inputs` and `is_storage` inspect it. Pin order is S/R or D/control.
- `TimedCircuitDefinition{inputs, elements, outputs}` is an owned, editable draft.
- `TimedCircuit(definition)` validates and owns resolved topology. `definition()`
  borrows a const reference from an lvalue only.
- `with_delay(const Circuit&, Delay)` explicitly creates an independent timed
  definition with original node IDs, names, ordering and the chosen gate delay.
- `SimulationRequest{horizon, initial_inputs, initial_storage, changes, clocks,
  observed, work}` owns input data. Initial inputs follow declaration order;
  `InitialState{element, q}` entries must identify every storage element exactly
  once, never a gate or input. `InputChange{at, input, value}` drives primary inputs.
- `Clock{input, first_edge, high, low}` describes a periodic input; see below.
- `Simulation(TimedCircuit, SimulationRequest)` owns a separate initialized session.
  `step()` processes one queued timestamp (including stale-only batches), or advances
  to the horizon if no deliveries remain. `run()` steps to completion.
  `current_time()` and `status()` expose progress; `StepStatus` is Ready/Complete/Failed.
- `snapshot()` returns owned `SimulationSnapshot{reached, status, inputs, elements,
  outputs, traces}`. Elements are **visible output values**, in declaration order.
- `SignalTrace{source, transitions}` owns `Transition{at, value}` values.
- `SimulationError` provides `at()` and optional `node()`, plus a diagnostic.

Copies own independent data; moves transfer ownership. Use moved-from circuit and
session objects only for destruction/reassignment. Borrowed definitions expire on
destruction/reassignment. Snapshots do not borrow from sessions. No global state,
wall clock, Qt timer or random generator affects core results. There is no API to
mutate a session's topology or stimuli after construction; create a new session.

## Validation and feedback

Inputs and **all** element outputs share one unique NodeId namespace. Names are
exact bytes, with the v0.4 nonempty/unique-per-input-or-output-list/128-byte rules.
At least one primary input and output are required. Output aliases, fan-out,
tied pins, disconnected nodes and forward references work as in v0.4. Every
reference must resolve explicitly, including observations and initial states.
No missing value is silently filled with zero. Invalid enums are rejected.

All gate/storage delays are strictly positive and at most one second. The horizon
is inclusive, between zero and one second. Manual changes and first clock edges
must be strictly after zero and within the horizon. Duplicate manual assignments
to the same input at the same time are rejected, even if the values match.

Storage outputs are dependency boundaries. Kahn ordering must consume the entire
remaining gate-to-gate graph, including disconnected gates. A pure gate cycle is
rejected; a path through an explicit storage element may feed back. Diagnostics
list unresolved/blocked gate IDs in declaration order, including possible downstream
gates; they do not claim each listed gate is directly on a cycle. Legal storage
feedback can still oscillate and hit the simulation work budget.

Malformed definitions/requests throw `std::invalid_argument`; hard count/time
limits throw `std::length_error`. Invalid adjustable budgets throw
`std::invalid_argument`. Allocation failures may propagate.

## Initialization and timestamp ordering

At t=0, every input and storage Q is explicitly supplied. Pure gates settle in
cached topological order using those Q values as boundaries, with no startup gate
wavefront. Initial trace values are recorded. SR/D latch controls are then evaluated
once and may schedule delayed Q. A flip-flop has no invented initial clock edge.
This is a deterministic initial-condition convention, not a power-up simulation.

The priority queue sorts by integer time, then monotonic insertion sequence. At t:

1. Remove all already-due events and count work, including stale entries. Keep only
   valid deliveries; generation tokens identify cancelled gate entries.
2. Apply the surviving visible-node/input changes as one batch.
3. Compare pre-batch and post-batch values to find changed nodes and edges.
4. Evaluate affected elements at most once, in element declaration order.
5. Schedule resulting output deliveries strictly after t. Clock successor events
   are also scheduled after applying the batch, always strictly in the future.
6. Record actual visible changes at t.

No newly scheduled event is processed recursively at the same timestamp. Input
stimulus vector order does not change results. Outputs at exactly the horizon are
delivered; deliveries beyond it are omitted using subtraction before time addition.
When the queue becomes empty, a final step advances to the requested horizon.
Stepping a complete session is idempotent.

## Inertial gates and transport storage

A gate tracks one logically pending output transition. If the desired value returns
to its visible output, the pending transition is cancelled. If its desired value
still matches the pending target, that event retains its original deadline.
Otherwise a new generation replaces it. Stale physical queue entries remain until
popped and count toward queue and processing limits.

A pulse **narrower** than gate delay is suppressed. A pulse **exactly equal** to delay
survives: the already-due output is delivered before the returning input can trigger
a reaction. For a 5 ps buffer initially 0, input rises at 10 and falls at 15:
output rises at 15 and falls at 20. Falling at 14 would suppress that output pulse.
All gate types use the same positive delay for both transition directions.

Storage maintains **internal captured state separately from externally visible Q**.
Changing captured state queues a transport-style Q delivery; later input changes
never cancel it. Consecutive captures retain ordering even if Q has not caught up.
Capturing the same internal value creates no redundant delivery.

| Element | Capture/hold convention |
| --- | --- |
| SR latch | Post-batch S,R: 00 hold, 10 set, 01 reset, 11 terminal error |
| D latch | Post-batch enable=1 tracks post-batch D; closing enable holds pre-batch D; otherwise hold |
| D flip-flop | On selected pre→post clock edge, capture **pre-batch D**; other edges/data-only changes hold |

For example, a DFF with 1 ns clock-to-Q may capture at 10 ns while Q remains old
until 11 ns. Data changing simultaneously with that edge is not captured. This
also applies when D is another storage output delivered at the same timestamp.
No setup/hold, metastability, asynchronous preset/clear or electrical model is implied.
SR=11 is checked at initialization and after affected batches and stops the session;
it does not invent Unknown. On runtime failure `status()` becomes Failed; `step`,
`run` and normal `snapshot` are unavailable. Construct a new session to recover.

## Clocks

The clock input's explicit normal initial value determines the first transition:
at `first_edge`, it toggles away from that value. `high` is the time spent high
before a falling edge; `low` is the time spent low before a rising edge. Both are
strictly positive and bounded by the maximum simulation time. Only the next edge
is queued, so a long clock description does not allocate every transition upfront.

At most one Clock may drive each primary input. A clocked input cannot also appear
in manual InputChange stimuli. Clocks may share timestamps with each other and
with other inputs. The declared initial level remains visible from t=0 until the
first edge. High/low durations can differ; both rising and falling DFFs are supported.

## Traces and resource limits

Each observed node has its initial value at zero followed only by actual visible
changes. Timestamps are strictly increasing; traces are right-continuous. Lane
order follows the request's observed-ID order, not topology or output names.
An empty observation list is legal. Snapshots retain input/element/output declaration
order independently of trace order. Displaying an output alias never creates a node.

Inclusive core policies are centralized in `timed_circuit.hpp`:

| Resource | Maximum |
| --- | --- |
| Inputs / elements / outputs | 64 / 4096 / 256 |
| References (all pins plus output sources) | 16,384 |
| Name | 128 bytes |
| Horizon or delay | 1,000,000,000,000 ps (one second) |
| Manual stimuli | 100,000 |
| Physically queued events, including stale | 100,000 |
| Processed events, including stale | 1,000,000 |
| Evaluated pin visits, including initialization | 64,000,000 |
| Observed nodes | 64 |
| Recorded values, including initial values | 1,000,000 |

`WorkLimits` may lower the queue, processing, pin or recording cap, but must remain
positive and cannot raise them. Counts and time arithmetic are checked before
allocation/addition/work. A runtime budget exhaustion throws SimulationError and
makes the session terminal. These bounds are resource policies, not real-time guarantees.

## Desktop workflow and limits

Choose **Digital Logic → Timing / Sequential**. The example has D rising at 2000 ps,
falling at 12000 ps, a clock first rising at 5000 ps with 5000 ps high/low durations,
and a rising-edge DFF with 1000 ps delay. Expected Q rises at 6000 ps and falls at
16000 ps. Run it before editing; then try Step timestamp to inspect delayed Q.

Editor tabs contain inputs/clocks, elements, outputs and manual input changes.
Double-click cells. The element dropdown chooses a gate, latch or rising/falling
DFF. Pin lists are comma-separated node IDs in the documented order. Names retain
all whitespace; integer fields/ID-list tokens trim surrounding whitespace and
accept ASCII decimal digits only. Times do not accept decimals, locale separators
or scientific notation. Initial Q must be blank for gates and explicit 0/1 for
storage. All clock cells must be blank for a manual input, or fully specified.
Node IDs are read-only and never reused within a draft. Removing a source leaves
its old references intact; validation reports them, and you repair them explicitly.
Loading an example or copying a circuit replaces the whole timing draft.

Run/resume advances batches; Pause stops at the last processed timestamp. Step
processes one queued timestamp, which may contain only stale events and no visible
change. Reset or any draft edit discards the session and displayed results. Failed
runs clear traces/outputs and show the error. The QTimer yields between at most
100 timestamps (or about 8 ms of stepping), but a single timestamp and plot refresh
are not preemptible. It is not the simulation clock. Display units only rescale
the plot; stored timestamps remain exact integer picoseconds.

Observed IDs determine diagram lanes. Low/high levels mean 0/1; vertical steps are
visible transitions, and the plot stops at current simulation progress, not an
uncomputed future. Traces retain physical model delays without compensation.
Copying a validated combinational draft preserves IDs and input values with an
explicit common gate delay; subsequent edits in either tab are independent.
The copy action initially observes at most the first 16 declared nodes; edit the
visible observation list to choose others.

GUI limits in `timing_view.hpp`: 8 inputs, 64 elements, 16 outputs, 8 gate pins,
16 observed nodes, 10,000 manual stimuli/queued events, 100,000 processed events,
5,000,000 evaluated pin visits and 50,000 recorded values. The core horizon bound
still applies. No full schematic editor, persistence, undo/redo, buses, X/Z, constants,
JK/T primitives, FSM tooling, Boolean minimization, HDL or analog timing is implemented.

## Testing and tradeoffs

The headless timing suite uses explicit expected traces for delays, narrow/exact
pulses, cancellation, pre-batch capture, transparent/closing latches, SR forbidden
states, clock polarity/duty cycle, feedback and shift registers. A separate
fixed-tick reference (no priority queue/generation tokens, independently evaluated
Boolean functions) checks 30 generated DAGs, including reversed declaration order.
Limits, invalid definitions/stimuli, trace ownership and failure recovery are tested.
GUI tests inspect actual Qwt curve samples and exercise controls, invalid drafts,
copy isolation and unchanged existing domain workflows.

Session snapshots copy traces; timestamp processing scans bounded state and element
vectors. Plot refresh rebuilds bounded curves. This is intentionally a small,
inspectable simulator, not optimized for very large circuits or real-time execution.
The editor retains raw draft cells locally; a future schematic/persistence workflow
may justify an independent editor model. Exception messages are user diagnostics,
not a structured multi-error reporting API. Two-state initial conditions and terminal
SR=11 are deliberate scope choices, not placeholders for an HDL simulator.
