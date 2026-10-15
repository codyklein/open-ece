# OpenECE 1.1.0 — Linear Transient Circuit Analysis

Release notes prepared for the 1.1.0 candidate. Publication is pending signed-package
validation, exact-artifact physical Windows 11 acceptance and final authorization.
The published 1.0.0 and 1.0.1 releases remain immutable.

## New circuit-analysis capability

OpenECE adds linear time-domain analysis of resistors,
capacitors, inductors and independent voltage/current sources in **Circuits →
Transient**. The Qt-independent C++20 engine owns validated circuit/request values
and uses modified nodal analysis with first-order backward-Euler integration.
Existing DC/AC, Signals/DSP, Digital Logic/Timing and Communications APIs remain
unchanged.

- Constant, piecewise-hold and piecewise-linear sources with explicit ordered knots.
- Operating-point initialization (capacitors open, inductors short), or specified
  capacitor voltages and inductor currents with checked consistency.
- Differential voltage and component-current probes in the user's mixed order.
- Separate instantaneous voltage/current plots and a complete model-backed numerical
  trace, with explicit timestamps and ordered before/after samples at source jumps.
- Run for a fresh experiment, Step for one integration interval, Pause/Resume for
  retained execution, terminal Cancel with the accepted prefix, and Reset results.
- Structured failures that distinguish attempted failure time from last accepted
  time. A rejected interval does not append partially accepted state.
- Worker-owned simulation, bounded snapshot publication and plot decimation that
  preserves discontinuities. The GUI never performs dense stepping on its event loop.
- Frozen probe identities, labels, units and references; edits mark retained results
  stale and prevent reuse of execution constructed from an older active draft.

Values use SI units internally. Transient voltages/currents are instantaneous,
not RMS AC phasors. Component current is positive from positive to negative terminal;
a delivering voltage source can therefore have negative reported current.

## Workspace and examples

Execution controls precede compact row-aware tables. A live vertical splitter has
visible grips, noncollapsible panels and scrolling for smaller windows/larger fonts.
Component selectors use readable names without changing serialized kind tokens.
Resizable columns, tooltips and multiline numerical headers improve trace inspection.
Resizing and result updates do not alter project inputs or mark the document dirty.

Four ordinary schema-2 examples supplement the nine existing projects:

- `transient-rc-step`: 10 V RC charging, explicit zero initial capacitor voltage.
- `transient-rl-response`: RL current growth and decaying inductor voltage.
- `transient-rlc-damping`: oscillation and under/critical/overdamped comparisons.
- `transient-source-breakpoints`: hold jumps and linear ramps with exact knot rows.

See [examples and analytical expectations](../examples/README.md) and the
[execution guide](transient-execution.md). All examples open inertly.

## Project compatibility

OpenECE 1.1 reads schema versions 1 and 2 and writes schema 2. It preserves supported
editable state from schema-1 projects produced by v0.9/1.0.x, including incomplete
or invalid drafts, and adds an empty inert Transient workspace. Release-pinned
v0.9 fixtures are unchanged.

**OpenECE 1.0.x cannot open schema-2 files, even with an empty Transient workspace.**
Save As preserves an older original. Saving an imported schema-1 project over its
original path requires upgrade confirmation. There is no schema-1 exporter or
general migration framework.

Schema 2 preserves transient draft text, SI-prefix choices, source points, inactive
settings, initialization records, ordered probes, stable/dangling IDs and allocators.
It does not store results, accepted storage state, workers, progress or plot caches.
Opening never starts a solve, simulation, sweep or BER experiment. Unknown optional
fields remain accepted with a warning that re-saving discards them; future schemas
are rejected. No C++ ABI, plugin ABI or arbitrary future-schema promise is made.

## Numerical and execution limits

Backward Euler is first-order and introduces numerical damping, particularly for
oscillations. A stable or accepted solve is not an integration-accuracy guarantee.
Choose a small maximum step relative to relevant time constants/periods, then
compare smaller steps. There is no adaptive stepping or alternative integration
method, nonlinear-device model, impulse simulation, SPICE integration or schematic
capture. Floating, inconsistent, nonunique and unsupported initialization cases
fail explicitly rather than being silently regularized.

Core, storage and dense-work limits are explicit. Dense solving is bounded but not
real-time. Pause, cancellation and destruction wait for the current atomic operation;
full-prefix copies and table-model updates can still incur visible costs in large
Debug workloads. One voltage and one current trace are selected for plots; all
requested probes remain in the complete numerical table.

Project persistence retains atomic replacement and transactional inert restoration.
It still excludes undo/redo, autosave/recovery, cloud collaboration and external assets.

## Platforms and distribution

The validation matrix covers Fedora 44 GCC/Clang desktop/headless, Clang ASan/UBSan,
Windows MSVC 2022 Debug/Release with Qt 6.8.3, and fresh packaged-runtime startup,
persistence and all thirteen examples. Historical platform records and current
candidate acceptance are distinct; see [physical validation](physical-validation.md).
Windows 10, MinGW, true mixed-monitor DPI and screen-reader behavior are not newly
claimed validated.

The Windows distribution remains a portable x64 ZIP. The protected Azure Artifact
Signing workflow signs OpenECE and the discovered Release Qwt runtime, preserves
Qt/Microsoft vendor signatures and verifies all packaged executable binaries before
manifest/ZIP generation. Signing does not guarantee that every Windows security
policy will allow launch. Smart App Control remains enabled for physical acceptance.
The exact signed ZIP/hash and final acceptance evidence must be recorded before
publication; no artifact hash is implied by these notes.

See [release procedure](releasing.md), [signing gates](windows-signing.md) and
[transient numerical policies](transient-analysis.md). Dependencies/licenses and
published release assets are unchanged by this release-preparation work.
