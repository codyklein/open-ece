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
rg --files core signals dsp gui tests -g '*.cpp' -g '*.hpp' | xargs clang-format -i
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
certification. CI and broader platform validation remain roadmap work.

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

## Understand these before the next feature

- Trace a button click through `MainWindow::generate`, `generate_sine`,
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
- Read `MainWindow::change_phase_unit()`: pending text is parsed with the old
  unit and converted with 17 significant digits. Qt signals are blocked during text
  updates and failed selector changes so rollback does not trigger another conversion.
- Find which tests catch a symmetric Hann denominator, gain based on padded N,
  doubled DC/Nyquist, changed source samples, or a degree value reinterpreted as radians.

## Remaining design concerns

Weights add one O(L) temporary allocation per analysis; there is no cache or streaming
buffer reuse. This favors inspectable coefficients and simple ownership for now.
Spectrum metadata remains a public value struct whose consistency callers must preserve.
GUI calculations remain synchronous and capped at 65,536 samples. Phase conversions
have floating-point roundoff; the GUI's previous-unit flag must stay aligned
with its selector. These concerns do not require a generic window or units framework.

The proposed next milestone is FIR/convolution composition. It is not implemented
as part of windowing or phase-unit work.
