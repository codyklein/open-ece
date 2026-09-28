# v0.9 schema-1 compatibility fixtures

Frozen provenance: annotated release tag `v0.9.0`, commit
`62bdd7fe3b5a974983d03f29e125a28147e320f3`.

- `incomplete.openece` is byte-for-byte `tests/fixtures/project-v1.openece` from that tag.
- `complete.openece` is the output of `encode_project(default_project())` compiled
  from that tag's `project/src/{model,codec}.cpp` and `project/include/`, using
  nlohmann/json 3.12.0 and C++20. The generator writes the returned UTF-8 bytes to
  stdout without adding a newline. It uses no current candidate model/codec code.

These are compatibility evidence, not evolving example templates. Do not regenerate
from current defaults to fix a test. The complete file covers all workspaces;
the incomplete file covers Unicode, raw invalid phase/resistance/pin/bit text and
dangling digital/DC references. Both retain IDs, allocator positions, units and
selections. Tests pin bytes by SHA-256, round trip semantic state and restore
inert workspaces. Existing broader invalid-draft tests remain intact.

- `complete.openece`: `0ba53c8e653ebbbc8720960743b40c3781ec717bbcaa3c718dfe1599a0e0ad0f`
- `incomplete.openece`: `0b9b4580268de760fb4b27901fabeb3655ee94af2a4885fc13466e8e7ecc3ac2`

`.gitattributes` pins these fixtures to LF on every checkout so Windows CRLF conversion cannot invalidate their byte-level provenance. Ordinary project parsing still accepts JSON whitespace normally.
