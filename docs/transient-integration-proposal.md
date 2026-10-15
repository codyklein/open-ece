> Persistence/editor corrections are finalized in [transient-project-format.md](transient-project-format.md). Its allocator, schema-1 import, provenance, conversion and storage rules supersede this initial proposal. The implemented execution decision is recorded in [transient-execution.md](transient-execution.md); this file remains the historical integration proposal.

# Transient GUI and schema 2 — proposal for review

This document proposes the next v1.1 milestone. It does not implement a GUI,
change a codec, or authorize schema changes. The implemented core contract is
[transient-analysis.md](transient-analysis.md). Existing schema-1 meanings,
DC/AC APIs and release-pinned fixtures are unchanged at this checkpoint.

## GUI composition and ownership

Add `TransientView` to the existing CircuitsWorkspace alongside DC and AC;
MainWindow remains navigation/composition. The view borrows a new
`project::TransientDraft` inside the authoritative ProjectSnapshot. Extend the
existing circuit-row/unit adapters where appropriate, without making the DC/AC
solver models universal. Construct an owned transient Circuit and Request only
on an explicit Run/Step action, after exact pending-editor synchronization.
No solver objects are created by persistence or inert restoration.

A table/form editor provides nodes, explicit ground, R/C/L/V/I components,
source-point editing for the selected independent source, initial-state rows,
stop/maximum-step quantities, and ordered voltage/current probes. Display units
must identify instantaneous volts/amperes and seconds, not RMS or digital ticks.
Expose the initialization policy prominently: default operating point is not
zero-energy startup. Show the first-order/numerical-damping warning in help.

Use Run, Pause/Resume, Step, Cancel and Reset results. A QTimer advances a bounded
number of intervals per callback; cancellation is observed between intervals.
Pause stops advancing the same owned Simulation, while Cancel terminates it and
retains a labelled accepted prefix. Run after Cancel creates a new execution.
Reset removes derived execution/results, not editable configuration. If a single
maximum-size solve exceeds a measured GUI latency budget, reduce GUI execution
limits or use a worker with owned snapshots before declaring the workflow ready.
Do not assume the core work limit is a responsiveness guarantee.

Any execution-relevant raw edit stops the old execution and marks retained
results stale. Document dirty state continues to track persisted edits only;
result navigation, stepping, progress, cancellation and plots do not dirty it.
Provide clear ready/running/paused/partial/cancelled/failed/complete text and the
failure time/IDs. Constructor failures leave no pretend normal solution.

Voltage/current plots have separate SI-labelled axes and explicit-time samples.
Before/after samples at the same timestamp represent a vertical discontinuity,
never a smooth interpolation across it. A numerical trace table includes seconds,
sample side, ordered probes and partial/failure status. Keep the latest full
node/branch state available textually. Snapshot copying/plot decimation must not
change the authoritative accepted trace or hide failures.

## Schema-2 envelope and compatibility

Retain format `org.openece.project`, with integer `schema_version: 2` for new
semantics. All existing root objects and their supported field meanings remain.
Extend `circuits` with required `transient` and permit `selected_tab: transient`.
Use the same bounded strict UTF-8/SAX accounting, duplicate-key rejection,
unknown-field warnings, exact draft strings, null references and ordered arrays.
No results, traces, simulation state, matrices, progress or external assets.

Recommend a writer that emits schema 2 after integration, and a reader with two
explicit dispatch paths. Schema-1 decoding continues to enforce every original
required field, and constructs an **empty, inert** new TransientDraft using the
specified schema-1 import policy. It does not invent transient components from
DC/AC or run anything. Schema-2 decoding requires every new field; it must not
fill missing fields permissively. Reject unsupported versions. Keep the pinned
v0.9 fixture bytes/hashes unchanged and compare all their supported original
editable state through open/save/reopen, including invalid raw text and IDs.

Saving an imported schema-1 project as schema 2 must explain that OpenECE 1.0.x
cannot read the newer format; recommend Save As when an old-reader copy matters.
There is no downgrade/export or general migration framework in this proposal.
This explicit one-version import extends the existing schema-1 compatibility
promise; it introduces no ABI/plugin/future-schema promise.

## Proposed required TransientDraft fields

Use existing Text, Edit, Quantity, Id, Ref and Next storage types from
[project-format.md](project-format.md). Numeric drafts remain strings: no numerical
parsing at load, save, capture or restore. These are draft storage rules, not
engineering validity rules. An incomplete source row is savable.

| Field | Representation |
|---|---|
| next_node, next_component | Next; independent monotonic transient namespaces |
| nodes | ordered `{id: Id, name: Text}` records |
| components | ordered component records below |
| ground | Ref in transient node namespace |
| stop, maximum_step | Quantity with `s|ms|us|ns` unit |
| initialization | `operating_point|specified_storage` |
| initial_conditions | ordered `{component: Ref, kind: capacitor_voltage|inductor_current, value: Quantity}` |
| probes | ordered probe records below |
| display_time_unit | `s|ms|us|ns` |
| selected_tab | `editor|sources|initial_conditions|probes|help` |

A component has required `id`, `name`, `kind`, `positive`, `negative`, `value`
and `source`. Kind is `resistor|capacitor|inductor|voltage_source|current_source`.
`value` is a Quantity with the selected component's unit family: `ohm|kohm|Mohm`,
`F|mF|uF|nF|pF`, `H|mH|uH`, `V|mV|uV`, or `A|mA|uA`. For a source it represents
the initial instantaneous value. Wrong/incomplete text remains an execution
error; unit/type pairing is a structural token contract. Normal type/unit edits
must keep existing explicit conversion and raw-text-preservation behavior.

`source` is always present, including for passive rows, so disabled source
configuration is retained. It has required `mode: constant|hold|linear` and
`points`, an ordered list of `{time: Quantity(s|ms|us|ns), value_text: Edit(128)}`.
Point values use the component value's explicit unit; switching units converts
valid values as a user edit, or rejects the switch while leaving invalid text
intact. Constant mode retains but does not execute the point list. Loading never
sorts points, resolves terminals, changes modes or installs defaults.

An initial-condition value uses `V|mV|uV` or `A|mA|uA` according to its kind.
Operating-point mode retains disabled initial-state rows; execution ignores them
when assembling the core request (which requires empty lists in that mode).
A probe has required `name`, `kind: voltage|current`, `positive: Ref`,
`negative: Ref`, `component: Ref`. Voltage uses the node references; current
uses the component reference. Inactive references are retained too. Display names
never resolve identity. Nodes and components have separate namespaces; zero is
an ordinary ID, and ground can be null or dangling.

Allocator invariants include every declared and retained dangling ID in terminals,
ground, probes and initial-state rows. Next counters exceed declared IDs or hold the existing exhausted sentinel; allocation skips all retained references without requiring counters to exceed dangling IDs. Keep deletion monotonic; a new component
must not reconnect a retained dangling initialization/probe reference.

Storage proposals: 32 nodes, 128 components, 64 probes, 128 initialization rows,
1024 points per source and 4096 point rows total. All consume the unchanged global
8 MiB file / 4 MiB text / 12000-row and depth/value/member budgets, including
unknown fields. Text/quantity limits follow existing schema types; runtime bounds
are checked separately. Benchmark representative Fedora and Windows workloads
before freezing GUI interval/trace/work limits. Never truncate on restore.

A representative transient subobject (the other required schema-2 root/domain
fields remain present) is:

```json
{
  "next_node": "3", "next_component": "13",
  "nodes": [{"id":"0","name":"ground"}, {"id":"1","name":"supply"}, {"id":"2","name":"output"}],
  "ground": "0",
  "components": [
    {"id":"10","name":"V","kind":"voltage_source","positive":"1","negative":"0","value":{"text":"10","unit":"V"},"source":{"mode":"constant","points":[]}},
    {"id":"11","name":"R","kind":"resistor","positive":"1","negative":"2","value":{"text":"1","unit":"kohm"},"source":{"mode":"constant","points":[]}},
    {"id":"12","name":"C","kind":"capacitor","positive":"2","negative":"0","value":{"text":"1","unit":"uF"},"source":{"mode":"constant","points":[]}}
  ],
  "stop":{"text":"5","unit":"ms"},
  "maximum_step":{"text":"10","unit":"us"},
  "initialization":"specified_storage",
  "initial_conditions":[{"component":"12","kind":"capacitor_voltage","value":{"text":"0","unit":"V"}}],
  "probes":[{"name":"output","kind":"voltage","positive":"2","negative":"0","component":null}, {"name":"supply current","kind":"current","positive":null,"negative":null,"component":"10"}],
  "display_time_unit":"ms", "selected_tab":"editor"
}
```

## Reviewable implementation order

1. Approve/refine this schema contract; add owned DTOs, strict schema-2 codec,
   schema-1 explicit import, allocator guards, invalid-draft and compatibility tests.
2. Add inert TransientView draft bindings, exact pending-editor synchronization,
   explicit units and full round trips. No execution on restore.
3. Wire validated owned requests and bounded incremental controls, stale-state
   handling, plots and numerical traces. Test documented RC/RL/RLC responses,
   before/after discontinuities, failures and cancellation through GUI workflows.
4. Add ordinary project examples/help, full Fedora/Windows/sanitizer/package
   regression validation, then bounded physical Windows/Fedora checks.

All File workflows remain transactional with QSaveFile direct-write fallback
disabled. No numerical API redesign, schematic editor, adaptive integrator,
nonlinear devices, external assets or unrelated domain work is required.
