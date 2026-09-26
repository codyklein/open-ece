# AC circuit analysis (v0.7)

## Model and phasors

`openece::circuits::ac` belongs to the Qt-independent `OpenECE::circuits` target.
It shares stable NodeId/ComponentId, Node and Resistor types with DC, but owns a
separate Component variant, CircuitDefinition and validated Circuit. DC's public
model, value ranges and solve behavior remain unchanged. Internal structural
validation is shared; electrical validation belongs to the respective solver.

AC components are resistors, capacitors, inductors, independent VoltageSource and
CurrentSource. Sources store rectangular `std::complex<double>` RMS phasors:
`x(t) = sqrt(2) * real(X * exp(j*2*pi*f*t))`. The reference waveform is cosine;
phase is not a DSP sine-generator phase. All source phasors refer to the same
analysis frequency. There is no DC offset, transient or initial-condition meaning.
Positive voltage is V(positive)-V(negative); positive branch current flows from
positive to negative. No Eigen or Qt type is exposed by the model.

Definitions own their nodes and components and may be invalid while edited.
Circuit construction copies/moves a definition into an owned structurally valid
snapshot. Its const definition reference is borrowed from an lvalue owner only.
Copies own independent snapshots; moved-from objects are only for destruction or
reassignment. A draft cannot mutate an existing validated circuit.

Node IDs are unique among nodes; component IDs are unique across all component
kinds. Connections use IDs, never names. Ground must explicitly name an existing
node; zero is not reserved. Every component connects distinct existing terminals.
Forward references are allowed. Names are exact nonempty byte strings, at most
128 bytes and unique within the node/component list respectively. The core does
not trim whitespace, change case or normalize Unicode.

## Structural limits and SI values

The shared limits are 128 nodes including ground, 512 components, 64 voltage
sources and 191 MNA unknowns. Counts are checked before numerical allocation.
Resistors accept 1e-9 through 1e12 ohms, capacitors 1e-15 through 1 farad, and
inductors 1e-12 through 1e6 henries, inclusively. Zero and negative passive values
are invalid. Source phasor magnitude must be zero or 1e-12 through 1e9 V/A RMS;
this applies to magnitude, not to each rectangular component separately. Both
rectangular components must be finite. No invalid value is clamped or repaired.

Structural validation deliberately allows electrically floating, contradictory
or resonant networks. An accepted definition is not a promise of solvability.
AC analysis frequencies are strictly positive, bounded at 1e-6 through 1e12 Hz;
zero-frequency RLC behavior is outside this milestone.

## Complex MNA and single-frequency solve

`ac/analysis.hpp` provides `solve_ac(const Circuit&, double frequency_hz)` and an
owned AcSolution. It preserves the requested frequency, node voltages in node
order including exact-zero ground, voltage-source currents in source order, and
NumericalQuality. Errors use the existing structured CircuitError with codes,
messages and relevant stable IDs. No partial solution is returned on failure.

For omega=2*pi*f, branch admittances are 1/R, j*omega*C and 1/(j*omega*L).
Unknowns are non-ground node voltages followed by voltage-source currents:

```text
[ Y(f)  B ] [ V  ] = [ J ]
[ B^T   0 ] [ IV ]   [ E ]
```

B^T is the ordinary transpose, NEVER conjugate transpose. The MNA matrix is
complex symmetric but generally not Hermitian. A branch admittance y adds y to
its two nodal diagonals and -y to both off-diagonals; omit ground rows/columns.
A current source p->n subtracts I from Jp and adds I to Jn. A voltage source adds
+1 at its positive-node coupling, -1 at its negative-node coupling, identical
entries in the constraint row, and its complex voltage on that row's RHS.
Inductors use admittance stamps, so do not add branch unknowns. Source phasors
are held constant when evaluating different positive frequencies.

At nonzero frequency R/C/L and voltage-source edges establish reference
connectivity. Current sources never do. Connectivity alone does not prove
solvability: reactive cancellation may make an ideal LC network singular.
Voltage-source constraint forests compare signed complex path sums with complex
source phasors. Redundant/consistent loops are rejected because individual source
currents are non-unique; contradictory loops have a separate diagnostic. The
existing loop tolerance uses complex magnitudes. Floating diagnostics take
precedence, and diagnostics do not promise exhaustive classification.

The private Eigen FullPivLU uses the same centralized numerical policies as DC:
four row/column max-magnitude equilibration passes, relative rank threshold
64*k*epsilon, scaled reciprocal condition at least 1e-12, original-matrix backward
error at most max(1e-12,256*k*epsilon), physical KCL tolerance
1e-15 A + 1e-10*sum(abs(incident currents)), and voltage-constraint tolerance
1e-12 V + 1e-10*(abs(Vp)+abs(Vn)+abs(E)). Complex absolute values mean magnitudes.
Scaling depends on the matrix, not excitation. Physical checks include ground.
Nonfinite intermediate values, unreliable conditioning, rank deficiency, or
failed residual checks reject the result. No solver acceptance threshold is
relaxed for AC, and no resistance, grounding, pseudoinverse or regularization is
inserted at resonance. Near resonance an ideal model may produce a large finite
accepted result; this is not a prediction of real-device losses or voltage limits.

Regressions include a 10 V RMS source across 1 kohm returning source current
-10 mA; capacitor current leads voltage by 90 degrees and inductor current lags
by 90 degrees. A first-order RC low-pass measured ACROSS THE CAPACITOR has
H=1/(1+j*omega*R*C); at f=1/(2*pi*R*C), H=(1-j)/2, -3.01029995664 dB and -45 degrees.
An independent branch-impedance/current formulation and test-only Gauss-Jordan
solve check mixed networks without reusing production MNA stamps or Eigen.

## Frequency sweeps and response presentation

`ac/sweep.hpp` provides SweepSpec (positive start/stop Hz, point count, spacing),
`frequency_grid(circuit, spec)`, `solve_ac_point(circuit, frequency_hz)` and
`sweep_ac(circuit, spec)`. A SweepPoint ALWAYS owns its requested frequency and
`variant<AcSolution, CircuitError>`. A completed SweepResult retains every point
in ascending grid order, including failures with diagnostic codes/messages/IDs.
It never drops, replaces with zero, or interpolates a failed point. A malformed
request fails before solving; allocation/system exceptions propagate rather than
being disguised as electrical failures. Individual CircuitError failures do not
prevent solving later frequencies. Results and errors own their data.

Linear grids use std::lerp(start,stop,i/(P-1)). Logarithmic interiors use
exp(lerp(log(start),log(stop),i/(P-1))). Both endpoints are explicitly assigned the
requested double values, exactly. Require start<stop, 2<=P<=4096, valid spacing,
and supported finite frequencies. Reject a grid if its represented values are
not strictly increasing. Construction is deterministic on a given implementation;
libm rounding is not promised bit-identical across platforms. No frequency is
snapped to a nominal resonance or preferred engineering value.

Before grid/result allocation, validate P*max(1,k)^3<=1,000,000,000, where k is the
non-ground node count plus voltage-source count. The bounded k^3 and division
comparison avoid unchecked product arithmetic. This is an aggregate work proxy,
not a latency promise. The GUI uses at most 1001 points and smaller circuit limits.

VoltageProbe explicitly selects positive and negative node IDs (equal IDs read
zero). VoltageTransfer additionally names one nonzero AC voltage source. Every
OTHER independent voltage AND current source must have zero phasor magnitude.
`validate_voltage_transfer` checks this before a sweep; `voltage_transfer` also
validates before returning H=(Vp-Vn)/E. It does not modify or suppress excitation.
Its Circuit and AcSolution must represent the same validated snapshot. Absolute
`probe_voltage` remains V RMS; normalized voltage transfer is dimensionless.

`wrapped_phase_degrees` returns a value in (-180,180], mapping -180 to +180,
including a negative-real phasor with negative-zero imaginary part. Exactly zero
has no phase (nullopt), not an invented zero-degree response. Nonzero tiny values
still have a mathematical phase. `gain_magnitude_db` computes 20*log10(abs(H))
with a finite -240 dB display floor, including zeros. Neither helper changes the
stored phasor. The GUI may hide transfer phase at/below the display floor; raw
complex results remain available. Nonfinite response values are rejected.

### Workload benchmark

`openece_ac_benchmark` is an optional target excluded from normal builds/CTest.
It measures grounded reactive networks at 4, 62 and 191 unknowns, using the
largest point count accepted by the work and grid limits. Build/run it with:

```sh
cmake -S . -B build/ac-benchmark -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENECE_BUILD_GUI=OFF
cmake --build build/ac-benchmark --target openece_ac_benchmark
./build/ac-benchmark/tests/openece_ac_benchmark
```

On Fedora, GCC 16.2.1, Intel i9-12900H, Release measured approximately 0.017 s
(4 unknowns/4096 points), 1.63 s (62/4096), and 1.18 s (191/143), with no failed
points. Debug measured 0.28 s, 28.64 s and 19.30 s respectively on the same
networks. These observations support retaining the provisional 1e9 bound as the
v0.7 policy. They are examples, not portable upper bounds; Debug and sanitizer
builds are slower. GUI work must yield between solves for progress/cancellation;
a numerical solve itself is bounded but not interruptible. No acceptance policy
was loosened to improve these measurements.

## GUI workflow and cancellation

The persistent Circuits workspace has DC and AC / Phasors tabs. The previous DC
view and tests are preserved. AC has Circuit, Single-frequency results, Frequency
sweep and Conventions pages. The default RC low-pass measures across its capacitor;
the second example is series RLC with output across the resistor. Single-frequency
results show real/imaginary RMS values, magnitude and wrapped phase for each node
and voltage-source current. Ground and other exact-zero values display no phase.

The editor uses stable IDs, an explicit ground, independent component/source
selection and editable values. Source input is nonnegative RMS magnitude and
phase in degrees; any finite degree input is reduced modulo 360 before conversion
to a rectangular phasor. Passive components have no phase field. Component type
changes clear the old quantity. Prefixes include F/µF/nF/pF, H/mH/µH and
Hz/kHz/MHz/GHz; existing resistance/voltage/current units remain physical SI
conversions. Invalid numeric text remains visible and a failed unit change rolls
back the unit selector. Input is locale-independent dot-decimal/scientific text.

Sweep mode explicitly selects absolute voltage (V RMS) or voltage transfer (dB),
output terminal IDs, and a reference source for transfer. Source/probe changes
invalidate old results. Every frequency is prelisted with its exact double in
item data and a 17-digit display. Successful rows show complex response and
magnitude/phase; failed rows retain frequency and diagnostic IDs with blank numeric
cells. Failure/undefined-phase gaps are separate Qwt curves, so interpolation
cannot cross them. Phase wrap jumps are also split. Gain-floor phase suppression
is presentation only; the accepted AcSolution retains its unmodified phasors.

Each Run owns a validated circuit snapshot and a core-validated grid. A GUI QTimer
advances one bounded `solve_ac_point` per event, with periodic plot updates. No
worker thread or global state is involved. GUI limits are 32 nodes, 128 components,
32 voltage sources and 1001 points, plus the core work bound. There is no guarantee
that one frequency solve completes within a fixed time; cancellation occurs
between solves. Changes to any calculation input stop the timer, discard the old
snapshot and clear results. Switching pages/domains preserves state.

Cancel retains evaluated point results and EVERY requested frequency row, marking
all remaining rows "Not evaluated (cancelled)". Such a table is explicitly an
incomplete run, not a completed core SweepResult. It does not fabricate electrical
errors or numeric values for unevaluated points. Starting again creates a new
snapshot and grid; deleting the view destroys its timer and owned run state.

GUI workflow tests cover analytical results, source phase, prefix conversion,
invalid pending input, explicit missing references, both source-type normalization
restrictions, exact singular-point gaps, cancellation/edit invalidation/destruction,
zero gain/phase, plot gap and phase-wrap segmentation, domain/DC/AC persistence,
RLC example, resource bounds, Unicode labels and locale independence.


v0.9 project persistence saves the editable configuration described above, including
invalid pending text and unit/selection choices. Numerical results and runtime
progress are excluded. Open leaves results empty and starts no execution; invoke
the relevant solve/simulation explicitly after loading. See [project guide](projects.md).
