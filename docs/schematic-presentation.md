# Schematic presentation model — checkpoint 1

The internal `openece_schematic_model` target holds Qt-independent, owned circuit
presentation data. It is built and tested in headless configurations. This document
records the checkpoint-1 model contract. The 1.2.0 candidate consumes this same model
through [verified layout](schematic-layout.md), the [Qt renderer](schematic-renderer.md)
and [read-only workspace views](schematic-workspaces.md). Numerical APIs and project
schemas remain unchanged.

`openece::schematic::project_schematic()` accepts a const DC, AC or Transient project
draft and returns a `SchematicGraph` with const access to its owned node/component
vectors and ground reference. Those vectors retain declaration order. NodeKey and
ComponentKey are separate types, each containing analysis identity and the original
32-bit ID. Neither row positions nor names resolve identity; numeric IDs never
size arrays. The same ID in different analyses does not identify the same object.

Each node reference is exactly one of Unselected, DeclaredNode or DanglingNode.
Declared means only that the ID exists in the draft, not that the circuit is
electrically valid. Ground uses the same representation; ID 0 has no special
meaning. Missing references do not create nodes, and tied terminals, unused nodes,
floating circuits, duplicate/empty names and invalid numeric text remain intact.
Current-source connections are included regardless of solver-reference behavior.

Quantities contain raw text and stable unit tokens, with no parsing, normalization,
unit conversion or phasor construction. AC phase is retained on every component,
including inactive passive fields. Transient source mode and every ordered source
point are copied, including points retained in constant mode or on passive
components. The component value is the initial source quantity; point amplitudes
use its unit, and each point retains its own raw time quantity. AC source quantities
remain RMS/cosine-reference quantities; Transient values remain instantaneous.

The projection is not a second editable document or a persistence format. It does
not copy allocator counters, analysis requests, probes, storage initialization,
navigation, solver results or runtime execution state. All remain in their existing
authoritative locations; projection neither alters them nor calls a solver.

Defensive storage checks reuse project node/component, per-source/aggregate point,
cell-text and aggregate-text limits before materializing the copied graph. Duplicate
declared node/component IDs raise typed ProjectionError diagnostics rather than
merging objects. These checks are not full schema or engineering validation: raw
kind/unit/mode tokens are retained, not interpreted. Project-file acceptance still
belongs to the existing codec. Numerical ranges and solvability remain solver concerns.

The graph deliberately has no positions, wire segments, graphics objects, selection,
editing commands or persisted schematic fields. Layout/rendering derive
those from explicit terminal keys without changing the electrical draft.

Checkpoint 2 adds a **separate**, Qt-independent layout layer consuming this unchanged
graph. See [schematic-layout.md](schematic-layout.md) for geometry, strategies,
verification, resource limits and review examples. The model itself remains free of
positions, rendering and persisted schematic state.
