# GitHub cloud IPA signing

Open **Actions → Sign IPA → Run workflow**. This uses a GitHub-hosted macOS runner, not an Apple certificate issued by GitHub. The workflow needs your code-signing certificate/private key and provisioning profile; none are configured automatically. It follows GitHub's [Apple certificate installation guidance](https://docs.github.com/en/actions/how-tos/deploy/deploy-to-third-party-platforms/sign-xcode-applications).

For SideStore, use **`YouTube-21.39.4-RVPort-0.3.4-SideStore-auth-unsigned.ipa`** and let SideStore sign it. Version 0.3.4 adds the dedicated ReVanced settings pane and retains the authentication implementation that now permits login according to the user. All six extensions are removed.

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
| `preset` | `expanded`, `defaults` or experimental `sideload-auth`; applies only when patching an original IPA. |
| `bundle_id` | Optional new main app bundle ID. Empty derives the exact ID from the main profile. A wildcard profile requires an explicit matching bundle ID. |
| `strip_extensions` | Defaults to `true`. Set `false` only with matching profiles for every retained extension. |

Find a local IPA hash with `Get-FileHash -Algorithm SHA256 path\to\app.ipa`. For the locally generated 0.3 expanded artifact, the hash is recorded in `patcher/build/release-manifest.json` in the original workspace. Upload the IPA to storage you control and use its direct download URL; the workflow does not publish it to repository commits or Releases.

The certificate must be authorized by each profile, and all profiles must belong to the same team and be unexpired. For development/Ad Hoc installation, the profile must include your **iPhone's UDID**; a model name is insufficient. App Store profiles are refused by this direct-install workflow. Apple describes profile authorization and signed entitlements in [TN3125](https://developer.apple.com/documentation/technotes/tn3125-inside-code-signing-provisioning-profiles) and [TN2415](https://developer.apple.com/library/archive/technotes/tn2415/_index.html).

Retained extension IDs are mapped from the original main prefix to the new main ID while preserving their suffixes. Each gets a matching profile and its own signature. Entitlements are derived from those profiles; original Google capabilities are not carried over. Unresolved wildcard capabilities stop signing instead of being guessed. Custom Google groups, push, account integration and extension behavior may need additional porting for your own identifiers; stripping extensions avoids their profile requirement.

## Download and test

After success, download **signed-ipa** from the run's **Artifacts** section. It contains `signed.ipa` and a hash/signature report. The artifact retention is three days. Provisioning profiles are embedded in the IPA, as required for installation, so treat the artifact as containing your profile/device metadata. The `.p12` and its private key are never included in the artifact.

Install using your existing sideloading workflow, then follow [DEVICE_TESTS.md](patcher/DEVICE_TESTS.md). `codesign --verify --deep --strict` and ZIP CRC checks establish host signature/archive validity; iOS installation authorization and runtime behavior remain device checks. The unsigned patch-manifest verifier is not a signed-IPA verifier because signing changes the executable and bundle identity.

This repository's signing workflow has been implemented and its host-side checks tested. End-to-end signing requires the secrets and source IPA URL above; it cannot be verified without them.

The user previously reported Google rejecting sign-in, and now reports login success after the authentication revision. Cloud signing alone does not implement account-authentication compatibility. See [the authentication scheme](patcher/AUTHENTICATION_SCHEME.md) for the verified native request/keychain path and the experimental adapter introduced in 0.3.1. Select `sideload-auth` when patching an original IPA to enable both authentication switches. Refresh and persistence remain device checks; older payloads are unchanged.

## Extension-prefix installation failure

`IXErrorDomain Code=8` with `AppMigrationExtension` and a required prefix such as `com.google.ios.youtube.<team>.` means the sideloader renamed the main app while an extension retained its old ID. The ordinary 0.3.1 expanded IPA retains six extensions, including the migration extension under `Extensions/`, so removing only `PlugIns/` is insufficient.

For SideStore, select `YouTube-21.39.4-RVPort-0.3.1-SideStore-auth-unsigned.ipa` and let SideStore sign that file. This artifact removes all six extensions and includes the expanded preset plus the experimental authentication adapters. Its SHA-256 is `1481372967024a657c94381324b36a6f90148245adc9b0d80407de63d719bb70`. Rebuilding from the original uses:

```powershell
python patcher.py patch original.ipa -o sideload-auth-unsigned.ipa --config configs/sideload-auth.json --strip-extensions
```

The GUI now removes extensions by default. Extension removal addresses this placeholder failure; installation and Google login still need device verification. Retaining extensions requires remapping their IDs and re-signing them with compatible profiles, as the cloud workflow already does.

## 0.3.2 authentication revision

The user installed the extension-free 0.3.1 auth IPA and supplied diagnostics confirming its identity hook ran, but its keychain probe failed and repeated SSO reads returned missing-entitlement errors. The new 0.3.2 auth artifact uses native private-keychain storage and scoped SSO request-user-agent identity, with redacted auth-advice diagnostics. Select `sideload-auth` for a new source build. Successful login remains a device acceptance test; cloud signatures alone do not prove it.

Previous SideStore experiment: `YouTube-21.39.4-RVPort-0.3.2-SideStore-auth-unsigned.ipa`, SHA-256 `f1b3e2c752e3187d72b48eb365260cc414bd9921cc19d6a6137f0daae9a59fa0`. All six extensions are removed. See [the 0.3.2 receipt](patcher/profiles/release-0.3.2.json); old artifacts are preserved.

## 0.3.3 miniplayer and shortcut release

The previous extension-free SideStore artifact is `YouTube-21.39.4-RVPort-0.3.3-SideStore-auth-unsigned.ipa`, SHA-256 `2728c87e903c310af89dd87866f768cd44d1c43096985d4fd0b0e7ab158e1dd4`. It preserves the 0.3.2 authentication implementation and adds optional miniplayer and app-shortcut controls; the new options retain native defaults in this preset. All six extensions are removed. [Release receipt](patcher/profiles/release-0.3.3.json) and [behavior/limits](patcher/MINIPLAYER_SCHEME.md). At publication, login and the new UI behavior were unverified; the user subsequently reported login success.

## 0.3.4 dedicated ReVanced settings

The latest SideStore IPA is `YouTube-21.39.4-RVPort-0.3.4-SideStore-auth-unsigned.ipa`, SHA-256 `ea90a0543758404ce9bfe993a523f76f4902275880e67d04ea10dc7f257d45ab`. It includes the expanded preset and both authentication adapters. Existing `RVPort` preferences carry over. Open **YouTube Settings → ReVanced** for grouped controls, search, import/export, reset, Video tools and diagnostics. The three-finger hold remains a fallback. [Release receipt](patcher/profiles/release-0.3.4.json) and [settings integration](patcher/SETTINGS_SCHEME.md). User-reported login success is recorded separately from host tests; the new settings UI still requires device checks.
