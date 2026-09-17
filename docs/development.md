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
rg --files core signals dsp digital circuits gui tests -g '*.cpp' -g '*.hpp' | xargs clang-format -i
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

## v0.5 timing development

`openece_timing_tests` is a separate Qt-independent GoogleTest binary. Its 18 cases
cover validation, initialization, forward references, owned sessions/snapshots,
inertial pulse boundaries, stale events, simultaneous changes, deadlines, inclusive
horizons, work limits, clocks, SR/D latches, rising/falling DFFs and storage feedback.
The independent discrete-tick reference uses separate Boolean calculations and
scans each time tick rather than sharing the production priority queue or tokens.
It checks 30 deterministic circuits with mixed transitions, tied pins, fan-out and
reversed gate declaration order. Existing digital and numerical tests are retained.

`timing_gui_workflow` adds five functional methods: example curve values/display
units/domain persistence, Step/Pause/Reset/edit invalidation, invalid draft/SR error
recovery, validated independent copying with stable IDs, and inertial pulse/GUI
limit behavior. The existing GUI tests and their assertions are unchanged. All
Qt workflow tests use the same offscreen platform and Windows runtime path handling.

```bash
QT_QPA_PLATFORM=offscreen OPENECE_TIMING_SCREENSHOT="$PWD/build/dev/timing.png" ./build/dev/tests/openece_timing_gui_tests exampleTracesAndDisplayUnits
```

Read [the timing conventions](digital-timing.md) before changing the scheduler.
Trace an input pulse of width d−1, d and d+1 through an inertial buffer with delay d.
Explain why the pending generation is checked before reactions, why a DFF samples
pre-batch D, and why captured state can differ from Q. Follow a two-stage register
whose Q delivery coincides with a clock edge. Distinguish a valid storage feedback
oscillator that reaches its work limit from an invalid pure gate cycle.

The application version and CI artifact are now v0.5.0. Dependency bootstrap,
Windows deployment, package-startup checks and Fedora repository policy remain
unchanged; no new third-party dependency or network access during configure was
introduced. The results below record the completed validation.


### v0.5 validation results

[The implementation CI run](https://github.com/codyklein/open-ece/actions/runs/35151383540)
at `4e895b5476116d12db0c17bacade91a068a4180b` passed all eight jobs on September 16, 2026:

| Configuration | Result |
| --- | --- |
| Fedora 44 GCC desktop / headless | 80 / 76 CTest entries passed |
| Fedora 44 Clang desktop / headless | 80 / 76 passed |
| Fedora 44 Clang ASan/UBSan desktop / headless | 80 / 76 passed |
| Windows Server 2022, MSVC 2022, Qt 6.8.3 Debug / Release | 80 / 80 passed, zero failures/skips/disabled tests |
| Fresh Windows runner, packaged startup | Native window opened, packaged Qt/Qwt/CRT modules verified, normal exit |

All six local Fedora configurations also passed with leak detection enabled and
no sanitizer findings. GCC/Clang/MSVC project compilation produced no warnings;
clang-format and diff checks passed. The offscreen timing rendering was inspected:
D at 2/12 ns, clock rising at 5/15/25 ns, Q at 6/16 ns. Qwt curve samples are also
checked numerically. Existing DSP, SignalsDspView, PlotWidget, phase parser,
combinational evaluator and truth-table implementations are unchanged.

Artifact `OpenECE-v0.5.0-windows-x86_64` contains the same-named ZIP. Its 31 files
include the application, Release Qt/Qwt/CRT DLLs, Windows platform and other Qt
plugins, qt.conf, runtime notes, dependency licenses/notices and SHA256SUMS.txt.
All 30 manifest entries were independently verified; no Debug runtime artifacts
were present. Startup extracted into a fresh path containing spaces and Unicode,
removed development/plugin paths and used a working directory outside the package.
This is actual hosted Windows execution, not cross-compilation. Windows 10/11
physical hardware, high-DPI and accessibility checks remain manual; MinGW is
unvalidated. No GitHub Release was published for this milestone.

Remaining timing technical debt is bounded: state scans and trace snapshot copies,
curve rebuilding, GUI-local table drafts, textual exception diagnostics and no
hard UI latency guarantee. These tradeoffs and the educational timing conventions
are explicit in digital-timing.md. No subsequent milestone was started.

## v0.6 Circuit Analysis development

`openece_circuit_tests` links only the circuits library and GoogleTest. Tests cover
all stamps/signs, reference connectivity, source-loop diagnostics, analytical
networks, declaration permutations, range/size limits and numerical-quality failures.
Forty deterministic networks are compared with independent branch-current equations
and a test-only Gauss-Jordan solver, without production stamps or Eigen. The exact
rational bridge fixture is reproducible with `python3 tests/reference/dc_exact.py`;
Python is not required during normal configure/build/test.

`circuit_gui_workflow` covers unit conversion of pending edits, invalid text
preservation, SI source behavior, explicit missing references/stable IDs, ground
selection, source loops, Unicode names, locale-independent decimals, GUI resource
limits, and persistence across all three domains. Existing suites remain intact.

```bash
QT_QPA_PLATFORM=offscreen OPENECE_CIRCUIT_SCREENSHOT="$PWD/build/dev/circuits.png" ./build/dev/tests/openece_circuit_gui_tests dividerAndDomainPersistence
```

Read [MNA and numerical policy](circuit-analysis.md) before changing the solver.
Explain the negative current of a supplying voltage source, why a current source
cannot ground an island, and why equal parallel ideal voltage sources still fail.
Trace the physical residual test in which a weak conductance is lost during matrix
addition; a small assembled-system residual alone must not authorize a result.

The CI retains Fedora 44 GCC/Clang desktop/headless, Clang ASan/UBSan in both modes,
Windows MSVC Debug/Release, and a fresh Windows packaged-startup runner. Fedora
installs `eigen3-devel`; Windows explicitly bootstraps Eigen 5.0.0 with a pinned
SHA-256 and packages its MPL2 notice. Configure never downloads dependencies.
### v0.6 validation results

[The implementation CI run](https://github.com/codyklein/open-ece/actions/runs/35223556798)
at `4d9402444a8698b07a9f9fe409f06e7b0c24b1d5` passed all eight jobs on September 17, 2026:

| Configuration | Result |
| --- | --- |
| Fedora 44 GCC desktop / headless | 102 / 97 CTest entries passed |
| Fedora 44 Clang desktop / headless | 102 / 97 passed |
| Fedora 44 Clang ASan/UBSan desktop / headless | 102 / 97 passed |
| Windows Server 2022, MSVC 2022, Qt 6.8.3 Debug / Release | 102 / 102 passed, no failures/skips/disabled tests |
| Fresh Windows runner, packaged startup | Native window opened; packaged Qt/Qwt/CRT verified; normal exit |

The six local Fedora configurations also passed, including leak detection and
undefined-behavior checks. Local builds used user-installed pinned Eigen 5.0.0;
Fedora CI independently exercised its system `eigen3-devel` package. Compiler
warning, formatting and diff checks were clean. The rendered Circuits page was
inspected. The 21 circuit tests include 40 independent branch-reference networks
and an exact-rational fixture; the GUI suite has ten functional test methods.

Windows CI initially exposed quadratic selector rebuilding during repeated
component insertion. Refreshing only the newly inserted row resolved the timeout;
no assertion or 30-second timeout was weakened. The Windows Debug circuit workflow
then completed in 3.33 seconds, Release in 0.42 seconds.

The implementation artifact `OpenECE-v0.6.0-windows-x86_64` contains the inner ZIP.
All 31 manifest entries in that 32-file artifact were independently verified,
including the added Eigen MPL2 notice. Qt/Qwt/CRT runtimes remain Release-only;
Eigen introduces no DLL. Startup used a fresh path containing spaces and Unicode,
sanitized development/plugin paths and an external working directory. The incoming
main-branch MIT license is subsequently included in the final package as LICENSE;
that packaging-only update is validated again by the same workflow.

Signals/DSP and digital implementation files are unchanged. The sole existing
GUI-test adjustment is the expected sidebar count from two to three. Remaining
technical debt is explicit: dense bounded synchronous solving, GUI-local drafts,
no save/load/undo or schematic canvas, and no exhaustive multi-error diagnostic
report. Windows physical hardware, high-DPI and accessibility checks remain manual;
MinGW is not validated. No release, AC/transient work or subsequent milestone was started.
