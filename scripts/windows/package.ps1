[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$BuildDir = "$PSScriptRoot/../../build/windows",
    [string]$DependenciesRoot = "$PSScriptRoot/../../build/windows-deps",
    [string]$OutputDir = "$PSScriptRoot/../../build/packages"
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}
$QtRoot = (Resolve-Path $QtRoot).Path
$BuildDir = (Resolve-Path $BuildDir).Path
$DependenciesRoot = (Resolve-Path $DependenciesRoot).Path
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if ((& "$QtRoot/bin/qmake.exe" -query QT_VERSION) -ne '6.8.3') { throw 'Package with Qt 6.8.3.' }
# CMake remains the single source of the application version.
$cache = Get-Content "$BuildDir/CMakeCache.txt" -Raw
$version = [regex]::Match($cache, '(?m)^CMAKE_PROJECT_VERSION:STATIC=(.+)$').Groups[1].Value.Trim()
if (!$version) { throw 'Cannot read OpenECE version from CMake cache.' }
$name = "OpenECE-v$version-windows-x86_64"
# Always stage into a fresh directory; stale Debug DLLs cannot survive a rerun.
$staging = Join-Path $OutputDir ([guid]::NewGuid().ToString())
$destination = Join-Path $staging $name
New-Item -ItemType Directory -Force $destination | Out-Null
Run 'cmake.exe' @('--build', $BuildDir, '--config', 'Release', '--parallel', '4')
Run 'cmake.exe' @('--install', $BuildDir, '--config', 'Release', '--prefix', $destination, '--component', 'Unspecified')
$qwtRuntime = (Get-Content "$BuildDir/qwt-runtime-Release.txt" -Raw).Trim()
if (!(Test-Path (Join-Path $destination ([IO.Path]::GetFileName($qwtRuntime))))) { throw 'Release Qwt runtime was not installed.' }
$oldPath = $env:PATH
try {
    $env:PATH = "$QtRoot/bin;$([IO.Path]::GetDirectoryName($qwtRuntime));$oldPath"
    # Scan both binaries for their actual Qt dependencies. The raster-widget
    # app needs neither a software OpenGL renderer nor runtime shader compilers.
    Run "$QtRoot/bin/windeployqt.exe" @('--release', '--compiler-runtime', '--no-translations', '--no-opengl-sw', '--no-system-d3d-compiler', '--no-system-dxc-compiler', '--dir', $destination, "$destination/openece.exe", (Join-Path $destination ([IO.Path]::GetFileName($qwtRuntime))))
} finally { $env:PATH = $oldPath }
if (!(Test-Path "$destination/vcruntime140.dll") -or !(Test-Path "$destination/msvcp140.dll")) {
    Write-Host 'Qt deployed a redistributable installer; adding app-local Release CRT through CMake discovery.'
    Run 'cmake.exe' @('--install', $BuildDir, '--config', 'Release', '--prefix', $destination, '--component', 'CompilerRuntimeFallback')
}
foreach ($required in @('openece.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms/qwindows.dll', 'vcruntime140.dll', 'msvcp140.dll')) {
    if (!(Test-Path "$destination/$required")) { throw "Missing packaged runtime: $required" }
}
$debugQwt = (Get-Content "$BuildDir/qwt-runtime-Debug.txt" -Raw).Trim()
if (Test-Path (Join-Path $destination ([IO.Path]::GetFileName($debugQwt)))) { throw 'Debug Qwt in Release package.' }
if (Get-ChildItem $destination -Recurse -File | Where-Object { $_.Name -match '^(Qt6.*d|qwindowsd|msvcp\d+d|vcruntime\d+d)\.dll$' }) { throw 'Debug runtime in Release package.' }
@'
[Paths]
Prefix=.
Plugins=.
'@ | Set-Content "$destination/qt.conf"
Copy-Item "$PSScriptRoot/../../packaging/README-windows.txt" "$destination/README.txt"
Copy-Item "$PSScriptRoot/../../packaging/THIRD-PARTY-NOTICES.txt" $destination
$notices = "$destination/licenses"
New-Item -ItemType Directory -Force "$notices/Qwt", "$notices/GoogleTest", "$notices/Qt" | Out-Null
Copy-Item "$DependenciesRoot/sources/qwt-6.3.0/COPYING" "$notices/Qwt/"
Copy-Item "$DependenciesRoot/sources/googletest-1.17.0/LICENSE" "$notices/GoogleTest/"
# Notices are generated from the checksum-pinned upstream archive by the
# maintainer utility; packaging does not download or process Qt sources.
Copy-Item "$PSScriptRoot/../../packaging/Qt-6.8.3-NOTICES.txt" "$notices/Qt/"
Get-ChildItem $destination -Recurse -File | Where-Object Name -ne 'SHA256SUMS.txt' | ForEach-Object {
    "$((Get-FileHash $_.FullName).Hash.ToLowerInvariant())  $([IO.Path]::GetRelativePath($destination, $_.FullName).Replace('\', '/'))"
} | Set-Content "$destination/SHA256SUMS.txt"
$zip = Join-Path $OutputDir "$name.zip"
Write-Host "Compressing $name..."
Compress-Archive -Path $destination -DestinationPath $zip -Force
Write-Host "Created $zip"
Get-ChildItem $destination -Recurse -File | Select-Object FullName, Length
