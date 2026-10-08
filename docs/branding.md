# OpenECE 1.0.1 — branding and acceptance

This patch integrates the approved Waveform O / Technical cobalt identity.
Artwork is copied unchanged from the frozen source package and verified by hash.
It changes no numerical API, project schema, plotting convention or execution
workflow. The published v1.0.0 tag, release and Windows ZIP remain immutable.
Version 1.0.1 is published. The exact signed Windows ZIP passed physical Windows 11
acceptance with Smart App Control enabled; extra manual Fedora testing was waived.
Automated Fedora tests passed. See the appended [physical evidence](physical-validation.md#final-101-acceptance-and-publication).
Published release tags and assets remain immutable.

## Integration

- Qt application/window icons use the approved transparent PNGs at 16, 20, 24,
  32, 40, 48, 64, 96, 128, 256, 512 and 1024 px. Explicit `Q_INIT_RESOURCE` keeps
  them reachable through the static GUI library. No QtSvg dependency is added.
- Windows embeds the approved ICO as resource 101, with ten frames from 16–256 px.
  CMake supplies file/product version metadata from its single version source.
- About adds a modest icon and retains all plain-text version/license information.
- README uses approved light/dark lockups; screenshots show the current workbench.
- The ZIP contains useful exports and Noto Sans OFL attribution, without fonts.

The [brand guide](../assets/branding/README.md) documents palette, optical artwork,
font provenance, license and export procedure. Normal builds never regenerate
artwork. The primary icon is fixed cobalt; no theme-switching feature is added.

## GitHub presentation — apply manually

Recommended repository description:

> Cross-platform C++20/Qt ECE workbench for signals/DSP, digital logic and timing,
> DC/AC circuit analysis, digital communications, and editable projects.

Recommended topics:

```text
electrical-engineering computer-engineering signal-processing digital-logic
circuit-analysis digital-communications cpp qt6 cmake educational-software
```

Upload [the approved social preview](images/openece-social-preview.png) in
repository settings if desired. It is exactly 1280×640. This integration does
not change GitHub settings. A brief future demo could show example Open → Run →
Save, but no video infrastructure or additional UI is needed for this patch.

## Bounded physical candidate checks

Use the **exact inner ZIP and SHA-256 from the candidate report**, not the outer
GitHub Actions wrapper. Do not rebuild/recompress it after testing. Record OS,
commit, hash, scaling and pass/fail. CI evidence is separate from physical evidence.

### Windows 11

1. Verify the inner ZIP hash; extract freshly into a path with spaces and Unicode,
   such as `OpenECE branding π test`. Launch without SDK/development PATH entries.
2. Check Explorer's executable icon at small/large views, the taskbar and window
   icon. They should show the approved Waveform O, without blurry/missing frames.
   Inspect at the normal 150% scaling used for previous physical validation.
3. Help → About must report **OpenECE 1.0.1**, show the cobalt icon and preserve
   readable MIT/repository/notice text. Dismiss using the keyboard.
4. Open `examples/sine-fft-fir.openece` inertly, then Generate and analyze. The
   20 Hz tone passes the 40 Hz/127-tap FIR. Wheel/arrow-edit frequency: stale
   status and focus/traversal behavior must remain correct.
5. Open the intentionally incomplete example; Save As to a Unicode/space path,
   close/reopen, and verify invalid text is retained and results stay empty.
6. Switch domains; run one short QPSK link/BER Step and check colored legend keys.
   A clean project should exit without an unnecessary save prompt.

### Native Fedora

Build the reported commit with the existing desktop workflow. Check window icon
where the desktop displays it, About 1.0.1 readability, keyboard focus/tab order,
one Signals example, one communications example, and Unicode/space-path project
Save/reopen with inert load. Exit normally. No Linux desktop installation system
is introduced; desktop-shell grouping behavior is not an installed-app claim.

The full previous platform checklist need not be repeated for unchanged engine
code. Windows 10, MinGW, true mixed-monitor DPI and native screen-reader behavior
remain unvalidated. Any demonstrated defect needs a narrow fix, fresh package/hash
and affected retest. These are the retained candidate-check instructions; the
final acceptance record identifies the exact published artifact.

## Automated acceptance

The branding test checks every imported asset hash, all embedded PNG sizes,
static-library linkage, application version/icon and About content. The Windows
package script validates PE file/product versions. The SDK-free runtime probe
checks About and compares every PE icon frame byte-for-byte with the approved ICO,
then runs existing persistence and all nine inert example round trips. The complete
Fedora GCC/Clang desktop/headless, ASan/UBSan and MSVC Debug/Release suites remain
enabled. The manifest covers every packaged file except itself.

## Excluded

No installer, splash, file association, updater, Linux desktop/AppStream system,
UI redesign, new dependency or engineering capability is included. Schema-1
compatibility with supported v0.9 editable state remains unchanged.
