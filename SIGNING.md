# GitHub cloud IPA signing

Open **Actions → Sign IPA → Run workflow**. This uses a GitHub-hosted macOS runner, not an Apple certificate issued by GitHub. The workflow needs your code-signing certificate/private key and provisioning profile; none are configured automatically. It follows GitHub's [Apple certificate installation guidance](https://docs.github.com/en/actions/how-tos/deploy/deploy-to-third-party-platforms/sign-xcode-applications).

## Configure repository secrets

Open **Settings → Secrets and variables → Actions → New repository secret**:

| Secret | Value |
|---|---|
| `BUILD_CERTIFICATE_BASE64` | Base64 bytes of your Apple development/distribution `.p12`, including its private key. |
| `P12_PASSWORD` | The password protecting that `.p12`; may be empty only if the export has no password. |
| `BUILD_PROVISION_PROFILE_BASE64` | Base64 bytes of the main app's development, Ad Hoc or enterprise `.mobileprovision`. |
| `IPA_SOURCE_URL` | Optional direct HTTPS source IPA URL, used when the workflow URL input is empty. Useful for a private, expiring download URL. |
| `EXTENSION_PROFILES_ZIP_BASE64` | Optional Base64 ZIP of matching extension `.mobileprovision` files; needed when retaining extensions unless the main profile also covers them. |

Base64 is an encoding; store these values in **secrets**, never files committed to Git. On Windows, prepare a value without printing it to a terminal:

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes('C:\path\certificate.p12')) | Set-Clipboard
[Convert]::ToBase64String([IO.File]::ReadAllBytes('C:\path\app.mobileprovision')) | Set-Clipboard
```

Paste each clipboard value into its corresponding GitHub secret. Clear the clipboard when finished. The workflow generates its temporary keychain password and deletes the keychain in a `finally` block; certificates/profiles remain only in the temporary runner workspace. GitHub-hosted runner teardown provides an additional cleanup boundary.

## Workflow inputs

| Input | Behavior |
|---|---|
| `ipa_url` | Direct HTTPS download; falls back to secret `IPA_SOURCE_URL`. Redirects must remain HTTPS. No Google Drive login/cookie handling is provided. |
| `ipa_kind` | `original`: build RVPort, patch the supported original IPA and then sign. `patched`: verify an existing unsigned RVPort IPA and then sign. |
| `ipa_sha256` | Required for `patched`. For `original`, empty uses the exact analyzed source hash `37fd59f89d706fb7f93614e12ddb9c09fe3f4a18c1e4609c2b715db6e9f3d88c`. A mismatched hash stops the job. |
| `preset` | `expanded` or `defaults`; applies only when patching an original IPA. |
| `bundle_id` | Optional new main app bundle ID. Empty derives the exact ID from the main profile. A wildcard profile requires an explicit matching bundle ID. |
| `strip_extensions` | Defaults to `true`. Set `false` only with matching profiles for every retained extension. |

Find a local IPA hash with `Get-FileHash -Algorithm SHA256 path\to\app.ipa`. For the locally generated 0.3 expanded artifact, the hash is recorded in `patcher/build/release-manifest.json` in the original workspace. Upload the IPA to storage you control and use its direct download URL; the workflow does not publish it to repository commits or Releases.

The certificate must be authorized by each profile, and all profiles must belong to the same team and be unexpired. For development/Ad Hoc installation, the profile must include your **iPhone's UDID**; a model name is insufficient. App Store profiles are refused by this direct-install workflow. Apple describes profile authorization and signed entitlements in [TN3125](https://developer.apple.com/documentation/technotes/tn3125-inside-code-signing-provisioning-profiles) and [TN2415](https://developer.apple.com/library/archive/technotes/tn2415/_index.html).

Retained extension IDs are mapped from the original main prefix to the new main ID while preserving their suffixes. Each gets a matching profile and its own signature. Entitlements are derived from those profiles; original Google capabilities are not carried over. Unresolved wildcard capabilities stop signing instead of being guessed. Custom Google groups, push, account integration and extension behavior may need additional porting for your own identifiers; stripping extensions avoids their profile requirement.

## Download and test

After success, download **signed-ipa** from the run's **Artifacts** section. It contains `signed.ipa` and a hash/signature report. The artifact retention is three days. Provisioning profiles are embedded in the IPA, as required for installation, so treat the artifact as containing your profile/device metadata. The `.p12` and its private key are never included in the artifact.

Install using your existing sideloading workflow, then follow [DEVICE_TESTS.md](patcher/DEVICE_TESTS.md). `codesign --verify --deep --strict` and ZIP CRC checks establish host signature/archive validity; iOS installation authorization and runtime behavior remain device checks. The unsigned patch-manifest verifier is not a signed-IPA verifier because signing changes the executable and bundle identity.

This repository's signing workflow has been implemented and its host-side checks tested. End-to-end signing requires the secrets and source IPA URL above; it cannot be verified without them.
