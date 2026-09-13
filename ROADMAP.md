# OpenECE roadmap

Milestones are ordered by learning value and useful integration. They are not
delivery promises; later work should be revised as actual requirements emerge.

## v0.1 — Signals/FFT foundation

- [x] Launchable Qt workbench and scientific plots.
- [x] Qt-independent sampled data, sine generation, and radix-2 FFT.
- [x] Adjustable amplitude, frequency, phase, sample rate, and duration.
- [x] Explicit time and spectrum conventions, validation, numerical and GUI tests.
- [x] CMake targets/presets and architecture/development documentation.

## v0.2 — Spectral windows and phase units (implemented)

- [x] Selectable Rectangular and periodic Hann windows in the Qt-independent DSP layer.
- [x] Coherent-gain compensation with explicit DC/Nyquist conventions.
- [x] Coefficient, amplitude, padding, short-record, and sidelobe-leakage tests.
- [x] GUI window selector and displayed window/gain; original time samples preserved.
- [x] Degrees/Radians phase selector, defaulting to Degrees and converting existing values.
- [x] GUI tests for selectors, phase round trips, and pending text conversion.
- [x] Documented coherent/off-bin experiments, main-lobe tradeoff, and amplitude limits.

The implemented comparison uses the existing plot with selectable windows; simultaneous
comparison views remain optional future presentation work. RMS and PSD are not implemented.

The project license still needs to be selected. Hosted CMake/CTest CI and portable
Windows packaging are addressed in v0.3.1; see the platform development guides.

## v0.2.1 — Phase expressions (implemented)

- [x] Decimal/pi phase parser with range checks and a radians-only π insertion button.
- [x] Parse current text on Generate and unit switching; retain invalid input and roll back failed switches.
- [x] Parser and GUI tests for expressions, conversions, insertion, and error recovery.

## v0.3 — Convolution and FIR filtering (implemented)

- [x] Qt-independent direct full convolution with zero extension and explicit resource limits.
- [x] Owning FIR coefficients; full filtering preserves rate and t=0 origin.
- [x] Odd-length symmetric Hamming-windowed sinc low-pass, normalized to unity DC gain.
- [x] Complex frequency response on an inclusive DC–Nyquist grid.
- [x] GUI original/filtered time and spectrum comparisons, response in dB, and visible delay.
- [x] Analytical and independent reference tests, including two-tone numerical attenuation.
- [x] Documented length, edge behavior, delay, normalization, and cutoff conventions.

The generator remains a single sine; the two-tone case is a numerical regression.
IIR, streaming/stateful filtering, automatic delay compensation, and general filter
design are outside this milestone.

## v0.3.1 — Windows portability, validation and packaging

- Windows x86_64 / VS 2022 / Qt 6.8.3 build and test presets.
- Explicit checksum-pinned Qwt/GoogleTest bootstrap; Fedora system packages retained.
- Fedora 44 GCC/Clang and sanitizer CI; Windows MSVC Debug/Release CI.
- Release ZIP with Qt/Qwt/CRT runtime deployment and independent Windows startup check.
- Separate Windows build/package documentation; unchanged engineering APIs and numerics.

## v0.4 — Reproducible experiments

Introduce a small versioned experiment format, save/load parameters, and data
import/export with explicit sample-rate and unit metadata. Separate experiment
state from widgets when persistence supplies a concrete reason to do so.

Acceptance: round trips and malformed files tested; saved results identify all
parameters needed to reproduce the analysis. Avoid silent unit conversions.

## Later — Expand only through useful connections

1. Add multiple signals and typed operation composition when the first few DSP
   workflows reveal shared requirements. Define resampling and rate compatibility.
2. Introduce complex/IQ data and streaming blocks with explicit ownership,
   backpressure, cancellation, and performance measurements before SDR hardware.
3. Explore circuit analysis with established linear algebra; expose an appropriate
   response representation to the signals/DSP tools instead of coupling GUIs.
4. Add communications models and simulated demodulation before live SDR input.
5. Consider digital logic and FPGA/HDL integration around a specific interoperable
   experiment, using existing toolchains where sensible.

A runtime plugin ABI, generic simulation scheduler, node editor, hardware drivers,
and custom linear algebra are deliberately absent from the foundation. Each needs
a demonstrated use case, design discussion, and tests before implementation.
