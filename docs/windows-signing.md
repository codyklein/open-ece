# Windows distribution signing

OpenECE's portable Windows distribution requires embedded Authenticode signatures
for all packaged EXE/DLL files. This is a packaging boundary; application behavior,
artwork, engineering APIs and the published v1.0.0 release remain unchanged.

## Protected candidate workflow

Run **Build, test and package** on the reviewed candidate branch/commit with the
`signed_candidate` workflow-dispatch input set to `true`. Push/PR runs and the
default dispatch remain unsigned developer validation; they never receive Azure
credentials or permission to request an OIDC token. Unsigned developer ZIPs are
not release-distribution candidates.

The signing job waits for the complete Fedora matrix and Windows Debug/Release
tests, then uses the reviewer-protected `windows-signing` GitHub environment.
Approve only the intended commit and staged artifact. The job has `contents: read`
and `id-token: write`; it does not tag, publish, or modify repository settings.

Provisioned configuration:

- Public Trust account: `openece-signing-cody`.
- Certificate profile: `openece-public`.
- Expected publisher CN: `Cody Klein` (confirmed from the profile preview).
- Environment secrets: `AZURE_CLIENT_ID`, `AZURE_TENANT_ID`,
  `AZURE_SUBSCRIPTION_ID`.
- Environment variables: `ARTIFACT_SIGNING_ACCOUNT`, `ARTIFACT_SIGNING_PROFILE`,
  `ARTIFACT_SIGNING_ENDPOINT`.
- Entra federation uses the repository's immutable owner/repository IDs and this
  environment. The service principal has Certificate Profile Signer scoped to
  this profile. Do not replace federation with a client secret or private key.

`azure/login` obtains short-lived Azure CLI credentials through OIDC. The official
`azure/artifact-signing-action` uses only that credential path; all other credential
providers are excluded. Both actions are pinned to reviewed commits in the
workflow. The signing action pins its PowerShell module and signing-tool packages;
its dependency cache is disabled for this protected job. No private key is stored
in the repository, GitHub artifacts or GitHub secrets.

## Exact order and gates

1. Build/test the commit and stage Release runtime, branding, docs and licenses.
   `package.ps1 -StageOnly` creates neither `SHA256SUMS.txt` nor a ZIP. Its sidecar
   identifies the source commit and actual installed Release Qwt DLL basename.
2. Check every vendor binary with `Get-AuthenticodeSignature` and x64 SDK
   `signtool verify /pa /all /v /tw`. Require valid embedded, timestamped RSA
   code-signing signatures from Qt or Microsoft. The two signing targets must be
   unsigned. A vendor verification failure stops the process; it is not repaired
   by overwriting a vendor signature.
3. Sign **only** staged `openece.exe` and the discovered Release Qwt DLL with
   SHA-256 and RFC 3161 timestamping at `http://timestamp.acs.microsoft.com`, using
   SHA-256 for the timestamp digest. Artifact Signing certificates are short-lived,
   so successful timestamp verification is required.
4. Verify every EXE/DLL again under Windows Authenticode policy. Both new signatures
   must identify `Cody Klein`; all timestamps and code-signing usages must pass.
   Compare a pre-signing inventory to require every other package file, including
   vendor binaries, to remain byte-identical. Missing/added files fail the gate.
5. Only after verification, `finalize-package.ps1 -RequireSigned` creates the
   complete manifest, ZIP and `.zip.sha256` sidecar. Verification reports and stage
   metadata are separate CI artifacts, not release package contents.
6. A fresh Windows runner extracts the final ZIP into a Unicode/space path,
   verifies all signatures and manifest hashes, and runs existing packaged
   startup, branding, persistence and thirteen-example checks with development paths
   removed. The separate persistence probe is never distributed.

Local `package.ps1 -QtRoot ...` retains the existing unsigned developer-package
workflow. Local signed finalization requires an already signed stage and Windows
SDK SignTool; it cannot sign by itself. `test-package.ps1 -RequireSigned` adds
signature verification to the existing package tests.

## Evidence and physical acceptance

The signed workflow uploads `OpenECE-v1.1.0-windows-x86_64` containing the inner ZIP
and its SHA-256 sidecar, plus a separate `windows-signature-verification` artifact
with before/after inventories, public certificate subjects/thumbprints and hashes.
The fresh-runner logs include signature verification. Test seams use ephemeral
self-signed certificates and mocked trust solely for policy regression tests;
production never accepts those certificates as trusted.

On Windows, inspect the actual extracted package:

```powershell
Get-ChildItem 'C:/OpenECE candidate' -Recurse -File |
  Where-Object { $_.Extension -in '.exe', '.dll' } |
  ForEach-Object { Get-AuthenticodeSignature -LiteralPath $_.FullName } |
  Format-Table Path, Status, SignerCertificate, TimeStamperCertificate
signtool verify /pa /all /v /tw 'C:/OpenECE candidate/openece.exe'
```

All distributed binaries must verify, not merely the application. A valid
timestamp can preserve a signature after the signer certificate expires; do not
judge trust using the leaf certificate's expiry date alone.

Signing changes binary hashes. RFC 3161 timestamps and rotating certificates mean
a signed rebuild is not promised byte-identical. Retain the exact accepted ZIP,
source commit, tool pins, signature reports, manifest and final hash. Never rebuild
or re-sign the artifact after physical acceptance and call it the same candidate.

Physical Windows 11 acceptance uses the exact final ZIP with **Smart App Control
enabled**, in a normal user context without development paths: verify the ZIP
hash, freshly extract under a Unicode/space path, launch, inspect About 1.1.0,
and run the transient RC/RL/RLC and source-breakpoint examples. Check Step,
Pause/Resume, terminal Cancel, ordered before/after samples, current signs, mixed
probe ordering and live splitter/scrolling. Import a schema-1 file, check upgrade
protection and Save As schema 2; reopen inertly. Run a short existing-domain smoke
and exit normally. Also inspect the executable/window icon. Windows Server CI is not evidence of desktop
Smart App Control acceptance. Signing does not override a malware verdict or
guarantee every SmartScreen/enterprise-policy decision. Do not disable SAC,
install a private root, or remove download security markings as a release fix.

## References

- [Microsoft signing integrations](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations)
- [Official signing action and pinned inputs](https://github.com/Azure/artifact-signing-action)
- [GitHub OIDC with Azure](https://docs.github.com/en/actions/how-tos/secure-your-work/security-harden-deployments/oidc-in-azure)
- [Smart App Control signing guidance](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control)
