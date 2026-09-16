# Digital Logic conventions (v0.4)

## Public model and ownership

Link `OpenECE::digital` and include `openece/digital/logic.hpp`, `circuit.hpp`,
or `truth_table.hpp`. This library uses only the C++20 standard library. It has
no Qt, Qwt, DSP, or `SampledSignal` dependency.

- `LogicValue::{zero, one}` and `GateKind::{Not, And, Or, Nand, Nor, Xor, Xnor}`.
- `evaluate_gate(GateKind, span<const LogicValue>) -> LogicValue`.
- `NodeId{uint32_t value}` is identity, independent of labels or declaration indices.
- `PrimaryInput{id, name}`, `Gate{id, kind, vector<NodeId> inputs}`, and
  `PrimaryOutput{name, source}` describe one-bit sources and named observations.
- `CircuitDefinition{inputs, gates, outputs}` owns the editable draft vectors.
- `Circuit(CircuitDefinition)` owns and validates a snapshot, resolving connections
  and caching an evaluation order. Its definition is exposed by a const lvalue-only
  accessor. There is no mutation API that could leave the cache stale.
- `Circuit::evaluate(span<const LogicValue>) -> Evaluation{gate_values, outputs}`.
- `truth_table(const Circuit&) -> TruthTable{input_names, output_names, rows}`;
  each `TruthTableRow` owns `inputs` and `outputs`.

Definitions, circuits, and results own their storage. Copying copies values; moving
transfers storage. Use moved-from circuits only for destruction or reassignment.
Input spans are borrowed only during a call. A borrowed definition reference expires
when its circuit is destroyed or reassigned. No result borrows from its caller.
The GUI may retain an invalid draft; only a successfully constructed `Circuit`
can be evaluated or supplied to truth-table generation. No missing input becomes 0.

Names are **exact byte strings**: nonempty, at most `limits::name_bytes` (128) bytes,
and unique separately among primary inputs and among primary outputs. An input and
an output may share a name. No trimming, case folding, Unicode normalization, or
encoding validation occurs in the core. Thus `A`, `a`, and ` A` differ; a whitespace-only
name is nonempty and accepted. The GUI converts Qt names to UTF-8, so the bound is
bytes, not displayed characters. Node IDs, not names, identify connections.

## Gate semantics

| Gate | Pin count | Result |
| --- | --- | --- |
| NOT | Exactly one | Inversion |
| AND | One or more | 1 exactly when all inputs are 1 |
| OR | One or more | 1 exactly when any input is 1 |
| NAND | One or more | Inversion of the complete AND result |
| NOR | One or more | Inversion of the complete OR result |
| XOR | One or more | 1 for an odd number of 1 inputs |
| XNOR | One or more | 1 for an even number of 1 inputs |

Unary AND/OR/XOR are identity; unary NAND/NOR/XNOR invert. XNOR is **not** an
all-equal test for three or more pins. Zero-pin gates are invalid. Invalid enum
values are rejected, including assignments on unused primary inputs.

Two-state logic is sufficient for fully assigned, single-driver combinational
circuits. Unknown/High-Z would require new propagation and driver-resolution
semantics without a v0.4 use case. Future timing or sequential simulation will
need a separate state/trace design; no event, clock, bus, or placeholder API is
included now. `SampledSignal` continues to mean uniformly sampled real data.

## Connections, validation and evaluation

At least one primary input and one primary output are required. A gate-free
pass-through circuit is valid. Inputs and gates share a unique `NodeId` namespace;
outputs are observations and do not define additional nodes. Each pin and each
output refers to exactly one input or gate. Fan-out and repeated source pins are
valid. All definitions are validated, including unused nodes and disconnected
subgraphs. Gates may precede their sources in declaration order.

Construction validates counts, names, kinds, arities, IDs and references, then
uses iterative Kahn topological ordering. A deterministic FIFO starts with gates
whose gate dependencies are zero in declaration order. Dependencies are traversed
in declaration/pin order. Repeated pin dependencies are counted and removed with
the same multiplicity. No recursive evaluation or unordered-map iteration defines
observable ordering. Construction and each evaluation are O(nodes + references),
with bounded owning buffers.

If not all gates can be ordered, construction throws: a combinational cycle exists.
The diagnostic names **unresolved/blocked gate IDs**, in declaration order. These
include cycle members and may include downstream gates; it does not claim that
every listed gate directly participates in a cycle. Self-loops and disconnected
cycles are rejected too. There is no implicit feedback or fixed-point iteration.

Assignments follow primary-input declaration order. Gate result values follow gate
**declaration** order, even when computation uses a different topological order.
Outputs follow output declaration order. No previous evaluation state is retained.
Malformed definitions and assignments throw `std::invalid_argument`; resource
violations throw `std::length_error`. Allocation failures may propagate.

## Truth tables and limits

Columns follow input/output declaration order. Row numbers count upward in binary,
with the **first declared input most significant**: two inputs A,B produce 00, 01,
10, 11. Unused inputs remain columns and contribute combinations. Generation does
not change the GUI toggles or mutate the circuit. The half-adder example is:

| A | B | Sum (XOR) | Carry (AND) |
| --- | --- | --- | --- |
| 0 | 0 | 0 | 0 |
| 0 | 1 | 1 | 0 |
| 1 | 0 | 1 | 0 |
| 1 | 1 | 0 | 1 |

Core limits are centralized in `circuit.hpp`, namespace `openece::digital::limits`:

| Resource | Inclusive maximum |
| --- | --- |
| Primary inputs | 64 |
| Gates | 4096 |
| Primary outputs | 256 |
| References (gate pins plus output sources) | 16384 |
| Name length | 128 bytes |
| Truth-table inputs / rows | 10 / 1024 |

Truth-table input count is checked **before** shifting or allocating exponential
storage. Tables omit internal gate values, bounding retained table data to input
and output columns. The node/reference limits also bound total table evaluation
work. The pure single-gate helper borrows a span and is linear in its pin count;
circuit resource limits apply when compiling circuits, not to that allocation-free helper.

Desktop limits are centralized in `gui/digital_logic_view.hpp`: 8 inputs, 64 gates,
16 outputs and 8 pins per gate. At most 256 rows can be requested through this UI.
These bounds support synchronous execution; they are not real-time guarantees.

## Desktop workflow

1. Choose **Digital Logic** in the domain sidebar; Signals / DSP is the default.
2. Start with the editable half-adder. Double-click names to rename them. Check an
   input for 1; leave it unchecked for 0.
3. Select a gate row, choose its type, pin count and source for each pin. Changing
   type preserves connections; selecting NOT with multiple pins is visibly invalid
   until you explicitly reduce the count to one. Reducing the pin count removes
   the trailing pins as requested; increasing it adds unconnected pins.
4. Add or remove inputs, gates and outputs. New gates/outputs start unconnected.
   Removing a source preserves its references as **Missing node ID**; repair them
   explicitly with the source selectors. IDs are never reused during this session.
5. Choose **Evaluate** for settled gate and output values, or **Generate truth
   table** for every combination. Any draft/assignment edit clears old values and
   the table. Invalid drafts retain their edited contents and display the error.
6. Switch domains freely: both pages retain their state.

GUI-generated IDs start at 1; the GUI represents an unconnected pin by a reference
to absent ID 0. The core itself permits ID 0 as an ordinary node identity. It never
interprets it as a logical constant or repairs that reference. The draft UI can
therefore remain incomplete without weakening the validated core API.

This is a table-based editor, without schematic wiring, saving/loading, undo/redo,
Boolean expressions, minimization, or Karnaugh maps. There is no time axis: values
are settled Boolean results, not physical transient behavior. Propagation delays,
clocks, latches, flip-flops, FSMs, buses and HDL are deferred until a separate design
establishes their state, scheduling and correctness contracts.
