# Transient project configuration — schema 2

OpenECE 1.1 saves schema 2, which adds editable transient configuration. File loading/restoration never
constructs a Simulation or runs numerical analysis. Explicit GUI execution is
described in [the execution workflow](transient-execution.md); runtime results
remain separate from persistence.

## Envelope and compatibility

`format` remains `org.openece.project`; the integer `schema_version` is 2 on
new saves. Every existing root/domain field keeps its schema-1 meaning and limits.
The reader supports exactly versions 1 and 2. Schema 1 follows its original
strict field decoder and parser budgets, including unknown-field warnings; it
cannot select the transient tab. The pinned v0.9 files are not regenerated.

Schema-1 import constructs exactly this new draft: empty nodes, components,
initial conditions and probes; null ground; node/component counters zero;
`operating_point`; blank stop and maximum-step text with `s` units; display unit
`s`; selected tab `editor`. It does not derive a circuit from DC or AC.

The decoded version is document provenance, outside ProjectSnapshot. Saving an
imported schema-1 project over its original path warns that OpenECE 1.0.x cannot
read schema 2 and offers Save As or Cancel. Successful saves update provenance to
2; failed/cancelled saves keep provenance and path unchanged. A Save As also
checks path identity, so choosing an alias of the original file cannot bypass
this warning. No general migration or downgrade infrastructure is provided.

Unknown optional fields consume parser budgets, are reported, and are discarded
on re-save. Required semantics need a version change. Duplicate keys, malformed
UTF-8, missing required fields, wrong types and unsupported versions are rejected.

## Required fields

`circuits` adds required `transient`, and its `selected_tab` permits
`dc|ac|transient`. The transient object has:

| Field | Meaning |
|---|---|
| next_node, next_component | Canonical decimal-string monotonic counters, through 4294967296 (exhausted) |
| nodes | Ordered `{id,name}`; IDs decimal strings 0..4294967295 |
| ground | Node reference: null or canonical ID, including dangling IDs |
| components | Ordered records described below |
| stop, maximum_step | `{text,unit}` with exact draft text and `s|ms|us|ns` |
| initialization | `operating_point|specified_storage` |
| initial_conditions | Ordered `{component,kind,value}` records |
| probes | Ordered mixed voltage/current records |
| display_time_unit | `s|ms|us|ns` |
| selected_tab | `editor|sources|initial_conditions|probes|help` |

Every component requires `id,name,kind,positive,negative,value,source`.
Kinds and value-unit families are:

- `resistor`: `ohm|kohm|Mohm`
- `capacitor`: `F|mF|uF|nF|pF`
- `inductor`: `H|mH|uH`
- `voltage_source`: `V|mV|uV`
- `current_source`: `A|mA|uA`

`value` stores the component quantity, or initial source amplitude.
`source` is always present (even on passive/disabled rows), with required
`mode: constant|hold|linear` and ordered `points`. Each point requires
`time: {text,unit}` in seconds-family units and `value_text`. Amplitude uses the
component's explicit value unit; there is no implicit physical dimension.
Constant mode retains points without executing them.

Initial-condition `kind` is `capacitor_voltage|inductor_current`; `value` uses
voltage/current units accordingly and `component` is null or an ID. Operating
point retains disabled rows. Probe records require `name,kind,positive,negative,
component`; kind is `voltage|current`. All inactive references remain stored.
Names never resolve references. Empty/invalid text and unresolved references
are valid draft storage, not evidence of engineering validity.

## Identity, ordering and safety

Transient node/component namespaces are separate from each other and DC/AC.
Array declaration order is meaningful; JSON object-key order is irrelevant.
Counters must exceed declared IDs, preserving existing monotonic semantics.
They need not exceed dangling IDs. Allocation skips *all* retained references,
including inactive probe terminals/components and initial conditions; exhausted
counters never wrap. Deletion never rewinds a counter or repairs references.

Schema 2 preserves the existing global limits: 8 MiB file, 4 MiB JSON text,
32 nesting levels, 200000 values, 128 object members, 10000 items per JSON array,
12000 aggregate known-record rows, 512 KiB JSON strings and 256-byte keys.
Unknown fields consume SAX/preflight budgets too. Transient limits are 32 nodes,
128 components, 64 probes, 128 initial conditions, 1024 points per component,
and 4096 points total, **including inactive/constant/passive configurations**.
Names use 4096 UTF-8 bytes/UTF-16 units. Quantity and amplitude draft text permits
128 UTF-16 units and the existing bounded UTF-8 string capacity. These are storage
limits, not numerical limits: invalid times, huge values and incomplete text save.

No files, URLs, commands, plugins, external assets, results, traces, runtime
objects or execution progress are interpreted or persisted.

## Editor conversion contract

Same-dimension unit edits parse only as an explicit user conversion operation.
Convert a component value and every retained point amplitude together into a
complete temporary value set. Reject invalid, underflowed, overflowed or
unrepresentable conversion without changing any text/unit. Source-point times,
stop/step and initial values use the same checked conversion rule.

Component physical kind is fixed when added. A type change across dimensions is
rejected with an explanation; add a separate component instead. This leaves the
old row, raw text, references and inactive source points intact. Initial-condition
kind is similarly fixed at creation. Probe kind may switch because all three
references are retained independently and have no implicit numeric dimension.

Restoration is inert and notification-suppressed. Widgets edit borrowed draft
state; exact pending table/line text is synchronized before capture. Navigation
is retained without dirtying; persisted data edits dirty normally. Transient execution controls and result tabs are separate transient UI state,
not additional schema fields.

A complete cross-domain schema-2 fixture is [project-v2.openece](../tests/fixtures/project-v2.openece). It deliberately includes incomplete text and inactive/dangling references.
