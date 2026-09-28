# Release readiness and packaging

A release is a separate authorized step, not an automatic consequence of passing
CI. The v1.0 stabilization branch must finish its five checkpoints before tagging
or publishing. Track open physical-platform gates in [release readiness](v1-release-readiness.md).

1. Review scope, warnings, sanitizer logs, current documentation, shipped example
   expectations and schema-1 fixture compatibility. Do not update immutable
   compatibility fixtures to conceal a breaking change.
2. At final candidate freeze, align the CMake project version, GitHub Actions
   artifact names/paths, Windows guide/runtime notes and candidate records. Current
   checkpoint-3 metadata is deliberately still **0.9.0**, not a released 1.0.
   About uses the actual CMake version; no independent UI version string is maintained.
3. Run all six Fedora configurations and Windows MSVC Debug/Release. Use the
   [Windows package script](windows.md) to deploy Release Qt/Qwt/CRT and plugins,
   MIT and dependency notices, ordinary examples and offline documentation.
   The build/install/package steps are explicit; normal configure remains offline.
4. Verify the complete SHA256SUMS manifest and absence of Debug/development
   artifacts. Test the **same ZIP** on the fresh packaged-runtime runner, including
   startup, persistence and inert example round trips. Record its SHA-256, file and
   manifest counts. Do not rebuild or recompress after validation.
5. Complete exact-artifact physical Windows and native Fedora checklists. Record
   OS/build/scaling, user context, hash and failures honestly. A hosted Server run
   is not physical Windows 10/11 evidence; leave an unverified platform criterion
   open or explicitly narrow support before release.
6. Only after approval: normal merge preserving checkpoints, green post-merge main
   CI, annotated tag and GitHub release. Attach the verified inner Windows ZIP,
   re-download it and compare SHA-256. Do not substitute the outer CI artifact wrapper.

Licensing: OpenECE is MIT. Qt/Qwt are dynamically linked and retain their respective
terms; shipped `licenses/`, `THIRD-PARTY-NOTICES.txt` and `LICENSE` must remain with
redistributions. Eigen and nlohmann/json are private header dependencies with no
additional runtime DLL. Pins and checksums are in the explicit Windows bootstrap;
update notices and run full validation for any justified dependency change.

Historical v0.x counts and benchmarks are [archived separately](validation-history.md).
No installer, updater, activation system, migration framework or new engineering
capability is part of this release-readiness process.
