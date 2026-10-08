[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [string]$OutputDir = "$PSScriptRoot/../../build/packages",
    [switch]$RequireSigned,
    [string]$QwtRuntime
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path -LiteralPath $PackageDirectory).Path
$name = Split-Path -Leaf $root
if ($name -notmatch '^OpenECE-v\d+\.\d+\.\d+-windows-x86_64$') { throw 'Unexpected package directory name.' }
if ($RequireSigned) {
    if (Test-Path -LiteralPath "$root/SHA256SUMS.txt") { throw 'Signed packaging requires a fresh, unmanifested stage.' }
    & "$PSScriptRoot/verify-signatures.ps1" -PackageDirectory $root -QwtRuntime $QwtRuntime
} else { Write-Warning 'Unsigned developer package; not a signed distribution candidate.' }
New-Item -ItemType Directory -Force $OutputDir | Out-Null
Get-ChildItem -LiteralPath $root -Recurse -File | Where-Object Name -ne 'SHA256SUMS.txt' | Sort-Object FullName | ForEach-Object {
    "$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $([IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/'))"
} | Set-Content -LiteralPath "$root/SHA256SUMS.txt" -Encoding utf8NoBOM
$zip = Join-Path $OutputDir "$name.zip"
Compress-Archive -LiteralPath $root -DestinationPath $zip -Force
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zip.sha256" -Value "$hash  $name.zip" -Encoding utf8NoBOM
Write-Host "Created $zip; SHA-256: $hash"
$manifestCount = @(Get-Content -LiteralPath "$root/SHA256SUMS.txt").Count
Write-Host "Package files: $(@(Get-ChildItem -LiteralPath $root -Recurse -File).Count); manifest entries: $manifestCount"
