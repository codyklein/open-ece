OpenECE v0.4.0 - Windows x86_64

Extract this entire folder, then double-click openece.exe. Keep the DLLs,
qt.conf, platforms directory and other plugin directories alongside it.
No Qt installation, environment configuration or administrator access is needed.
The optional vc_redist.x64.exe is retained if provided by windeployqt; the
app-local Release MSVC runtime DLLs allow launch without running its installer.

Target: Windows 10 (1809 or later) / Windows 11 x86_64.
Build: Visual Studio 2022, Qt 6.8.3, Qwt 6.3.0.
See THIRD-PARTY-NOTICES.txt and licenses/ for dependency notices.
This is an unsigned portable application, not an installer. Windows may show
an unrecognized-publisher/download warning. Obtain builds from the project.

Usage, source and build instructions:
https://github.com/codyklein/open-ece
https://github.com/codyklein/open-ece/blob/main/docs/windows.md

App-local dependencies do not update themselves. Obtain a rebuilt OpenECE ZIP
when dependency security updates are incorporated. Numerical behavior is the
same as v0.3: full causal FIR convolution, visible uncompensated group delay,
and unchanged FFT/window normalization. No new engineering features are added.

Use the sidebar to switch between Signals / DSP and Digital Logic.
Digital Logic starts with an editable half-adder; Evaluate shows settled 0/1
values and Generate truth table lists every input combination. Drafts are not saved.
