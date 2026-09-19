# Digital Communications (v0.8)

The Qt-independent `OpenECE::communications` target owns its bit, constellation,
baseband and result types in `openece::communications`. It does not depend on DSP,
digital logic, circuits, Qt, or Eigen. Existing SampledSignal is unchanged.

## Mapping and records

BitSequence owns 1 through 1,048,576 byte-valued bits, each exactly 0 or 1.
BPSK maps 0 to +1 and 1 to -1. QPSK consumes consecutive bits with the first
controlling I and the second Q: 00=(1+j)/sqrt(2), 01=(1-j)/sqrt(2),
11=(-1-j)/sqrt(2), 10=(-1+j)/sqrt(2). These Gray-coded symbols have unit energy.
Odd QPSK bit counts are rejected, never padded, rounded or discarded.
Hard decisions use each coordinate's sign; an exact positive or negative zero
maps to bit 0. BPSK ignores the finite imaginary coordinate.

BasebandSignal owns finite complex samples and a finite positive sampling rate,
with a representable positive time interval and duration. Samples start at t=0;
t[n]=n/fs and duration=N/fs. The largest record is 1,048,576 complex samples.
These are normalized discrete-energy coordinates, not RMS circuit phasors or
physical volts/watts. Const spans borrow only from lvalue owners. Copies own
independent data; moved-from objects are for destruction or reassignment only.

FrameSpec selects BPSK/QPSK, 1..1e9 symbols/s and 1..256 samples/symbol.
validate_frame checks counts and products before waveform allocation. The sample
rate is L*Rs, bit rate k*Rs and sample count (bit_count/k)*L. Limits are centralized
resource policies. Invalid bits, lengths, values, limits and numerical failures
have structured ErrorCode values; allocation errors are not disguised.

The remaining channel, BER and GUI contracts are implemented in later checkpoints.
