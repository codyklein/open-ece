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

Before publishing: choose the project license and add hosted CI using the existing
CMake/CTest commands. Add a release validation record and screenshot.

## v0.2.1 — Phase expressions (implemented)

- [x] Decimal/pi phase parser with range checks and a radians-only π insertion button.
- [x] Parse current text on Generate and unit switching; retain invalid input and roll back failed switches.
- [x] Parser and GUI tests for expressions, conversions, insertion, and error recovery.

## v0.3 — First connected DSP operation (proposed next; not started)

Implement direct convolution and a small FIR filter interface. Compose:
**generator → FIR → FFT** with input/output time and spectral views. Keep operations
as explicit functions before introducing a graph editor.

Acceptance: impulse response, passband/stopband examples, output length, boundary
handling, delay, and units documented and independently tested. A two-tone input
should demonstrate measurable attenuation rather than only a visually plausible plot.

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
