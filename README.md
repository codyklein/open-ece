# OpenECE

An extensible desktop engineering workbench for learning and connecting Electrical
and Computer Engineering tools. The long-term direction includes Signals and
Systems, DSP, circuits, digital logic, communications, and SDR. This repository
provides three domains: **Signals / DSP** (sine → optional FIR → FFT → plots) and
**Digital Logic** (combinational circuits / truth tables and timed sequential simulation),
and **Circuits** (linear DC resistors and independent sources).

OpenECE is a student-led engineering project, with numerical correctness,
understandable code, and incremental development as priorities. It is not yet a
general simulator, real-time system, or validated measurement instrument.

## Current status: v0.6 Linear DC Circuit Analysis

- Independent validated circuit model and Modified Nodal Analysis with explicit signs.
- Node voltages, ideal-voltage-source currents, and floating/constraint/numerical diagnostics.
- Persistent Circuits editor with explicit SI/prefix conversion and a voltage-divider example.

- Independent event-driven timing sessions, inertial gates, clocks, SR/D latches and D flip-flops.
- Timing diagrams with explicit visible delays and Run/Pause/Step/Reset controls.
- Independent two-state digital core: seven gate kinds, validated acyclic circuits, bounded truth tables.
- Digital Logic editor with input toggles, gate/source selectors, outputs, and an editable half-adder.
- Persistent domain navigation; Windows portability and packaging retained.
- Qt 6 desktop application with amplitude, frequency, phase, sample-rate, and duration controls.
- Original/filtered time-domain and one-sided peak-amplitude spectrum comparisons; rectangle zoom.
- Full causal convolution, owned FIR coefficients, and a Hamming-windowed sinc low-pass designer.
- Filter magnitude response in dB with explicit uncompensated linear-phase delay.
- Owning, uniformly sampled real-signal representation with validated inputs.
- Our own sine generator and iterative radix-2 FFT; explicit zero-padding.
- Selectable Rectangular/periodic Hann windows with coherent-gain amplitude correction.
- Degrees/Radians phase entry, including pi expressions and a π insertion button.
- Qt-independent engineering libraries, GoogleTest numerical tests, and Qt Test GUI integration checks.
- CMake presets for desktop, headless, and address/undefined-behavior sanitizer builds.

No AC/transient/nonlinear analysis, FSM/HDL tooling, communications, hardware,
persistence, or plugin features are implemented.
See [ROADMAP.md](ROADMAP.md) for the proposed sequence.

## Dependencies and Fedora setup

Inspect before installing:

```bash
g++ --version
cmake --version
git --version
rpm -q gcc-c++ cmake ninja-build qt6-qtbase-devel qwt-qt6-devel gtest-devel eigen3-devel
pkg-config --modversion Qt6Widgets Qt6Qwt6 gtest
```

Install only missing packages. This is the complete Fedora dependency command
(copy it as one line):

```bash
sudo dnf install gcc-c++ cmake ninja-build git-core pkgconf-pkg-config qt6-qtbase-devel qwt-qt6-devel gtest-devel eigen3-devel
```

| Dependency | Purpose |
| --- | --- |
| C++20 compiler, CMake ≥ 3.24, Ninja | Standard C++ build and reproducible local presets |
| Qt 6 ≥ 6.4 Widgets | Desktop controls, layout, event loop, and widget ownership |
| Qwt ≥ 6.2, built for Qt 6 | Scientific axes, curves, and zoom; no custom plotting infrastructure |
| Eigen ≥ 3.4 (Windows pins 5.0.0) | Private header-only DC linear algebra |
| GoogleTest ≥ 1.12 | Numerical unit tests, discovered by CTest |
| Qt Test (with Qt development packages) | Phase parser and desktop integration tests |
| pkg-config | Discover Fedora's `Qt6Qwt6` imported dependency |

CMake uses system packages and never downloads dependencies. Qt/Qwt are optional
when building the numerical libraries. GoogleTest is optional when `BUILD_TESTING=OFF`.
Other Linux distributions may use a different Qwt pkg-config module name; the
current discovery is tested on Fedora. Windows uses the separately documented pinned dependency bootstrap.

## Linux: build, test, run

From the repository root:

```bash
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev
./build/dev/gui/openece
```

The GUI opens with a 1-unit, 8 Hz sine sampled at 1024 Hz for 1 second. Change the
controls and select **Generate and analyze**. Plots retain the previous result
while controls are being edited; the status says when an update is pending.
Invalid generation requests clear the plots and display the reason.

**Spectral window** defaults to Rectangular, preserving v0.1 results. Hann reduces
sidelobes at the cost of a wider main lobe. The summary identifies the selected
window and its coherent gain. The time plot always shows the original samples.

**Phase** defaults to Degrees, accepting decimals within ±360. Radians accepts
decimals within ±2π or signed pi expressions such as `pi`, `pi/2`, `3*pi/4`,
`3π/4`, and `-pi/2`. A decimal coefficient and positive decimal divisor are
supported, including scientific notation. `pi` is case-insensitive; whitespace
may separate tokens. Sums, parentheses, and general arithmetic are not supported.
The **π** button is enabled in Radians and inserts at the cursor or replaces selected text.

Generate (or Enter in the phase field) parses the current text immediately.
Switching units first parses in the old unit, then converts to decimal text with
up to 17 significant digits. Invalid input stays visible with an error and clears
the results; a failed unit switch keeps the previous unit. Inputs are limited to
128 characters and rejected outside the range, without wrapping. The signal
library still accepts radians only.

**Filter** defaults to Off, preserving the unfiltered workflow. Select **FIR low-pass
(Hamming)**, choose a cutoff strictly below Nyquist and an odd tap count (3–511),
then Generate. The designed coefficients sum to one. Cutoff is the ideal sinc
cutoff, not a guaranteed exact −3 dB frequency. More taps trade a narrower
transition for more delay and computation.

The time and spectrum plots overlay **Original** and **Filtered** with legends.
Filtering uses full zero-extended convolution: N input samples and M taps produce
N+M−1 samples at the same rate, starting at t=0. Duration extends by (M−1)/fs;
the designed filter's (M−1)/2-sample delay remains visible. Nothing is cropped or
shifted. The full output must fit the 65,536-sample desktop limit. The summary
shows both records' duration, FFT length, bin spacing, and coherent gain, plus
delay and duration extension. Short inputs are identified when no fully immersed
interval exists.

**Filter response** shows 20 log10(|H(f)|) at 1025 points from DC through Nyquist,
with a −120 dB display floor for |H| ≤ 10⁻⁶. It describes the coefficients, while
signal spectra describe finite records with startup/tail effects and different
normalization lengths. Their peak ratio need not equal the response gain. See
[numerics.md](docs/numerics.md#full-convolution-and-fir-filtering) for equations and
reproducible examples. The response remains independent of the spectral-window selector.

Drag a rectangle on a plot to zoom. Right-click steps back; Ctrl+right-click
returns to the full view. The **Conventions** tab explains the plots.

Numerical libraries and tests without Qt or a display:

```bash
cmake --preset headless
cmake --build --preset headless -j 4
ctest --preset headless
```

AddressSanitizer and UndefinedBehaviorSanitizer for our numerical code (the preset
uses Clang and its compiler-rt runtime, both present on the initial development
machine; Fedora packages are `clang` and `compiler-rt`):

```bash
cmake --preset sanitize
cmake --build --preset sanitize -j 4
ctest --preset sanitize
```

Use a separate build directory for a different compiler:

```bash
cmake -S . -B build/clang -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DOPENECE_BUILD_GUI=OFF
cmake --build build/clang -j 4
ctest --test-dir build/clang --output-on-failure
```

CTest runs the GUI integration test using Qt's offscreen platform automatically;
normal application launches use the desktop. Compilation databases are written
into each build directory. Build artifacts are ignored by Git.

## Digital Logic

Choose **Digital Logic** in the sidebar. The initial half-adder has A/B input
checkboxes, XOR Sum and AND Carry. Edit names, add/remove nodes, select a gate to
configure its type and pins, and connect named outputs to source IDs. Choose
**Evaluate** or **Generate truth table**. Invalid drafts stay editable; missing
sources and cycles cause explicit errors. Edits clear stale digital results.
Switching domains preserves all pages' state.

NOT takes one pin; AND/OR/NAND/NOR/XOR/XNOR take one or more. XOR means odd parity,
XNOR even parity. Truth-table columns follow declaration order; rows count upward
in binary with the first input most significant. The desktop allows 8 inputs,
64 gates, 16 outputs and 8 pins per gate. The independent core has larger explicit
limits and permits tables through 10 inputs (1024 rows). This is settled two-state
combinational evaluation, without timing, feedback or sequential elements.
See [Combinational conventions and API](docs/digital-logic.md).

The **Timing / Sequential** tab has its own editable draft. Start with the DFF
example, enter integer-picosecond delays and stimulus times, and Run or Step.
Clock and manual drivers are exclusive per input. Observe nodes by ID; change
display units without changing simulation ticks. Gates use inertial delay; storage
captures have delayed visible Q. At simultaneous data/clock changes a DFF samples
pre-batch data. SR=11 stops the simulation. Pure gate cycles are rejected, while
feedback through storage is permitted. The copy action validates the combinational
draft and replaces the timing draft with an independent copy and an explicit gate
delay. See [Timing conventions, limits and API](docs/digital-timing.md).

## Circuits

Choose **Circuits** in the sidebar. The default 10 V / two-1 kΩ divider shows
5 V at the midpoint and −0.005 A through the supply. Edit node/component names
in place, select an explicit ground and +/− terminals, enter a value and unit,
then choose **Solve DC**. Edits clear stale results; invalid drafts remain editable.
Unit switching converts the pending physical value; invalid text stays visible.
Positive current always means + terminal → − terminal, including voltage sources.
Current sources do not establish a voltage reference. Floating networks and
redundant/conflicting ideal-voltage-source loops are rejected without regularization.
See [Circuit Analysis API, MNA, policies and limits](docs/circuit-analysis.md).

## Windows: build, test, run and package

The Windows target remains Windows 10 (1809+) / Windows 11 x86_64, Visual Studio 2022,
and Qt 6.8.3. Follow [the Windows guide](docs/windows.md) for prerequisites,
checksum-pinned Qwt/GoogleTest/Eigen bootstrap, explicit Debug/Release presets, and
portable ZIP creation. GitHub Actions uses Fedora 44 for GCC/Clang and actual
Windows MSVC runners for tests and packaged startup. MinGW is not validated.
The timing layer preserves all v0.4 combinational and DSP APIs and behavior.

## Architecture

```text
core/       SampledSignal: owning real samples and sample rate
signals/    sine generator → core
dsp/        convolution, FIR/design/response, windows, FFT, and spectrum → core
digital/    combinational evaluation / truth tables and separate timed simulation (stdlib only)
circuits/   validated linear DC model and MNA solver (private Eigen dependency)
gui/        persistent SignalsDspView, DigitalWorkspace and CircuitsView; Qt Widgets + Qwt
tests/      independent numerical checks, phase parser, and desktop workflow tests
docs/       architecture decisions, mathematical conventions, development guide
```

Public engineering headers live under each library's `include/openece/` tree.
The CMake targets are `OpenECE::core`, `OpenECE::signals`, `OpenECE::dsp`, and
`OpenECE::digital`, and `OpenECE::circuits`. Circuit Analysis is independent of the other domains. Digital Logic does not depend on the sampled-signal core.
Neither signals nor DSP depends on the other. The GUI composes them, while `core`
depends only on the C++ standard library. `openece_workbench` is an internal GUI
library shared by the application executable and its integration test.

Read [architecture.md](docs/architecture.md) for decisions and alternatives,
[numerics.md](docs/numerics.md) for mathematical contracts, and
[development.md](docs/development.md) for the workflow and learning exercises.

## Important limits

Records are finite, real, single-channel, uniformly sampled, and begin at zero.
Amplitudes have no physical unit metadata yet. The numerical record/FFT limit is
1,048,576 samples; the synchronous GUI accepts at most 65,536 to bound work on the
event thread. Direct convolution and response evaluation each have a 64-million
product budget. This does not establish a real-time latency guarantee.

The FFT accepts power-of-two lengths. The spectrum convenience function pads
other record lengths after applying weights over the original record. Spectrum
amplitudes divide by the sum of weights; only interior one-sided bins are doubled.
Hann reduces distant leakage, but cannot remove off-bin amplitude error or resolve
arbitrarily close tones. Zero-padding does not improve resolving power. This is
not a power spectrum or PSD. Above-Nyquist
generator frequencies are rejected. Full engineering details are in the numerics document.

OpenECE is licensed under the [MIT License](LICENSE). Dependencies retain their own licenses;
no third-party source is vendored in this repository.
