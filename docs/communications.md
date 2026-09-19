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

## BER Monte Carlo experiment

BerExperiment validates an owned BerRequest before allocating point state. Each
point preserves its original index and requested Eb/N0, owns bit/noise generators,
and retains integer bit_errors, bits_tested and requested_bits. Results are owned
snapshots in request order. Repeated Eb/N0 values are allowed and get distinct
streams by index. Changing processing order or chunk size leaves each completed
point unchanged. Altering a different point does not reseed existing identities.
The bit budget and every advance budget must be divisible by k; odd QPSK requests
are rejected, never rounded. An advance consumes at most the remaining budget;
completed points are unchanged. Run continues any partial work to completion.

Monte Carlo operates at matched-filter decision rate: symbol energy one and
independent Gaussian decision coordinates of variance N0/2. This is statistically
equivalent to the normalized waveform chain, but uses distinct BER stream tags
and retains no waveform records. It is not the same noise realization as LinkResult.
Each BPSK symbol consumes one bit and an I/Q Gaussian pair (Q ignored); QPSK consumes
two bits and an I/Q Gaussian pair. RNG state is never reconstructed at chunk edges.

Theory for both mappings is 0.5*erfc(sqrt(10^(Eb/N0_dB/10))). Measured BER is the
integer error count divided by tested bits; no tested bits means no estimate.
Zero errors is reported as "0 errors in N bits", with a one-sided 95% upper bound
1-0.05^(1/N), evaluated as -expm1(log(0.05)/N). There is no invented measured BER
floor. Fixed bit budgets avoid error-triggered stopping. Partial cancelled counts
remain partial, with the originally requested budget available for comparison.

Theory regression tests use fixed seeds and a predeclared two-sided Bernstein
binomial bound: with t=log(2/alpha), accept |errors-Np| <= sqrt(2*N*p*(1-p)*t)+2*t/3.
Alpha=1e-6 for each of eight modulation/Eb/N0 cases gives a union bound <=8e-6
under the independent Bernoulli model. Tests do not demand exact theory equality
or monotonic empirical curves. Gaussian mean and second-moment checks use normal
and chi-square concentration bounds; no empirical variance renormalization occurs.

## Execution policy and benchmark

Core limits are 64 BER points, 10,000,000 bits per point and 50,000,000 aggregate
bits; multiplication is checked by division before point allocation. One advance
is bounded to 65,536 bits. GUI policy will be stricter: 41 points, 1,000,000 bits
per point and 10,000,000 aggregate bits. Benchmark results are evidence for a
resource policy, not a time guarantee. The largest BPSK/QPSK single-point and
64-point workloads can be reproduced with:

```sh
cmake -S . -B build/communications-benchmark -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENECE_BUILD_GUI=OFF
cmake --build build/communications-benchmark --target openece_communications_benchmark
./build/communications-benchmark/tests/openece_communications_benchmark
```

Fedora GCC Release measured 0.32/1.61 seconds for BPSK 10M/50M bits and 0.18/0.92
seconds for QPSK 10M/50M bits. Windows MSVC Release measured 0.53/2.48 seconds
for BPSK and 0.28/1.42 seconds for QPSK at the same 10M/50M workloads. These
measurements support retaining the proposed core and GUI budgets as v0.8 policy.
The benchmark is excluded from ordinary
builds and CTest, and has no machine-dependent pass/fail time threshold.

Checkpoint 3 validation passed all eight jobs in [CI run 35418109825](https://github.com/codyklein/open-ece/actions/runs/35418109825): 164 desktop / 158 headless tests, MSVC Debug and Release, and fresh packaged startup.

## Communications workspace

MainWindow composes a persistent fourth CommunicationsView, with I/Q waveforms,
constellation/bit decisions, BER experiment and conventions tabs. Domain switches
preserve controls and results. This is in-memory workspace state, not file persistence.
The existing domains keep their implementations and APIs; navigation-count tests
now account for the fourth page.

The link editor accepts seeded random bits or manual 0/1 text with whitespace.
Invalid characters, odd QPSK counts and over-budget records produce visible errors
without repairing inputs. Seeds are unsigned decimal uint64 values. Rates accept
symbols/s, ksymbols/s or Msymbols/s; switching units converts the physical value,
and failed conversions retain text and roll back the selector. Numeric parsing
uses the C locale with grouping separators rejected. Noise can be disabled explicitly.

GUI link limits are 65,536 bits and complex samples, and 1..64 samples/symbol.
Waveform and constellation plots show the first 2048 samples/decisions, not an
undocumented decimation; bit tables show the first 256 bits. Labels expose these
limits, while error counts include the entire record. I/Q constellation axes use
equal physical display scaling. Curves/markers are Qwt-owned and copy their data.

BER is a distinct random experiment with its own point/bit budget; link rate,
manual bits, samples/symbol and link noise checkbox do not redefine its channel.
The GUI allows 1..41 points, up to 1,000,000 bits per point and 10,000,000 aggregate
bits. One point requires equal endpoints; a sweep requires strictly increasing
finite Eb/N0 values, preserving endpoints exactly. The core validates each point.

Run/resume and Step use an owned BerExperiment; each GUI event advances at most
4096 bits. Cancel stops the timer, retains integer counts and labels every incomplete
row (including points not yet evaluated). Resume continues those RNG states.
Edits cancel and discard stale results. Destruction deletes the timer and experiment.
BER results are shown at their actual Eb/N0 values, without connecting incomplete
or zero-error observations as measured lines. Zero-error downward triangles mark
fixed-N one-sided 95% upper bounds, not measured BER. Partial results are explicit;
user-adaptive cancellation does not imply the fixed-N statistical coverage guarantee.

Nine GUI workflows test mapping, odd/invalid inputs, pending unit conversion,
Run/Step repeatability, cancellation/resume/destruction, zero-error plot semantics,
resource/plot limits, locale/seed handling and persistence across all four domains.
