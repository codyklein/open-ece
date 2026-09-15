# Windows build and portable ZIP (v0.3.1)

The v0.3.1 Windows target is Windows 10 (1809+) / Windows 11 x86_64,
Visual Studio 2022 / MSVC, and **Qt 6.8.3**, with **Qwt 6.3.0** and
**GoogleTest 1.17.0**. CI uses an actual Windows Server 2022 GitHub runner with
VS 2022; this is MSVC/Windows runtime validation, not a manual Windows 10/11
hardware test. MinGW has not been validated and is not a supported configuration
for this milestone. Numerical APIs and behavior remain identical to v0.3.

## Prerequisites

Install Git, CMake >= 3.24, PowerShell 7, and Visual Studio 2022 (Community or
Build Tools) with **Desktop development with C++**, the **MSVC v143 x64/x86**
toolset, and a Windows 10/11 SDK. Keep the Release redistributable components
installed; packaging uses Visual Studio's redistributable runtime files.

Install the **Qt 6.8.3 MSVC 2022 64-bit** kit with the Qt online installer.
Alternatively use the exact CI acquisition mechanism with Python and aqtinstall:

```powershell
python -m pip install aqtinstall==3.3.0
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --archives qtbase --outputdir C:/Qt
```

The Qt Base archive includes Widgets, Test, PrintSupport, Concurrent, qmake and
windeployqt. The application does not need Qt Creator or Qt Designer. CI explicitly
installs this pinned SDK rather than using runner-provided Qt. aqt retrieves Qt's
published binary archives and checks their upstream checksums. Tool version and
Qt version/architecture are pinned; Python's transitive package dependencies and
hosted VS patch updates are not a byte-for-byte toolchain lock.

Open **x64 Native Tools Command Prompt for VS 2022**, then run `pwsh` to enter
PowerShell 7 with that compiler environment. Alternatively use a VS 2022 Developer
PowerShell explicitly targeting x64. Confirm `$env:VSCMD_ARG_TGT_ARCH` is `x64`.
All commands below run from the repository root in that shell.

```powershell
git clone https://github.com/codyklein/open-ece.git
cd open-ece
$env:QT_ROOT = 'C:/Qt/6.8.3/msvc2022_64'
./scripts/windows/bootstrap.ps1 -QtRoot $env:QT_ROOT
cmake --preset windows
cmake --build --preset windows-debug --parallel 4
ctest --preset windows-debug
cmake --build --preset windows-release --parallel 4
ctest --preset windows-release
```

Check each command succeeds before continuing. Visual Studio is a multi-config
generator: `CMAKE_BUILD_TYPE` does not select a configuration. The build/test
presets pass Debug or Release explicitly. Qt's Debug DLLs, Qwt's Debug DLL/import
library and GoogleTest's `/MDd` build are used together; Release uses `/MD`.

For development launches, add the matching runtime directories for that shell:

```powershell
$env:PATH = "$env:QT_ROOT/bin;$PWD/build/windows-deps/qwt/lib/Debug;$env:PATH"
./build/windows/gui/Debug/openece.exe
```

For Release, use `lib/Release` and `gui/Release/openece.exe`. CTest adds the
correct Qt and Qwt paths automatically and runs the existing workflow offscreen.
The package described below needs none of these development environment settings.

## Dependency bootstrap

Normal CMake configuration **never downloads anything**. The explicit bootstrap
verifies SHA-256-pinned Qwt and GoogleTest ZIPs before extracting them into the
ignored `build/windows-deps` directory. Re-running rechecks the archives and
reuses installations only when the script/toolchain/Qt fingerprint matches and
all installed outputs still match their recorded hashes. CI caches this verified
dependency directory by bootstrap-script hash, Qt version and MSVC toolset; the
bootstrap still checks a restored cache before reusing it. A missing or changed
output triggers an incremental rebuild. A corrupt downloaded archive fails
clearly; remove that archive and retry. Source directories are a local build
cache, not intended for manual editing. When deliberately changing toolchains,
use a fresh dependency/build directory rather than reusing CMake compiler caches.

Qwt is built with the selected Qt kit's qmake and MSVC nmake. Its unmodified
sources use `QWT_LOCAL_PRI` overrides to build a shared library per configuration,
without Designer, examples, tests, SVG or OpenGL canvases. OpenECE uses the
ordinary raster Qwt canvas, so this preserves every application feature. The
script stages headers and the discovered DLL/import-library pairs, and writes
`OpenECEQwtArtifacts.cmake`. OpenECE consumes that map through `Qwt::Qwt`; packaging
uses the exact Release target file, not an assumed DLL basename. GoogleTest is
installed through upstream CMake with `gtest_force_shared_crt=ON`, both configs,
a distinct Debug library postfix, and no GoogleMock. Third-party code stays outside version control.

Fedora continues to use `dnf` and `Qt6Qwt6` pkg-config discovery. Windows uses
standard Qt/GTest CMake package discovery and `QWT_ROOT`; no package-management
framework or alternate plotting layer is introduced. For custom dependency
locations, pass `-DQWT_ROOT=...` and `-DCMAKE_PREFIX_PATH='Qt-path;GTest-path'` at
configure time (or use ignored `CMakeUserPresets.json`). Custom Windows Qwt
installations can explicitly set `Qwt_LIBRARY_DEBUG/RELEASE` and
`Qwt_RUNTIME_DEBUG/RELEASE` if their artifact names differ from upstream defaults.
They must be shared builds for the same Qt major version, architecture and ABI.

## Release ZIP

From the configured developer shell:

```powershell
./scripts/windows/package.ps1 -QtRoot $env:QT_ROOT
./scripts/windows/test-package.ps1 -Archive ./build/packages/OpenECE-v0.3.1-windows-x86_64.zip
```

The script builds and installs **Release only** into a fresh staging directory,
then runs the selected kit's `windeployqt --release --compiler-runtime` on both
OpenECE and Qwt. Qt 6.8.3 deploys an MSVC redistributable installer in Release;
when app-local CRT DLLs are absent, a separate CMake install component uses
`InstallRequiredSystemLibraries` to locate legitimate Release redistributable
DLLs without hard-coded Visual Studio version paths. Missing runtimes fail
packaging. The redistributable installer may remain in the ZIP as an optional
file; no installer run or elevation is needed to launch with the app-local DLLs.

The package has one root folder:

```text
OpenECE-v0.3.1-windows-x86_64/
    openece.exe
    <Release Qwt DLL>
    Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, <supporting Qt Base DLLs>
    msvcp140.dll, vcruntime140.dll, <supporting Release CRT DLLs>
    platforms/qwindows.dll
    <other required Qt plugin directories>
    qt.conf
    README.txt
    THIRD-PARTY-NOTICES.txt
    licenses/
    SHA256SUMS.txt
```

Runtime notes and original Qwt/GoogleTest license texts are included. Qt's license
texts and third-party attribution are retained in the checked-in
`packaging/Qt-6.8.3-NOTICES.txt`, with source-relative headings and the upstream
archive checksum. Packaging copies this notice file without network access or
processing the full Qt source archive. No Qt implementation sources are vendored.
The notices link corresponding sources and explain shared-library replacement.
Translations, the unused software OpenGL renderer, and optional Direct3D shader
compilers are excluded using supported windeployqt switches; the application
uses the ordinary raster widget canvas.

When updating the pinned Qt version, maintainers can regenerate the notices with
Python 3 and the upstream source archive (the script verifies SHA-256 and checks
that every attribution's referenced license is present):

```powershell
python scripts/windows/generate-qt-notices.py path/to/qtbase-everywhere-src-6.8.3.tar.xz
```

The source URL and checksum are recorded in the generator and the notice file.
This utility is not required for normal Windows builds or packaging.

CI uploads `OpenECE-v0.3.1-windows-x86_64` containing the runnable ZIP. A separate
fresh Windows runner downloads it, extracts into a path with spaces and π,
removes development and Qt plugin paths, launches from outside the package,
requires a native OpenECE window, verifies loaded Qt/Qwt/CRT modules come from the
ZIP, and closes it normally. Existing numerical and offscreen GUI tests run
unchanged beforehand; the startup check does not replace them. No GitHub Release
is published automatically. CTest, configure and plugin-loader diagnostics are
retained as separate CI artifacts even after failures.

## Validation scope and limitations

The Fedora 44 container matrix covers GCC and Clang, desktop and headless, plus
Clang ASan/UBSan in both modes. Windows CI covers MSVC Debug and Release, all
numerical, phase and GUI tests, and the native packaged launch. Existing numerical
tolerances are unchanged. Additional tests check explicit UTF-8 π bytes, dot
parsing under a German default locale, comma rejection, and screenshot paths
containing spaces and Unicode. The app has no other persistence/file workflows.

MSVC's `long double` has double precision; reference calculations retain their
independent algorithms rather than assuming extra precision. MSVC warnings and
UTF-8 encoding are target-local. ASan/UBSan validation remains on Linux Clang;
the combined sanitizer option rejects MSVC/clang-cl explicitly.

The ZIP is unsigned, has no installer or updater, and targets x86_64 only.
Hosted Windows validation does not establish DPI, GPU, accessibility or visual
correctness on every Windows 10/11 desktop. Manual checks on those systems remain
useful. App-local runtime updates require a new package. Qt 6.8.3 is pinned for
this milestone, not promised to be the newest security-maintained Qt release.
