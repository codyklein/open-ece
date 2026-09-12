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
and must satisfy `f <= fs/2`. φ is in radians in C++; the GUI accepts degrees and
converts at the UI boundary. All input parameters must be finite; rate and duration
must be strictly positive. Zero amplitude and zero frequency are valid.

At f = 0, samples equal `A sin(φ)`. At Nyquist, samples equal
`A (-1)^n sin(φ)`; thus a zero-phase sine is approximately zero. Allowing the
Nyquist endpoint does not guarantee recoverability of an arbitrary sine phase.
Above-Nyquist frequencies are rejected in v0.1, rather than silently aliased.

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

## One-sided amplitude spectrum

`dsp::amplitude_spectrum` takes a real record of L samples, applies a rectangular
window (all weights 1), and pads zeros on the right to `N = bit_ceil(L)`.
It keeps bins `k = 0 ... N/2` (DC only for N = 1):

```text
frequency[k] = k fs / N
amplitude[k] = |X[k]| / L             for DC and Nyquist
amplitude[k] = 2 |X[k]| / L           for other retained bins
```

Normalization uses **L, not padded N**, to preserve the observation's amplitude
scale. Doubling combines the positive/negative contributions for a real signal;
DC and Nyquist have no distinct conjugate partners. DC magnitude discards its sign.

For an interior, bin-centered sine with L = N and an integer number of cycles,
the plotted bin height equals A, independent of phase. The plotted quantity is
peak amplitude: it is not RMS, power, power spectral density, or decibels. For a
sine away from DC/Nyquist, RMS would be A/√2, but that is not what this view displays.

The bin spacing is fs/N. The observation length L/fs determines a characteristic
frequency scale fs/L, with resolution also dependent on the window and the tones
being compared. Zero-padding samples the finite-record spectrum more densely; it
adds no information and does not separate otherwise unresolved tones.

With a rectangular window, non-integer record cycles cause spectral leakage.
The largest bin can underestimate amplitude, and padding does not remove leakage.
Window selection and coherent-gain correction are the recommended next milestone.
No detrending or mean subtraction occurs in v0.1.

## Plot interpretation and validation

Time plots connect every sample with straight segments; these are a visual aid,
not bandlimited analog reconstruction. Spectrum plots use sticks at the computed
bin frequencies, include DC/Nyquist, and use a linear magnitude axis. There is no
data decimation in this release. A one-sample record is shown with a point marker.

Numerical tests cover known quarter-cycle samples, phase, rounding and validation,
the FFT versus an independent O(N²) DFT on deterministic complex records, impulse
response, exponential sign, Parseval scaling, coherent-tone amplitude, DC/Nyquist,
non-power-of-two padding, off-bin leakage, silence, singleton and maximum records,
and nonfinite/overflow handling. Tolerances reflect floating-point calculations;
they are not a universal error bound or a substitute for future validation.
