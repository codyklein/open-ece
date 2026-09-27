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
- Persisted navigation can mark a project dirty without invalidating execution.
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

Checkpoint 2 must complete result-state/replacement-operation review and practical
keyboard/scaling validation. Checkpoint 3 must update onboarding and add tested
ordinary examples and release-pinned v0.9 compatibility fixtures. Checkpoint 4
requires recorded native Fedora and physical Windows 10/11 evidence; hosted CI
and offscreen tests do not substitute for physical-platform checks. Checkpoint 5
must review dependencies/notices and validate the exact final candidate package.
No v1.0 support or release claim is made until these gates are resolved.
