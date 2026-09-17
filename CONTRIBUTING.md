# Contributing to OpenECE

Thanks for your interest in contributing to OpenECE.

OpenECE is a cross-platform electrical and computer engineering workbench written primarily in modern C++ and Qt. The project emphasizes numerical correctness, clear engineering conventions, modular architecture, automated testing, and support for both Fedora Linux and Windows.

## Before Contributing

For significant changes, please open an issue first so the proposed design and scope can be discussed before implementation.

Bug fixes, documentation improvements, tests, and small maintenance changes may generally be submitted directly as pull requests.

Please keep changes focused. Avoid combining unrelated refactors or features in the same pull request.

## Development Workflow

Create a feature branch from the latest `main`:

```bash
git checkout main
git pull --ff-only origin main
git checkout -b feature/your-feature-name
```

Use descriptive branch names such as:

```text
feature/circuit-analysis
feature/sequential-logic
fix/fft-normalization
docs/windows-build
chore/community-files
```

Make logical commits with concise commit messages.

Before opening a pull request:

1. Make sure your branch is current with `main`.
2. Build the project successfully.
3. Run the applicable automated tests.
4. Verify that unrelated OpenECE domains still behave correctly.
5. Confirm the working tree is clean.

## Building

OpenECE uses CMake presets.

On Fedora:

```bash
cmake --preset dev
cmake --build --preset dev -j 4
ctest --preset dev
```

Additional presets are available for headless and sanitizer builds.

See the project documentation for detailed Linux and Windows setup instructions.

## Testing

New behavior should include appropriate tests.

For engineering and numerical functionality, prefer tests based on:

- analytically known results
- independently calculated reference values
- boundary and invalid-input cases
- resource-limit behavior
- regression cases for previously discovered bugs

Avoid relying only on GUI output to establish numerical correctness.

Changes affecting the GUI should include appropriate Qt workflow tests when practical.

Existing tests should not be weakened merely to make new functionality pass.

## Numerical and Engineering Conventions

OpenECE intentionally documents its mathematical and engineering conventions.

When adding or modifying engineering functionality:

- Define units explicitly.
- Document sign and polarity conventions.
- Define normalization and scaling behavior.
- Handle numerical edge cases deliberately.
- Avoid silently correcting malformed or physically invalid inputs.
- Preserve deterministic behavior where possible.

If a design decision has multiple reasonable engineering interpretations, document the chosen convention.

## Architecture

Keep engineering logic independent of the GUI whenever practical.

The project currently separates major domains into independent libraries, including:

- Signals / DSP
- Digital Logic
- Circuit Analysis

Do not force unrelated domains into a shared data model merely to reduce the number of types.

Qt-specific behavior belongs in the GUI layer unless there is a clear architectural reason otherwise.

## Cross-Platform Support

OpenECE supports Fedora Linux and Windows.

New features should not knowingly break either platform.

Changes affecting:

- CMake
- dependencies
- compiler options
- Qt behavior
- packaging
- filesystem behavior
- Unicode handling

should be validated carefully across supported platforms.

GitHub Actions performs the primary cross-platform validation.

## Formatting and Code Style

Follow the existing project style.

Use modern, readable C++ and prefer explicit ownership and lifetime semantics.

Run the project's formatting checks before submitting a pull request.

Avoid introducing unnecessary dependencies or abstractions.

## Pull Requests

A pull request should explain:

- what changed
- why the change is needed
- important design decisions
- testing performed
- any known limitations or technical debt

Large features should be divided into logical commits where practical.

Do not include generated build directories, local IDE configuration, or unrelated formatting changes.

## Reporting Bugs

When reporting a bug, please include:

- OpenECE version or commit
- operating system
- compiler/toolchain if relevant
- steps to reproduce
- expected behavior
- actual behavior
- screenshots or logs when useful

For numerical issues, include the exact engineering parameters needed to reproduce the result.

## Feature Requests

Feature requests should explain:

- the engineering use case
- the proposed behavior
- why the feature fits OpenECE
- any relevant mathematical or UI conventions

Large requests may be split into multiple milestones.

## License

By contributing to OpenECE, you agree that your contributions will be licensed under the same license as the project.
