<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/branding/openece-lockup-dark.svg">
  <img src="assets/branding/openece-lockup-light.svg" alt="OpenECE — waveform O and wordmark" width="360">
</picture>

# OpenECE

OpenECE is an MIT-licensed C++20/Qt desktop workbench for exploring electrical and
computer engineering. It combines inspectable, Qt-independent engineering
libraries with editable experiments, plots and automated numerical tests.

- **Signals / DSP:** sine generation, FFT amplitude spectra, spectral windows,
  full discrete convolution and FIR low-pass filtering.
- **Digital Logic:** combinational circuits, truth tables, deterministic timing,
  clocks, SR/D latches and D flip-flops.
- **Circuits:** linear DC modified nodal analysis, RMS AC phasors, R/C/L networks,
  frequency sweeps, voltage-transfer plots and RC/RL/RLC transient simulation.
- **Communications:** ideal coherent BPSK/QPSK, normalized complex baseband,
  deterministic AWGN and bounded BER experiments.
- **Projects:** versioned `.openece` files preserve editable state across all
  domains, including incomplete drafts. Results are recomputed explicitly after
  opening; project files do not contain plot or solver caches.

**OpenECE 1.1.0 is in release preparation; it is not yet published.** Transient
analysis is implemented, with worker-owned Run/Step/Pause/Resume/Cancel controls,
voltage/current plots, numerical traces and schema-2 projects. See the
[release notes](docs/release-notes-v1.1.md), [numerical conventions](docs/transient-analysis.md)
and [execution workflow](docs/transient-execution.md). The latest published release
remains [1.0.1](https://github.com/codyklein/open-ece/releases/tag/v1.0.1).
The exact signed 1.1.0 Windows candidate still requires physical acceptance.

## Try it

Follow the [first-session walkthrough](docs/first-session.md) and open an
[example project](examples/README.md) using **File → Open**. The Windows portable
ZIP from [GitHub releases](https://github.com/codyklein/open-ece/releases) includes
Qt/Qwt runtimes; extract the entire archive before launching `openece.exe`.

Fedora 44 uses system development packages:

```bash
sudo dnf install gcc-c++ cmake ninja-build git-core pkgconf-pkg-config qt6-qtbase-devel qwt-qt6-devel gtest-devel eigen3-devel json-devel
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev
./build/dev/gui/openece
```

Run these commands from a clone of this repository. See [build instructions](docs/building.md)
for prerequisites, headless and sanitizer configurations. Ordinary CMake
configuration is offline. [Windows instructions](docs/windows.md) use Visual
Studio 2022 x64 and explicitly acquired Qt 6.8.3, with checksum-pinned dependencies.

CI covers Fedora 44 GCC/Clang desktop/headless, Clang ASan/UBSan, Windows MSVC
Debug/Release, and a fresh Windows packaged-runtime test. Physically validated
platforms are Fedora 44 and Windows 11 x86_64; see the recorded physical scope
below. Windows 10 and MinGW may work but are not validated or claimed supported
for v1.1. Hosted Windows Server tests do not establish desktop, mixed-DPI or
screen-reader validation. See the [physical validation record and checklist](docs/physical-validation.md).

## Documentation

- [First session](docs/first-session.md), [examples and expected results](examples/README.md),
  [current screenshots](docs/screenshots.md)
- [Saving projects](docs/projects.md), [schema 1](docs/project-format.md), [schema 2 / transient drafts](docs/transient-project-format.md),
  [ownership and transactions](docs/project-transactions.md)
- [Signals/DSP conventions](docs/numerics.md), [Digital Logic](docs/digital-logic.md),
  [Timing](docs/digital-timing.md), [DC](docs/circuit-analysis.md),
  [AC](docs/ac-analysis.md), [Transient](docs/transient-analysis.md),
  [Communications](docs/communications.md)
- [Architecture](docs/architecture.md), [development](docs/development.md),
  [contributing](CONTRIBUTING.md), [release procedure](docs/releasing.md),
  [1.1 release notes](docs/release-notes-v1.1.md), [1.0 release notes](docs/release-notes-v1.0.md),
  [1.0.1 branding and acceptance](docs/branding.md)

The source tree separates `core/`, `signals/`, `dsp/`, `digital/`, `circuits/`,
`communications/` and `project/`. `gui/` composes the domain views and project
workflow; `tests/` covers numerical contracts and GUI behavior. Numerical APIs do
not depend on Qt, and persistence does not turn them into serialization models.

OpenECE v1.0 supports schema-version-1 .openece project files produced by v0.9 and preserves their supported editable project state.

OpenECE 1.1 imports supported schema-1 editable state, including incomplete drafts,
and saves schema 2. OpenECE 1.0.x cannot read schema-2 files; use Save As to retain
an older original. This promises neither a stable C++/plugin ABI nor future-schema
compatibility. See [compatibility limits](docs/projects.md). There is no undo/redo,
autosave, nonlinear circuit analysis, schematic editor, RF synchronization or SDR integration.

## License

OpenECE uses the [MIT license](LICENSE). Dependencies retain their own licenses;
see [third-party notices](packaging/THIRD-PARTY-NOTICES.txt). **Help → About OpenECE**
shows the built version and notice locations. Historical validation belongs in
[validation history](docs/validation-history.md), not current setup instructions.
