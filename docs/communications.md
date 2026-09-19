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

The remaining BER and GUI contracts are implemented in later checkpoints.

## Deterministic random contract (specified before channel implementation)

All integer arithmetic below is unsigned 64-bit, modulo 2^64. The generator is
`std::mt19937_64` constructed directly from one uint64_t seed (no seed_seq,
random_device, normal_distribution, or uniform_real_distribution).

Define mix(x) by x+=0x9e3779b97f4a7c15, then
x=(x^(x>>30))*0xbf58476d1ce4e5b9,
x=(x^(x>>27))*0x94d049bb133111eb, return x^(x>>31).
The engine seed is mix(base_seed ^ stream_tag ^ mix(point_id)). Stream tags are
0x4c494e4b42495453 (link bits), 0x4c494e4b4e4f4953 (link noise),
0x4245525f42495453 (BER bits), and 0x4245525f4e4f4953 (BER noise).
Link functions use point_id=0. BER points use their explicit immutable index in
the original request, independently of processing order. Bit and noise base seeds
are separate inputs. Stream derivation offers reproducibility and domain separation,
not a mathematical guarantee of independent random sequences or cryptographic security.

Each random bit consumes one engine word and uses word>>63. Each open uniform
consumes one word: (double(word>>12)+0.5)*2^-52. Its endpoints are 2^-53 and
1-2^-53, both representable; neither zero nor one occurs. Box-Muller draws u1 then
u2, r=sqrt(-2*log(u1)), theta=2*pi*u2, returning r*cos(theta) and caching
r*sin(theta). The next Gaussian call consumes that cached value without drawing
new integers. Complex noise consumes real then imaginary Gaussian values. Engine
and cached-Gaussian state persist across chunks. GUI scheduling never draws noise.

Integer outputs and bit streams are portable. Gaussian floating results may differ
slightly between math libraries; bitwise cross-platform Gaussian/BER equality is
not promised. Identical requests and chunk partitions on one build give identical
results. No distribution algorithm is selected by the host standard library.

## Waveform, channel and receiver

A mapped symbol s occupies L samples s/sqrt(L), a rectangular unit-energy pulse.
Waveforms are actual complex baseband I/Q samples; there is no RF carrier. Time
rate parameters do not turn normalized amplitudes into physical RMS voltages.
AWGN uses gamma=10^(Eb/N0_dB/10), Es=1, Eb=1/k, N0=1/(k*gamma), and independent
real/imaginary Gaussian noise each of variance N0/2. The supported Eb/N0 range
is -20..+20 dB. Explicit nullopt disables noise; infinity is not a noise-off flag.

The aligned receiver sums L received samples with weight 1/sqrt(L), recovering s
without noise and retaining coordinate noise variance N0/2 independently of L.
It rejects incomplete symbols and nonfinite accumulation. Decision m is available
at (m+1)/Rs, including the last decision at record duration. There is no tail,
padding, timing search or delay compensation. Perfect coherent phase/timing is
assumed. LinkResult owns full waveform and decision records plus integer error
counts. Mutating the request cannot change an existing result.
