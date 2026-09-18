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
