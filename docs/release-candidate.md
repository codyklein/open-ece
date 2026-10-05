# OpenECE 1.0.0 — historical final RC smoke

This checklist is the historical 1.0.0 acceptance procedure. The exact RC passed
physical Windows 11 acceptance and was released unchanged at
`3e688548d3d71a12fdceee173ee780047f23f854`. Published ZIP SHA-256:
`7883bef22db15027bc2992d64b227175cb34561beaed42f663eaab0c16a30431`.
The released artifact must not be replaced. For the current branding candidate,
use the [1.0.1 checks](branding.md) instead.

## Identify the exact artifact

Download the CI artifact `OpenECE-v1.0.0-windows-x86_64` from the candidate report.
Its inner archive is `OpenECE-v1.0.0-windows-x86_64.zip`; the GitHub wrapper is not
the release ZIP. Compare the inner ZIP's hash to the reported value:

```powershell
Get-FileHash .\OpenECE-v1.0.0-windows-x86_64.zip -Algorithm SHA256
Expand-Archive .\OpenECE-v1.0.0-windows-x86_64.zip -DestinationPath "$env:USERPROFILE\OpenECE RC π test"
```

Use a fresh extraction, keep the full application directory intact and launch
`openece.exe` normally. No Qt SDK, build tools or development PATH should be
needed. Record OS/build, scaling, extraction path, commit, hash and each result.
The package's `SHA256SUMS.txt` covers every file except the manifest itself;
complete verification is recorded in the candidate report. Do not recompress,
rebuild or substitute an older ZIP after testing.

## Short smoke on Windows 11 and native Fedora

On Fedora use the same candidate commit's native build and record that commit;
a Windows ZIP is not a Fedora executable. Current build commands are in
[building](building.md). Open each example via **File → Open**; select Discard
only when intentionally abandoning a disposable edit. Immediately after Open,
confirm the relevant derived results are empty; nothing should run automatically.

1. **Startup/About:** launch; Help → About OpenECE must say **OpenECE 1.0.0**,
   MIT license and notice locations. Switch all four domains.
2. **Signals:** open `examples/sine-fft-fir.openece`, Generate and analyze.
   Confirm the 20 Hz tone passes the 40 Hz/127-tap FIR; full output is 1150
   samples and visible delay is 61.5234375 ms. Wheel/arrow-edit frequency to
   25 Hz: old plots must immediately say stale until regenerated.
3. **Digital:** open `examples/half-adder.openece`; evaluate or generate its
   truth table: A,B → Sum,Carry is 00→00, 01→10, 10→10, 11→01.
4. **Timing:** open `examples/dff-timing.openece`; Run (or Step to completion).
   Delayed Q rises at 6 ns and falls at 16 ns. Pause/Reset remain reachable.
5. **DC:** open `examples/dc-divider.openece`; Solve DC: supply +10 V,
   midpoint +5 V, voltage-source current −0.005 A (+ to − convention).
6. **AC:** open `examples/rc-lowpass.openece`; solve at the stored corner.
   Output across C is approximately −3.0103 dB and −45° in transfer mode.
7. **Communications:** open `examples/qpsk-link-ber.openece`; simulate the
   noiseless link: eight bits, four symbols, zero errors. Step BER once and
   confirm integer errors/bits are readable. No exact finite-sample theory
   match is required. Cancel if running; do not start a lengthy benchmark.
8. **Schema-1 persistence:** open `examples/intentionally-incomplete.openece`
   inertly. Confirm invalid text/missing references remain (Signals `3π/`,
   DC `1e-`, communications `01 1`). Save As to a disposable path containing
   spaces and Unicode, e.g. `RC π projects/My incomplete π.openece`.
   Close/relaunch, reopen, check those editable fields and empty results.
   Make one small text edit, Save over the same file, reopen and verify the edit.
   Confirm About still says 1.0.0 and exit normally.

Opening a project does not generate FFT/filter output, evaluate logic, run timing,
solve DC/AC, start a sweep, simulate a link or resume BER. Project files store
editable state, not cached results. The release-pinned v0.9 fixtures and schema-1
semantic round trips are also checked automatically; do not edit fixture bytes.

## Return evidence

Report pass/fail for startup, About, each domain, schema-1 Save/reopen, inert
loading, Unicode/space paths and clean exit. Include the exact ZIP hash on Windows
and build commit on Fedora. Record a reproducible failure rather than assuming
older physical evidence applies. Any application fix requires a new candidate and
hash plus affected retests. Windows 10, mixed-monitor DPI and screen-reader gaps
remain explicit; this short check makes no new claims about them.
