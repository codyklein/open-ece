# Saving and reopening OpenECE projects

OpenECE saves editable experiments from all four domains in one `.openece`
file. It is a UTF-8 JSON document with a versioned schema, not a snapshot of plots
or a cache of numerical results. New saves use schema 2 and require a schema-2-compatible
OpenECE installation on Fedora or Windows. No Qt installation is needed when using
the complete Windows portable package.

## File actions

| Action | Behavior |
|---|---|
| New | Start a clean, unnamed project with the default editable examples and empty results. |
| Open… | Select a project; OpenECE checks the entire file before replacing the current workspace. |
| Save | Save to the current path atomically; an unnamed project asks for Save As. |
| Save As… | Choose a new destination; the current path changes only after a successful save. |
| Recent Projects | Open one of the last ten successfully opened/saved paths. No project reopens automatically at startup. |
| Close / Exit | Close the application after resolving unsaved edits; execution stops on accepted close. |

Actions use normal Qt platform shortcuts, including New, Open, Save and Close.
Save As appends `.openece` if the selected name does not already end in that
extension, case-insensitively: `lab` becomes `lab.openece`, `lab.txt` becomes
`lab.txt.openece`, and `lab.OPENECE` is unchanged. Replacing an existing Save As
destination requires confirmation against this final filename.

The title is `Untitled — OpenECE` or `lab.openece — OpenECE`. An asterisk immediately
after the name means persisted editable content has changed. The status bar and
window tooltip show the full current path. Domain/tab navigation alone does not mark the project modified or prompt to save.
An explicit Save still captures those selections. Physical inputs, units and
configuration changes do mark it modified. Computing results, progress,
cancellation and automatic navigation to result tabs do not mark it modified.

Before dirty New/Open/Close, choose:

- **Save**: save the current project, then continue. A failed save or cancelled Save
  As stops the original operation and keeps the current project.
- **Discard**: continue without saving the current edits.
- **Cancel**: keep the current project and abandon the requested operation.

Clean projects do not prompt. Opening the current file follows the same rules.
If Save or Save As overwrites a file already staged for Open, OpenECE reads the
saved content again instead of installing stale pre-save bytes.

Recent projects are application preferences stored with QSettings, separately
from project files. Success moves a path to the front; failed/cancelled operations
do not. Missing files remain marked `(missing)` and produce a read error if
selected. Use **Remove from recent projects** to remove a stale entry; this does
not delete its file. OpenECE does not search for moved files. A preference-write
failure does not undo a successful project save.

## What is saved

| Workspace | Editable state retained |
|---|---|
| Signals / DSP | Amplitude, frequency, phase text/unit, sample rate, duration, spectral window, FIR selection/cutoff/taps, selected tab. |
| Combinational logic | Ordered inputs/gates/outputs, stable IDs, names, input toggles, gate kinds, pin references/count text, allocator position. |
| Timing / sequential | Ordered inputs, clock definitions, elements/pins/delays/initial Q, outputs, stimuli, observed-node text, horizon, display unit, allocator and editor tab. |
| DC circuits | Ordered nodes/components, stable IDs, names, terminals, ground, values/units, allocators and selected tab. |
| AC circuits | DC-style editable records plus R/C/L/source phases, frequency, probe/reference selections, transfer/absolute mode, sweep range/count/grid and selected tab. |
| Transient configuration | Nodes/components, retained source points, initialization, ordered probes, units, exact text, references and counters; accepted traces/progress/runtime are excluded. |
| Communications | Modulation, bit source, manual bits (even when disabled), counts, samples/symbol, rate/unit, noise setting, seeds, Eb/N0, BER configuration and selected tab. |
| Workspace navigation | Top-level domain and the meaningful Digital/Circuits and per-domain tab selections. |

Raw pending editor text is saved exactly, including whitespace and Unicode. An
empty value, `1e-`, an invalid phase expression, an incomplete pin list, an odd QPSK
bit string or a deleted-node reference remains editable after reopening. Saving
does not validate whether the experiment can run, parse numbers, convert units,
repair connections or discard incomplete rows. Structural limits still apply:
invalid JSON, duplicate identities, unsupported unit tokens or excessive file size
are project-format errors rather than engineering-draft errors.

## What is not saved

No samples, FIR outputs/coefficients, FFT/spectrum results, logic evaluation or
truth tables, timing events/traces/runtime state, DC/AC solutions, sweep points or
progress, link waveforms/decisions, BER counts/progress, plot samples, zoom caches
or in-progress execution are stored. Seed text and experiment configuration are
saved; random-engine/cached-Gaussian state is not.

**Open and New leave results empty.** Loading never generates DSP output, evaluates
logic, runs timing, solves DC/AC, starts an AC sweep, simulates a communications
link or resumes BER. Choose the workspace's Generate, Evaluate, Solve, Simulate,
Run or truth-table command explicitly to recompute results. Invalid drafts must
be corrected before their engineering operation can run. The initial application
launch retains its existing generated Signals/DSP example; that is separate from
restoring a file or using New.

## Compatibility, errors and file safety

OpenECE supports schema versions **1 and 2**, identified by
`format: "org.openece.project"` and integer `schema_version`. New saves use 2;
schema-1 imports retain their existing editable state and add an empty inert
Transient draft. Saving over an imported schema-1 original warns that OpenECE
1.0.x cannot reopen the newer format and offers Save As. Failed/cancelled saves
keep the original path and provenance. Application version and schema version
are different. Other versions are rejected; there is no general migration
framework. Unknown optional fields are
accepted but ignored, and a warning explains that saving discards them. This is
not lossless forward compatibility. Fields essential to future interpretation
must use an appropriate schema-version change.

Open stages and verifies a complete replacement workspace before installing it.
Read, parse, validation or restoration failure leaves the active project/path and
its dirty state intact. Messages include the filename, field or byte offset and
filesystem detail when available. Correct the file or choose a compatible version
instead of removing required fields to bypass validation.

Save uses QSaveFile with direct-write fallback disabled. It writes a temporary
file beside the destination and commits an atomic replacement; it never deletes
or truncates the old project first. Open/write/commit failure preserves the previous
destination and path and does not clear unsaved edits. Ensure the destination
directory exists and is writable and that enough disk space is available. This
is atomic replacement under Qt/filesystem semantics, not a universal guarantee
against power loss, hardware failure or another process editing the same file.
Keep backups of important projects; OpenECE has no autosave or crash recovery.

Files are limited to 8 MiB plus smaller limits for strings, rows, nesting and
aggregate content. Unknown fields consume the same budgets. No project property
loads an external asset, URL, script or plugin, or executes commands. Text that
resembles a path or command remains inert text. See the complete
[schema-1 specification](project-format.md), [schema-2 extension](transient-project-format.md) and [transaction/ownership contract](project-transactions.md).
A complete deliberately incomplete cross-domain example is
[`tests/fixtures/project-v1.openece`](../tests/fixtures/project-v1.openece).

## Current limitations

Schemas 1 and 2 only; no arbitrary-version migration, undo/redo, autosave/recovery,
cloud sync, collaboration, external assets, embedded results or plugin state.
Incidental UI state such as geometry, selection highlights and runtime progress
remains in memory; recent-project history is the explicit QSettings exception.
There is no cross-process conflict detection, file watching or moved-file search.
Windows CI validates MSVC 2022/Qt 6.8.3 and the packaged runtime on hosted Windows
Server 2022. Checkpoint-4 physical Fedora and Windows 11 retests passed, including
Windows Signals layout at 150%. True mixed-DPI and native screen-reader behavior
remain unvalidated. The exact 1.0.0 ZIP passed its final Windows 11 RC smoke
and is published; the branding patch requires its own bounded candidate checks.
Windows 10 and MinGW may work but are not validated or claimed
supported for v1.0. Published v1.0.1 Windows packages are signed; development artifacts may be unsigned. There is no installer/updater.
See the [physical validation record](physical-validation.md).

## v1.0 compatibility promise

OpenECE v1.0 supports schema-version-1 .openece project files produced by v0.9 and preserves their supported editable project state.

This does not promise a stable C++ ABI, plugin ABI, arbitrary future-schema
compatibility, automatic migration of unsupported schemas, preservation of unknown
optional fields on re-save, or persisted numerical results. Unknown-field warnings
remain meaningful. [Release-pinned fixtures](../tests/fixtures/v0.9/README.md)
verify complete and incomplete v0.9 drafts; [examples](../examples/README.md) are
editable teaching projects. The 1.0 release is published; this compatibility
promise and schema semantics also apply to the 1.0.1 branding patch.
