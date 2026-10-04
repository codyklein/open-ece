# v1.0 checkpoint 4: physical validation

Checkpoint 4 is **open**. This record does not claim physical Windows success or
complete native keyboard/accessibility validation. Do not freeze or release v1.0
on the strength of hosted/offscreen tests alone.

## Initial evidence (2026-09-28)

The source baseline was checkpoint 3 commit
`c932f83d456fef6c69859737e6ec474ab0dfe372`. Native probes below used that source
plus the two warning corrections described in the [stabilization record](v1-release-readiness.md).
The checkpoint commit identifies those corrections. No engineering/schema change
was made. Rebuild and repeat affected checks after any subsequent code change.

| Environment/check | Evidence and result |
| --- | --- |
| Physical Windows 11 | Initially unavailable; see the 2026-09-30 physical report below. A Signals stale-result blocker was found. |
| Physical Windows 10 | No installation available. Decision: may work, but **not validated or claimed supported for v1.0**. Windows Server CI is not substitute evidence. |
| Native Fedora | Fedora 44 KDE Plasma Desktop, Plasma 6.7.5, Wayland, x86_64; Acer Predator PH315-55; Qt 6.11.2, GCC 16.2.1. `loginctl` subsequently reported the active session locked (`LockedHint=yes`); this was not an interactive human desktop pass. |
| Fedora display | One connected eDP-1 display, 2560×1440 at 240 Hz, desktop scale 125%, logical 2048×1152. Desktop settings were not changed. |
| Native example workflows | `openece_example_gui_tests examples` using `QT_QPA_PLATFORM=wayland`: all nine example rows passed (11 including setup/cleanup). Includes explicit analytical calculations after inert opening, editable-state checks and screenshots. |
| Native persistence | `openece_packaged_persistence` with the build-tree runtime and shipped examples: passed Unicode/space paths, invalid drafts, inert reopen, overwrite, isolated recent preferences, normal close and all nine example round trips. Uses scripted file choosers, not manual native dialogs. |
| Protected Fedora destination | `openece_project_document_tests readOnlyDirectoryWhenEnforced`: passed, not skipped. Atomic save rejected a read-only directory and preserved existing destination bytes. |
| Native keyboard/layout probe | Five test cases failed: focus/shortcut editing, modal observation, and two resize cases. Adding bounded exposure/activation/resize waits did not resolve them. The compositor did not report activation and the test window stayed at 900×650 after requesting 800×600. The temporary harness changes were removed; assertions remain unchanged. This does not establish an application defect or a native keyboard pass. Manual reproduction is required. |
| True mixed-DPI | Not validated; one connected Fedora display and one active Windows display. |
| Native screen reader / enlarged system text | Not validated. |

The native probes are automated Qt interactions on the real desktop. They do not
establish human keyboard-only usability, native chooser behavior, screen-reader
usability or physical Windows behavior. Offscreen 100/125/150/200% and 18-point
text tests remain automated coverage, not physical display-scale evidence.
Local probe logs/screenshots use `build/v1-cp4-native-*`; these ignored artifacts
are not portable evidence unless copied with the report. The diagnostic failures
remain recorded rather than being counted as passes. The locked session is a
relevant environment limitation, not proof of an application focus defect; repeat
the keyboard/resize checks in an unlocked interactive session. No attempt was
made to unlock or change the user’s session.

## Physical Windows report: 2026-09-30

User-reported environment: Windows 11 Pro 25H2, build 26200.9457, AMD64,
2560×1440 at 150%, one active display. Package from commit
`17f58597e2461cc40320cd7b0dce08ef4692a876`:
`OpenECE-v0.9.0-windows-x86_64.zip`, SHA-256
`4c2c23d875a2afe2a5a828d2245c37327ae4a77acc3653413d495f493a4d0347`.
Extraction/launch path:
`C:\Users\Cody\Desktop\OpenECE physical π test\OpenECE-v0.9.0-windows-x86_64`.
Account privileges and installed development tools were not reported; no broader
workflow/scaling/accessibility pass is inferred from this reproduction.

**Blocker found:** open `examples/sine-fft-fir.openece`, Generate at 20 Hz with
40 Hz FIR cutoff / 127 taps, then scroll the mouse wheel upward over Frequency to
25 Hz. Old plots remain visible, but the status incorrectly says they match the
current parameters. Generate again correctly recomputes at 25 Hz. The tester
clarified that the change used the mouse wheel, not typed replacement digits.

Local Qt-event tests reproduce this with five wheel events and also five keyboard
Up presses. The visible field becomes `25.0000 Hz`. Qt's accepted stepping path
updates the inner editor with its signals blocked, then emits the spin box's own
`textChanged` signal. Persistence already observes both; Signals result invalidation
observed only the inner editor. The same gap affects amplitude, sample rate,
duration, FIR cutoff and taps. Typed valid/incomplete text, phase and the three
selectors already invalidate correctly.

The narrow fix observes both text signals for the six spin-box fields, leaving
raw-text persistence, numerical parsing and dirty-state bookkeeping unchanged.
Regression cases cover the exact 20→25 Hz wheel/keyboard reproduction, accepted
steps for all six fields, typed valid/invalid text for all seven fields, and
phase-unit/window/filter choices. They verify unchanged retained plot samples
until Generate, stale status, exact draft round trips, inert restoration, and
no extra document revision from calculation. Before the fix, fourteen stepping
cases failed; all pass with the fix. Existing empty-buffer regressions remain.

**Physical retest pending:** use the new candidate identified in the fix report,
not the above defective ZIP. Repeat the exact wheel reproduction and keyboard
Up/Down, then Generate; check stale immediately after editing and current only
after generation. Repeat representative amplitude/rate/duration/cutoff/taps and
phase/selector edits. Record the new hash and results here before closing this
blocker. Other checkpoint-4 manual checks remain open.

## Follow-up physical findings received 2026-10-04

The tester reports completion of the Windows 11 and native Fedora pass and these
four findings. The exact artifact hash, Fedora environment and enlarged-text
setting for this follow-up were not supplied; the earlier Windows reproduction
has its own identified hash above. Do not silently transfer earlier physical
evidence to the new code/package. Targeted retests are pending after review.

| Issue / priority | Physical observation | Contained correction / evidence | Retest status |
| --- | --- | --- | --- |
| [#14](https://github.com/codyklein/open-ece/issues/14), blocker | Tab/Shift+Tab focus and actions work on Windows/Fedora, but retained selection highlights disagree with active focus in Signals, Digital and Timing. | A contrasting Qt focus frame tracks the actual focused control. Existing selections, focus order and input behavior remain intact. Tests follow focus through Tab/Backtab, domains, tables and timing actions; inspect the current frame target and teardown. | Pending Windows/Fedora visual/keyboard retest. |
| [#15](https://github.com/codyklein/open-ece/issues/15), blocker | Switching workspace alone marks a clean project dirty and prompts on close. | Domain/tab choices still update the authoritative snapshot and are captured on explicit Save. Navigation emits no project-data edit notification. Tests cover all saved tabs, no revision/prompt, explicit save/reopen, real raw/unit/configuration edits and result-only actions. | Pending Windows/Fedora workflow retest. |
| [#16](https://github.com/codyklein/open-ece/issues/16), minor | BER upper-bound legend entry and adjacent labels shift during computation; result columns resize. Final results are stable. | Retain the four BER curves/legend entries across updates. Reserve bounded result-column widths once per experiment, retaining manual resizing. Tests cross pending, zero-error, partial and complete states, cancellation/resume, legend-widget identity and widths. Exact counts/confidence semantics stay unchanged. | Pending live-computation visual retest. |
| [#17](https://github.com/codyklein/open-ece/issues/17), minor | Fedora enlarged text crowds/clips Signals Sample rate, Spectral window and Degrees; controls/scrolling remain usable. | Remove fixed form width caps, wrap long rows, size phase controls from their contents. 18/24-point test reproduced selector clipping before the fix and passes after it. Existing four scales and small-window checks remain. | Pending Fedora enlarged-text retest and short Windows layout smoke. |

These corrections introduce no engineering feature or schema change. The latest
user instruction supersedes the earlier checkpoint requirement that user
navigation itself mark the project dirty. Navigation choices are still saved;
physical input, units, configuration, IDs and pending text remain dirty-tracked.
All four issues remain open until targeted retests have been reviewed.

### Bounded retest for the new candidate

Identify the **new inner ZIP hash** from the final fix report, extract freshly,
and record OS/build, scaling/text setting, commit/hash and pass/fail per row.
Review changes before starting the retest. The previous defective/stabilization
hashes do not validate these corrections. This is an affected-behavior pass plus
a short regression smoke, not a repeat of the entire physical checklist.

Windows 11 and Fedora:

1. **Focus (#14):** in Signals, Combinational and Timing, use Tab and Shift+Tab,
   arrows and Space/Enter. Confirm the contrasting outline follows actual focus,
   remains visible on light/dark backgrounds, and differs from retained selections.
   Enter/leave a table editor with F2/Enter/Escape, switch tabs and scroll a focused
   control into view. Verify no stuck outline or focus trap. Open a normal File
   dialog and return; the workspace cue must resume on the actual focused control.
2. **Navigation (#15):** open a saved clean project, switch all top-level domains
   and representative domain/editor/result tabs. Verify no asterisk and no save
   prompt on close. Explicitly Save navigation, reopen and check those selections.
   Change a physical input/raw invalid text or unit/configuration, verify the
   modified marker and Save/Discard/Cancel. Computation must not add a data edit.
3. **BER (#16):** run BPSK/QPSK with a budget long enough to observe updates. Check
   legend entries do not appear/disappear or shift, columns stay fixed, and long
   text is available through horizontal scrolling. Cancel/resume/Step; inspect
   exact errors/bits, partial/complete labels and zero-error upper-bound entries.
   Resize a column manually and confirm updates retain the chosen width.
4. **Short regression:** open examples inertly; Generate Signals and wheel-edit
   20→25 Hz (immediate stale, then current after Generate), evaluate half-adder,
   Step Timing, solve DC divider and RC corner, run a noiseless communications
   link. Save/reopen one Unicode/space path, confirm preserved draft and empty
   derived results, and exit normally.

Fedora enlarged text (#17): use the previously problematic desktop-text setting,
maximize and also shrink the window. Inspect Sample rate, Spectral window, phase
Degrees/Radians and π, cutoff/taps, Generate/status and scrolling. Required
controls must be readable and reachable. Windows: inspect the same form at the
previous 150% scaling and one enlarged setting if available. Some horizontal
workspace scrolling is expected with large text; this is not a responsive redesign.

True mixed-DPI and screen-reader validation remain explicitly unvalidated if
hardware/tools are unavailable. Do not infer those passes from focus tests.

## Identifying the Windows candidate

Use the normal branch CI `Build, test and package` run for the checkpoint commit.
Download the artifact named `OpenECE-v0.9.0-windows-x86_64`; the **inner ZIP** is
`OpenECE-v0.9.0-windows-x86_64.zip`. Metadata remains 0.9.0 during stabilization;
this is not a v1.0 release. The checkpoint report supplies the source commit, CI
URL, exact inner-ZIP SHA-256, file count and manifest verification. Retain that
receipt alongside the test record below. Do not reuse checkpoint-3 hashes for a
build containing the warning corrections.

On the physical machine, before extraction:

```powershell
Get-FileHash .\OpenECE-v0.9.0-windows-x86_64.zip -Algorithm SHA256
Expand-Archive .\OpenECE-v0.9.0-windows-x86_64.zip -DestinationPath "$env:USERPROFILE\OpenECE test π"
```

Compare against the receipt, extract into a fresh directory, and keep the entire
application folder intact. Verify `SHA256SUMS.txt` against every listed file and
record any discrepancy. The package must contain Release `openece.exe`, Qt Core/
Gui/Widgets, the Release Qwt DLL, app-local CRT, platform plugins, `qt.conf`,
licenses/notices, docs and nine ordinary examples. No SDK/development PATH may
be needed. The hosted package probe checks this on a fresh Windows Server runner;
physical testing must still launch normally from the extracted folder.

## Physical test record template

Record each check as pass, fail or not tested. Keep failures and retests; never
replace evidence from an older artifact with an assumption about a newer build.

- Date and tester:
- Source commit / CI run:
- ZIP filename / SHA-256 / manifest result:
- OS edition, version/build and x64 architecture:
- Hardware, monitors, resolution, refresh and scaling:
- Normal non-administrator account / development tools installed (if any):
- Extraction path / launch environment:
- Test, result, exact reproduction, screenshot/log when useful:
- Severity and whether it blocks a primary workflow:
- Fix commit / new artifact hash / affected retest:
- Outstanding limitations:

## All-domain smoke checklist

Use the shipped files and [their expected results](../examples/README.md), not
special-case copies. Opening must leave derived results empty until an explicit
action. Save work before opening another example or deliberately choose Discard.

- Launch from the extracted folder, Help → About (actual version, MIT, notices),
  navigate all domains, exit and relaunch.
- Sine/FFT/FIR: Generate; check 20 Hz tone and 40 Hz low-pass, visible 61.5234375 ms
  delay. Edit an input and check the retained result is explicitly stale.
- Half-adder: evaluate input combinations and truth table (00→00, 01→10, 10→10,
  11→01 for Sum/Carry).
- DFF: Run/Pause/Step/Reset; verify delayed Q rises at 6 ns and falls at 16 ns.
- DC divider: Solve; verify supply +10 V, midpoint +5 V, source current −5 mA.
- RC: solve at the documented corner across the capacitor; verify −3.0103 dB and
  −45°. Run a sweep, cancel and inspect retained partial/cancelled state.
- Series RLC: solve at resonance; verify output across R, unit gain/zero phase
  and source current −10 mA under the documented conventions.
- BPSK and QPSK: run the noiseless link, check recovered bits; Step/Run/Cancel/
  Resume BER. Inspect integer errors/bits and theory, including the 95% upper
  bound for zero observed errors rather than a fabricated BER floor.
- Incomplete project: open inertly, inspect raw `1e-`, missing references and odd
  bits; repair an issue and verify the relevant normal workflow can run.

## File and failure checklist

Use a disposable directory and copies of projects; do not risk personal files.

- Save As under a path with spaces and Unicode, omitting the extension; verify
  `.openece` is appended. Close/reopen, check exact draft values and empty results.
- Modify, Save over the existing file, reopen and check the edit. Save As to a
  second path; confirm overwrite prompts where appropriate.
- Reopen the current file while dirty, exercising Save/Discard/Cancel, including
  saving to the very file selected for Open. Confirm no stale pre-save draft wins.
- Exercise New, cancelled file choosers and dirty Close with Save/Discard/Cancel.
  Failed Save and cancelled Save As must veto replacement/close.
- Open a recent project; rename a disposable recent file outside the app and try
  reopening it. Confirm an actionable error and unchanged active document.
- Try a destination the account cannot write (without changing system ACLs), and
  a missing/malformed project. Confirm existing destination bytes, active draft,
  path and dirty state are preserved as applicable. Record the actual OS error.
- Recent history is an application preference and must not appear in `.openece`.

## Keyboard, scaling and accessibility checklist

For each domain, use Tab/Shift+Tab, arrows, F2, Enter, Escape, Space and normal
File shortcuts. Reach/edit forms and table cells, leave editors, run/evaluate/
solve, pause or cancel as appropriate, recover from invalid input, change domains
and tabs, and complete File/Recent/Save-Discard-Cancel/native chooser workflows.
Record focus visibility, traps, lost focus, label relationships and inaccessible
status/results. No pixel-only interpretation should be needed for critical state.

On physical Windows test 100%, 125%, 150%, 200%; record any unavailable factor.
At each inspect navigation, tables, timing/AC/BER action rows, forms, dialogs,
long status/help, plots, smaller windows and scrolling after results. Test
Windows enlarged text separately. With two differently scaled monitors, move and
resize the running app, reopen delegates/native dialogs and inspect Qwt/text.
Without that hardware, record true mixed-DPI as not validated.

Use Narrator if available for a bounded navigation/primary controls/File/dialog
smoke check; record passed bounded check, specific issues or not validated.
This is practical review, not formal accessibility certification.

On native Fedora repeat representative domain and file workflows, native
choosers, keyboard navigation, normal/enlarged text and smaller-window resizing.
The automated Wayland focus/resize failures above particularly need manual review.
Do not infer that passing offscreen tests resolve those observations.

## Completion boundary

Remaining release gates include physical Windows 11, practical native keyboard/
chooser/scaling checks, and review of the native probe failures. Mixed-DPI and
screen-reader gaps must remain explicit if unavailable. Narrow, demonstrated
fixes require new artifact identification and affected retests. Finish with the
full Fedora/Windows/package matrix; only then seek checkpoint-4 review. Do not
start checkpoint 5, tag or publish v1.0 while this checkpoint is open.
