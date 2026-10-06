# Shared release-verification functions. No credentials or signing operations.
Set-StrictMode -Version Latest

function Get-ReleaseSigningFiles([string]$PackageDirectory, [string]$QwtRuntime) {
    if ([string]::IsNullOrWhiteSpace($QwtRuntime) -or
        [IO.Path]::GetFileName($QwtRuntime) -cne $QwtRuntime -or
        $QwtRuntime -notmatch '(?i)\.dll$') { throw 'Expected a Release Qwt DLL basename.' }
    $root = (Resolve-Path -LiteralPath $PackageDirectory).Path
    $files = @((Join-Path $root 'openece.exe'), (Join-Path $root $QwtRuntime))
    foreach ($file in $files) {
        if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing signing target: $file" }
    }
    return $files
}

function Get-ReleaseSignTool {
    $sdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
    $tools = @(Get-ChildItem -LiteralPath $sdk -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Name } -Descending | ForEach-Object { Join-Path $_.FullName 'x64/signtool.exe' } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    if (!$tools.Count) { throw 'Install the Windows SDK x64 SignTool for signature verification.' }
    return $tools[0]
}

function Assert-ReleaseSignature($Signature, [string]$Path, [string[]]$Publishers) {
    if ([string]$Signature.Status -ne 'Valid' -or [string]$Signature.SignatureType -ne 'Authenticode') {
        throw "Embedded Authenticode verification failed for ${Path}: $($Signature.Status) / $($Signature.StatusMessage)"
    }
    $cert = $Signature.SignerCertificate
    if (!$cert -or !$Signature.TimeStamperCertificate) { throw "Missing publisher or timestamp: $Path" }
    $publisher = $cert.GetNameInfo([Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false)
    if ($publisher -cnotin $Publishers) { throw "Unexpected publisher for ${Path}: $publisher" }
    if ($cert.PublicKey.Oid.Value -ne '1.2.840.113549.1.1.1') { throw "Expected RSA signing certificate: $Path" }
    $codeSigning = $false
    foreach ($extension in $cert.Extensions) {
        if ($extension -is [Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]) {
            foreach ($usage in $extension.EnhancedKeyUsages) {
                if ($usage.Value -eq '1.3.6.1.5.5.7.3.3') { $codeSigning = $true }
            }
        }
    }
    if (!$codeSigning) { throw "Missing code-signing certificate usage: $Path" }
}

function Assert-SigningPreservedFiles($Baseline, [string]$PackageDirectory, [string[]]$OwnedPaths) {
    $files = @(Get-ChildItem -LiteralPath $PackageDirectory -Recurse -File)
    if ($files.Count -ne @($Baseline).Count) { throw 'Package file inventory changed during signing.' }
    $owned = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $OwnedPaths) { [void]$owned.Add($path) }
    foreach ($entry in $Baseline) {
        $file = [IO.Path]::GetFullPath((Join-Path $PackageDirectory $entry.path))
        if (!$file.StartsWith($PackageDirectory + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            !(Test-Path -LiteralPath $file -PathType Leaf)) { throw 'Invalid or missing baseline file.' }
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        if (!$owned.Contains($file) -and $hash -ne $entry.sha256) { throw "Non-target package file changed during signing: $($entry.path)" }
        if ($owned.Contains($file) -and $hash -eq $entry.sha256) { throw "Signing target did not change: $($entry.path)" }
    }
}

function Test-PackageSignatures {
    param(
        [Parameter(Mandatory)][string]$PackageDirectory,
        [Parameter(Mandatory)][string]$QwtRuntime,
        [ValidateSet('BeforeSigning', 'AfterSigning')][string]$Phase = 'AfterSigning',
        [string]$ExpectedPublisher = 'Cody Klein',
        [string]$SignTool = (Get-ReleaseSignTool)
    )
    $root = (Resolve-Path -LiteralPath $PackageDirectory).Path
    $owned = @(Get-ReleaseSigningFiles $root $QwtRuntime)
    $binaries = @(Get-ChildItem -LiteralPath $root -Recurse -File |
        Where-Object { $_.Extension -in '.exe', '.dll' } | Sort-Object FullName)
    $report = @()
    foreach ($file in $binaries) {
        $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
        $isOwned = $file.FullName -in $owned
        if ($Phase -eq 'BeforeSigning' -and $isOwned) {
            if ([string]$signature.Status -ne 'NotSigned') { throw "Signing target is not unsigned: $($file.Name)" }
        } else {
            $publishers = if ($isOwned) { @($ExpectedPublisher) } else { @('The Qt Company Oy', 'Microsoft Corporation', 'Microsoft Windows Software Compatibility Publisher') }
            Assert-ReleaseSignature $signature $file.FullName $publishers
            # /pa verifies embedded user-mode signatures; /all checks every
            # signature and /tw makes a missing timestamp a failing warning.
            & $SignTool verify /pa /all /v /tw $file.FullName | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "SignTool rejected $($file.FullName) ($LASTEXITCODE)." }
        }
        $report += [pscustomobject]@{
            path = [IO.Path]::GetRelativePath($root, $file.FullName).Replace('\', '/')
            status = [string]$signature.Status
            subject = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { $null }
            thumbprint = if ($signature.SignerCertificate) { $signature.SignerCertificate.Thumbprint } else { $null }
            timestampSubject = if ($signature.TimeStamperCertificate) { $signature.TimeStamperCertificate.Subject } else { $null }
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    Write-Host "PASS: $Phase verification of $($binaries.Count) packaged EXE/DLL files."
    return $report
}
