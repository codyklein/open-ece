# 1.0 candidate dependency and license review

Reviewed during checkpoint 5 (2026-10-04). No dependency version, acquisition
checksum, API or algorithm changes are part of this freeze. Fedora uses system
packages; these are Windows bootstrap pins, not universal Fedora versions.

| Dependency | Windows pin | Use / notice |
| --- | --- | --- |
| Qt Base | 6.8.3 | Shared Core/Gui/Widgets and runtime plugins; original licenses and bundled notices in `licenses/Qt/Qt-6.8.3-NOTICES.txt` |
| Qwt | 6.3.0 | Shared plotting library; Qwt License 1.0 in `licenses/Qwt/COPYING` |
| GoogleTest | 1.17.0 | Tests only, absent from application runtime; BSD notice retained for provenance |
| Eigen | 5.0.0 | Private header-only dense solve, `EIGEN_MPL2_ONLY`; MPL2 notice in `licenses/Eigen/` |
| nlohmann/json | 3.12.0 | Private header-only codec; MIT and embedded Hedley notice in `licenses/nlohmann-json/` |
| MSVC runtime | VS 2022 runner toolset | Release redistributable DLLs discovered by Qt/CMake, governed by Microsoft terms |

OpenECE is MIT. [Third-party notices](../packaging/THIRD-PARTY-NOTICES.txt) retain
source links and library replacement information. Packaging copies the original
license files from checksum-verified source archives. CMake configuration remains
offline; explicit bootstrap verifies/reuses pinned dependencies. Qt acquisition
pins its SDK version; CI actions and aqtinstall are also pinned. Final manifest
verification must include notices, licenses, all runtime files and examples.

## Advisory review and limits

Qt 6.8.3 is a retained validated pin, **not a claim of the latest security fixes**.
The [Qt advisory index](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products)
lists later fixes affecting Qt facilities. In particular,
[CVE-2025-5455](https://www.qt.io/blog/security-advisory-recently-discovered-issue-in-qdecodedataurl-in-qtcore-impacts-qt)
affects malformed data URLs; [CVE-2025-5992](https://www.qt.io/blog/security-advisory-recently-reported-denial-of-service-issue-in-qcolortransfergenericfunction-impacts-qt)
affects crafted color profiles. Project loading does not accept URLs, HTML,
images or external assets: it uses the bounded JSON codec, with user-controlled
messages rendered as plain text. Rich help is fixed application text. No Qt
network/XML/SVG parsing API is invoked by OpenECE project handling. This is a
source-path assessment, not proof that every OS/native-dialog/library path is
free of defects. Do not use this record as a blanket security certification.

The [JSON upstream security page](https://github.com/nlohmann/json/security)
had no published advisories at review. [Eigen release notes](https://gitlab.com/libeigen/eigen/-/blob/5.0.1/CHANGELOG.md)
and [GoogleTest releases](https://github.com/google/googletest/releases) describe
newer bug-fix releases; no exercised-path defect requiring a pin change was
identified in this bounded review. [Qwt](https://qwt.sourceforge.io/index.html)
documents the retained 6.3.0 license/platform baseline. Absence of an identified
applicable issue is not a promise of ongoing security support.

A future justified dependency update requires notice review, the full regression
matrix and fresh exact-artifact physical checks. App-local DLLs do not update
with the operating system; users must obtain a rebuilt package. This candidate
keeps the physically tested pins rather than changing dependencies during freeze.
