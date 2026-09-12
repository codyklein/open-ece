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
initial data, all five controls, plot values, invalid input, recovery, and the
desktop sample limit. It is a functional smoke/integration test, not a comprehensive
accessibility or visual regression suite. For a local screenshot during that test:

```bash
QT_QPA_PLATFORM=offscreen OPENECE_SCREENSHOT="$PWD/build/dev/workbench.png" ./build/dev/tests/openece_gui_tests
```

Sanitizer tests intentionally use Clang and a headless preset so diagnostics concern the
engineering code rather than third-party GUI runtime allocations. Compiler changes
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

## Understand these before the next feature

- Trace a button click through `MainWindow::generate`, `generate_sine`,
  `amplitude_spectrum`, and the Qwt plot adapter. Identify the owner of each buffer.
- Explain why a sample span cannot outlive its record, and how Qt ownership differs
  from a raw pointer that owns heap memory. Examine the deleted rvalue accessor.
- Work an eight-point FFT by hand: bit reversal, butterfly stages, twiddle sign,
  and why its cost grows as O(N log N).
- Predict a 1-unit, 8 Hz sine at 1024 Hz for one second: 1024 samples, 1 Hz FFT
  spacing, amplitude 1 in bin 8, Nyquist at 512 Hz.
- Change the frequency to 8.5 Hz and explain the leakage. Then change duration
  to 2 seconds and explain why the result differs.
- Explain why the normalization divides by the original record length, why only
  interior one-sided bins double, and why an FFT magnitude is not automatically PSD.
- Find which test would fail if the FFT sign, normalization, phase units, or last
  sample timestamp were wrong. Prefer tests that catch a plausible engineering mistake.

The next design discussion should focus on rectangular versus Hann windows,
coherent gain, and what the UI should claim about amplitude and frequency resolution.
