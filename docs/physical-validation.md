# v1.0 checkpoint 4: physical validation

Checkpoint 4 is **closed** following the final user-reported Fedora and physical
Windows 11 retests recorded below. Earlier failures and pending requests remain
as historical evidence and are superseded only by the identified final retests.
This is not formal accessibility certification or a v1.0 release freeze.
This opening summary records checkpoint-4 closure. Final RC acceptance and
publication subsequently completed; see the final record below. The chronology
retains earlier pending gates rather than treating them as current instructions.

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

## Fedora targeted retest received 2026-10-04

The tester reports the following targeted results. The tested hash, Fedora
version/session and exact enlarged-text setting were not restated in this report;
do not infer those details or Windows results from these observations.

- **#14 FAIL:** Tab/Shift+Tab and keyboard actions work without traps, but
  previously visited numeric fields retain selection highlights and the thin
  focus frame is not sufficiently distinct. This remains a release blocker.
- **#15 PASS:** switching top-level sections leaves the saved project clean,
  without a modified marker or unnecessary exit prompt. No new change requested.
- **#16 FAIL (minor regression):** live flicker improved, but legend keys do not
  visibly identify series and table columns leave a large unused tail.
- **#17 PASS:** maximized and smaller-window Signals layouts no longer clip
  labels; hidden content remains reachable through scrolling. No new change requested.

### Narrow revisions and required retest

Only #14 and #16 are revised. The current-focus frame uses a wider blue ring with
white contrast, above native borders and without the native thin-frame mask.
Inactive numeric editor selection is cleared on focus departure, without changing,
parsing or committing text. Keyboard regressions check the actual frame target,
visible ring, inactive selection and exact valid/empty/incomplete/whitespace draft
preservation without edit notifications, at four scaling factors.

The four BER legend widgets remain stable. Their keys are painted directly from
the series pens/symbols because a local rendering test found fully transparent
Qwt vector legend pixmaps even when icon dimensions were nonzero. Keys identify
blue theory, green complete circles, purple partial diamonds and orange
zero-error upper-bound triangles, including empty series. Tests verify rendered
colors across pending/partial/complete states, entry identity, cancel/resume,
manual column widths and use of wide table viewports. The last table column fills
spare space; count/probability columns do not resize on live value updates.
No numerical, schema, dirty-state or enlarged-text layout change is introduced.

Repeat **#14 and #16 on the newly identified candidate** on Fedora and Windows 11:
Tab/Shift+Tab through several numeric controls and the Digital/Timing editors,
confirm only the current control has the blue/white ring and old numeric
selections no longer masquerade as focus; check mouse activation, editor exit,
scrolling and focus return from a native dialog. During BPSK/QPSK BER execution,
check all four colored line/marker keys remain visible and stationary through
Step/Run/Cancel/Resume and completion, and that wide/narrow tables use available
space while manual widths remain stable. Retain #15/#17 Fedora passes; only a
short navigation/save/reopen/enlarged-text regression smoke is needed if affected.
Previous artifact hashes are not evidence for these revised binaries.
Checkpoint 4 remains open; checkpoint 5 is not authorized.

## Fedora acceptance and traversal follow-up (2026-10-04)

The tester reports that the revised **#14 focus-visibility acceptance passes**:
focus is clearly identifiable, Tab/Shift+Tab work and no trap was observed.
**#16 also passes**: colored/marker keys are visible and remain stable during
Run/Cancel/Resume; columns are stable and use available width appropriately.
These passes supersede the earlier Fedora failures for those acceptance criteria.
The tester did not repeat the artifact hash or OS/scaling details in this update;
Windows acceptance and true mixed-DPI are not inferred from this Fedora report.

A separate Signals keyboard-order defect was reported: Spectral window is visited
before Sample rate and Duration despite appearing below them. Actual Qt keyboard
tests reproduce this for Degrees/Radians and FIR off/on. Widget creation order,
which Qt uses by default, differs from form-row order. Four explicit Qt tab links
now order Phase unit → Sample rate → Duration → Spectral window → Filter.
The accepted focus treatment and BER implementation are unchanged.

Regression tests verify every forward step from Amplitude through Generate and
every reverse step using both Backtab and literal Shift+Tab, in all four mode
combinations and all four automated scale configurations. Disabled Cutoff/Taps
are skipped; with FIR enabled they follow Filter, then Generate. In Radians the
existing enabled π insertion button remains between Phase value and Phase unit;
it is skipped in Degrees. Traversal preserves the exact draft, emits no edit
notification and starts no analysis.

**Targeted new-candidate physical retest:** on Fedora and Windows 11, traverse
Amplitude → Frequency → Phase value → Phase unit → Sample rate → Duration →
Spectral window → Filter, then Generate with filtering off, or Cutoff → Taps →
Generate with FIR enabled. Check the exact reverse using Shift+Tab. In Radians
also check the enabled π button at its normal visual position. Confirm the
accepted current-focus ring remains clear and there is no trap. Only a short
BER/navigation/save-reopen smoke is needed for the unchanged accepted behaviors.
Record the new candidate hash; prior artifact results do not validate the revised
binary. Checkpoint 4 remains open and checkpoint 5 has not begun.

## Final physical retest and checkpoint-4 closure (2026-10-04)

The tester confirms that physical retesting is complete and passes on Fedora
and Windows 11 using the final candidate. Candidate identity:

- Source commit: `a3814953002a4d9718debcf8ae22680a75c0f47f`.
- [Branch-tip CI run](https://github.com/codyklein/open-ece/actions/runs/37257151404):
  all eight required jobs passed.
- [Windows artifact](https://github.com/codyklein/open-ece/actions/runs/37257151404/artifacts/11323756990),
  inner ZIP: `OpenECE-v0.9.0-windows-x86_64.zip`.
- Inner-ZIP SHA-256:
  `66d8ec56a55377b63b0d75cab288924c1f0222f2a650a2571ea6f8f0f4bb246f`.
- Automated package inspection: 68 files, 67 manifest entries; every manifest
  hash and exact file coverage verified. Release Qt/Qwt/CRT runtime, plugins,
  licenses/notices and all nine examples verified. Packaged startup, persistence
  round trip and inert example round trips passed on the fresh Windows runner.

The previously reported physical Windows installation is Windows 11 Pro 25H2,
build 26200.9457, AMD64, 2560×1440, one active display. The final report confirms
the Signals layout at **150%**. Fedora is a native user retest; the exact final
Fedora OS/session, scale/enlarged-text setting and extraction paths were not
restated. Account privileges and installed development tools were not supplied.
Do not infer those details or extra display-scale coverage from the passes.

| Final Fedora check | User-reported result |
| --- | --- |
| #14 focus visibility | **PASS**. |
| #15 navigation-only dirty state | **PASS**. |
| #16 BER legend/table stability | **PASS**. |
| #17 enlarged-text Signals layout | **PASS**. |
| Signals forward/reverse tab order | **PASS**. |

| Final physical Windows 11 check | User-reported result |
| --- | --- |
| Focus visibility and tab order | **PASS**. |
| Navigation-only dirty state | **PASS**. |
| Actual edits still dirty-track | **PASS**. |
| BER Step/Cancel/Resume legend and table stability | **PASS**. |
| Signals layout at 150% | **PASS**. |
| Domain smoke tests | **PASS**. |
| Signals wheel/arrow stale-state regression | **PASS**. |
| Unicode/space-path Save/reopen | **PASS**. |
| Inert project loading | **PASS**. |
| Clean exit | **PASS**. |

These results resolve the Signals stale-result blocker, the tab-order regression
and the physical acceptance criteria for issues #14–#17. They supersede the
earlier failed focus/BER observations and pending affected-behavior retests;
they do not retroactively validate older artifacts. Issues #14–#17 are closed
with the corresponding final evidence. This closure changes documentation only;
the tested executable and candidate hash above remain the physical reference.

Automated evidence for that exact source commit is separate from physical
evidence: Fedora GCC, Clang and Clang ASan/UBSan each passed **216/216 desktop**
and **188/188 headless** tests; Windows MSVC Debug and Release each passed
**216/216**. No compiler-warning or sanitizer findings were identified in the
final CI log. All existing suites remained enabled.

Remaining validation limitations are explicit: physical Windows 10 is
unavailable and not claimed validated/supported for v1.0; MinGW, true mixed-DPI
and native screen-reader behavior remain unvalidated. The final physical report
does not establish Windows 100%, 125% or 200% coverage, full native chooser/
protected-destination coverage, or a formal complete accessibility audit.
Automated scale, file-failure and workflow tests do not substitute for those
manual checks. These limitations must remain visible during checkpoint-5
release-readiness review. No unresolved blocker was reported in the final
affected-behavior retests. Checkpoint 4 is closed. Checkpoint 5 now prepares a
new 1.0.0 ZIP for the [bounded RC smoke](release-candidate.md); the checkpoint-4
hash above remains historical evidence for that tested application.

## Identifying the historical checkpoint-4 Windows candidate

Use the normal branch CI `Build, test and package` run for the checkpoint commit.
Download the artifact named `OpenECE-v0.9.0-windows-x86_64`; the **inner ZIP** is
`OpenECE-v0.9.0-windows-x86_64.zip`. Checkpoint-4 metadata was 0.9.0; the
checkpoint-5 candidate is now 1.0.0 and uses [separate RC steps](release-candidate.md).
The checkpoint report supplies the source commit, CI
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

The final identified physical retests and full Fedora/Windows/package matrix
close checkpoint 4. Historical native probe failures are retained above rather
than rewritten as automated passes; the human focus/traversal reports provide
the corresponding practical evidence. Unreported manual checks and mixed-DPI/
screen-reader gaps remain explicit limitations. Any later application change
requires fresh candidate identification and affected retesting. Checkpoint 5
is authorized for metadata/documentation and exact RC validation; v1.0 must not
be tagged or published before physical RC smoke and release approval.

## Final 1.0.0 acceptance and publication

The user reported final physical Windows 11 acceptance of the exact RC at
`3e688548d3d71a12fdceee173ee780047f23f854`. Inner ZIP SHA-256:
`7883bef22db15027bc2992d64b227175cb34561beaed42f663eaab0c16a30431`.
Fresh startup, About 1.0.0, the shipped Signals example, Save As/close/reopen,
editable-state preservation, inert load and clean exit all passed. That artifact
was published unchanged as v1.0.0. This closes the historical checkpoint-5 gate;
it does not establish Windows 10, mixed-monitor DPI or screen-reader evidence.

The subsequent 1.0.1 branding package needs its own exact hash and bounded
physical review. Use [branding candidate checks](branding.md); no 1.0.1 physical
acceptance is claimed by the historical v1.0 records here.


## Final 1.0.1 acceptance and publication

The user reported acceptance of the exact signed candidate at
`287972ae8b41a088b2c7cd27edfba66f9edbf992` on physical Windows 11.
Inner ZIP SHA-256: `b39814c2217920a2bbb8363f1f36e16d4889416321015b72ead2e1db049e9068`.
Smart App Control remained enabled and allowed launch without warnings. Icons,
About 1.0.1, sine/FIR generation, Unicode-path Save/reopen and inert loading passed.
Additional manual Fedora testing was explicitly waived; automated Fedora validation
passed. No new Windows 10, mixed-monitor or screen-reader evidence is claimed.

All nine accepted-candidate jobs passed in run 37521125062, including signature
verification for all 24 EXE/DLL files and unchanged vendor binaries. PR #19 merged
at `476786917acb988f5bbdafbc31c9f184b12ae20f`; post-merge run 37713805294 passed.
The exact ZIP was published unchanged as v1.0.1 and independently re-downloaded
and hash-verified. This closes the earlier 1.0.1 pending statements without
erasing their chronology. Published 1.0.0 and 1.0.1 artifacts remain immutable.
