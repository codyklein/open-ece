# Transient execution milestone acceptance

Development branch: `feature/v1.1.0`, starting at
`b027a1416fb284e79640a5fbf9ea16e9d00c64e1`. This is an engineering checkpoint,
not release preparation. Stable metadata remains 1.0.1. Published releases and
signing infrastructure are unchanged; normal unsigned CI runtime staging is not
an approved signed release candidate. No tag, release or signing is performed.

## Automated evidence

The complete suite now has 262 desktop and 227 headless CTest entries. Numerical
transient tests already cover independent branch-system reference solves and all
RLC damping regimes; they remain unchanged. New coverage is concentrated in:

- `tests/transient_execution_gui_test.cpp`: analytical RC charge/discharge, RL,
  under/critical/overdamped RLC, active/inactive conversion, failed-request baseline,
  duplicate timestamp sides, frozen typed references and mixed probe mapping,
  exact keyboard/delegate edits, Run/Step equivalence, rapid Resume/Pause,
  Cancel/prefix retention, both failure stages, active replacement/close,
  bounded memory and GUI heartbeat workloads, complete trace and plot agreement.
- `tests/transient_gui_test.cpp`: the existing raw draft, allocator, provenance,
  atomic conversion, file-failure and inert-restoration tests remain enabled.
- `tests/example_codec_test.cpp` / `tests/example_gui_test.cpp`: all four new
  ordinary schema-2 files decode/round-trip and execute against documented results.
- `tests/packaged_persistence.cpp`: all thirteen shipped examples restore inertly,
  round-trip semantically and the four transient examples execute explicitly under
  the extracted runtime, without Qt/build development paths.

Run GCC/Clang desktop/headless, Clang ASan/UBSan desktop/headless and Windows MSVC
Debug/Release; the fresh Windows package job checks startup, manifests, examples
and persistence. Final run URLs and outcomes accompany the milestone report.
Benchmark and workflow logs are uploaded by CI; Windows benchmark files are
`transient-benchmark-{Debug,Release}.log` in `windows-build-test-logs`, alongside
`tests/transient-execution-{Debug,Release}.log`. See [measured architecture](transient-execution.md).

Pinned v0.9 compatibility fixture SHA-256 values remain:

- complete: `0ba53c8e653ebbbc8720960743b40c3781ec717bbcaa3c718dfe1599a0e0ad0f`
- incomplete: `0b9b4580268de760fb4b27901fabeb3655ee94af2a4885fc13466e8e7ecc3ac2`

## Short physical Fedora / Windows acceptance

Record OS/scaling, commit/build identity and runtime used. Hosted/offscreen test
success does not claim physical Windows/DPI validation. On Fedora use the branch
build. On Windows use a development build in the supported MSVC/Qt environment,
or a separately approved future signed candidate; signing is not authorized by
this checkpoint. Do not disable Smart App Control to turn an unsigned development
ZIP into the release solution.

1. Open `examples/transient-rc-step.openece`. Verify empty plots/table and Ready;
   no computation should start. Step: exactly one integration interval is added
   and state becomes Paused. The interval can be smaller than the configured
   maximum because the core subdivides endpoint spans evenly. Reset results:
   empty results and unchanged editable configuration.
2. Run RC: ~9.93 V capacitor voltage at 5 ms; source current negative and capacitor
   current positive. Inspect the mixed-order numerical table. With Operating point,
   rerun: starts at 10 V. Restore Specified storage (zero initial V_C), rerun, then
   edit step/value text to `1e-` without committing the delegate: retained results
   immediately become Stale and cannot resume. Save/reopen the invalid text.
3. Open `transient-rl-response`: Run, check ~9.93 mA at 500 µs, negative source
   current, instantaneous amp/volt labels. Open `transient-rlc-damping`: Run, check
   damped capacitor oscillation and first peak ~1.73 V. R=200/400 Ω should remove
   oscillation; smaller steps reduce backward-Euler damping/error.
4. Open `transient-source-breakpoints`: Run. In the table verify consecutive
   before/after rows at exactly 1 and 3 ms, with continuous capacitor voltage and
   jumping source voltage. Select each voltage/current plot and confirm orientation
   and labels. Linear mode should give a continuous source ramp at the same knots.
5. Step several intervals, Resume/Pause or Cancel while a sufficiently long bounded
   experiment is still active. Pause remains resumable; Cancel retains an accepted
   prefix and is terminal. If a small example completes before a click, use Step
   for deterministic paused inspection. Reset/New/Open/close must stop owned work
   safely. Editing probe names/references must leave old trace metadata unchanged
   and clearly stale. Results and controls alone must not add the dirty marker.
6. Save As in a Unicode/space path, reopen and verify raw inputs/units/probe order
   and empty derived results. Open a pinned schema-1 fixture: the Transient draft
   must remain empty/inert. Run one pre-existing Signals/Digital/DC/AC/communications
   workflow as a brief regression smoke, then exit normally.

Remaining limitations: fixed-step first-order BE, dense solving and numerical/core
limits unchanged; no interruption inside an atomic solve; full-prefix snapshots
at bounded running cadence; table-model resets during publication; one selected
voltage/current plot at a time, with all probes in the complete numerical table.
No adaptive integration, nonlinear devices or unrelated capability is introduced.

## Transient GUI polish physical review

This follow-up changes presentation only: execution controls precede the editor,
a vertical splitter shares space between editor/results, and circuit tables show
only a bounded number of rows initially (four nodes, six other editor rows).
Additional rows remain available by scrolling. Component labels are human-readable;
the original schema tokens, IDs, numerical trace and execution semantics are unchanged.
Splitter position and column sizes are transient UI state, not project-file fields.

Use the final polish commit identified in the accompanying CI report. On Fedora
use the branch build; Windows review still requires a development build or a
separately approved signed candidate. This milestone does not authorize signing
or stable-version changes.

1. At a maximized 1920×1080 desktop with normal fonts, open `transient-rc-step`.
   Confirm the three-row node/component tables are compact, Run/Step/Cancel/Reset
   are above the editor, and the results area is visible. Drag the vertical splitter
   toward the editor to give the plot/table more space; restore the editor afterward.
   Keep the mouse held and confirm both panels and the plot resize continuously.
   Drag to both extremes: neither panel may disappear. Use the editor's local
   scrollbar to reach its bottom controls when it is compact. Verify the grip
   remains visible/easy to grab in the active desktop theme.
   Repeat at a smaller window and with enlarged text/125%, 150%, and 200% scaling.
   Required controls must remain reachable using workspace scrolling.
2. Inspect the compact **Component** selector beside **Add component**. Verify
   Resistor, Capacitor, Inductor, Voltage Source and Current Source labels. Add a
   temporary component; its kind, units and ID must survive Save/reopen. Remove it
   afterward. Retained missing references and invalid raw text must not be repaired.
3. Add enough temporary rows to exceed the initial table heights. Scroll to the
   final row and edit it. Resize name/reference columns; horizontally scroll the
   component table at smaller widths. Hover a truncated name/reference to see its
   complete text. Verify long Unicode names remain readable and savable.
4. Use Tab/Shift+Tab between the component selector and Add component, then reach
   the execution commands and table editors. Focus must remain visible; F2 editing
   and leaving a delegate must work without trapping focus or losing pending text.
5. Step RC once, inspect **Numerical trace**, and check the mixed current/voltage
   order. Two-line headers show probe names and frozen orientation/units; tooltips
   contain the complete labels. Resize columns and horizontally scroll. Run, pause
   if still active, cancel or reset; editing must still mark retained results stale.
   While paused, drag the splitter repeatedly: sample count/status must stay fixed,
   and Step/Resume must still continue the same accepted trace afterward.
6. Save/reopen in a Unicode/space path. Verify editable state is retained and results
   are empty on load. Resizing the splitter/columns must not add the dirty marker.
   Exit normally. Record platform, scaling, commit and any clipping/focus defects.

Automated coverage in the existing transient editor/execution suites checks four
window/font combinations, bounded row growth and final-row scrolling, kind-label
mapping/round trips, forward/reverse toolbar focus, inert loading, read-only cell
and trace-header tooltips, resizable columns and unchanged execution workflows.
Splitter regressions check geometry/plot repaint before mouse release, both
non-collapsing extremes, editor scrolling, window/font changes and preserved paused
execution/trace/document state. Qt scale-factor runs complement physical review.
Physical display/desktop-style review remains required; offscreen checks alone
are not a claim of mixed-monitor DPI acceptance.
