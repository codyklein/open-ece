# Schematic layout geometry — checkpoint 2

This internal, headless facility derives a complete owned diagram from the approved
`SchematicGraph`. It does not edit circuits, run a solver, introduce a second editable
electrical model, or add saved fields. This document records the checkpoint-2
contract. The 1.2.0 candidate links it through the accepted [Qt renderer](schematic-renderer.md)
and [workspaces](schematic-workspaces.md). The layout target itself remains
Qt-independent; project schemas and numerical APIs are unchanged.

## API and geometry

`gui/schematic/layout.hpp` exposes:

```cpp
LayoutResult layout_schematic(const SchematicGraph&, const LayoutMetrics& = {},
                              const LayoutLimits& = {});
VerificationResult verify_layout(const SchematicGraph&, const LayoutDiagram&,
                                 const LayoutMetrics&, const LayoutLimits& = {});
```

`LayoutResult` holds either a complete verified `LayoutDiagram` or a typed
`LayoutFailure`. Failure never carries a partial diagram. Categories are
`invalid_metrics`, `resource_limit`, `invalid_geometry` and `connectivity`.
The public boundary also translates allocation failure into a resource diagnostic.

Coordinates are signed 64-bit **integer diagram units**, not physical pixels.
Placement/routing is orthogonal and uses a caller-selected integer grid. The Qt
renderer supplies its own view transform. Symbols carry their kind, center, body box,
positive and negative anchors, and positive-to-negative direction. These semantic
descriptors cover resistors, capacitors, inductors, independent voltage/current
sources and generic components; electrical glyph strokes belong to the Qt renderer.
Ground has a separate body/anchor and an explicit net-tagged lead.

Every placement retains owned original component or node metadata. AC quantities
retain raw RMS cosine-reference magnitude/phase; Transient quantities retain raw
instantaneous source mode and ordered points, including inactive records. Capacitors
and inductors remain their original symbols, never numerical companion elements.
`LayoutDiagram::analysis` remains available to present the correct analysis context.

Names are not identities. Display labels append `[ID]` to original names, including
empty and duplicate names. `node_label_text()` and `component_label_text()` supply
the exact strings a caller should measure. Raw original names/values remain intact
in inspection metadata. Nothing is parsed, normalized or elided.
Source-mode labels apply only to independent sources; passive components' inactive
source records remain in inspection metadata without being described as excitation.
The metrics contain default label boxes and optional per-owner/role measured extents;
duplicate/unknown measurement keys and nonpositive dimensions are rejected. Defaults are nominal
geometry for tests, **not a claim that all Unicode/font combinations fit**. The Qt
integration must measure using its chosen font and diagram units. It must either
accommodate full labels or explicitly elide their displayed form while retaining
full text for inspection; silent clipping is not an acceptable rendering policy.
No elision is performed by this layout layer.

The diagram freezes marker radius, label boxes and complete bounds. Bounds include
all symbols, labels, routes, terminal/net markers, junction/crossing marks, ground,
and one clearance of padding. No borrowed draft, graph or widget pointers survive.

## Conservative structured recognition

This is deliberately narrower than general series-parallel graph decomposition:

1. Require declared, distinct endpoints for all components and no unused nodes.
2. Find two electrical hubs with internally degree-two, edge-disjoint paths between
   them. A single open chain is allowed. All edges and nodes must be covered.
3. For a simple cycle (all degrees two), choose the endpoints of the lowest-ID voltage
   source if present; otherwise choose the lowest-ID component. An explicit ground
   hub becomes the bottom hub. Ground is never inferred from ID 0 or a name.
4. Walk paths iteratively with a visited-edge set; no recursive or exhaustive search.
5. Order adjacency and complete paths by stable component IDs. Place paths in vertical
   columns between top/bottom net backbones. Series components occupy ordered vertical
   slots; intermediate node anchors sit between them. Preserve actual polarity even
   when that requires an upward symbol/current direction.

This covers the shipped divider, RC, RL and series RLC examples, parallel components,
and several simple multi-branch circuits. It does not claim nested series-parallel,
bridge, disconnected or general planar layout. If recognition fails, or structured
geometry fails its geometric/resource checks, try the connectivity fallback using
the remaining shared operation budget. There is no unlimited retry loop.

## Complete connectivity fallback

Every declared node receives a distinct vertical labeled rail, sorted by stable ID.
Unused/disconnected nodes retain rails too. Components occupy rows to the right,
sorted by stable ID. Positive terminals lead left to their referenced rail. Negative
terminals first escape right, then return below the symbol to their referenced rail.
Row height and rail/column spacing account for supplied label boxes and clearances.

`Unselected` terminals have no wire and request an open-terminal mark. `DanglingNode`
terminals have no wire and request a missing-ID mark; detail labels identify the missing
ID. They never acquire a rail or connect to another occurrence of that dangling ID.
Unknown component kinds use a generic symbol with original token/data and a diagnostic.
Same-node terminal pairs keep separate anchors and routes to the same declared net.
Missing/dangling ground remains a diagnostic/reference, not an invented symbol.

This is an unambiguous connectivity diagram, not necessarily a textbook schematic.
The Wheatstone and K3,3 regression fixtures deliberately use this strategy. At maximum
size it can be wide/tall and have thousands of crossings. The accepted Qt canvas
provides zoom/scroll and inspection; full-diagram Fit is an overview, not a
textbook-readability guarantee. The layout itself remains headless.

## Wires, contacts and verification

Each wire owns an ordered point path, an analysis-scoped `NodeKey`, and a role:
backbone, terminal or ground. A terminal wire also owns a component/polarity key.
Layout-local wire indices are permitted for contact metadata; electrical IDs never
index arrays. Every graph component has two separately identified terminal records.

Junction records give a declared net, location and exact sorted incident-wire indices.
They include degree-two joins; later rendering may reserve visible dots for branch
points without discarding contact metadata. Every perpendicular different-net crossing
has a separate nonconnection record. Its second wire is designated to cross over the
first in the Qt renderer. Crossings do not union nets. Collinear overlaps,
different-net endpoint touches, and self-intersections are rejected as unsupported or
ambiguous rather than disguised by markers.

The verifier does not trust the selected strategy, router's annotations or layout
terminal references. It independently compares frozen metadata/IDs with the input
graph, accounts for every terminal and wire owner, reconstructs segment intersections,
requires exact junction/nonconnection records, and checks connected wire sets against
each declared net and its anchor using disjoint sets. It also checks polarity/body
geometry, distinct anchors, measured labels, symbol/label/wire collisions, explicit
ground, dimensions, complete bounds and resource limits. Checked arithmetic,
primitive intersection arithmetic and label formatting are shared utilities; the
verifier does not call recognition, placement or annotation generation.

Corruption tests remove/change entities, connect foreign nets, disconnect routing,
alter junctions/crossings, overlap anchors, change labels and exceed coordinate bounds.
The factory returns a diagram only after verification succeeds.

## Determinism and limits

Electrical identity remains analysis-scoped and type-scoped. Declaration order is
retained by the projection, but geometric ordering uses stable IDs/connectivity.
Reordering declarations therefore preserves geometry and inspection metadata. Measured
operation counters can differ because sorting/lookup work differs; they are not circuit
state or geometry. Repeated calls with identical inputs produce identical complete
values. There is no floating-point positioning, randomness or external layout library.

Centralized hard ceilings in `layout_limits` are also the defaults; callers may tighten
them. Counts/ranges are checked before large derived structures are materialized.

| Resource | Ceiling |
|---|---:|
| Declared nodes / components | 32 / 128 (existing project draft limits) |
| Terminals | 256 |
| Orthogonal routing segments | 1,024 |
| Junctions | 1,024 |
| Nonconnecting crossing markers | 16,384 |
| Label rectangles | 416 (32 + 3 × 128) |
| Total geometry primitives | 20,000 |
| Counted geometric/inspection operations, including verification and retries | 2,000,000 |
| Conservative derived working-storage capacity estimate | 32 MiB |
| Absolute coordinates and dimensions | 1,000,000 diagram units |

Operations count bounded algorithmic loop/comparison work, not CPU instructions or
text-copy bytes. Source text/point records remain bounded by the projection's existing
4 MiB text and 4,096 retained-point budgets. The working-storage preflight conservatively
budgets owned text, labels, source-point records, segment/contact maps and allocator
overhead. It is not whole-process RSS, a global allocator quota or a power-of-two estimate
based on the largest ID. Path/annotation/primitive counts are enforced as they grow.
All coordinate addition, spacing multiplication and grid alignment are checked.

The maximum dense regression has 32 nodes, 128 same-node components, 256 terminals,
545 segments, 257 junctions and 7,936 nonconnecting crossings. A separate maximum sparse-ID
fixture uses IDs near `UINT32_MAX`. Both fit defaults, including a fully measured-label
variant. Larger caller label measurements may exhaust coordinates: this produces a
typed failure, never truncation or a supposedly complete partial diagram.

## Validation and developer tools

The regular headless schematic tests include projection, layout, shipped examples,
schema-1/schema-2/pinned v0.9 fixtures, 24 bounded generated drafts, maximum storage/work
cases and adversarial verification mutations. Existing engineering/GUI suites remain
unchanged. Normal CI runs six Fedora configurations, Windows Debug/Release, and fresh
packaged runtime tests. Signing is not requested.

`openece_schematic_benchmark` is an optional **non-shipping** target, excluded from
ordinary builds and CTest. CI builds/runs it in Release on Fedora and Windows. It uses
only existing dependencies and OS SDK facilities (Psapi on Windows, getrusage on Linux)
to record whole-process peak memory. This adds no application dependency or package file.

```bash
cmake --build build/layout-release --target openece_schematic_benchmark
build/layout-release/tests/openece_schematic_benchmark
build/layout-release/tests/openece_schematic_benchmark --dump
```

Configure that directory with the existing build instructions, GUI off and build type
Release. The benchmark records one warm-up and seven complete layout+verification runs,
median/max time, operation counts, geometry counts and conservative working-storage bounds.
Whole-process peak memory includes loader/runtime, fixture decoding and retained benchmark
cases; it is not per-layout allocation or comparable bit-for-bit across operating systems.
Timings are observations, not responsiveness promises or acceptance thresholds.
Fedora's log is `schematic-layout-benchmark.log` in its GCC/headless CI artifact; Windows'
is in `windows-build-test-logs`. Use the branch-tip CI run for exact environment measurements.

The developer-only `--dump` produces the review record in
[schematic-layout-examples.md](schematic-layout-examples.md), covering divider, RC, RL,
RLC and Wheatstone geometry. It is neither SVG/image export nor a product file format.
There are no new persisted positions, project fields, runtime state, editing commands
or renderer objects. Font measurement, literal/plain-text drawing, marker legibility,
theme/DPI behavior and keyboard/inspection UX are implemented by the accepted
[Qt renderer](schematic-renderer.md); exact-candidate physical review remains separate.
