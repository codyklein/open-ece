# Development and learning guide

## A small professional workflow

1. Describe one observable behavior and its numerical contract before coding.
2. Add or update domain tests using an analytic result or independent reference.
3. Implement the smallest coherent change in the owning module.
4. Connect it to the GUI only after headless behavior is clear.
5. Run the relevant CTest preset, review warnings, and update mathematical docs.
6. Review `git diff --check` and the staged diff; commit a focused change with a
   message explaining the behavior, not only listing edited files.

Use CMake target properties rather than global include paths or compiler flags.
Public interfaces belong under `include/openece/`; implementation details stay in
`src/`. Headers should include what they use. Avoid Qt types in engineering APIs.
Comments should explain contracts or mathematical reasoning, rather than narrate syntax.

Warnings enabled for GCC/Clang: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`.
They apply to project targets, not system headers. Builds do not use fast-math:
input validation relies on IEEE finite/nonfinite behavior. Do not enable it casually.

Format C++ files using the checked-in style:

```bash
rg --files core signals dsp digital gui tests -g '*.cpp' -g '*.hpp' | xargs clang-format -i
git diff --check
```

Numerical tests use GoogleTest and CTest discovery. The Qt Test workflow checks
initial data, all five signal controls, plot values, invalid input, recovery, and
the desktop sample limit. It also exercises window selection and gain display,
unchanged time samples, degree/radian input, range-endpoint round trips, and
conversion of pending phase expressions before units change, π insertion, invalid
text retention, and recovery. A separate Qt Test suite checks the parser grammar
and range limits. These are functional tests, not a comprehensive
accessibility or visual regression suite. For a local screenshot during that test:

```bash
QT_QPA_PLATFORM=offscreen OPENECE_SCREENSHOT="$PWD/build/dev/workbench.png" ./build/dev/tests/openece_gui_tests
```

The standard sanitizer preset uses Clang and a headless build to keep numerical
checks independent of Qt. A separate GUI sanitizer build exercises the full workflow
as well; third-party system libraries themselves are not rebuilt with instrumentation. Compiler changes
belong in separate build directories. Optional tools such as clang-tidy can consume
the generated `compile_commands.json`; they are not required build dependencies.

The sanitizer test preset makes undefined-behavior reports fail the run. LeakSanitizer
needs normal process-inspection permissions; run it from a normal terminal if a
debugger or sandbox prevents that inspection, rather than disabling leak detection.

## Initial validation environment

The foundation was built on Fedora 44 with GCC 16.2.1, Clang 22.1.8, CMake 4.3.0,
Ninja 1.13.2, Qt 6.11.2, Qwt 6.2.0, and GoogleTest 1.17.0. The desktop suite contains
18 numerical tests plus one Qt workflow test; the headless suite runs the same 18
numerical tests. GCC desktop/headless, Clang headless, and Clang ASan/UBSan runs
passed. The desktop executable was launched, and an offscreen rendering was inspected.

The standard compiler-warning builds and clang-format check were clean. This is a
baseline on one development machine, not a portability or long-term numerical
certification. This historical baseline predates the v0.3.1 CI matrix.

## v0.2 validation

The windowing/phase-unit milestone was verified on the same Fedora toolchain:

| Configuration | Result |
| --- | --- |
| GCC desktop (`dev`) | 28 numerical tests + GUI workflow passed |
| GCC headless (`headless`) | 28 numerical tests passed |
| Clang desktop (`build/clang-desktop`) | 28 numerical tests + GUI workflow passed |
| Clang headless (`build/clang`) | 28 numerical tests passed |
| Clang ASan/UBSan headless (`sanitize`) | 28 numerical tests passed |
| Clang ASan/UBSan desktop (`build/sanitize-gui`) | 28 numerical tests + GUI workflow passed |

The workflow contains five functional Qt test methods (plus Qt's setup/cleanup).
Sanitizer runs retained leak detection and produced no findings. The updated desktop
was launched and its offscreen rendering inspected. No additional packages were needed.

To reproduce the additional desktop configurations:

```bash
cmake -S . -B build/clang-desktop -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build/clang-desktop -j 4
ctest --test-dir build/clang-desktop --output-on-failure

cmake -S . -B build/sanitize-gui -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DOPENECE_ENABLE_SANITIZERS=ON
cmake --build build/sanitize-gui -j 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/sanitize-gui --output-on-failure
```

## v0.2.1 validation

The phase-expression update passed all 30 CTest entries in GCC desktop (`dev`)
and Clang ASan/UBSan desktop (`build/sanitize-gui`): 28 numerical tests, the GUI
workflow, and the phase parser suite. The GUI suite has eight functional methods;
the parser covers 14 pi-expression cases plus decimals, round trips, malformed
input, and range rejection. Deprecated Qt exception assertions were replaced;
the updated targets build without compiler warnings and pass clang-format and
`git diff --check`.

Leak detection remained enabled. The sandbox prevented LeakSanitizer process
inspection, so the sanitizer suite was rerun outside it and passed with no findings.
The offscreen workbench screenshot was inspected for control and plot layout.

## v0.3 validation

The convolution/FIR milestone was implemented and checked in three increments:
convolution with analytical/reference tests (34 numerical tests), FIR/design/
response and signal integration (44 numerical tests), then GUI comparisons and
workflow coverage (46 CTest entries). The final validation matrix is:

| Configuration | Result |
| --- | --- |
| GCC desktop (`dev`) | 44 numerical tests + GUI workflow + phase parser passed |
| GCC headless (`headless`) | 44 numerical tests passed |
| Clang desktop (`build/clang-desktop`) | 44 numerical tests + GUI workflow + phase parser passed |
| Clang headless (`build/clang`) | 44 numerical tests passed |
| Clang ASan/UBSan headless (`sanitize`) | 44 numerical tests passed |
| Clang ASan/UBSan desktop (`build/sanitize-gui`) | 44 numerical tests + GUI workflow + phase parser passed |

The 16 new numerical tests use hand results, an independent output-side
long-double convolution, complex Horner-polynomial and analytical response
references, the existing FFT, and a hand-windowed filtered-signal DFT. The
129-tap, 80 Hz cutoff two-tone regression at fs=1024 Hz verifies 16 Hz passband
gain and 256 Hz attenuation over a fully immersed interval. Response tests cover
P=2 endpoints, arbitrary grids, maximum point count, work rejection, invalid
rates, and overflow. The GUI suite now has eleven functional methods (plus Qt's
setup/cleanup), including causal impulse display, independent spectrum grids,
rate-driven redesign, window independence, finite dB flooring, bypass, limits,
invalid input, and recovery.

GCC/Clang builds and clang-format/diff checks were clean. Leak detection remained
enabled; sanitizer tests ran outside the process-inspection sandbox and reported
no findings. The Qt offscreen platform emits its expected `propagateSizeHints`
warning. Original/filtered and response renderings were visually inspected,
including colored legend labels and full-record axes. No packages were added.
Existing core, sine, FFT, windowing, and phase-parser implementations are unchanged.

To reproduce the FIR screenshots after building `dev`:

```bash
QT_QPA_PLATFORM=offscreen OPENECE_FIR_SCREENSHOT="$PWD/build/dev/fir" ./build/dev/tests/openece_gui_tests firOutputLimitAndScreenshots
```

This writes `fir-signals.png` and `fir-response.png` in the ignored build directory.
CTest logs are also local build artifacts. Numerical tests establish correctness;
the screenshots check presentation only. All configurations use the existing
commands above and in README; no new validation tooling is required.

## Understand these before the next feature

- Trace a button click through `SignalsDspView::generate`, `generate_sine`,
  `amplitude_spectrum`, and the Qwt adapter. Identify every owned buffer and copy.
- Explain how `window_coefficients(Window, L)` stays independent of Qt and why the
  window length is the original L rather than padded N. There is no window-aware FFT.
- Work an eight-point FFT by hand: bit reversal, butterfly stages, and twiddle sign.
- Predict a 1-unit, 8 Hz sine sampled at 1024 Hz for one second: bin 8 has amplitude
  1 with both windows, but periodic Hann adds adjacent main-lobe bins of amplitude 0.5.
- Repeat the coherent/off-bin comparison in `numerics.md`. Explain why Hann can
  reduce distant sidelobes while increasing some bins close to the tone.
- Derive S=sum(w), G=S/L, endpoint scaling, and why amplitude correction is different
  from power/PSD normalization. Explain the limits of correcting off-bin or short records.
- Read `SignalsDspView::change_phase_unit()`: pending text is parsed with the old
  unit and converted with 17 significant digits. Qt signals are blocked during text
  updates and failed selector changes so rollback does not trigger another conversion.
- Find which tests catch a symmetric Hann denominator, gain based on padded N,
  doubled DC/Nyquist, changed source samples, or a degree value reinterpreted as radians.

- Work `[1,2,3] * [0.5,0.5]` by hand, including the first and final samples.
- Explain why 63 taps add 62 samples of record length but only 31 samples of
  linear-phase delay. Find both quantities in the GUI summary.
- Trace `apply_fir_full` into `convolve_full` and then into the unchanged spectrum
  function. Identify which record length sets each normalization denominator.
- Explain why symmetric Hamming design and periodic Hann analysis have different
  formulas and purposes, and why arbitrary FIR taps are not normalized.
- Compare a steady-state two-tone projection with the finite-record spectrum.
  Explain why their amplitudes can differ, and why a dB response floor is only visual.

## Remaining design concerns

Weights add one O(L) temporary allocation per analysis; there is no cache or streaming
buffer reuse. This favors inspectable coefficients and simple ownership for now.
Spectrum metadata remains a public value struct whose consistency callers must preserve.
GUI calculations remain synchronous and capped at 65,536 samples. Phase conversions
have floating-point roundoff; the GUI's previous-unit flag must stay aligned
with its selector. These concerns do not require a generic window or units framework.

FIR/convolution composition was implemented in v0.3; v0.4 adds the independent digital domain.
The generator still produces a single sine; two-tone composition exists only in
numerical regression data. Convolution/response are direct algorithms with explicit
work caps, and SignalsDspView coordinates synchronous local values. Future
performance/state work should follow a measured need.

## v0.3.1 portability validation

See [Windows development and packaging](windows.md) for the pinned toolchain,
bootstrap, presets, deployment and CI matrix. Fedora continues using its system
packages. The pipeline exercises all numerical tests without changing tolerances,
plus the existing Qt workflows and explicit UTF-8, locale, and Unicode-path checks.
MSVC uses `/W4 /utf-8`; Linux warning and sanitizer flags are unchanged. Windows
runtime tests execute on real hosted Windows runners, not cross-compilation.


Validation on 2026-09-15 used Qt 6.8.3 and MSVC 2022 on the hosted Windows Server
2022 x64 runner. [The complete CI run](https://github.com/codyklein/open-ece/actions/runs/34980745722)
passed all eight jobs:

| Configuration | Result |
| --- | --- |
| Fedora 44 GCC desktop / headless | 47 / 45 CTest entries passed |
| Fedora 44 Clang desktop / headless | 47 / 45 CTest entries passed |
| Fedora 44 Clang ASan/UBSan desktop / headless | 47 / 45 CTest entries passed |
| Windows MSVC Debug / Release | 47 / 47 CTest entries passed; no skips |
| Separate Windows packaged-app runner | Native window appeared; packaged Qt/Qwt/CRT modules loaded; normal exit |

The local Fedora GCC/Clang desktop/headless and Clang ASan/UBSan suites also passed,
with leak detection enabled and no sanitizer findings. Formatting and diff checks
passed. The tests add explicit UTF-8 pi, locale independence and Unicode screenshot
paths; no numerical tolerance or engineering implementation changed.

The startup job extracts into `OpenECE fresh π path`, removes development paths
and Qt plugin overrides, launches with an external working directory, and checks
loaded module paths. The downloaded ZIP's SHA-256 file manifest was independently
verified. The package contains Release artifacts, runtime notes and third-party
notices. Windows 10/11 remain the desktop target; no manual test on those operating
systems or on physical Windows hardware is claimed. MinGW remains unvalidated.

Windows CI exposed and resolved Qwt imported-target visibility and GoogleTest
Debug/Release filename collisions. Dependency downloads retain their pinned hashes
and use official mirror fallbacks. Processing the full Qt source archive during
Windows packaging stalled on the runner, so its license notices are generated once
from the verified source archive and checked in as text; normal packaging is offline.
The generator and archive provenance are documented in the Windows guide. App-local
runtime and notice updates must be maintained when dependency versions change.

## v0.4 Digital Logic development

The headless suite includes a separate `openece_digital_tests` binary linked only
to `OpenECE::digital` and GoogleTest. Its 13 tests exhaustively check each gate
through five pins, full-adder results against integer addition, a mux against
selection, tied pins, fan-out, forward references, declaration ordering, snapshot
ownership, exact names, invalid data, cycle diagnostics and inclusive limits.
Truth-table rejection includes a 64-input circuit to catch unsafe exponential shifts.

The existing Signals/DSP workflow suite is retained, with a new domain-persistence
check. `digital_gui_workflow` contains seven functional methods for evaluation,
truth-table rows, draft invalidation, missing-source preservation, cycles, NOT
arity, Unicode/duplicate names, empty drafts, recovery and all GUI count limits.
Qt tables defer deletion of removed cell widgets; tests process those deferred
deletions between button actions, as the normal event loop does. Assertions are
not weakened for Windows. All GUI suites run with the existing offscreen platform.

To inspect the digital view after numerical tests pass:

```bash
QT_QPA_PLATFORM=offscreen OPENECE_DIGITAL_SCREENSHOT="$PWD/build/dev/digital.png" ./build/dev/tests/openece_digital_gui_tests halfAdderEvaluationTruthTableAndDomainPersistence
```

Trace a half-adder draft through validation, source resolution, Kahn's queue and
owned output vectors. Explain why declaration order can differ from evaluation
order, why a blocked gate need not belong to a cycle, and why three-input XNOR is
not all-equal. Read [the contracts](digital-logic.md) before changing the model.
The editable GUI draft and immutable validated circuit are deliberately distinct.

### v0.4 validation results

The [implementation CI run](https://github.com/codyklein/open-ece/actions/runs/35039610043)
passed all eight jobs on September 15, 2026 (local development date):

| Configuration | Result |
| --- | --- |
| Fedora 44 GCC desktop / headless | 61 / 58 CTest entries passed |
| Fedora 44 Clang desktop / headless | 61 / 58 CTest entries passed |
| Fedora 44 Clang ASan/UBSan desktop / headless | 61 / 58 CTest entries passed |
| Windows Server 2022, MSVC 2022, Qt 6.8.3 Debug / Release | 61 / 61 passed; zero failures, skips or disabled tests |
| Fresh Windows runner, packaged startup | Native window opened; packaged Qt/Qwt/CRT modules verified; normal exit |

The local Fedora six-configuration matrix also passed, with leak detection enabled
and no sanitizer findings. GCC/Clang builds, clang-format and diff checks were clean.
The rendered Digital Logic page was inspected with the half-adder and its truth table.
The Signals/DSP extraction was compared mechanically with the original implementation:
only class/base/central-layout changes and an explicit tab object name were introduced;
all numerical processing and existing test assertions remain unchanged.

The Release artifact is `OpenECE-v0.4.0-windows-x86_64`, containing the same-named
portable ZIP. Startup uses the existing fresh Unicode/space-containing extraction
path and sanitized environment, without a Qt SDK or development paths. The existing
runtime dependency, notice and checksum deployment process is unchanged. This is
actual hosted Windows execution, not cross-compilation. Windows 10/11 physical
desktop, high-DPI and accessibility checks remain manual work; MinGW is unvalidated.

Remaining digital technical debt is bounded and explicit: calculations are synchronous,
GUI drafts live in the view, structural edits rebuild small tables, and errors use
exception text rather than a structured diagnostic API. A future editor may warrant
a shared draft model and richer diagnostics. No scheduler or simulation framework
is needed to address the current use case. Saving, undo/redo, schematic editing and
sequential/timing behavior are outside v0.4, not partially implemented features.
