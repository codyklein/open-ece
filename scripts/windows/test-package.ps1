[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Archive,
    [string]$Destination = (Join-Path $env:TEMP ('OpenECE fresh π path ' + [guid]::NewGuid()))
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Test-Path $Destination) { throw 'Startup test requires a fresh extraction path.' }
Expand-Archive -LiteralPath $Archive -DestinationPath $Destination
$executables = @(Get-ChildItem $Destination -Recurse -Filter openece.exe)
if ($executables.Count -ne 1) { throw 'Expected one packaged application.' }
$exe = $executables[0].FullName
$root = $executables[0].DirectoryName
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
    if (!$process.CloseMainWindow() -or !$process.WaitForExit(10000)) { throw 'Application did not close normally.' }
    if ($process.ExitCode -ne 0) { throw "Application exit code: $($process.ExitCode)" }
    Write-Host 'PASS: packaged application opened a native Windows window, loaded packaged Qt/Qwt modules, and closed normally.'
} finally {
    if ($process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    foreach ($variable in $variables) { [Environment]::SetEnvironmentVariable($variable, $saved[$variable], 'Process') }
    Get-ChildItem $Destination -Filter '*.log' | ForEach-Object { Write-Host $_.Name; Get-Content $_.FullName }
}
