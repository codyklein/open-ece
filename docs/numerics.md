# Numerical conventions and contracts

## Sampled records and sine generation

All arithmetic is double precision. A record contains L real, finite samples
with a finite positive sample rate fs in hertz. It starts at t = 0:

```text
t[n] = n / fs,                       n = 0, ..., L-1
x[n] = A sin(2π f n / fs + φ)
L = floor(fs × requested_duration + 0.5)
actual_duration = L / fs
```

Duration is in seconds; the record interval is half-open `[0, L/fs)`, and its last
sample occurs at `(L-1)/fs`. Requested durations round to the nearest sample with
positive half-samples rounded upward, according to the computed floating-point
product. The UI displays the actual duration and count; it does not silently
change the rate. L must be between 1 and 1,048,576 (65,536 in the GUI).

A is a nonnegative peak amplitude in unspecified sample units. It is not volts
unless the caller explicitly interprets it as such. Frequency f is nonnegative
and must satisfy `f <= fs/2`. φ is always in radians in C++. The GUI defaults to
Degrees (±360) and also accepts Radians (±2π). The GUI parser accepts signed
decimal literals, including scientific notation; Radians additionally accepts a
signed decimal multiple of `pi`/`π`, optionally divided by a positive decimal
(e.g. `3*pi/4`). Whitespace separates tokens; `pi` is case-insensitive. General
arithmetic, nonfinite/unrepresentable literals, zero divisors, and out-of-range
results are rejected. Input is limited to 128 characters; angles are not wrapped.

Generate parses the current phase text. Switching units parses in the old unit
before converting to decimal text with up to 17 significant digits, retaining
double precision subject to floating-point conversion roundoff. Invalid text
remains visible and clears the results; failed switches retain the old unit.
Generation converts degrees to radians at the GUI boundary; radian input passes
through. All input parameters must be finite; rate and duration
must be strictly positive. Zero amplitude and zero frequency are valid.

At f = 0, samples equal `A sin(φ)`. At Nyquist, samples equal
`A (-1)^n sin(φ)`; thus a zero-phase sine is approximately zero. Allowing the
Nyquist endpoint does not guarantee recoverability of an arbitrary sine phase.
Above-Nyquist frequencies are rejected rather than silently aliased.

Phase is reduced modulo 2π before generation. This bounds the trigonometric
argument, but very large double-precision phases have already lost fractional
information; it cannot recover that information. Floating-point sine at multiples
of π is approximately, not exactly, zero.

## Forward FFT

`dsp::fft` accepts complex samples with power-of-two length N, including N = 1,
up to the numerical resource limit. It leaves input unmodified and returns N
complex coefficients in natural bin order:

```text
X[k] = Σ(n=0 to N-1) x[n] exp(-j 2π k n / N)
```

There is **no forward normalization**. For the corresponding inverse convention,
the exponential sign would be positive and the sum divided by N; an inverse FFT
is not implemented yet. Parseval under this convention is
`Σ |x[n]|² = (1/N) Σ |X[k]|²`.

For even N, bins `0 ... N/2` represent DC through positive Nyquist. Bins above
N/2 represent negative frequencies `(k-N) fs/N`. A real record has conjugate
symmetry. The FFT routine itself has no sample-rate metadata; interpretation is
the caller's responsibility. Nonfinite input is rejected. Nonfinite output from
arithmetic overflow is reported rather than plotted. Large finite inputs can
overflow the unnormalized transform even when a normalized result would fit.

## Spectral windows

`dsp::Window` has exactly two choices, `Rectangular` and `Hann`.
`window_coefficients(window, L)` returns L owned weights, rejects L=0 or lengths
above the signal limit, and rejects unknown enum values. It does not touch samples.

```text
Rectangular: w[n] = 1
Hann:        w[n] = 0.5 - 0.5 cos(2π n / L),   n = 0, ..., L-1, L > 1
```

Hann is **periodic**, using denominator L rather than L-1. This is the spectral
analysis convention, also distinguished from the symmetric filter-design form in
[SciPy's Hann documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.windows.hann.html).
For L=4 the weights are `[0, 0.5, 1, 0.5]`; the last weight is not zero.

For L=1, both windows are explicitly defined as `[1]` (identity). This avoids zero
coherent gain for the singleton. For L=2, periodic Hann is `[0, 1]`: it removes the
first sample, and no normalization can recover that information. Tiny records are
supported for a consistent API, not as useful general spectral measurements.

`amplitude_spectrum(signal, window = Window::Rectangular)` multiplies a separate
complex buffer by the weights over the original **L** samples, then pads zeros on
the right to `N = bit_ceil(L)`. The original `SampledSignal` and its time-domain
plot are unchanged. The raw `fft()` remains independent of all window choices.

## One-sided amplitude spectrum and coherent gain

Define coherent gain G as the mean of the original window weights:

```text
S = sum(n=0 to L-1) w[n]
G = S / L
Xw[k] = FFT of the windowed, zero-padded record

frequency[k] = k fs / N
amplitude[k] = |Xw[k]| / S             for DC and Nyquist
amplitude[k] = 2 |Xw[k]| / S           for other retained bins
```

Retained bins are `k = 0 ... N/2`, with DC only for N=1. Dividing by S is equivalent
to dividing by L and compensating by 1/G. Rectangular has G=1 and S=L, preserving
v0.1 normalization. Periodic Hann has G=0.5 and S=L/2 for L>1; the identity singleton
has G=1. The implementation sums the actual coefficients, instead of assuming an
ideal gain. `AmplitudeSpectrum` records `window` and `coherent_gain` alongside the
original sample count, FFT length, rate, and magnitudes.

Never use padded N in place of original L when calculating weights or gain.
Doubling combines positive/negative contributions for a real signal; DC and
Nyquist have no distinct partners and are never doubled. Magnitudes discard phase
and the sign of DC. Pure DC and alternating Nyquist records retain their endpoint
amplitudes after compensation, but Hann also spreads them into neighboring bins.
For example, a constant record of magnitude A produces an adjacent one-sided Hann
bin of height A as well as DC of height A. These bins are not separate physical tones.

For an isolated, coherent interior sine with L=N, the tone bin recovers input peak
amplitude A (tested across phases and bins for both windows). Hann also creates
neighboring main-lobe bins; this is intentional. Correction is not a universal
peak estimator: off-bin tones have scalloping error, nearby tones can overlap,
and positive/negative lobes can overlap near DC or Nyquist, especially in short
records. A frequency aligned to a zero-padded FFT bin does not necessarily complete
an integer number of cycles in the original observation.

The quantity is peak amplitude, not RMS, power, PSD, or dB. Coherent gain corrects
tone amplitude; a future noise/PSD estimator would require a different normalization
involving squared window weights. No detrending or mean subtraction occurs.

## Leakage, resolution, and a reproducible comparison

A finite record imposes a rectangular time window even when no taper is selected.
Off-bin tones leak into other bins. Hann tapers the record and reduces distant
sidelobes, helping reveal weaker components away from a strong tone. Its wider main
lobe can make nearby tones harder to separate. Windowing does not add information.

The bin spacing is fs/N. Observation duration L/fs sets a characteristic frequency
scale fs/L; actual resolving power also depends on the window and tones. Zero-padding
samples the same finite-record spectrum more densely and does not increase resolution.

Try amplitude 1, frequency 32.5 Hz, sample rate 256 Hz, duration 1 s, phase 0°.
Generate with Rectangular, then Hann. Zoom away from the dominant main lobe to
compare sidelobes on the linear amplitude plot. The time waveform is unchanged.
For a coherent comparison, set frequency to 32 Hz: the center bin is approximately
1 with either window; Hann has neighboring main-lobe bins. This demonstrates why
comparing *all* non-peak bins would be a misleading leakage test.

The leakage regression uses the 32.5 Hz case at phases 0 and 0.7 radians. It excludes
the same ±4 Hz region around the tone for both windows, then checks that Hann's
maximum distant amplitude is less than 10% of Rectangular's and the sum of squared
distant amplitudes is less than 1%. These are scoped regression thresholds for this
example, not general guarantees or a PSD/power measurement.

## Full convolution and FIR filtering

For input x of length N and causal taps h of length M:

```text
y[n] = Σ(k=0 to M-1) h[k] x[n-k],       n = 0, ..., N+M-2
x[n] = 0 outside 0, ..., N-1
output length = N+M-1
output sample rate = input sample rate fs
output duration = (N+M-1)/fs = input duration + (M-1)/fs
last output sample time = (N+M-2)/fs
```

`convolve_full` returns an owned vector; `apply_fir_full` returns an owned
`SampledSignal`. Both inputs remain unchanged. Records begin at t=0. There is no
cropping, reflection, periodic wrapping, delay compensation, resampling, or
streaming state. This is linear convolution, not circular convolution or correlation.
See the [DSP Guide's finite convolution definition](https://dspguide.com/ch6/4.htm).

Examples: `[1,2,3] * [0.5,0.5] = [0.5,1.5,2.5,1.5]`; convolving an impulse `[1]`
returns the taps themselves. With N≥M, indices M−1 through N−1 are fully immersed
in the finite input. The first M−1 samples contain startup behavior, and the final
M−1 samples contain the ending tail. When N<M, these boundary regions overlap;
there is no fully immersed interval. A finite sine is not presumed to have existed
before the record or to continue afterward. Boundary transients are real results
of this chosen zero extension, not an FFT error.

`FirCoefficients` contains nonempty finite real taps h[0..M−1], with no rate or
normalization. `h[0]` multiplies the current input, `h[1]` the previous one. DC gain
is sum(h); general coefficients may amplify, invert, or reject DC. The filter is
applied at the input's rate. Reusing coefficients at another rate shifts their
frequency interpretation in Hz. This type does not promise symmetry or constant delay.

Convolution rejects empty/nonfinite inputs, output above 1,048,576 samples, or
N×M above 64,000,000 products. Size checks precede arithmetic/allocation. FIR tap
count is bounded by 1,048,576; the full result must also satisfy `SampledSignal`'s
finite-duration contract. Invalid requests throw `std::invalid_argument`; detected
nonfinite arithmetic throws `std::overflow_error`. Standard double roundoff and
underflow remain possible. There is no compensated summation or universal relative
error bound, especially near cancellation zeros.

## Low-pass design and delay

`design_lowpass(M, cutoff_hz, fs)` accepts odd M≥3 up to the coefficient limit,
finite positive fs, and representable normalized cutoff r=cutoff_hz/fs in (0,0.5).
For D=(M−1)/2 and sinc(u)=sin(πu)/(πu), sinc(0)=1:

```text
ideal[k] = 2r sinc(2r(k-D))
w[k] = 0.54 - 0.46 cos(2πk/(M-1))
a[k] = ideal[k] w[k]
h[k] = a[k] / sum(a)
```

This is **symmetric Hamming** for FIR design, with denominator M−1. It is entirely
separate from **periodic Hann** for signal spectral analysis, whose denominator
is record length L. The designer constructs mirrored pairs explicitly and
normalizes only the designed coefficients to unity DC gain (to roundoff).
Nonrepresentable normalization is rejected. See the
[DSP Guide's windowed-sinc construction](https://dspguide.com/ch16/2.htm).

For M=3 and r=1/4, the unnormalized coefficients are `[0.08/π, 0.5, 0.08/π]`.
Divide all three by `0.5+0.16/π`. This independent analytical case tests the sinc,
Hamming endpoints, and normalization.

Symmetric real coefficients imply linear phase with delay D samples, or D/fs
seconds, wherever response phase is defined. Phase can jump by π as the real
zero-phase response changes sign; group delay is undefined at exact response
zeros. Odd M gives an integer D. The GUI displays this known design delay and
leaves the filtered curve on its causal time axis. Duration extension is **2D/fs**,
not D/fs; these describe different things. Arbitrary nonsymmetric FIR coefficients
do not inherit this constant-delay claim.

The cutoff is the ideal sinc cutoff. Finite length and windowing produce a
transition band, ripple, and imperfect rejection; cutoff is not guaranteed to be
the exact −3 dB frequency. More taps usually narrow the transition at the cost of
increased delay and computation. No attenuation/passband specification is solved,
and no filter order is chosen automatically.

## Filter frequency response and spectrum comparisons

`frequency_response(filter, fs, P)` evaluates:

```text
f[p] = (fs/2) p/(P-1),                  p = 0, ..., P-1
H[p] = Σ(k=0 to M-1) h[k] exp(-jπ p k/(P-1))
H[0] = sum(h[k])
H[P-1] = sum((-1)^k h[k])
```

P must be from 2 to 1,048,576 inclusive and M×P≤64,000,000. P=2 evaluates only
DC and Nyquist. Arbitrary allowed P is supported, not only powers of two. The rate
must be finite and positive with positive representable spacing `(fs/2)/(P−1)`.
The endpoints are real sums, avoiding spurious trigonometric imaginary residuals.
Invalid requests throw `std::invalid_argument`; nonfinite complex components or
magnitude throw `std::overflow_error`.

This is an unnormalized complex transfer response: no signal window, division by
record length/coherent gain, or one-sided doubling. The API preserves complex
phase. The GUI uses P=1025 and plots `20 log10(max(|H|, 10^-6))`, with a **−120 dB
display floor** for zeros and smaller magnitudes. The floor changes only plotting,
never coefficients, filtering, or API response values. Connecting grid points is
a visual aid; this fixed grid can miss narrow extrema and is not a specification check.

Each original/filtered signal spectrum retains the existing per-record windowing
and amplitude normalization. Full convolution changes length, duration, often FFT
grid, and normalization denominator. A filtered transient record's tallest-bin
ratio is therefore not a direct measurement of |H|. In particular, the raw linear
convolution transform identity Y=XH does not imply an identical relationship
between independently windowed and normalized amplitude plots.

For a visual experiment, use fs=1000 Hz, duration=0.25 s, frequency=100 Hz,
phase=0°, 63 taps, cutoff=64 Hz. Inspect the attenuated middle of the output,
startup/tail, 31-sample delay and 62 ms duration extension. Change frequency to
16 Hz to inspect a passband example; increase duration to reduce the relative
importance of boundaries. The response tab describes the coefficients in both cases.

The numerical two-tone regression uses fs=1024 Hz, 129 taps, cutoff=80 Hz, and
`sin(2π16n/fs)+0.5 sin(2π256n/fs)`. Orthogonal projections on 2048 fully immersed
output samples verify passband amplitude within 0.01 of one and stopband amplitude
below 0.001, and agree with independent response evaluation. These are scoped
regression thresholds for this design, not general filter guarantees. The GUI
continues to generate one sine at a time.

## Plot interpretation and validation

Time plots connect every sample with straight segments; these are a visual aid,
not bandlimited analog reconstruction. Spectrum plots use sticks at the computed
bin frequencies, include DC/Nyquist, and use a linear magnitude axis. There is no
data decimation in this release. A one-sample record is shown with a point marker.

Numerical tests cover known quarter-cycle samples, phase, rounding and validation,
the FFT versus an independent O(N²) DFT on deterministic complex records, impulse
response, exponential sign, Parseval scaling, coherent-tone amplitude, DC/Nyquist,
non-power-of-two padding, off-bin leakage, silence, singleton and maximum records,
and nonfinite/overflow handling. Window tests add analytical periodic coefficients,
coherent gains, coherent-tone and endpoint normalization, original-record immutability,
short records, a hand-weighted padded DFT oracle, and distant-sidelobe comparisons.
GUI tests check window selection, preserved time data, phase units and conversions,
pending expressions, π insertion, invalid input retention, and recovery. Parser
tests cover the grammar, limits, and repeated unit conversions. Tolerances reflect floating-point calculations;
they are not a universal error bound or a substitute for future validation.

Convolution tests include analytical impulses, delay, silence, unequal lengths,
linearity, commutativity, a separate output-side long-double reference, limits,
and overflow. FIR tests add ownership, unnormalized arbitrary taps, rate/origin/
duration, analytical and Horner-polynomial responses, agreement with the existing
FFT, response-grid validation, symmetric design/delay, two-tone attenuation, and
filtered spectra against a hand-windowed DFT. GUI checks cover causal impulse
plots, independent spectrum grids, dB flooring, redesign after rate changes,
window independence, bypass, full output limits, invalid input, and recovery.
