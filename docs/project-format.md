# OpenECE project schema 1

Status: checkpoints 1–4 provide the model/codec, authoritative GUI bindings,
transactional file storage, document-controller APIs, and File-menu workflows
with unsaved-change prompts and application-global recent projects. See [project-transactions.md](project-transactions.md).
The complete, loadable cross-domain fixture is `tests/fixtures/project-v1.openece`.
It deliberately includes pending phase text, missing node references, an incomplete
timing pin list, and odd QPSK input. None is a structural project error.

## Encoding and compatibility

Files use the `.openece` extension and UTF-8 JSON. The root is an object with
`format` exactly `"org.openece.project"` and integer `schema_version` exactly `1`.
Application version does not determine schema version. `1.0`, `"1"`, booleans,
missing headers and unsupported versions are rejected. A UTF-8 BOM is accepted by
the reader; the writer emits none. Comments, trailing commas, NaN/Infinity literals,
invalid UTF-8, unpaired escaped surrogates and duplicate object keys are rejected.
Duplicate checks compare decoded keys, including in unknown objects.

Every field listed below is required. There are no optional known fields and no
missing-field defaults. Null is legal only for explicitly nullable references.
Unknown optional fields are accepted within the same resource limits, reported
as JSON Pointer paths in `DecodedProject::ignored_fields`, and discarded on save.
The GUI must show that warning; this is not lossless forward compatibility.
Required new semantics must change the schema version. No migration is implemented.

Object key order is irrelevant; the writer uses deterministic keys/indentation.
Array order is meaningful: declarations, output ordering, ordered pins, and stimuli
are not sorted or deduplicated. Strings preserve decoded UTF-8 bytes, including
spaces, line breaks, Unicode normalization form, and embedded U+0000. JSON escape
spelling itself is not preserved. Display names never identify references.

## Common field types and limits

- `Text`: UTF-8 string, at most 4096 bytes and 4096 UTF-16 code units. Names and
  ordinary timing table cells use Text. Empty/duplicate names are legal drafts.
- `Edit(N)`: UTF-8 string of at most N UTF-16 code units and 524288 UTF-8 bytes.
  Limits in UTF-16 units match Qt editor capacities without truncating supplementary
  characters. These are storage limits, not numerical execution limits.
- `Quantity(U,N)`: object with required `text: Edit(N)` and `unit: Token(U)`.
  N defaults to 128. Text is never parsed during persistence. Numeric draft input
  uses C-locale notation at execution; a comma or incomplete exponent remains savable.
- `Token(a|b)`: exact case-sensitive string from the listed set, at most 64 bytes.
- `Id`: canonical unsigned decimal string in `0..4294967295`, no signs, whitespace
  or leading zeroes (except `"0"`). Digital combinational declarations exclude zero.
- `Ref`: Id or null. Null means intentionally unselected/unconnected. A well-formed
  dangling Id is accepted without checking whether it resolves.
- `Next`: canonical decimal string in `0..4294967296`. The last value is the
  exhausted sentinel, not an allocatable ID.
- `Bool`: JSON true or false, never a numeric/string substitute.
- `List(T,N)`: ordered array of T, from zero through N elements. Empty drafts are
  accepted. Lists/rows are never silently padded, repaired or removed.

Counts editable in spin boxes are strings too. Examples include taps, pin count,
AC points, and BER budget. Values outside execution limits remain savable. A pending
pin-count edit and the current stored pin list can differ; restoring must not resize
connections just because a count editor contains parseable text.

## Root and Signals/DSP

Root fields: `format`, `schema_version`, `selected_domain`, `signals`, `digital`,
`circuits`, `communications`. `selected_domain` is
`signals|digital|circuits|communications`. Domain fields contain objects below.

Signals object (`SignalsDraft`):

| Field | Type |
|---|---|
| amplitude | Quantity(1) — dimensionless signal amplitude |
| frequency, sample_rate, cutoff | Quantity(Hz) |
| phase | Quantity(deg\|rad,32767) |
| duration | Quantity(s) |
| window | Token(rectangular\|hann_periodic) |
| filter | Token(off\|fir_lowpass) |
| taps_text | Edit(128) |
| selected_tab | Token(signals\|help\|response) |

Disabled filter controls are retained. No samples, FFT, FIR coefficients, spectra,
response values, status summaries or plots are saved.

## Digital workspace and combinational draft

Digital object (`DigitalDraft`): `selected_tab: Token(combinational|timing)`,
`copy_delay: Quantity(ps)`, `combinational: CombinationalDraft`, `timing: TimingDraft`.
Copying to timing is an explicit operation, never performed during loading.

Combinational fields: `next_id: Next`, `inputs: List(DigitalInput,8)`,
`gates: List(DigitalGate,64)`, `outputs: List(DigitalOutput,16)`.

| Record | Required fields |
|---|---|
| DigitalInput | id: Id; name: Text; high: Bool |
| DigitalGate | id: Id; kind: Token(not\|and\|or\|nand\|nor\|xor\|xnor); pins: List(Ref,8); pin_count_text: Edit(128) |
| DigitalOutput | name: Text; source: Ref |

Inputs and gates share one unique nonzero ID namespace. Zero references must use
null, matching the existing editor's internal zero sentinel. Gate arity, missing
sources and cycles are execution concerns, not persistence validation. Inputs retain
assignments by their record, not by name or a separately reordered value vector.
No gate values, output values or truth tables are saved.

## Timing/sequential draft

Timing fields: `next_id: Next`, `inputs: List(TimingInput,8)`,
`elements: List(TimingElement,64)`, `outputs: List(TimingOutput,16)`,
`stimuli: List(Stimulus,10000)`, `horizon: Quantity(ps,13)`,
`observed_text: Edit(192)`, `display_unit: Token(ps|ns|us|ms|s)`,
`selected_tab: Token(inputs|elements|outputs|stimuli)`.

| Record | Required fields |
|---|---|
| ClockDraft | first_edge_text: Text; high_text: Text; low_text: Text; unit: Token(ps) |
| TimingInput | id: Id; name: Text; initial_text: Text; clock: ClockDraft |
| TimingElement | id: Id; kind: Token(not\|and\|or\|nand\|nor\|xor\|xnor\|sr_latch\|d_latch\|dff_rising\|dff_falling); pins_text: Text; delay: Quantity(ps,4096); initial_q_text: Text |
| TimingOutput | name: Text; source_text: Text |
| Stimulus | time: Quantity(ps,4096); input_text: Text; value_text: Text |

Clock fields are always present; all three blank means no clock at execution.
Partial clocks remain partial. Input/element IDs are fixed identities in a shared
namespace separate from combinational logic. Timing ID zero is allowed by the
existing timing core. Reference cells remain raw text because the current editor
allows incomplete/malformed ID lists. No parsing or rewriting of those cells occurs
on load. All time input is in ps; display_unit changes only diagram presentation.
No event queues, traces, captured/visible simulation state, elapsed time or progress
are saved. Initial Q is editable initialization configuration and is saved.

## Circuits workspace, DC and AC

Circuits object: `selected_tab: Token(dc|ac)`, `dc: DcDraft`, `ac: AcDraft`.
Each DC/AC object requires `next_node: Next`, `next_component: Next`,
`nodes: List(Node,32)`, `components: List(Component,128)`, `ground: Ref`.
Node record: `id: Id`, `name: Text`.
Node and component IDs occupy separate namespaces; DC and AC are independent.
Zero is an ordinary node ID, never an implicit reference. Ground can be null or dangling.

DC additionally requires `selected_tab: Token(editor|help)`.
DC component fields: `id: Id`, `name: Text`,
`kind: Token(resistor|voltage_source|current_source)`, `positive: Ref`, `negative: Ref`,
`value: Quantity(U)`.

AC additionally requires:

| Field | Type |
|---|---|
| frequency, start, stop | Quantity(Hz\|kHz\|MHz\|GHz) |
| count_text | Edit(128) |
| spacing | Token(linear\|logarithmic) |
| mode | Token(transfer\|absolute) |
| probe_positive, probe_negative | Ref in node namespace |
| reference_source | Ref in component namespace |
| selected_tab | Token(editor\|single\|sweep\|help) |

AC component fields: DC component fields plus `phase: Quantity(deg)`; kind allows
`resistor|capacitor|inductor|voltage_source|current_source`. Phase text is retained
even for passive components with disabled phase editors. Source magnitude is RMS
and phase is cosine-reference as in the existing AC contract.

Allowed component value units U:

| Component | DC | AC |
|---|---|---|
| resistor | ohm, kohm, Mohm, mohm | ohm, kohm, Mohm |
| voltage_source | V, mV, uV | V, mV, uV |
| current_source | A, mA, uA | A, mA, uA |
| capacitor | not supported | F, uF, nF, pF |
| inductor | not supported | H, mH, uH |

Prefix multipliers are fixed by these tokens; they are never inferred from names.
The current GUI has a 32-voltage-source execution limit. Persistence permits any
mix within 128 component rows, because a draft exceeding that execution limit must
still save. No source-count solvability check, topology repair, grounding, SI value
conversion, phasor conversion or solver construction happens during decoding.
No solutions, matrices, frequency grids, sweep results/progress or caches are saved.

## Communications draft

| Field | Type |
|---|---|
| modulation | Token(bpsk\|qpsk) |
| source_mode | Token(random\|manual) |
| manual_bits_text | Edit(131072) |
| bit_count_text, samples_text, points_text, budget_text | Edit(128) |
| bit_seed_text, noise_seed_text | Edit(64) |
| symbol_rate | Quantity(symbol/s\|ksymbol/s\|Msymbol/s,32767) |
| eb_n0, start, stop | Quantity(dB,32767) |
| noise_enabled | Bool |
| selected_tab | Token(waveforms\|constellation\|ber\|help) |

The runtime limits (65536 GUI samples/bits, 41 BER points, 1M bits per point, 10M
aggregate bits) are not JSON numerical constraints. Odd QPSK counts, malformed
manual bits and seed overflow remain editable drafts. Bit/noise seed text is saved;
PRNG engines, cached Gaussian values, generated bits/waveforms, decisions and BER
results are not. Restoring never resumes an experiment.

## Allocator invariant

Every namespace counter is in range and strictly greater than every declared ID
in that namespace. Combinational next_id is at least 1. No counter must exceed
retained dangling IDs: instead each allocation skips reserved IDs before returning.
Counters only advance, never wrap, and become 4294967296 on exhaustion. Deletion
does not reduce a counter. Duplicate fixed identities are structural errors;
duplicate editable display names are not.

Reservation includes declared IDs, all retained typed references (including ground,
AC probes/reference source), and timing pin/output/stimulus/observed references.
For raw timing cells, reserve every maximal ASCII decimal run representable as
uint32, including leading zeros and runs inside malformed text. This conservative
rule can skip more IDs than execution parsing would use, but cannot silently reconnect
an invalid draft and never rewrites its text. Empty/unparseable/overflow runs reserve
nothing. Timings/delays and ordinary names do not reserve IDs. Output names have no
independent identity. Public allocator callers must provide the appropriate
reserved_ids/reserved_node_ids/reserved_component_ids set from the authoritative draft.

## Untrusted input budgets and errors

Limits are centralized in `openece::project::limits`: 8 MiB input/output, depth 32,
200000 JSON values, 128 members/object, 10000 elements/any array, 524288 decoded
bytes/string, 256 decoded bytes/key, 4 MiB aggregate decoded string/key bytes, and
12000 aggregate typed array entries (including pin references).
All fields have the more specific limits above; all unknown data uses the same
parser budgets. No file may introduce external paths, assets, commands or plugins.
Unknown text is never dereferenced or executed.

Read is bounded before parse. A lexical preflight validates UTF-8 and bounds decoded
string storage and numeric token length (64 bytes) before the JSON lexer allocates
its token buffers. A strict SAX pass rejects duplicate keys and bounds nesting,
container members, array entries and total values before DOM construction. Explicit
field decoders then check type and per-field limits before allocating draft vectors.
Encode validates model limits first and rechecks emitted bytes against parser budgets.
No permissive automatic DTO conversions/default-filled decoding are used.

`Error` carries ErrorCode, JSON Pointer path where available, optional parser byte
offset and a message. Codes: malformed_json, invalid_utf8, duplicate_key,
wrong_format, unsupported_version, missing_field, wrong_type, invalid_value,
invalid_identity, duplicate_identity, invalid_allocator_state, resource_limit.
Invalid enum/unit tokens use invalid_value. Lexer/SAX-wide errors may have no field
path. Allocation failures are not hidden as engineering errors.

## Ownership and next checkpoints

ProjectSnapshot is the authoritative editable model; numeric libraries are unchanged.
GUI controls edit it. A narrow capture synchronization transfers pending editor
text without parsing, normalization, conversion, validation or execution. Selected
user tabs/domain are persisted; automatic result-tab navigation is transient.
The codec contains no Qt, domain solver types, JSON types in public headers, files,
preferences, or global mutable state. Copies are owned independent values.

Qt file storage uses QSaveFile with direct-write fallback disabled: write checked
bytes then commit(), with no public flush/close finalization contract. Transactional
Open prepares and verifies a complete inert replacement session before replacing
live state. The controller is implemented; user-facing file workflows remain checkpoint 4.

`ProjectWorkspace(snapshot, true)` validates storage structure, creates all domain
views without executing engineering operations, and verifies that capture equals
the supplied snapshot. Child views borrow subdrafts; the session destroys its views
before the owned snapshot. `capture()` first synchronizes exact active editor text,
then copies the model. It does not validate engineering inputs. Standalone views
can own a draft for existing workflow tests; within a project they never keep an
independent editable copy. Derived results remain view-local and start empty.

Spin-like editors preserve raw text across focus, hide/show and close. Table
delegates synchronize active buffers without committing table items or parsing
cells; Escape restores the pre-edit text. Reference selectors explicitly retain
missing IDs. Normal user unit changes still perform the documented conversions;
restoration blocks those handlers. `draftEdited` reports persisted edits only.
Automatic result navigation leaves the last user-selected persisted tab unchanged.
