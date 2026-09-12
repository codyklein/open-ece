# OpenECE

An extensible desktop engineering workbench for learning and connecting Electrical
and Computer Engineering tools. The long-term direction includes Signals and
Systems, DSP, circuits, digital logic, communications, and SDR. This repository
starts with one working vertical slice: **sine generator → FFT → plots**.

OpenECE is a student-led engineering project, with numerical correctness,
understandable code, and incremental development as priorities. It is not yet a
general simulator, real-time system, or validated measurement instrument.

## Current status: v0.2 spectral windows and phase units

- Qt 6 desktop application with amplitude, frequency, phase, sample-rate, and duration controls.
- Time-domain plot and one-sided, linear peak-amplitude spectrum; rectangle zoom.
- Owning, uniformly sampled real-signal representation with validated inputs.
- Our own sine generator and iterative radix-2 FFT; explicit zero-padding.
- Selectable Rectangular/periodic Hann windows with coherent-gain amplitude correction.
- Degrees/Radians phase entry; switching units converts the existing value.
- Qt-independent engineering libraries, GoogleTest numerical tests, and Qt Test GUI integration checks.
- CMake presets for desktop, headless, and address/undefined-behavior sanitizer builds.

No circuit, communications, hardware, persistence, or plugin features are implemented.
See [ROADMAP.md](ROADMAP.md) for the proposed sequence.

## Dependencies and Fedora setup

Inspect before installing:

```bash
g++ --version
cmake --version
git --version
rpm -q gcc-c++ cmake ninja-build qt6-qtbase-devel qwt-qt6-devel gtest-devel
pkg-config --modversion Qt6Widgets Qt6Qwt6 gtest
```

Install only missing packages. This is the complete Fedora dependency command
(copy it as one line):

```bash
sudo dnf install gcc-c++ cmake ninja-build git-core pkgconf-pkg-config qt6-qtbase-devel qwt-qt6-devel gtest-devel
```

| Dependency | Purpose |
| --- | --- |
| C++20 compiler, CMake ≥ 3.24, Ninja | Standard C++ build and reproducible local presets |
| Qt 6 ≥ 6.4 Widgets | Desktop controls, layout, event loop, and widget ownership |
| Qwt ≥ 6.2, built for Qt 6 | Scientific axes, curves, and zoom; no custom plotting infrastructure |
| GoogleTest ≥ 1.12 | Numerical unit tests, discovered by CTest |
| Qt Test (with Qt development packages) | Desktop integration test only |
| pkg-config | Discover Fedora's `Qt6Qwt6` imported dependency |

CMake uses system packages and never downloads dependencies. Qt/Qwt are optional
when building the numerical libraries. GoogleTest is optional when `BUILD_TESTING=OFF`.
Other Linux distributions may use a different Qwt pkg-config module name; the
current discovery is tested on Fedora. No cross-platform support is claimed yet.

## Build, test, run

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

**Phase** defaults to Degrees. The adjacent selector converts the current value
and range between ±360 degrees and ±2π radians; it does not reinterpret the number.
Conversion rounds to the displayed precision (8 decimal places in degrees, 12 in
radians). Select Generate after editing or switching units. The signal library
still accepts radians only.

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

## Architecture

```text
core/       SampledSignal: owning real samples and sample rate
signals/    sine generator → core
dsp/        windows, forward FFT, and amplitude spectrum → core
gui/        Qt Widgets + Qwt; consumes signals and dsp
tests/      independent numerical checks and desktop workflow test
docs/       architecture decisions, mathematical conventions, development guide
```

Public engineering headers live under each library's `include/openece/` tree.
The CMake targets are `OpenECE::core`, `OpenECE::signals`, and `OpenECE::dsp`.
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
event thread. This does not establish a real-time latency guarantee.

The FFT accepts power-of-two lengths. The spectrum convenience function pads
other record lengths after applying weights over the original record. Spectrum
amplitudes divide by the sum of weights; only interior one-sided bins are doubled.
Hann reduces distant leakage, but cannot remove off-bin amplitude error or resolve
arbitrarily close tones. Zero-padding does not improve resolving power. This is
not a power spectrum or PSD. Above-Nyquist
generator frequencies are rejected. Full engineering details are in the numerics document.

The project license has not been selected yet. Dependencies retain their own licenses;
no third-party source is vendored in this repository.
