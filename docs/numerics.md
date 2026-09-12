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
Degrees and also accepts Radians. Switching the adjacent unit selector commits any
pending text in the old units, converts the value and range (±360° / ±2π rad), and
rounds to 8 decimal places for degrees or 12 for radians. It preserves the phase
to that display precision, rather than reinterpreting the old number. Generation
converts degrees to radians at the GUI boundary; radian input passes through. All input parameters must be finite; rate and duration
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
and pending phase text. Tolerances reflect floating-point calculations;
they are not a universal error bound or a substitute for future validation.
