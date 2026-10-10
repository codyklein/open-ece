# Release readiness and packaging

A release is a separate authorized step, not an automatic consequence of passing
CI. OpenECE 1.0.0 completed its five stabilization checkpoints and is published.
OpenECE 1.0.1 is also published after automated and physical Windows 11 acceptance.
Future releases require their own exact-artifact acceptance and authorization.
Published tags and assets are immutable.

1. Review scope, warnings, sanitizer logs, current documentation, shipped example
   expectations, schema-1 fixture compatibility and schema-2 round trips. Do not
   update immutable compatibility fixtures to conceal a breaking change.
2. At final candidate freeze, align the CMake project version, GitHub Actions
   artifact names/paths, Windows guide/runtime notes and candidate records. Current
   branch/package metadata is **1.1.0** for authorized release preparation.
   Published 1.0.0/1.0.1 records remain unchanged. A version update alone is not
   signed-candidate acceptance or publication authorization.
   About uses the actual CMake version; no independent UI version string is maintained.
3. Run all six Fedora configurations and Windows MSVC Debug/Release. Use the
   [Windows package script](windows.md) to deploy Release Qt/Qwt/CRT and plugins,
   MIT and dependency notices, ordinary examples and offline documentation.
   The build/install/package steps are explicit; normal configure remains offline.
4. With separate approval, dispatch the [protected signing workflow](windows-signing.md)
   at the reviewed commit and approve the `windows-signing` environment for that
   commit only. Sign OpenECE and Qwt, preserve vendor signatures, verify all
   EXE/DLL files, then generate the manifest and ZIP. Ordinary eight-job CI is
   unsigned and never suffices for release distribution.
   Verify the complete SHA256SUMS manifest and absence of Debug/development
   artifacts. Test the **same ZIP** on the fresh packaged-runtime runner, including
   startup, persistence and inert example round trips. Record its SHA-256, file and
   manifest counts. Do not rebuild or recompress after validation.
5. Checkpoint-4 physical Fedora/Windows 11 retests are recorded. The final 1.0.0 RC smoke passed. The
   [bounded branding checks](branding.md) passed on the signed 1.0.1 artifact. For
   the 1.1.0 candidate, complete the remaining Fedora compressed-editor scrolling
   check and the signed Windows 11 transient checklist. For every candidate, record
   OS/build/scaling, user context, hash and failures honestly. A hosted Server run
   is not physical Windows 10/11 evidence; leave an unverified platform criterion
   open or explicitly narrow support before release.
6. Only after approval: normal merge preserving checkpoints, green post-merge main
   CI, annotated tag and GitHub release. Attach the verified inner Windows ZIP,
   re-download it and compare SHA-256. Do not substitute the outer CI artifact wrapper.

Licensing: OpenECE is MIT. Qt/Qwt are dynamically linked and retain their respective
terms; shipped `licenses/`, `THIRD-PARTY-NOTICES.txt` and `LICENSE` must remain with
redistributions. Eigen and nlohmann/json are private header dependencies with no
additional runtime DLL. Branding wordmarks are outlined from Noto Sans SemiBold;
retain its OFL attribution, with no font binary/runtime dependency. Pins and checksums are in the explicit Windows bootstrap;
update notices and run full validation for any justified dependency change.

Historical v0.x counts and benchmarks are [archived separately](validation-history.md).
No installer, updater, activation system, migration framework or new engineering
capability is part of this release-readiness process.

## 1.1.0 release preparation

Use [the finalized notes](release-notes-v1.1.md) and the exact physically accepted
`OpenECE-v1.1.0-windows-x86_64.zip` plus its SHA-256 sidecar. Obtain explicit
signing approval, protected-environment approval, physical acceptance and final
merge/publication approval separately. Preserve normal merge history and wait
for post-merge CI before the annotated tag. Re-download the published asset and
independently verify its hash. Do not rebuild, re-sign or recompress after acceptance.
GitHub source archives remain the established Linux/source deliverables; no new
Linux binary package or installer is implied. Keep existing tags/assets immutable.
