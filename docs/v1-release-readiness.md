# v1.0 stabilization record

This is a checkpoint record, not a declaration that v1.0 is released. The
engineering APIs and schema-1 project format remain unchanged.

## Checkpoint 1: raw edits and diagnostics

The original keyboard reproductions selected all text in an execution input and
pressed Backspace. No spin-box numeric `valueChanged` signal was emitted. Before
the fix, BER Step reused the existing experiment (seven result rows), an AC sweep
kept its existing 201 rows/session, and Signals did not label its retained plots
stale. All three tests failed before the production fix and pass afterward.

Execution invalidation now observes the raw editor's `textChanged` signal for
Signals amplitude, frequency, sample rate, duration, FIR cutoff/taps; AC sweep
point count; and Communications bit count, samples/symbol, BER points/budget.
This does not parse, normalize, or validate the text while editing. Signals keeps
its previous plots with an explicit stale message. AC/Communications clear old
results and stop/discard execution built from the previous inputs.

The remaining editor paths were checked against their existing raw-edit hooks:

| Workspace | Execution input paths |
| --- | --- |
| Signals/DSP | Phase text; window/filter/unit choices; raw numeric editors |
| Combinational | Active table text; raw pin count; type, pins and input values |
| Timing | Active table delegates, including pin lists; horizon/observed text; element choices |
| DC | Active node/component names; raw value fields; terminal/type/unit/ground choices |
| AC | DC-like component rows plus phase; frequency/start/stop text; probes, transfer source, grid/count |
| Communications | Manual bits, rate, Eb/N0, seeds, sweep bounds; raw counts; modulation/source/noise choices |

`release_correctness_workflow` adds keyboard regressions for the originally
failing paths and the other Signals/Communications raw fields, DC value input,
active timing delegates and digital pin count. It checks that timers stop, stale
execution is not resumed, and failed execution preserves pending text. Existing
persistence, domain and workflow tests remain in place.

Draft changes, dirty bookkeeping and execution invalidation remain separate:

- Model bindings copy exact editor text and report persisted edits.
- The document tracks those persisted edits for dirty state.
- Domain execution-input handlers invalidate their own derived state.
- Computation/result updates do not edit the project or mark it dirty.
- Navigation choices are retained without data-edit notifications or execution
  invalidation; checkpoint 4 supersedes the earlier navigation-dirty policy.
- Restoration remains inert, with editing signals suppressed.

A regression exercises successful calculations across all domains, verifies an
unchanged captured project and clean document, then verifies navigation changes
do not discard a BER experiment. Broader running/paused/partial/cancelled state
presentation belongs to checkpoint 2.

Status/diagnostic labels in every domain and project dialogs use explicit plain
text when they can include user content. Timing trace titles and lane labels now
also use `QwtText::PlainText`. Other plot titles/axis captions are fixed application
text; table/select controls display text directly. Static authored rich help is
still allowed. Tests cover markup-like Unicode names, long bounded diagnostic
content, timing labels and a real project error dialog. This is rendering
correctness hardening, not a claim of a demonstrated security exploit.

The first branch CI run also exposed a use-after-free during destruction of a
focused timing table editor: QWidget focus-loss processing could commit the
editor after the derived view's owned draft had been destroyed. The shared
DraftView destructor now disconnects descendant editor/model signals targeting
the view before QWidget teardown. This leaves normal editing and save-time
synchronization unchanged. Active-delegate destruction regressions cover
combinational, timing, DC and AC views, including no teardown draft edits.
The original timing test reproduced the ASan failure locally before this fix.

## Remaining release gates

Checkpoints 1–4 are approved and committed. Checkpoint 4 closed at
`443088d5e99ee488e1355b94635ceea804601571` following final physical Fedora and
Windows 11 acceptance. Hosted/offscreen tests remain separate evidence. Windows
10 is not physically validated or claimed supported for v1.0; mixed-DPI and
screen-reader gaps remain explicit. Checkpoint 5 prepares the 1.0.0 candidate,
reviews dependencies/notices and validates its exact ZIP. Final physical RC smoke
and approval remain release gates; no tag or publication is authorized yet.
See [physical evidence](physical-validation.md) and [RC steps](release-candidate.md).

## Checkpoint 2: UX, focus and scaling

### Result-state vocabulary

These are presentation states, not schema fields or a new numerical state machine.
Draft edits and document dirty state remain separate from result/runtime state.

| State | Meaning and presentation |
| --- | --- |
| Stale | Retained Signals plots no longer describe the visible inputs; the status explicitly says stale. Other domains clear results on execution-input edits. |
| Running | A timing, AC or BER timer is actively advancing execution. BER now reports running immediately after Run, rather than waiting for its next chunk. |
| Paused | Timing has a resumable session with no active timer, including after Step. |
| Partial | Some work is done but the request is unfinished. BER Step explicitly says partial and offers Run/Step; AC retains accepted/failed points on cancellation. |
| Cancelled | AC leaves unsolved rows marked cancelled; BER retains exact counts and permits Run/Step to resume. |
| Failed | Input validation or execution failed. Synchronous calculations have no accepted result; interrupted AC/BER work is explicitly labelled partial/failed. A failed BER execution requires Clear results or an input edit before restarting. Failure is not labelled user cancellation. |
| Complete | Requested work finished. AC completion still reports per-frequency failure counts and row diagnostics; complete does not imply every point was accepted. |

Timing retains Run/Pause/Step/Reset. AC retains Run/Cancel. BER retains
Run/resume/Step/Cancel/Clear results. Signals keeps Generate and explicit stale
plots. DC/AC single solves, combinational evaluation and link simulation identify
completion or failure in text. Engineering conventions remain distinct: unitless
signal amplitude, DC + to − currents, RMS AC phasors, normalized communications
I/Q and integer-picosecond timing. Tables still expose circuit results, sweep
failures, timing output states, received bits and exact BER/error counts.

### Replacement actions

Built-in DC/AC/Timing example buttons now say **Replace**. Copying combinational
logic is labelled **Replace Timing from logic**. Each replaces substantial draft
content and asks for explicit confirmation, with Cancel as default/Escape and an
explanation that undo is unavailable. Cancel leaves editable state unchanged;
users can save the project first. Invalid combinational sources are rejected
before asking to replace Timing. Tests cover both Cancel and Replace, and the
existing engineering checks after loading examples remain intact.

Timing Reset and BER Clear results affect derived execution only and do not ask
for confirmation. Row Add/Remove and component-type changes remain direct editing
operations; their existing descriptions identify their effect. New/Open/Close
continue using the project controller's Save/Discard/Cancel transaction.

### Concrete defects and bounded fixes

- Before the change, a requested 900×650 workbench expanded to 1022×805. A standard
  QScrollArea now contains the domain stack, so large forms/plots remain reachable
  at smaller sizes. Numerical views and borrowed draft ownership are unchanged.
- The fixed-width sidebar is replaced with Qt content-based sizing, including
  enlarged text. Digital table heights use a font-relative minimum rather than a
  fixed 120 pixels. Table headers size to their text with horizontal scrolling.
- Tab traversal could cycle inside the digital input table instead of reaching
  Evaluate. Tables now use arrows for cell selection, F2 for editing, Enter to
  commit, Escape to abandon the active edit, and Tab/Shift+Tab for control focus.
  Standard Qt focus rendering is retained; there is no custom focus painter.
- Initial focus is the domain list. Existing file-menu shortcuts remain standard
  Qt shortcuts. Additional accessible names identify timing controls, AC frequency
  and probe controls, circuit component IDs/terminals/units, and result tables.
- BER Step no longer claims that an inactive execution is running. Failures and
  cancellation are distinguished. Static AC/Communications help tabs use the same
  “Conventions and help” wording as DC.

### Validation and boundaries

`release_ux_scale_1`, `release_ux_scale_1.25`, `release_ux_scale_1.5` and
`release_ux_scale_2` run the same real Qt key-event workflows at 100%, 125%, 150%
and 200%. They use Tab/arrows/F2/Space/Escape and standard file shortcuts, rather
than invoking action slots for the keyboard checks. Coverage includes all six
workspaces, table editor exit, clocks/stimuli, reverse focus traversal, invalid
input recovery, timing controls, partial AC cancellation, BER cancel/resume,
New/Open/Save/Save As, the Recent Projects menu, and a real Save/Discard/Cancel
message box. File choosers use scripted paths; native platform chooser operation
remains part of physical validation. Replacement-decision tests use actual modal
message boxes. Existing checkpoint-1 teardown and result-only/dirty-state tests
remain enabled.

Layouts are checked at 900×650 and resized to 800×600 with generated results,
including 18-point inherited text. Optional `OPENECE_UX_SCREENSHOTS` output permits
visual review without pixel-exact assertions. Fedora offscreen rendering uses the
installed Qt 6.11.2; Windows CI uses pinned Qt 6.8.3/MSVC Debug and Release. Tests
exercise Qt scaling, not movement between physical monitors. True mixed-monitor
DPI transitions, native file choosers, screen-reader interaction and physical
Windows visual inspection remain checkpoint-4 evidence items. Horizontal scrolling
on dense forms is intentional; a comprehensive responsive redesign is post-1.0
polish, not a new feature in this checkpoint.

## Checkpoint 3 — onboarding, examples and schema-1 compatibility

Current instructions are in README, CONTRIBUTING, `docs/building.md` and the
short first-session guide. Earlier release counts and measurements moved to
`docs/validation-history.md`. Digital/Communications help no longer claims drafts
cannot be saved. Help → About reports the actual CMake version, MIT license,
repository and notice locations. Binary/package metadata stays 0.9.0 until final
candidate freeze; checkpoint 3 does not tag or release v1.0.

Nine ordinary project examples cover sine/FFT/FIR, half-adder, DFF, DC divider,
RC low-pass, series RLC, BPSK, QPSK and intentionally incomplete state. Their guide
states actions, analytical expectations, units and repair steps. Tests decode
and round-trip every file, restore empty/inert results and explicitly check the
stated calculations. Four reviewed current offscreen screenshots complement text;
they do not establish physical-platform certification.

Two immutable compatibility fixtures are pinned to v0.9.0 commit
`62bdd7fe3b5a974983d03f29e125a28147e320f3`: the release's existing incomplete file
and a complete file generated by the tagged model/codec. SHA-256 checks protect
the fixtures. Tests cover IDs, allocators, raw invalid text, Unicode, units,
selections, dangling references, file Save As/reopen and inert restoration. No
schema token, numerical API, algorithm or acceptance tolerance changed.

The Windows ZIP now includes ordinary examples and offline docs. The existing
packaged persistence probe additionally opens all nine shipped examples with the
packaged runtime, verifies empty results and semantic round trips. Numerical
example assertions also run under normal Fedora/Windows test configurations.

Validation adds eleven headless codec cases and ten GUI CTest entries (nine
individual examples, plus two compatibility rows and About). The full suites total **188
headless / 216 desktop**. The new 201-point RLC GUI example needs more than Qt
Test's default five seconds under sanitizers, so its bounded completion wait is
30 seconds; point count and assertions are unchanged. Existing tests are intact.
Branch-tip CI results are recorded in the checkpoint report, not assumed from
these expected counts.

Remaining release gates are unchanged: exact-candidate native Fedora and physical
Windows evidence, mixed-DPI/screen-reader gaps and final RC dependency/license/
package review. Windows 11 hardware exists but is not currently available for testing. The
checkpoint-4 record below supersedes earlier platform-target wording.

The first Windows Debug run exposed test-infrastructure issues: Git's CRLF checkout
conversion changed immutable fixture byte hashes, and the combined nine-example
process exceeded 120 seconds. Fixture-only LF attributes now preserve release
bytes. Each example runs in its own bounded CTest entry, with explicit Qt Test
logs uploaded on both platforms. No example, sweep point or assertion was removed.

## Checkpoint 4 — historical validation and closure

Physical Windows 11 testing has started and found a Signals stale-result blocker;
its narrow fix awaits physical retest, so this checkpoint is not complete. The [evidence record](physical-validation.md) distinguishes native
Fedora automated probes, hosted CI and still-pending manual checks.

Two OpenECE-controlled Clang warnings have narrow corrections: the replacement
confirmation labels its existing Yes button through `QAbstractButton::setText`
instead of deprecated `QMessageBox::setButtonText`, and the Digital-to-Timing
callback explicitly captures `this` under C++20. The replacement regression also
checks the visible button text. No numerical, persistence, schema or dependency
behavior changed. No release-pinned fixture bytes were modified.

The 2026-09-30 physical Windows 11 wheel-edit report exposed a second Signals
invalidation path: accepted spin steps suppress the inner editor signal. Signals
now also observes each spin box's text signal. The [physical record](physical-validation.md)
contains the defective artifact hash, environment, reproduction, root cause and
required retest. The fix changes result invalidation only; schema-1, exact pending
text, engineering algorithms and document dirty-state semantics are unchanged.

The follow-up physical pass reported focus visibility, navigation-only dirty state,
BER live-layout flicker and Fedora Signals enlarged-text clipping. Issues #14–#17
track these independently. Narrow corrections and automated checks are complete;
physical retests on the replacement artifact remain pending. See the
[targeted retest plan](physical-validation.md). The BER plots now retain curves/
legend entries between updates; result columns stay user-resizable with stable
initial widths. Signals forms use content sizing and wrapping. Navigation choices
remain schema-1 content but navigation alone no longer triggers Save prompts.
Checkpoint 4 remains open and checkpoint 5 has not begun.

The Fedora targeted retest passed navigation (#15) and enlarged-text layout
(#17), but rejected the focus treatment (#14) and found missing BER legend keys
and unused table width (#16). Only those two fixes are revised: a wider current
focus ring with inactive numeric-selection clearing, and explicit stable BER
line/marker keys with a stretching last result column. The [physical record](physical-validation.md)
retains the failed observations and specifies the bounded new-candidate retest.
Fedora/Windows physical acceptance of the revised #14/#16 remains open.


The final retests passed on Fedora and Windows 11 using the candidate from
`a3814953002a4d9718debcf8ae22680a75c0f47f`, inner-ZIP SHA-256
`66d8ec56a55377b63b0d75cab288924c1f0222f2a650a2571ea6f8f0f4bb246f`.
Issues #14–#17 are closed; the documentation-only closure commit is
`443088d5e99ee488e1355b94635ceea804601571`. This supersedes the historical
pending/failure statements above without erasing them.

## Checkpoint 5 — 1.0.0 candidate freeze

CMake, CI artifact paths, runtime notes and current guides identify 1.0.0. About
and application version already derive from CMake; the existing About test checks
that configured version. This checkpoint changes release metadata/documentation
only. Algorithms, APIs, solver tolerances, schema-1 tokens, shipped example bytes
and immutable v0.9 fixtures remain unchanged.

[Release notes](release-notes-v1.0.md) summarize the shipped capabilities and
bounded stabilization. [Dependency/license review](release-dependencies.md)
records retained pins, notices and the limited advisory review. CI must pass all
eight jobs and the final ZIP must pass complete manifest and packaged-runtime
verification. Exact commit, CI URL, counts and inner-ZIP hash are recorded in the
candidate report after validation; the hash is not embedded back into the ZIP.

The [physical RC smoke](release-candidate.md) is still pending on that new ZIP.
Passing checkpoint-4 binaries do not establish acceptance of the new artifact.
No tag, merge or GitHub release is created by candidate preparation.
