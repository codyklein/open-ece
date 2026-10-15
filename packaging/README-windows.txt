OpenECE v1.1.0 - Windows x86_64

Extract this entire folder, then double-click openece.exe. Keep the DLLs,
qt.conf, platforms directory and other plugin directories alongside it.
No Qt installation, environment configuration or administrator access is needed.
The optional vc_redist.x64.exe is retained if provided by windeployqt; the
app-local Release MSVC runtime DLLs allow launch without running its installer.

Desktop validation targets: Fedora 44 and Windows 11 x86_64. Historical 1.0.x
physical acceptance is recorded separately; native Fedora transient acceptance
has passed. Every new signed Windows release candidate requires its own physical
acceptance with Smart App Control enabled. CI is not physical desktop evidence.
Windows 10 and MinGW may work but are not validated or claimed supported for v1.1.
Build: Visual Studio 2022, Qt 6.8.3, Qwt 6.3.0, Eigen 5.0.0 and nlohmann/json 3.12.0 (header-only).
See THIRD-PARTY-NOTICES.txt and licenses/ for dependency notices.
This is a portable application, not an installer. Signed distribution candidates
use the verified publisher Cody Klein for openece.exe and the Release Qwt DLL;
Qt and Microsoft runtime files retain their vendor signatures. Ordinary local/PR
builds are unsigned developer packages. Verify signatures before distribution;
see docs/windows-signing.md. Obtain builds from the project. Signing does not
override Windows malware/security policy, and Smart App Control stays enabled
during physical release validation.

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

OpenECE 1.1 reads UTF-8 JSON schemas 1 and 2 and saves schema 2.
Schema-1 imports preserve supported editable state and add an empty inert Transient
draft. An in-place upgrade warns that OpenECE 1.0.x cannot read schema 2 and offers
Save As. Circuits > Transient provides linear RC/RL/RLC backward-Euler simulation,
constant/hold/linear sources, explicit initialization, voltage/current plots and
ordered numerical traces. Run starts fresh; Step advances one interval and pauses;
Pause/Resume retains execution; Cancel retains an accepted prefix but cannot resume;
Reset results clears derived data only. Edits to active inputs mark old results stale.
These are instantaneous SI voltages/currents, not RMS AC phasors. Current is positive
from the positive terminal to the negative. Choose a small step and compare smaller
steps: backward Euler is first-order and damps oscillations. No adaptive integration,
nonlinear devices, impulses, SPICE models or schematic capture are provided.
Unsupported versions and malformed files
are rejected without replacing the active project. Unknown optional fields prompt
a warning because they are discarded on re-save. Files are limited to 8 MiB with
additional row/text/nesting limits. No external assets, scripts or plugins load.

Saves use a checked temporary file and atomic replacement in the destination
directory, with no direct-write fallback. Failures preserve the previous project
file; this is not universal power-loss durability. Keep backups. No undo/redo,
autosave, crash recovery, cloud sync, collaboration or migration is included.

User guide: https://github.com/codyklein/open-ece/blob/main/docs/projects.md
Schema: https://github.com/codyklein/open-ece/blob/main/docs/project-format.md

Onboarding and examples
--------------------------------
Open examples/*.openece through File > Open. They are ordinary editable projects,
not automatically running demos. Start with docs/first-session.md and
examples/README.md for actions, analytical expectations and unit conventions.
Help > About OpenECE identifies the actual built version, MIT license and notices.
Documentation links to source/test files require the repository checkout; the
packaged offline guides and examples contain everything needed for a first session.
The package includes thirteen ordinary examples: nine schema-1 projects and four
schema-2 transient projects. Open them normally and run the analysis explicitly.
Published OpenECE 1.0.0/1.0.1 packages are immutable. Validation of a new candidate
must identify its exact ZIP hash; true mixed-monitor DPI and screen-reader behavior
remain unvalidated. No installer, updater or automatic migration is included.
See docs/physical-validation.md for the checklist and evidence boundaries.

Branding
--------
The approved cobalt Waveform O appears as the application/window icon and the
multi-resolution executable icon. About retains version/license information.
Useful exports are in branding/. The wordmark is outlined from Noto Sans SemiBold;
no font installation is required. See branding/README.txt and the Noto Sans OFL
notice in licenses/. Published v1.0.0 files are unchanged.
