# Read-only circuit schematics — 1.2.0 candidate

This integration is on `feature/v1.2.0`. OpenECE 1.1.0 remains the published stable
release. Checkpoint 3's standalone viewer was physically accepted on Fedora 44
and Windows 11 at `78b09b806113c1399d7e295fbe7f47d27f66531c`.
Checkpoint 4 at `c799175b6744b0dbe674d68d68ca4d8c36c5c909` was subsequently accepted
on both platforms. This is development-checkpoint evidence, not physical acceptance
of the new 1.2.0 unsigned candidate or a future signed ZIP. See the
[physical record](physical-validation.md#v120-schematic-development-acceptance).

## Finding the viewer

In **Circuits**, choose DC, AC / Phasors, or Transient. Inside the existing circuit
page, use **Circuit view: Tables / Schematic**:

- DC: **Circuit and results**. Solve DC and its voltage/current result tables stay
  outside the view switch, available with either presentation.
- AC: **Circuit**. Single-frequency results, Frequency sweep and help retain their
  existing tabs and functions.
- Transient: **Circuit**. Sources, Initial conditions, Probes and Conventions remain
  separate. The editor/results splitter, its scroll areas, execution commands,
  progress, numerical traces and voltage/current plots remain in place.

New and reopened workspaces start in **Tables**. The existing table widgets are
retained when switching views; their values, row selections and scroll state are
not reconstructed. A focused table delegate may finish editing when focus leaves
it. Its exact text is synchronized to the authoritative draft before the page is
hidden, without numerical parsing, repair, unit conversion or execution.

## What the schematic means

The diagram represents the **current editable circuit draft**, including node and
component IDs, names, component kinds, terminal references, explicit ground,
raw values and unit tokens. AC retains RMS magnitudes and phase text. Transient
retains source modes and point text, including inactive source settings.

This is not a live-results overlay: DC/AC solutions, sweep results, initial storage
values, probe traces and execution progress do not become electrical properties
on the diagram. The numerical engines and their validation remain separate.

An unsolvable draft can still have a useful schematic. Empty values, `1e-`, missing
ground, dangling references and disconnected nodes are preserved. Open circles
mean unselected terminals; crosses mean missing references. Unknown component
kinds use the accepted generic symbol. Inspectors expose complete original text,
including Unicode and markup-like names as plain text.

Straightforward circuits use structured layouts. More complex or incomplete
circuits may use the explicitly labeled **Connectivity layout**, with distinct
net rails, genuine junctions and gaps at nonconnecting crossings. Fit on a dense
network is an overview; zoom and textual inspection are necessary to follow its
connections. The viewer does not change topology to improve appearance.

## Interaction and refresh

Use Fit to View, Zoom In/Out, the entity list and the read-only details panel.
Tab enters the canvas's six controls in order: **Fit → Zoom In → Zoom Out →
viewport → entities → inspector**. Tab and Shift+Tab also exit to host controls.
The host uses Qt compound-focus handling; the canvas's own focus styling and
navigation remain unchanged. The view selector has an accessible name and label
buddy. Existing workspace focus indicators apply.

In the viewport, wheel scrolling and arrows scroll, Ctrl+wheel and +/- zoom,
F fits, and dragging empty background pans. Select a component or electrical net
to inspect its typed identity and highlight its connections. Selection never
creates a connection. The entity list and plain-text inspector provide a textual
alternative to interpreting the drawing.

The wrapper synchronizes pending editor text when switching, then projects an
owned snapshot. Visible draft-change notifications are coalesced into one queued
refresh. Reopening a previously constructed page checks the current draft again.
An identical projection keeps the existing scene, zoom, pan and selection. A changed
projection installs a freshly verified layout and fits it; a surviving typed
selection is restored. Deleting that entity clears its selection instead of
selecting another row. DC, AC and Transient maintain independent view state.

Only represented circuit fields require a scene rebuild. Frequency, probe or
initial-condition edits whose projection is unchanged do not rebuild it. Timer
progress, focus, mouse movement, zoom and selection do not schedule draft refreshes.
Projection/layout failure clears the previous scene and displays an explicit
plain-text diagnostic. It never presents an old successful diagram as current.

During an AC sweep or Transient run, viewing or navigating a schematic does not
start, pause, cancel, reset or invalidate execution. Frozen execution metadata
and retained results stay separate. Actual draft edits retain the existing
analysis-specific invalidation rules.

At small sizes, use the application's outer scroll and Transient's editor scroll.
Both splitter panels retain their minima. The initial Transient split is applied
once after the first layout, using populated editor dimensions; later navigation
and resize operations preserve the user's splitter choice. Extra circuit-page
padding is reduced to accommodate the selector without reducing table row heights.

## Persistence and ownership

`ProjectSnapshot` remains authoritative. The shared internal
`gui/circuit_presentation.*` widget receives a projection callback borrowing its
owning `DraftView`; it owns only derived presentation data. It creates the accepted
`SchematicCanvas` lazily, after the user selects Schematic, and installs geometry
only when visible. No numerical objects are constructed by this path.

Tables/Schematic selection, scene geometry, zoom, pan, selection and errors are
transient UI state. They are absent from project files and do not emit
`draftEdited()` or dirty the document. Existing persisted analysis/tab choices
retain their established meanings. Real data edits still dirty the document.
Schema 1 import and schema 2 serialization are unchanged; loading stays inert.

Refresh callbacks have QObject lifetime contexts and are cancelled on destruction.
They do not add or control execution timers. Old widgets and scenes are destroyed
during project/session replacement. The developer preview remains independent and
is excluded from the normal Windows ZIP. The workbench links the existing static
renderer and model; there are no new deployment dependencies.

## Validation and physical review

Build and run the production application on Fedora using the existing desktop
preset (not the standalone preview):

```bash
cmake --preset dev
cmake --build --preset dev --target openece -j 4
./build/dev/gui/openece
```

For Windows candidate review, use `OpenECE-v1.2.0-windows-x86_64.zip` from the
candidate report's CI run and verify its inner-ZIP hash. It is unsigned and excludes
the developer preview. It is not the published signed 1.1.0 release. Neither
automated tests nor accepted development checkpoints establish exact-candidate
physical or Smart App Control acceptance. Follow the [candidate checklist](v1.2-candidate.md).

`schematic_workspace_test.cpp` exercises real production views and project
sessions, exact pending text, raw AC/source metadata, solver-result preservation,
sweeps, paused/running/complete/stale transient states, splitter sizing, typed
selection, explicit refresh failures, schema round trips, Unicode paths and
replacement/destruction with queued work. It runs at 100/125/150/200% with Fedora's
offscreen backend and native Windows Qt. Existing numerical, compatibility,
workspace, renderer and packaged-runtime tests remain enabled. The existing
packaged persistence probe additionally opens all three schematic views using only
the extracted application's runtime libraries, verifies their geometry against
the owned drafts, and checks inert execution and clean-state semantics. It remains
a non-shipping test executable. Native Windows Qt supplies the font database for
this probe, matching the fresh-extraction job.

The following development-checkpoint review passed on Fedora 44 and Windows 11.
Repeat the bounded [candidate checklist](v1.2-candidate.md) for the exact new build;
use this longer checklist for any affected retests:

1. Open the divider, RC low-pass and transient RC examples. Switch Tables/Schematic
   in each analysis; inspect source/ground polarity, identities and raw values.
2. Edit names, units, values and terminals, add/remove a node/component, and return
   to Schematic. Confirm correct refresh and distinct missing/unselected references.
3. Retain incomplete text such as `1e-` in an active editor while switching. Return
   to Tables and verify it exactly. Try the incomplete project without solving.
4. On a clean project, switch views, select nets, inspect, fit/zoom/pan and resize.
   Verify no modified marker. Confirm actual table edits still mark the project.
5. Solve DC/AC, then navigate without changing results. Run/cancel an AC sweep
   while using the schematic. Step/Pause/Resume/Cancel a transient run; ensure
   navigation preserves its state and genuine edits mark old results stale.
6. Check keyboard entry and exit, visible focus, smaller windows, enlarged text
   and normal platform scaling. Compress and expand the Transient splitter; both
   panels and their scrolled contents must stay reachable.
7. Save/reopen through a Unicode/space path. Verify identical editable data, default
   Tables views, empty derived results and no automatic execution. New/Open/Close
   while execution is active must retain the established unsaved-change workflow.

Native accessibility and physical DPI acceptance are not inferred from CI.
Known native Windows density-render painter warnings remain unsuppressed and are
tracked separately from integration regressions.

Deferred: graphical editing, drag/rotate/wire commands, persistent positions,
undo/redo, result overlays and schematic export. No project fields or engineering
capabilities are added by this checkpoint.
