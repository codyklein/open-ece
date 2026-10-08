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
exercises the numerical 128-node limit. The core's work budget is not a latency bound.

| Nodes | Debug init ms | Debug p95/max step ms | Release init ms | Release p95/max step ms |
|---|---:|---:|---:|---:|
| 2 | 0.46 | 0.009 / 0.035 | 0.10 | 0.00034 / 0.004 |
| 32 | 1.93 | 0.76 / 0.87 | 0.11 | 0.016 / 0.044 |
| 128 | 35.76 | 17.95 / 19.35 | 1.31 | 0.596 / 0.615 |

A full 2,000,000-value snapshot payload is about 16.5 MB with timestamps.
A representative copy took 4.3 ms in both builds. Single-curve render with event
processing: 4096 samples 1.4–1.7 ms; 16384 samples 2.9–3.2 ms.

Decision: **worker-owned simulation**, not GUI-thread dense solves. Aim for less
than 16 ms ordinary GUI callbacks, a 50 ms polling interval, and at most five
snapshot publications per second. These are engineering budgets, not real-time
promises. Rendering shows one selected voltage and one selected current probe;
the complete mixed-order numerical trace remains available in a table model.
The single-slot mailbox replaces older unconsumed updates instead of queuing
unbounded full traces. No snapshot is made on every integration step.

Numerical state, mailbox and GUI trace can each hold one bounded trace (~50 MB
combined at the core limit, excluding matrix/Qt overhead). Plot envelope buckets
bound ordinary samples; every ordered breakpoint side is retained additionally.
The worker never accesses a widget, borrowed draft or GUI QObject. Control commands
are checked between atomic initialization/interval operations. Pause/Cancel cannot
interrupt a dense solve. Shutdown joins that operation before freeing session state.
All rendering and model publication occurs on the GUI thread. A cancelled or failed
result retains its diagnostic and accepted prefix; no new schema fields store it.

Windows and final responsive-workload measurements are recorded with the milestone
validation after the controller tests. No Fedora timings are a claim about Windows.
