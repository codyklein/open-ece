# OpenECE 1.0.0 — release notes

[OpenECE 1.0.0 is published](https://github.com/codyklein/open-ece/releases/tag/v1.0.0)
at commit `3e688548d3d71a12fdceee173ee780047f23f854`. This stabilization release brings the existing engineering workspaces together; it
does not introduce another engineering domain.

## Shipped capabilities

- Signals/DSP: sine generation, coherent-gain-corrected one-sided FFT amplitude
  spectra, periodic Hann analysis window, full discrete convolution and
  Hamming-windowed FIR low-pass design. Full causal output retains group delay.
- Digital Logic: two-state combinational gates, validated dependency graphs,
  deterministic evaluation and bounded truth tables.
- Timing/Sequential: deterministic event-driven simulation, inertial gate delays,
  clocks, SR/D latches and D flip-flops; captured storage and delayed visible Q
  remain distinct. This is an educational timing model, not electrical simulation.
- Circuits: linear DC and positive-frequency sinusoidal AC modified nodal
  analysis, R/C/L AC networks, independent sources, explicit diagnostics,
  frequency sweeps and voltage-transfer plots. AC uses RMS cosine-reference
  phasors; branch current is positive from positive to negative terminal.
- Communications: ideal synchronized coherent BPSK/Gray-coded QPSK, normalized
  complex baseband, deterministic seeded AWGN and bounded BER experiments.
  Results retain integer errors/bits and meaningful zero-error confidence bounds.
- Projects: schema-1 UTF-8 `.openece` files across all domains, exact pending and
  invalid text, stable/dangling IDs, units, selections and allocator state.
  Loading is transactional and inert; saving uses atomic replacement. New/Open/
  Save/Save As, dirty-state prompts and application-level recent projects are included.

## v1.0 stabilization

- Raw typing, mouse-wheel and arrow edits invalidate execution inputs correctly;
  retained Signals results are explicitly stale until regenerated.
- User-data edits remain dirty-tracked. Navigation alone does not produce an
  unnecessary save prompt, while explicit Save retains supported selections.
- Current focus is visually distinct from old numeric selections; keyboard
  traversal follows the Signals form in both directions.
- BER legend keys and table widths stay readable and stable during live updates.
- Enlarged Signals text and smaller windows retain reachable controls/scrolling.
- User-controlled diagnostics render as plain text. Editor teardown regressions,
  cross-domain keyboard/scale coverage and schema compatibility tests are retained.
- Nine ordinary project examples, offline guides, first-session onboarding and
  MIT/third-party license notices ship with the Windows portable package.

## Compatibility and validation boundary

OpenECE v1.0 supports schema-version-1 .openece project files produced by v0.9 and preserves their supported editable project state.

This does not promise stable C++/plugin ABI, arbitrary future-schema support,
unsupported-schema migration or retention of unknown optional fields on re-save.
Derived numerical results, plots and in-progress execution are not project content;
users explicitly recompute them after opening.

Physical checkpoint-4 retests passed on native Fedora and Windows 11 x64,
including Windows Signals at 150% scaling. CI covers Fedora 44 GCC/Clang desktop/
headless, Clang ASan/UBSan, MSVC 2022 Debug/Release with Qt 6.8.3, and the fresh
Windows packaged runtime. The exact 1.0.0 ZIP passed its bounded physical Windows 11 RC smoke before
publication; [the historical checklist](release-candidate.md) records its scope.

Windows 10, MinGW, true mixed-monitor DPI and native screen-reader behavior are
not validated. This is an unsigned portable ZIP, with no installer/updater.
There is no transient/nonlinear circuit analysis, schematic capture, higher-order
communications, RF synchronization recovery, SDR, undo/redo, autosave/recovery,
cloud collaboration or general schema migration.

See [physical evidence](physical-validation.md), [dependency review](release-dependencies.md)
and [release procedure](releasing.md). Historical v0.x measurements remain in
[validation history](validation-history.md).
