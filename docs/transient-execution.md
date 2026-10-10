# Transient execution architecture

The numerical `Simulation` owns a validated circuit and request. Initialization either
succeeds or throws without a usable result. `step()` accepts exactly one interval,
including both breakpoint-side solves atomically. Failure preserves the preceding
accepted state/trace and reports attempted time separately. `cancel()` is terminal
and preserves accepted samples; pausing is a scheduler operation, not cancellation.

## Baseline benchmark and scheduler decision

Measured 2026-10-08 on native Fedora 44, i9-12900H, GCC, Qt 6.11.2,
Qwt offscreen at 1000×500. Optional target `openece_transient_benchmark` is excluded
from normal builds/tests. Repeat with Debug and Release; results are observational,
not portable timing guarantees. Circuits have independent grounded RC branches and
one current source; the large case exceeds the editor's 32-node storage limit and
exercises the numerical 128-node limit. An additional maximal core case uses 128
nodes, 64 voltage sources and 64 inductors (255 unknowns), with specified storage
and 51 accepted intervals. The core's work budget is not a latency bound.

| Nodes | Debug init ms | Debug p95/max step ms | Release init ms | Release p95/max step ms |
|---|---:|---:|---:|---:|
| 2 | 0.46 | 0.009 / 0.035 | 0.10 | 0.00034 / 0.004 |
| 32 | 1.93 | 0.76 / 0.87 | 0.11 | 0.016 / 0.044 |
| 128 | 35.76 | 17.95 / 19.35 | 1.31 | 0.596 / 0.615 |
| 255 unknowns (separate maximal core case) | 24.51 | 133.66 / 135.34 | 0.93 | 3.87 / 4.06 |

The representative copy benchmark contains 2,000,000 double probe values across
64 probes and 31,250 timestamps: 16.5 MB of values/timestamp payload on the tested
64-bit ABI (`sizeof(TimePoint) == 16`). Its copy took 4.3 ms in both Fedora builds.
This is not a universal maximum: payload is the probe-value count times `sizeof(double)` plus the timestamp count
times `sizeof(TimePoint)`, before vector/object metadata, allocator overhead,
spare capacity and solver state. The independent 200,001-timestamp limit can add
about 3.2 MB to the 16 MB value limit on that ABI. Single-curve rendering with event
processing took 1.4–1.7 ms for 4096 samples and 2.9–3.2 ms for 16384 samples.

Decision: **worker-owned simulation**, not GUI-thread dense solves. Aim for less
than 16 ms ordinary GUI callbacks, a 50 ms polling interval, and at most five
snapshot publications per second. These are engineering budgets, not real-time
promises. Rendering shows one selected voltage and one selected current probe;
the complete mixed-order numerical trace remains available in a table model.
The single-slot mailbox replaces older unconsumed updates instead of queuing
unbounded full traces. No snapshot is made on every integration step.

Numerical state, mailbox and GUI trace can each retain a bounded trace. Three
copies of the representative benchmark payload are about 49.5 MB, but this is not
the worst permitted timestamp arrangement or total process memory. Publication
can briefly add another snapshot, and growing vectors have spare capacity.
Aim below 128 MB transient execution storage excluding the application baseline;
this is a design budget, not an enforced RSS cap or a guarantee for every workload.
Plot envelope buckets
bound ordinary samples; every ordered breakpoint side is retained additionally.
The worker never accesses a widget, borrowed draft or GUI QObject. Control commands
are checked between atomic initialization/interval operations. Pause/Cancel cannot
interrupt a dense solve. Shutdown joins that operation before freeing session state.
All rendering and model publication occurs on the GUI thread. A cancelled or failed
result retains its diagnostic and accepted prefix; no new schema fields store it.

### Verified Windows measurements

[CI run 38010628714](https://github.com/codyklein/open-ece/actions/runs/38010628714)
at `5ffb04eb62c35521744db3840aa60dd3ad75287d` recorded these observations on the
hosted Windows Server 2022 x64 runner, MSVC 2022, Qt 6.8.3 and Qwt 6.3.0. Logs are
`transient-benchmark-{Debug,Release}.log` and `tests/transient-execution-{Debug,Release}.log`
in `windows-build-test-logs`. Runner timings vary; this is neither physical Windows
11 evidence nor a performance promise.

| Case | Debug init ms | Debug p95/max step ms | Release init ms | Release p95/max step ms |
|---|---:|---:|---:|---:|
| 2 nodes | 2.281 | 0.098 / 0.221 | 0.282 | 0.002 / 0.045 |
| 32 nodes | 5.506 | 1.731 / 3.399 | 0.360 | 0.041 / 0.150 |
| 128 nodes (core-only size) | 84.281 | 41.286 / 41.941 | 2.288 | 1.148 / 1.287 |
| 255 unknowns (core-only size) | 50.755 | 269.693 / 270.275 | 1.730 | 6.697 / 7.141 |

The same 16.5 MB representative snapshot copied in 5.61 ms Debug / 6.63 ms Release.
Rendering 4096 samples took 17.52 / 2.23 ms and 16384 samples took 34.47 / 5.16 ms.

| GUI workload (64 probes) | Debug elapsed / max heartbeat gap ms | Release elapsed / max heartbeat gap ms |
|---|---:|---:|
| 32-node dense work, 1002 rows | 1953 / 246 | 75 / 24 |
| 1.6 million probe values, 25002 rows | 5006 / 99 | 219 / 18 |

These results support worker-owned dense solves and bounded publication. They do
not establish a 16 ms GUI deadline: snapshot rendering/publication can exceed the
ordinary-callback target, particularly in Debug. Pause/Cancel/shutdown still wait
for the current atomic operation. Native Fedora controller measurements and
physical acceptance belong in the [milestone record](transient-execution-acceptance.md);
no hosted timings replace exact-package physical validation.

## Creating and running an experiment

1. Open a [transient example](../examples/README.md#transient-examples-schema-2),
   or add nodes/components in **Circuits → Transient → Circuit**. Choose ground,
   terminals and physical values/units. IDs define references, not names.
2. Sources use component value as their initial amplitude. Constant mode ignores
   retained points; hold/linear mode uses ordered positive-time points. Points
   beyond the stop time stay saved but are not scheduled.
3. Choose **Operating point** for a steady-state start (C open, L short), or
   **Specified storage** with exactly one initial capacitor voltage/inductor
   current per storage component. Disabled initial rows are retained/ignored.
4. Add ordered voltage/current probes; inactive reference columns are ignored.
   Voltage is positive-node minus negative-node; branch current is positive →
   negative terminal. Probe names, references, units and labels freeze per run.
5. Choose finite positive stop time and maximum step, then **Run**. Choose a step
   substantially smaller than the fastest relevant time constant/oscillation
   period; halve it and compare. Backward Euler is first-order and numerically
   damps oscillations. Stability does not imply an accurate large step.

**Run** starts a fresh experiment. **Pause** preserves one resumable simulation;
**Resume** continues it. **Step** initializes if needed, advances precisely one
integration interval, and remains paused unless complete/failed. **Cancel** ends
execution and retains the final accepted prefix; it cannot resume. **Reset results**
clears results/diagnostics without changing project inputs. Completed/cancelled
experiments can be started afresh with Run or Step.

Progress shows accepted time, never an attempted failed endpoint. Initialization
failure has no accepted trace. Integration failure displays attempted time and last
accepted time separately, preserving the preceding accepted samples. Cancel does
not turn a failed/complete result into success. Diagnostics render plain text.
The numerical error contracts include floating references, nonunique/inconsistent
source constraints, unsupported storage/impulse constraints, conditioning, numerical
residual failures and resource limits. Nothing is regularized to force a result.

## Results, edits and persistence

Voltage/current tabs each select one frozen probe with physical units and terminal
orientation. The numerical table preserves mixed probe declaration order, explicit
timestamps and sample sides. Before/after breakpoint rows share the same timestamp;
no samples are removed from the authoritative trace. Plot envelope reduction keeps
bucket endpoints/min/max and **every** breakpoint side in order; the line represents
accepted samples, not an analytical reconstruction or hidden interpolation across
failed steps. No full-data table-cell widgets are allocated.

Editing active raw configuration immediately removes resumability and marks retained
results **Stale**; numeric parsing happens only for explicit execution/conversion,
never for saving. Inactive source settings, operating-point IC rows and inactive
probe references do not participate in execution/invalidation. Navigation/counters
and display time units are not numerical inputs. A run retains its selected display
unit; changing that selector applies to the next experiment. Data edits still dirty
normal project state, independent of execution invalidation. Runtime progress,
results, command actions and result-tab selection do not dirty or enter the schema.

Save/reopen retains incomplete drafts, ordered probes, IDs, units and source settings.
All project loading is inert, including examples. Run/Step explicitly recomputes
results. Open/New/close stop the worker and dispose the old owned session safely.
No core numerical APIs or schema tokens were changed for execution.

## Scope and limits

This is bounded dense linear R/C/L simulation with constant/hold/linear independent
sources and fixed maximum-step backward Euler. Core limits remain 100000 intervals,
200001 samples, 64 probes, 2000000 probe values, dense-work ≤1e9. The editor's schema
storage limits remain 32 nodes/128 components/4096 retained source points; storing
an invalid request does not make it executable. No silent truncation, adaptive
method, nonlinear device, impulse model, SPICE feature or external asset is added.
Full-prefix snapshots at five Hz and model resets remain a bounded simplicity
tradeoff. Dense initialization/step/publication must finish before shutdown joins;
there is no hard real-time guarantee or cancellation inside Eigen.

## Native responsiveness evidence

The GUI regression runs both a 32-node/64-probe dense workload (1002 samples,
~0.93 s) and an RC/64-probe trace workload (25002 samples, 1600128 values,
~1.23 s) while a 10 ms GUI heartbeat remains active. Maximum observed heartbeat
gap was 18 ms for both on the Fedora Debug host. A separate maximum-payload model
check keeps 2000000 values without allocating per-cell widgets. Combined process
peak RSS for these bounded checks was 105964 KiB (~103.5 MiB), including Qt and the
standalone view. This is not a measurement of the entire workbench baseline or a
hard maximum for all allowed circuits. GUI tests reject a one-second event-loop
stall; finer timing targets are benchmark evidence rather than flaky CI assertions.

The complete existing analytical/reference suite is retained. New GUI coverage
includes charging/discharging, RL and all three RLC damping regimes, operating
point and specified storage, inactive invalid fields, hold/linear source boundaries,
mixed probe ordering, plot/table sample agreement, Run/Step equivalence, rapid
Resume/Pause, Cancel, exact raw/delegate edits, frozen labels, both failure stages,
and active Open/New/close. Four ordinary schema-2 examples are decoded/round-tripped
headlessly, opened inertly and explicitly executed in GUI and fresh runtime probes.
Historical schema-1 fixtures remain byte-for-byte unchanged.

## Editor and trace layout

Execution controls/status remain above a vertically resizable editor/results split.
The theme-colored grip resizes both panels live during dragging. Neither panel
can collapse: the editor retains a font-scaled minimum and scrolls internally,
while results retain their content minimum. The outer workspace also scrolls
when a small window cannot fit both panels. Resizing only repaints existing plot
samples; it does not step, reset or recompute the simulation.
For small circuits the node/component tables fit their row counts rather than
reserving a large empty editor. Visible editor rows are capped and additional rows
scroll; heights follow font/style metrics. Columns are manually resizable and may
scroll horizontally. Full cell/reference labels and frozen numerical probe headers
are available through tooltips; the trace model still owns no per-cell widgets.
Component creation uses human-readable labels backed by unchanged schema tokens.
Neither splitter movement nor column resizing changes editable project state.
