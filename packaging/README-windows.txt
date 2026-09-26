OpenECE v0.9.0 - Windows x86_64

Extract this entire folder, then double-click openece.exe. Keep the DLLs,
qt.conf, platforms directory and other plugin directories alongside it.
No Qt installation, environment configuration or administrator access is needed.
The optional vc_redist.x64.exe is retained if provided by windeployqt; the
app-local Release MSVC runtime DLLs allow launch without running its installer.

Target: Windows 10 (1809 or later) / Windows 11 x86_64.
Build: Visual Studio 2022, Qt 6.8.3, Qwt 6.3.0, Eigen 5.0.0 and nlohmann/json 3.12.0 (header-only).
See THIRD-PARTY-NOTICES.txt and licenses/ for dependency notices.
This is an unsigned portable application, not an installer. Windows may show
an unrecognized-publisher/download warning. Obtain builds from the project.

Usage, source and build instructions:
https://github.com/codyklein/open-ece
https://github.com/codyklein/open-ece/blob/main/docs/windows.md

App-local dependencies do not update themselves. Obtain a rebuilt OpenECE ZIP
when dependency security updates are incorporated. Numerical behavior is the
same as v0.3: full causal FIR convolution, visible uncompensated group delay,
and unchanged FFT/window normalization. Digital Logic is a separate two-state
domain with separate combinational and timed simulation; it does not change the
sampled-signal representation.

Use the sidebar to switch between Signals / DSP, Digital Logic, Circuits and Communications.
Digital Logic starts with an editable half-adder; Evaluate shows settled 0/1
values and Generate truth table lists every input combination. Use File > Save to retain editable drafts.

Timing / Sequential starts with a D flip-flop example. Run/Pause/Step/Reset
control deterministic simulation; clock and delay entries are integer picoseconds.
Q transitions retain propagation delay. This is a two-state educational model,
not an analog timing or metastability model. See docs/digital-timing.md in source.

Circuits starts with a 10 V / two-1 kilohm voltage divider (midpoint 5 V).
Edit node/component tables, explicitly select ground and terminals, and Solve DC.
Positive current flows from + to -; the supply current is -0.005 A.
Prefixes convert values, invalid drafts remain editable, and edits clear results.
The DC tab supports resistors and independent current/voltage sources.
See docs/circuit-analysis.md in source for MNA, limits and numerical policies.

The AC / Phasors tab adds linear sinusoidal steady-state R/C/L analysis. Sources
use RMS magnitude and phase in degrees with a cosine reference. Solve one positive
frequency or run a bounded sweep. Voltage-transfer mode requires one nonzero
voltage source and every other voltage/current source set to zero. Absolute
response is V RMS; transfer gain is 20 log10|H| with a -240 dB display floor.
Failed frequencies remain visible as diagnostics and plot gaps. Cancel retains
completed points and explicitly marks unevaluated frequencies. Start with the
RC low-pass (output across C) or series RLC (output across R) examples.
No transient, nonlinear, schematic or undo features are included.
See docs/ac-analysis.md in source for phasor signs, limits and numerical policy.

Communications adds an ideal coherent complex-baseband BPSK/QPSK link. Use seeded
or manual bits, view transmitted/received I/Q and labelled constellations, or run
an independent bounded BER experiment. QPSK requires even bit counts. Eb/N0 noise
normalization preserves unit symbol energy regardless of samples/symbol. Run and
Step preserve the same random streams; Cancel retains partial counts and Resume
continues them. Zero errors is an observation, with an explicit 95% fixed-N upper
bound rather than a fabricated BER floor. No RF carrier/recovery/coding/SDR is
included. See docs/communications.md in source for full conventions and limits.

Project files (.openece)
-----------------------
File > New creates a clean Untitled project with editable default examples.
Open restores the entire editable workspace. Save writes the current project;
Save As chooses a new path, appends .openece when needed, and confirms replacement
of an existing file. A * after the filename means editable project changes have
not been saved. The status bar and tooltip show the full path.

New/Open/Close ask Save, Discard or Cancel when dirty. A failed save or cancelled
Save As aborts the original action. Recent Projects retains ten paths separately
in application settings; missing paths stay visible and can be explicitly removed.
Nothing is reopened automatically at startup.

Raw invalid/incomplete text, whitespace, Unicode, units, seeds and connections are
preserved within storage limits. Files contain editable experiment state, NOT
plots, numerical results, simulation state, sweep progress or BER results. Loading
runs nothing: choose Generate/Evaluate/Solve/Simulate/Run explicitly afterward.

Only UTF-8 JSON schema 1 is supported. Unsupported versions and malformed files
are rejected without replacing the active project. Unknown optional fields prompt
a warning because they are discarded on re-save. Files are limited to 8 MiB with
additional row/text/nesting limits. No external assets, scripts or plugins load.

Saves use a checked temporary file and atomic replacement in the destination
directory, with no direct-write fallback. Failures preserve the previous project
file; this is not universal power-loss durability. Keep backups. No undo/redo,
autosave, crash recovery, cloud sync, collaboration or migration is included.

User guide: https://github.com/codyklein/open-ece/blob/main/docs/projects.md
Schema: https://github.com/codyklein/open-ece/blob/main/docs/project-format.md
