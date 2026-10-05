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
rg --files core signals dsp digital circuits communications project gui tests benchmarks -g '*.cpp' -g '*.hpp' | xargs clang-format -i
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


## Current validation expectations

See [building](building.md) for all local configurations and [Windows](windows.md) for the pinned SDK, tests and packaging. CI runs six Fedora 44 configurations and Windows MSVC Debug/Release plus a fresh packaged-runtime job. Keep all engineering, persistence, sanitizer and workflow tests intact. Offscreen keyboard/scaling tests are useful coverage, not physical screen-reader or mixed-monitor certification.

[Historical records](validation-history.md) retain earlier counts and measurements. [v1.0 release readiness](v1-release-readiness.md) tracks current gates.

## Examples and compatibility

Every file in [examples](../examples/README.md) is an ordinary schema-1 project. Tests load the files, exercise expected results, check inert restoration and semantic round trips. Add an example to the explicit test list when extending the collection. Screenshots are documentation, not pixel tests.

[Release-pinned v0.9 fixtures](../tests/fixtures/v0.9/README.md) must not be regenerated from current defaults. A compatibility change requires an explicit schema decision, not updating the expected fixture to make a test pass. Keep project drafts authoritative, synchronize pending raw editor text without parsing, and derive numerical/runtime objects only on explicit execution. See [ownership and transactions](project-transactions.md).
