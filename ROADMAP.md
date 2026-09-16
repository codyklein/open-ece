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

## v0.3.1 — Windows portability, validation and packaging (implemented)

- Windows x86_64 / VS 2022 / Qt 6.8.3 build and test presets.
- Explicit checksum-pinned Qwt/GoogleTest bootstrap; Fedora system packages retained.
- Fedora 44 GCC/Clang and sanitizer CI; Windows MSVC Debug/Release CI.
- Release ZIP with Qt/Qwt/CRT runtime deployment and independent Windows startup check.
- Separate Windows build/package documentation; unchanged engineering APIs and numerics.

## v0.4 — Combinational Digital Logic (implemented)

- [x] Qt-independent two-state gate semantics and owning circuit definitions.
- [x] Validated snapshots, stable IDs, forward references, deterministic topological evaluation.
- [x] Explicit malformed-connection/cycle errors and bounded deterministic truth tables.
- [x] Persistent Signals / DSP and Digital Logic views; minimal Signals extraction.
- [x] Editable half-adder, node/pin selectors, toggles, outputs and truth-table GUI.
- [x] Independent logical tests plus GUI draft/error-recovery workflows.
- [x] Existing Fedora/Windows build, test and portable runtime workflows retained.

No propagation delay, clocks, latches, flip-flops, FSMs, buses, Unknown/High-Z,
constants, minimization, Karnaugh maps or HDL are included. These need separate
requirements and correctness contracts. Experiment persistence is deferred from
the earlier roadmap proposal; no save/load work is included in v0.4.

## v0.5 — Digital Timing and Sequential Logic

- [x] Independent validated timed topology and deterministic timestamp-batch scheduler.
- [x] Positive inertial gate delays, exact-delay pulse convention and owned traces.
- [x] Explicit clocks, SR/D latches, rising/falling D flip-flops and storage feedback.
- [x] Separate timing editor, diagrams, Run/Pause/Step/Reset, and combinational copy.
- [x] Independent tick-reference and sequential edge-case tests plus GUI workflows.
- [ ] Complete Fedora/Windows validation and packaged Windows startup.

The v0.4 evaluator and truth tables remain unchanged. Two-state timing conventions
are educational digital models, not analog device behavior. No X/Z, setup/hold,
metastability, async preset/clear, JK/T primitives, FSM tools, buses, HDL, saving or
subsequent engineering domain is included.

## Later — Expand only through useful connections

1. Add multiple signals and typed operation composition when the first few DSP
   workflows reveal shared requirements. Define resampling and rate compatibility.
2. Introduce complex/IQ data and streaming blocks with explicit ownership,
   backpressure, cancellation, and performance measurements before SDR hardware.
3. Explore circuit analysis with established linear algebra; expose an appropriate
   response representation to the signals/DSP tools instead of coupling GUIs.
4. Add communications models and simulated demodulation before live SDR input.
5. Extend digital timing only through separately designed requirements; consider
   FSM tools or FPGA/HDL interoperability when a concrete experiment needs them.
6. Add versioned experiment persistence when round-trip and malformed-file contracts
   are designed, with explicit sample-rate and unit metadata.

A runtime plugin ABI, cross-domain simulation scheduler, node editor, hardware drivers,
and custom linear algebra are deliberately absent from the foundation. Each needs
a demonstrated use case, design discussion, and tests before implementation.
