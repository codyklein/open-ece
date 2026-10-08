[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Archive,
    [string]$Destination = (Join-Path $env:TEMP ('OpenECE fresh π path ' + [guid]::NewGuid())),
    [string]$PersistenceProbe,
    [switch]$RequireSigned
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Test-Path $Destination) { throw 'Startup test requires a fresh extraction path.' }
Expand-Archive -LiteralPath $Archive -DestinationPath $Destination
$executables = @(Get-ChildItem $Destination -Recurse -Filter openece.exe)
if ($executables.Count -ne 1) { throw 'Expected one packaged application.' }
$exe = $executables[0].FullName
$root = $executables[0].DirectoryName
if ($RequireSigned) {
    # Before adding the separate test probe, verify all distributed binaries.
    $qwt = @(Get-ChildItem -LiteralPath $root -File | Where-Object { $_.Name -match '^qwt.*\.dll$' })
    if ($qwt.Count -ne 1) { throw 'Expected one packaged Release Qwt runtime.' }
    & "$PSScriptRoot/verify-signatures.ps1" -PackageDirectory $root -QwtRuntime $qwt[0].Name -ReportPath "$Destination/signatures.json"
}
$sourceCMake = Get-Content "$PSScriptRoot/../../CMakeLists.txt" -Raw
$expectedVersion = [regex]::Match($sourceCMake, 'project\(OpenECE VERSION ([0-9]+\.[0-9]+\.[0-9]+)').Groups[1].Value
if (!$expectedVersion) { throw 'Cannot determine expected package version.' }
$versionInfo = (Get-Item -LiteralPath $exe).VersionInfo
if ($versionInfo.FileVersion -ne $expectedVersion -or $versionInfo.ProductVersion -ne $expectedVersion) {
    throw "Executable version metadata differs from $expectedVersion."
}
foreach ($asset in @('branding/openece.ico', 'branding/png/openece-16.png', 'branding/png/openece-20.png', 'branding/png/openece-24.png', 'licenses/Noto-Sans/Noto-Sans-OFL.txt')) {
    if (!(Test-Path -LiteralPath "$root/$asset")) { throw "Missing packaged branding: $asset" }
}
# Check the artifact before placing the separate probe in the extracted directory.
$entries = @(Get-Content "$root/SHA256SUMS.txt")
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $entries) {
    if ($entry -notmatch '^([0-9a-f]{64})  (.+)$') { throw 'Malformed package manifest.' }
    $digest = $Matches[1]
    $relative = $Matches[2]
    $file = [IO.Path]::GetFullPath((Join-Path $root $relative))
    if (!$file.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or !$seen.Add($file)) {
        throw 'Duplicate or escaping manifest path.'
    }
    if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $digest) {
        throw "Package manifest mismatch: $relative"
    }
}
$files = @(Get-ChildItem $root -Recurse -File)
if ($files.Count -ne $entries.Count + 1) { throw 'Unmanifested package files.' }
foreach ($file in $files) {
    $relative = [IO.Path]::GetRelativePath($root, $file.FullName).Replace('\', '/')
    if ($relative -ne 'SHA256SUMS.txt' -and !$seen.Contains($file.FullName)) { throw "Unmanifested file: $relative" }
    # Original license headers are notices, not build inputs.
    if (!$relative.StartsWith('licenses/') -and $relative -match '(?i)(\.(pdb|obj|lib|ilk|cmake|cpp|hpp|h|log)$|CMakeCache.txt|(^|/)(sources|build|CMakeFiles)/)') {
        throw "Development artifact in release package: $relative"
    }
}
Write-Host "Verified package: $($files.Count) files, $($entries.Count) manifest hashes. ZIP SHA-256: $((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant())"
foreach ($name in @('sine-fft-fir', 'half-adder', 'dff-timing', 'dc-divider', 'rc-lowpass', 'series-rlc', 'bpsk-link-ber', 'qpsk-link-ber', 'intentionally-incomplete')) {
    if (!(Test-Path -LiteralPath "$root/examples/$name.openece")) { throw "Missing example $name" }
}
if (!(Test-Path -LiteralPath "$root/docs/first-session.md")) { throw 'Missing first-session guide.' }
$probePath = $null
if ($PersistenceProbe) { $probePath = (Resolve-Path $PersistenceProbe).Path }
$variables = @('PATH', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORM', 'QTDIR', 'QT_ROOT', 'QWT_ROOT', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH', 'QT_DEBUG_PLUGINS')
$saved = @{}
foreach ($variable in $variables) {
    $saved[$variable] = [Environment]::GetEnvironmentVariable($variable, 'Process')
    [Environment]::SetEnvironmentVariable($variable, $null, 'Process')
}
$env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
$env:QT_DEBUG_PLUGINS = '1'
$process = $null
try {
    # Working directory is deliberately outside the package, too.
    $process = Start-Process -FilePath $exe -WorkingDirectory $Destination -PassThru -RedirectStandardOutput "$Destination/stdout.log" -RedirectStandardError "$Destination/stderr.log"
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        if ($process.HasExited) { throw "Packaged application exited: $($process.ExitCode). See $Destination/stderr.log" }
    } while ($process.MainWindowHandle -eq 0 -and [DateTime]::UtcNow -lt $deadline)
    if ($process.MainWindowHandle -eq 0 -or $process.MainWindowTitle -notmatch 'OpenECE') { throw 'No OpenECE window appeared.' }
    $modules = @($process.Modules | Select-Object ModuleName, FileName)
    $modules | Format-Table -AutoSize | Out-String | Write-Host
    $qtModules = @($modules | Where-Object { $_.ModuleName -match '^(Qt6|qwt|qwindows)' })
    foreach ($name in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'qwindows.dll')) {
        if ($name -notin $qtModules.ModuleName) { throw "Expected loaded module $name" }
    }
    if (!($qtModules | Where-Object { $_.ModuleName -match '^qwt' })) { throw 'Qwt was not loaded.' }
    foreach ($module in $qtModules) {
        if (!$module.FileName.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "External dependency loaded: $($module.FileName)" }
    }
    foreach ($name in @('msvcp140.dll', 'vcruntime140.dll')) {
        $runtime = @($modules | Where-Object { $_.ModuleName -eq $name })
        if ($runtime.Count -ne 1 -or !$runtime[0].FileName.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Release CRT was not loaded from the package: $name"
        }
    }
    if (!$process.CloseMainWindow() -or !$process.WaitForExit(10000)) { throw 'Application did not close normally.' }
    if ($process.ExitCode -ne 0) { throw "Application exit code: $($process.ExitCode)" }
    Write-Host 'PASS: packaged application opened a native Windows window, loaded packaged Qt/Qwt/CRT modules, and closed normally.'
    if ($probePath) {
        # Probe is a separate CI artifact, never part of the release ZIP. Copy it
        # beside the extracted DLLs; no development/test DLLs may be added.
        $probe = Join-Path $root 'openece_packaged_persistence.exe'
        if (Test-Path $probe) { throw 'Release ZIP unexpectedly contains the persistence probe.' }
        Copy-Item -LiteralPath $probePath -Destination $probe
        try {
            $process = Start-Process -FilePath $probe -WorkingDirectory $Destination -PassThru -RedirectStandardOutput "$Destination/persistence-stdout.log" -RedirectStandardError "$Destination/persistence-stderr.log"
            if (!$process.WaitForExit(90000)) { throw 'Packaged persistence validation timed out.' }
            if ($process.ExitCode -ne 0) { throw "Packaged persistence exit code: $($process.ExitCode)" }
            if ((Get-Content "$Destination/persistence-stdout.log" -Raw) -notmatch 'PASS: packaged-runtime persistence:') {
                throw 'Persistence probe did not report a completed round trip.'
            }
            $probeOutput = Get-Content "$Destination/persistence-stdout.log" -Raw
            if ($probeOutput -notmatch "PASS: packaged branding: application icon and About OpenECE $([regex]::Escape($expectedVersion))\." -or
                $probeOutput -notmatch 'PASS: executable icon: ten approved') {
                throw 'Packaged About/icon verification did not complete.'
            }
            Write-Host 'PASS: packaged persistence and branding using only the Release ZIP runtime.'
        } finally {
            if ($process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
            Remove-Item -LiteralPath $probe
        }
    }
} finally {
    if ($process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    foreach ($variable in $variables) { [Environment]::SetEnvironmentVariable($variable, $saved[$variable], 'Process') }
    Get-ChildItem $Destination -Filter '*.log' | ForEach-Object { Write-Host $_.Name; Get-Content $_.FullName }
}
