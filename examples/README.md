# Example projects

Open these ordinary schema-1 `.openece` files with **File → Open**. They contain
all workspaces, with the relevant domain selected. **Every example opens inertly**:
results appear only after an explicit action. Save As to your own path to keep
changes. Units and raw draft text are project content; calculated results are not.

| File | Action and expected result |
|---|---|
| [sine-fft-fir](sine-fft-fir.openece) | Generate: a 1-unit, 20 Hz sine, fs=1024 Hz, duration 1 s (1024 samples). Rectangular FFT has a 20 Hz bin of amplitude 1. The 127-tap, 40 Hz low-pass passes 20 Hz with response magnitude within 0.01 of unity. Change cutoff to 10 Hz and regenerate: the 20 Hz response magnitude is below 0.05. Full zero-extended convolution gives 1150 samples, with visible uncompensated delay 63/1024 s = 61.5234375 ms. Record extension is 126/1024 s. |
| [half-adder](half-adder.openece) | Evaluate the input toggles or Generate truth table. A,B → Sum,Carry is 00→00, 01→10, 10→10, 11→01. XOR is odd parity; IDs, not names, define wires. Rows are binary order with A most significant. |
| [dff-timing](dff-timing.openece) | Run: D rises at 2 ns and falls at 12 ns; CLK rises at 5, 15, 25 ns. Rising-edge DFF, initial Q=0, delay 1 ns: Q rises at 6 ns and falls at 16 ns, remaining low through 30 ns. Integer core ticks are ps; display is ns. Simultaneous data/clock changes would sample pre-batch D. Step reaches the same trace as Run. |
| [dc-divider](dc-divider.openece) | Solve DC: ground=0 V, supply=10 V, midpoint=5 V. Two 1 kΩ resistors draw 5 mA; the source current is −0.005 A because branch current is positive from positive to negative terminal. |
| [rc-lowpass](rc-lowpass.openece) | Solve at fc=1/(2πRC)=159.15494309189535 Hz for R=1 kΩ and C=1 µF. Output is **across the capacitor**. With 1∠0° V RMS excitation, output=0.5−j0.5 V, magnitude=0.70710678 V, phase=−45°. Transfer magnitude=−3.0102999566 dB. Run sweep to see low-pass roll-off; the log grid need not contain fc exactly. |
| [series-rlc](series-rlc.openece) | Solve at f0=1/(2π√LC)=1591.5494309189535 Hz for L=10 mH, C=1 µF, R=100 Ω. Output across R is approximately 1∠0° V RMS, transfer 0 dB, source current −0.01 A. Sweep shows band-pass response: positive output phase below resonance and negative above it. |
| [bpsk-link-ber](bpsk-link-ber.openece) | Simulate link: noise disabled, manual `00 01 11 10` gives 8 bits, 8 symbols, 0 errors. Mapping 0→+1, 1→−1. Enable AWGN and simulate to see decision clouds around the two ideal points. Run BER for 0,4,8,12 dB, 20000 bits per point. |
| [qpsk-link-ber](qpsk-link-ber.openece) | Simulate link: noise disabled, the same 8 bits give 4 symbols, 0 errors. Gray mapping 00→(+I,+Q), 01→(+I,−Q), 11→(−I,−Q), 10→(−I,+Q), each divided by √2; first bit controls I. Enable noise to see four decision clouds. Run BER as above. Odd bit counts are rejected. |
| [intentionally-incomplete](intentionally-incomplete.openece) | **Intentionally unrunnable**, demonstrating faithful saving. Repair Signals phase `3π/` (e.g. `pi/2` radians); logic XOR's missing pin ID 77 (choose B/ID 2); Timing pin text `1,` (use `1,2`); DC R1's missing negative ID 9 (use Midpoint/ID 2) and `1e-` resistance (use `1000` Ω); QPSK manual `01 1` (use an even bit count, e.g. `01 10`). AC has a Unicode output name. Save/reopen before repair to verify raw text and missing references survive. |

Signals spectra use coherent-gain-corrected one-sided amplitude, not PSD. FIR
cutoff is ideal windowed-sinc cutoff, not a promised −3 dB frequency. Startup/tail
samples and each record's normalization mean a filtered finite-record FFT is not
identical to the filter's steady-state frequency response. See [DSP conventions](../docs/numerics.md).

AC uses RMS cosine-reference phasors; all independent sources other than the
selected nonzero voltage-transfer reference must be zero. Phase is wrapped to
(−180°,180°], undefined at zero. No resonance regularization is inserted.

Communications amplitudes are normalized complex baseband, **not RMS volts**.
The pulse has L=16 samples of s/√L, Rs=1 ksymbol/s; matched-filter decisions occur
at symbol boundaries. The noise checkbox controls the link, not the BER AWGN
experiment. BER reports integer errors and tested bits and compares against
½ erfc(√(10^(Eb/N0/10))). Do not expect exact theory agreement or monotonic
finite-sample estimates. Zero observed errors are reported as **0 errors in N bits**
with fixed-N one-sided 95% upper bound 1−0.05^(1/N); for N=20000 this is about
0.0001497754, not a BER floor. Seeds are 1 and 2. Run/Step chunking does not change
completed counts; cross-platform math-library rounding may differ.

Automated tests load every file, verify semantic round trips and inert restoration,
then explicitly execute the documented operations. Compatibility fixtures are
[separate immutable files](../tests/fixtures/v0.9/README.md), not user examples.
