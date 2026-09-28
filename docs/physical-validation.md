# v1.0 checkpoint 4: physical validation

Checkpoint 4 is **open**. This record does not claim physical Windows success or
complete native keyboard/accessibility validation. Do not freeze or release v1.0
on the strength of hosted/offscreen tests alone.

## Evidence as of 2026-09-28

The source baseline was checkpoint 3 commit
`c932f83d456fef6c69859737e6ec474ab0dfe372`. Native probes below used that source
plus the two warning corrections described in the [stabilization record](v1-release-readiness.md).
The checkpoint commit identifies those corrections. No engineering/schema change
was made. Rebuild and repeat affected checks after any subsequent code change.

| Environment/check | Evidence and result |
| --- | --- |
| Physical Windows 11 | User reports the installation is not available for testing now. OS build, displays, package launch and workflows are **pending**. |
| Physical Windows 10 | No installation available. Decision: may work, but **not validated or claimed supported for v1.0**. Windows Server CI is not substitute evidence. |
| Native Fedora | Fedora 44 KDE Plasma Desktop, Plasma 6.7.5, Wayland, x86_64; Acer Predator PH315-55; Qt 6.11.2, GCC 16.2.1. `loginctl` subsequently reported the active session locked (`LockedHint=yes`); this was not an interactive human desktop pass. |
| Fedora display | One connected eDP-1 display, 2560×1440 at 240 Hz, desktop scale 125%, logical 2048×1152. Desktop settings were not changed. |
| Native example workflows | `openece_example_gui_tests examples` using `QT_QPA_PLATFORM=wayland`: all nine example rows passed (11 including setup/cleanup). Includes explicit analytical calculations after inert opening, editable-state checks and screenshots. |
| Native persistence | `openece_packaged_persistence` with the build-tree runtime and shipped examples: passed Unicode/space paths, invalid drafts, inert reopen, overwrite, isolated recent preferences, normal close and all nine example round trips. Uses scripted file choosers, not manual native dialogs. |
| Protected Fedora destination | `openece_project_document_tests readOnlyDirectoryWhenEnforced`: passed, not skipped. Atomic save rejected a read-only directory and preserved existing destination bytes. |
| Native keyboard/layout probe | Five test cases failed: focus/shortcut editing, modal observation, and two resize cases. Adding bounded exposure/activation/resize waits did not resolve them. The compositor did not report activation and the test window stayed at 900×650 after requesting 800×600. The temporary harness changes were removed; assertions remain unchanged. This does not establish an application defect or a native keyboard pass. Manual reproduction is required. |
| True mixed-DPI | Not validated; only one connected Fedora display, Windows monitor configuration unknown. |
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
