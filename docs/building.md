# Build and test

## Fedora 44

Use CMake 3.24+, a C++20 compiler, Ninja, Qt 6.4+, Qt-6-built Qwt 6.2+,
GoogleTest 1.12+, Eigen 3.4+ and nlohmann/json 3.12+. Eigen and JSON are private
header dependencies; neither adds a runtime DLL. Fedora CI uses system packages:

```bash
sudo dnf install gcc-c++ clang compiler-rt cmake ninja-build git-core pkgconf-pkg-config qt6-qtbase-devel qwt-qt6-devel gtest-devel eigen3-devel json-devel clang-tools-extra
git clone https://github.com/codyklein/open-ece.git
cd open-ece
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev
./build/dev/gui/openece
```

Fedora CI disables only `fedora-cisco-openh264` for its install command to avoid
an unrelated optional mirror dependency; OpenECE does not need that repository.
For a numerical/project-codec build without Qt/Qwt:

```bash
cmake --preset headless
cmake --build --preset headless -j 4
ctest --preset headless
```

The headless preset uses GCC by default. Keep compiler variants in separate build
directories. Repeat the following with `-DOPENECE_BUILD_GUI=OFF` and a different
build directory for Clang headless:

```bash
cmake -S . -B build/clang-desktop -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build/clang-desktop -j 4
ctest --test-dir build/clang-desktop --output-on-failure
cmake --preset sanitize
cmake --build --preset sanitize -j 4
ctest --preset sanitize
cmake -S . -B build/sanitize-gui -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DOPENECE_ENABLE_SANITIZERS=ON
cmake --build build/sanitize-gui -j 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/sanitize-gui --output-on-failure
```

The sanitizer preset is headless Clang ASan/UBSan. Keep leak detection enabled;
run outside process-inspection-restricting sandboxes when necessary. Offscreen
GUI tests do not require a display server. Interactive use needs a desktop session.

## Windows

Use the [Windows guide](windows.md) for Visual Studio 2022 Desktop development
with C++, CMake, exact Qt 6.8.3 x64 acquisition, explicit dependency bootstrap,
Debug/Release presets and portable packaging. Normal configure never downloads.
Do not mix MinGW/MSVC or Debug/Release libraries. MinGW is not a validated target.

## Contributing

All six Fedora variants plus MSVC Debug/Release and fresh packaged startup and
persistence must pass for release readiness. Read [development](development.md)
and [CONTRIBUTING](../CONTRIBUTING.md). CI definitions, not historical test counts,
are authoritative for the current matrix.
