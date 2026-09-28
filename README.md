# OpenECE

OpenECE is an MIT-licensed C++20/Qt desktop workbench for exploring electrical and
computer engineering. It combines inspectable, Qt-independent engineering
libraries with editable experiments, plots and automated numerical tests.

- **Signals / DSP:** sine generation, FFT amplitude spectra, spectral windows,
  full discrete convolution and FIR low-pass filtering.
- **Digital Logic:** combinational circuits, truth tables, deterministic timing,
  clocks, SR/D latches and D flip-flops.
- **Circuits:** linear DC modified nodal analysis, RMS AC phasors, R/C/L networks,
  frequency sweeps and voltage-transfer plots.
- **Communications:** ideal coherent BPSK/QPSK, normalized complex baseband,
  deterministic AWGN and bounded BER experiments.
- **Projects:** versioned `.openece` files preserve editable state across all
  domains, including incomplete drafts. Results are recomputed explicitly after
  opening; project files do not contain plot or solver caches.

This branch is a **v1.0 release-readiness candidate**, building on v0.9.0. It is
not a released or fully physically validated v1.0. Binary/package version metadata
remains 0.9.0 until the final candidate checkpoint. No new engineering subsystem
is part of this stabilization milestone.

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
Debug/Release, and a fresh Windows packaged-runtime test. The Windows desktop
target is Windows 10/11 x86_64; hosted Windows Server tests do not establish
physical Windows 10/11, mixed-DPI or screen-reader validation. Those remain
[v1.0 release gates](docs/v1-release-readiness.md). MinGW is not validated.

## Documentation

- [First session](docs/first-session.md), [examples and expected results](examples/README.md),
  [current screenshots](docs/screenshots.md)
- [Saving projects](docs/projects.md), [schema 1](docs/project-format.md),
  [ownership and transactions](docs/project-transactions.md)
- [Signals/DSP conventions](docs/numerics.md), [Digital Logic](docs/digital-logic.md),
  [Timing](docs/digital-timing.md), [DC](docs/circuit-analysis.md),
  [AC](docs/ac-analysis.md), [Communications](docs/communications.md)
- [Architecture](docs/architecture.md), [development](docs/development.md),
  [contributing](CONTRIBUTING.md), [release procedure](docs/releasing.md)

The source tree separates `core/`, `signals/`, `dsp/`, `digital/`, `circuits/`,
`communications/` and `project/`. `gui/` composes the domain views and project
workflow; `tests/` covers numerical contracts and GUI behavior. Numerical APIs do
not depend on Qt, and persistence does not turn them into serialization models.

OpenECE v1.0 supports schema-version-1 .openece project files produced by v0.9 and preserves their supported editable project state.

This promises neither a stable C++/plugin ABI nor future-schema compatibility.
See [compatibility limits](docs/projects.md). There is no undo/redo, autosave,
transient circuit solver, schematic editor, RF synchronization or SDR integration.

## License

OpenECE uses the [MIT license](LICENSE). Dependencies retain their own licenses;
see [third-party notices](packaging/THIRD-PARTY-NOTICES.txt). **Help → About OpenECE**
shows the built version and notice locations. Historical validation belongs in
[validation history](docs/validation-history.md), not current setup instructions.
