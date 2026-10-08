# Bounded policy tests use ephemeral certificates and mocked Windows trust /
# SignTool results. Real trust verification runs separately on every candidate.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/authenticode.ps1"
function Assert-True([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}
function Assert-Rejected([scriptblock]$Operation, [string]$Message) {
    $rejected = $false
    try { & $Operation | Out-Null } catch { $rejected = $true }
    Assert-True $rejected $Message
}
function New-TestCertificate([string]$Name, [switch]$WithoutCodeSigning) {
    $rsa = [Security.Cryptography.RSA]::Create(2048)
    try {
        $request = [Security.Cryptography.X509Certificates.CertificateRequest]::new(
            "CN=$Name", $rsa, [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pkcs1)
        if (!$WithoutCodeSigning) {
            $usages = [Security.Cryptography.OidCollection]::new()
            [void]$usages.Add([Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.3'))
            $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($usages, $false))
        }
        return $request.CreateSelfSigned([DateTimeOffset]::UtcNow.AddMinutes(-1), [DateTimeOffset]::UtcNow.AddMinutes(10))
    } finally { $rsa.Dispose() }
}
function New-TestSignature($Certificate, [string]$Status = 'Valid') {
    return [pscustomobject]@{
        Status = $Status; StatusMessage = 'Test trust result'; SignatureType = 'Authenticode'
        SignerCertificate = $Certificate; TimeStamperCertificate = $Certificate
    }
}
$publisher = New-TestCertificate 'Cody Klein'
$vendor = New-TestCertificate 'The Qt Company Oy'
$wrong = New-TestCertificate 'Different Publisher'
$noUsage = New-TestCertificate 'Cody Klein' -WithoutCodeSigning
$temp = Join-Path ([IO.Path]::GetTempPath()) ('OpenECE signing π tests ' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    Assert-ReleaseSignature (New-TestSignature $publisher) 'test.exe' @('Cody Klein')
    Assert-Rejected { Assert-ReleaseSignature (New-TestSignature $wrong) 'test.exe' @('Cody Klein') } 'Wrong publisher was accepted.'
    Assert-Rejected { Assert-ReleaseSignature (New-TestSignature $publisher 'HashMismatch') 'test.exe' @('Cody Klein') } 'Invalid trust status was accepted.'
    Assert-Rejected { Assert-ReleaseSignature (New-TestSignature $noUsage) 'test.exe' @('Cody Klein') } 'Wrong certificate usage was accepted.'
    $missingTimestamp = New-TestSignature $publisher
    $missingTimestamp.TimeStamperCertificate = $null
    Assert-Rejected { Assert-ReleaseSignature $missingTimestamp 'test.exe' @('Cody Klein') } 'Missing timestamp was accepted.'
    $catalog = New-TestSignature $publisher
    $catalog.SignatureType = 'Catalog'
    Assert-Rejected { Assert-ReleaseSignature $catalog 'test.exe' @('Cody Klein') } 'Catalog-only verification was accepted.'

    $root = Join-Path $temp 'OpenECE-v1.0.1-windows-x86_64'
    New-Item -ItemType Directory -Path "$root/platforms" | Out-Null
    foreach ($name in @('openece.exe', 'qwt-release-custom.dll', 'Qt6Core.dll', 'platforms/qwindows.dll', 'README.txt')) {
        Set-Content -LiteralPath "$root/$name" -Value "original $name"
    }
    $targets = @(Get-ReleaseSigningFiles $root 'qwt-release-custom.dll')
    Assert-True ($targets.Count -eq 2 -and $targets[1].EndsWith('qwt-release-custom.dll')) 'Release Qwt filename was not preserved.'
    Assert-Rejected { Get-ReleaseSigningFiles $root '../Qt6Core.dll' } 'Escaping Qwt target was accepted.'
    Assert-Rejected { Get-ReleaseSigningFiles $root 'missing.dll' } 'Missing target was accepted.'

    $script:afterSigning = $false
    $script:rejectVendor = $false
    function Get-AuthenticodeSignature([string]$LiteralPath) {
        if ($LiteralPath -in $targets) {
            if ($script:afterSigning) { return New-TestSignature $publisher }
            return New-TestSignature $null 'NotSigned'
        }
        return New-TestSignature $vendor $(if ($script:rejectVendor) { 'HashMismatch' } else { 'Valid' })
    }
    $tool = Join-Path $temp 'mock-signtool.ps1'
    Set-Content -LiteralPath $tool -Value '$global:LASTEXITCODE = 0'
    $before = @(Test-PackageSignatures -PackageDirectory $root -QwtRuntime 'qwt-release-custom.dll' -Phase BeforeSigning -SignTool $tool)
    Assert-True ($before.Count -eq 4 -and @($before | Where-Object status -eq 'NotSigned').Count -eq 2) 'Before-signing inventory was incorrect.'
    $script:rejectVendor = $true
    Assert-Rejected { Test-PackageSignatures -PackageDirectory $root -QwtRuntime 'qwt-release-custom.dll' -Phase BeforeSigning -SignTool $tool } 'Invalid vendor signature was accepted.'
    $script:rejectVendor = $false
    Assert-Rejected { Test-PackageSignatures -PackageDirectory $root -QwtRuntime 'qwt-release-custom.dll' -Phase AfterSigning -SignTool $tool } 'Unsigned final package was accepted.'
    $script:afterSigning = $true
    $after = @(Test-PackageSignatures -PackageDirectory $root -QwtRuntime 'qwt-release-custom.dll' -Phase AfterSigning -SignTool $tool)
    Assert-True (@($after | Where-Object status -ne 'Valid').Count -eq 0) 'After-signing inventory was incorrect.'
    Set-Content -LiteralPath $tool -Value '$global:LASTEXITCODE = 2'
    Assert-Rejected { Test-PackageSignatures -PackageDirectory $root -QwtRuntime 'qwt-release-custom.dll' -SignTool $tool } 'SignTool warning was accepted.'

    $baseline = @(Get-ChildItem -LiteralPath $root -Recurse -File | ForEach-Object {
        @{ path = [IO.Path]::GetRelativePath($root, $_.FullName); sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash }
    })
    Assert-Rejected { Assert-SigningPreservedFiles $baseline $root $targets } 'Unchanged signing targets were accepted.'
    foreach ($target in $targets) { Add-Content -LiteralPath $target -Value 'signature' }
    Assert-SigningPreservedFiles $baseline $root $targets
    Add-Content -LiteralPath "$root/Qt6Core.dll" -Value 'changed'
    Assert-Rejected { Assert-SigningPreservedFiles $baseline $root $targets } 'Changed vendor bytes were accepted.'
    Set-Content -LiteralPath "$root/Qt6Core.dll" -Value 'original Qt6Core.dll'
    Add-Content -LiteralPath "$root/README.txt" -Value 'changed'
    Assert-Rejected { Assert-SigningPreservedFiles $baseline $root $targets } 'Changed nonbinary contents were accepted.'
    Set-Content -LiteralPath "$root/README.txt" -Value 'original README.txt'
    New-Item -ItemType File -Path "$root/unexpected.dll" | Out-Null
    Assert-Rejected { Assert-SigningPreservedFiles $baseline $root $targets } 'Added package file was accepted.'
    Write-Host 'PASS: signing policy, target selection, failure gates, and vendor/content preservation.'
} finally {
    foreach ($cert in @($publisher, $vendor, $wrong, $noUsage)) { $cert.Dispose() }
    Remove-Item -LiteralPath $temp -Recurse -Force
}
# The intentionally rejected SignTool warning must not become the GitHub pwsh
# wrapper's exit status after this script has completed successfully.
$global:LASTEXITCODE = 0
