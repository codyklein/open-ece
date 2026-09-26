# Linear DC Circuit Analysis (v0.6)

## Model, ownership and units

`OpenECE::circuits` is Qt-independent, in namespace `openece::circuits`.
Its `NodeId` and `ComponentId` are distinct uint32 wrappers, independent of Digital
Logic IDs. Nodes and all component kinds have stable IDs and display names.
Connections resolve IDs only. Node IDs are unique among nodes; component IDs are
unique across resistors and both source types. Their numeric namespaces may overlap.
Names are exact nonempty byte strings, at most 128 bytes, unique within the node
or component list respectively. No trimming, case folding or Unicode normalization
is performed by the core. Ground is an explicitly selected existing node; ID zero
is not reserved. Ground may appear anywhere in declaration order.

`CircuitDefinition` owns node/component vectors and an optional ground selection.
It is a draft and may be invalid. `Circuit(definition)` owns a structurally validated
snapshot, checking IDs, names, distinct existing terminals, finite values, ranges
and resource limits. Forward references are supported. `definition()` borrows a
const reference from an lvalue; keep the owner alive and unchanged while using it.
Treat moved-from circuits as suitable only for destruction or reassignment.

`solve_dc(const Circuit&)` performs electrical/numerical checks and returns an owned
`DcSolution`: node voltages in node declaration order (including exact-zero ground),
voltage-source currents in voltage-source declaration order, and `NumericalQuality`.
It does not mutate the circuit. Component declaration order does not change the
physical circuit; matrix accumulation uses stable component-ID order. Numerical
results are compared with tolerances, not promised bit-identical across compilers.
No partial successful solution is returned after failure.

All core quantities use named `double` fields in SI units:

| Field | Accepted values, inclusive |
| --- | --- |
| `resistance_ohms` | 1e-9 through 1e12 Ω |
| `voltage_volts` | zero, or signed magnitude 1e-12 through 1e9 V |
| `current_amperes` | zero, or signed magnitude 1e-12 through 1e9 A |

NaN/infinity and out-of-range values are rejected. Zero/negative resistors and
same-node components are rejected; no open/short approximation or clamping is
performed. A zero-volt source between distinct nodes can act as a current probe.
Valid individual values do not guarantee a well-conditioned network.

## MNA equations and signs

Every component's positive current orientation is **positive terminal → negative
terminal**, denoted p → n. A resistor has i=(Vp−Vn)/R. A current source imposes i=I.
A voltage source imposes Vp−Vn=E, with its solved current using the same p → n sign.
A negative voltage-source current normally indicates delivery from its positive terminal.

With n non-ground nodes and m voltage sources, the unknown vector is
`x = [v0 ... v(n-1), iV0 ... iV(m-1)]`, in the declaration orders above:

```text
[ G   B ] [ v  ] = [ j ]
[ Bᵀ  0 ] [ iV ]   [ e ]
```

OpenECE assembles and tests every stamp:

- Resistor, g=1/R: Gpp+=g, Gnn+=g, Gpn−=g, Gnp−=g.
- Current source p→n: jp−=I, jn+=I (j is net independent-current injection).
- Voltage source k: Bpk+=1, Bnk−=1; matching constraint row in Bᵀ; ek=E.
- Omit entries belonging to the ground-voltage row/column; Vground is known zero.

A 10 V source across 1 kΩ gives +10 V and source current **−0.01 A**.
The default 10 V divider with two 1 kΩ resistors gives 5 V at the midpoint and
source current −0.005 A. These are automated regression tests.

## Electrical diagnostics

Construction is not a promise of solvability. `solve_dc` checks:

1. Reference connectivity through **resistors and voltage sources only**.
   Current sources never establish a DC voltage reference. Unused non-ground
   nodes, isolated islands and islands connected to ground only by current
   sources report `floating_reference`, with node IDs. Such circuits have no
   unique accepted solution; this diagnostic does not assert whether every
   floating case is underdetermined or inconsistent. Separate branches sharing
   ground are valid. A ground-only circuit returns the trivial 0 V solution.
2. Ideal voltage constraints using an iterative source forest. Each non-tree
   source closes a loop. The signed path drop is compared with its source value.
   A discrepancy exceeding `64 * loop_edge_count * epsilon * sum(abs(loop voltages))`
   reports `contradictory_voltage_constraint`. Otherwise the loop reports
   `redundant_voltage_constraint`: consistency is only asserted within this
   numerical policy, and individual source currents are non-unique. Both cases
   are rejected because the API promises every voltage-source current.
   The forest check continues past redundant edges to prefer a contradictory
   loop if one is found. Reported component IDs belong to the diagnosed loop.
3. Scaled numerical rank, condition estimate and residual quality.

A floating-reference failure precedes source-loop diagnostics. Diagnostics are
not an exhaustive list of every defect in a draft. `CircuitError` carries an
`ErrorCode`, message, and relevant node/component IDs where available. Structural
codes include resource_limit, invalid_name, duplicate_id, missing_ground,
invalid_terminal and invalid_value. Numerical failures use rank_deficient,
ill_conditioned or numerical_failure. Allocation failures may also propagate.

No hidden grounding, gmin conductance, source perturbation, regularization,
pseudoinverse or least-squares fallback is used.

## Numerical acceptance policy

Eigen is private to the implementation. OpenECE owns indexing, stamping, scaling,
interpretation and diagnostics. Eigen `FullPivLU` provides the dense solve,
numerical rank and reciprocal-condition estimate; no explicit inverse is formed.
MNA is generally indefinite, so an SPD-only factorization is inappropriate.

Policies are centralized in `circuits/dc.hpp::policy`. For k unknowns and double
machine epsilon:

| Policy | Exact convention |
| --- | --- |
| Equilibration | Four alternating row then column max-absolute-norm passes |
| Scaling transform | A′=Dr A Dc, b′=Dr b, solve A′y=b′, recover x=Dc y |
| Rank threshold | 64 k epsilon, relative to the largest LU pivot |
| Minimum reciprocal condition | 1e-12, estimated on the equilibrated matrix |
| Backward residual limit | max(1e-12, 256 k epsilon) |
| Physical KCL limit | 1e-15 A + 1e-10 × sum of absolute incident branch currents |
| Voltage-constraint limit | 1e-12 V + 1e-10 × (abs(Vp)+abs(Vn)+abs(E)) |

Scaling depends on A, never the source vector. The backward error is the maximum
row ratio `abs((Ax-b)i)/(sum_j abs(Aij*xj)+abs(bi))` in the original assembled
system. A zero denominator requires a zero residual. Nonfinite values fail.
Independent branch equations check KCL **including ground** and all source voltage
constraints. This catches small conductances lost while summing matrix entries,
even when the assembled system itself has a tiny residual. Tests include that case.
Quality metadata records scaled reciprocal condition, backward error, and maximum
absolute KCL/voltage-constraint residuals in A/V.

These are **numerical solver policies, not physical component tolerances**. They
can reject an electrically unique circuit. A small residual is not a promise of
accurate digits in an ill-conditioned system. The condition estimate refers to
the scaled system, not an unqualified error bound on every physical output.

Core limits: 128 nodes including ground, 512 components, 64 voltage sources, and
at most 191 unknowns. Counts are checked before dense allocation and size products.
The bounded dense algorithm needs O(k²) storage and O(k³) work; there is no sparse
solver, background worker or hard latency guarantee.

## GUI workflow

Circuits is the third persistent sidebar page. Widgets edit a borrowed project
draft, including invalid numeric text and unresolved connections. Solve converts
text/prefixes into an owning definition, constructs Circuit, then calls solve_dc.
It never solves an unvalidated definition. Switching domains preserves state.

Edit names directly in table cells. Add/remove nodes/components, choose ground,
select component types and terminals, and enter values. New connections start
unselected. Deleting a node leaves explicit missing-ID entries. IDs are not reused
during editing; loading the example replaces the whole draft. Any edit clears
results. Changing component type clears its value rather than reinterpreting units.

Input accepts dot-decimal/scientific notation with surrounding whitespace, with
no locale grouping or expressions. Units Ω/kΩ/MΩ/mΩ, V/mV/µV, A/mA/µA are explicit
input conversions; core values remain SI. Unit switching parses the current text
in the old unit and rewrites up to 17 significant digits in the new unit. Failed
switches retain the old unit and invalid text. Result tables show V/A with up to
12 significant digits. This display precision is not an accuracy guarantee.

The GUI permits 32 nodes, 128 components and 32 voltage sources. Conventions/help
explains signs and failure behavior. Load voltage divider restores the example.

## Scope and remaining work

v0.6 is linear DC only: no capacitors, inductors, AC, transients, dependent sources,
nonlinear devices, SPICE, graphical schematics, or cross-domain coupling. Raw named
SI doubles avoid a general units framework. Dense bounded solves,
no undo and no exhaustive multi-error report remain deliberate limits. v0.9 saves
editable drafts via the project layer without changing the DC API or saving solutions.
No future-analysis placeholder APIs or universal circuit/signal representation
are introduced. Existing DSP and Digital Logic behavior is unchanged.
