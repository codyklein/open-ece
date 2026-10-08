[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [Parameter(Mandatory)][string]$QwtRuntime,
    [ValidateSet('BeforeSigning', 'AfterSigning')][string]$Phase = 'AfterSigning',
    [string]$BaselinePath,
    [string]$ReportPath,
    [string]$ExpectedPublisher = 'Cody Klein'
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/authenticode.ps1"
$root = (Resolve-Path -LiteralPath $PackageDirectory).Path
if ($BaselinePath) {
    if ($Phase -eq 'BeforeSigning') {
        $baseline = @(Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object FullName | ForEach-Object {
            @{ path = [IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        })
        ConvertTo-Json -InputObject $baseline -Depth 4 | Set-Content -LiteralPath $BaselinePath -Encoding utf8NoBOM
    } else {
        $baseline = Get-Content -LiteralPath $BaselinePath -Raw | ConvertFrom-Json
        Assert-SigningPreservedFiles $baseline $root @(Get-ReleaseSigningFiles $root $QwtRuntime)
    }
}
$report = @(Test-PackageSignatures -PackageDirectory $root -QwtRuntime $QwtRuntime -Phase $Phase -ExpectedPublisher $ExpectedPublisher)
if ($ReportPath) { ConvertTo-Json -InputObject $report -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM }
$report | Format-Table path, status, subject -AutoSize | Out-Host
