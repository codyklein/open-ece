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
