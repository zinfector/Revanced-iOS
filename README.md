# ReVanced iOS

Experimental Windows/Python patcher and ARM64 iOS adapter for the analyzed, decrypted **YouTube 21.39.4** IPA. The source exposes 79 feature switches and a 30-feature expanded preset. This is a partial native port; complete Android parity and full stream/header spoofing are not implemented. Device target: iPhone 17 Pro Max / iOS 27.0. The user reports a Google sign-in rejection; playback and feature behavior remain unverified.

- [Patcher usage and build instructions](patcher/README.md)
- [Every local Android patch and its iOS coverage/limits](patcher/COVERAGE.md)
- [Sign an IPA on GitHub's macOS cloud runner](SIGNING.md)
- [Device test checklist](patcher/DEVICE_TESTS.md)
- [Sign-in failure analysis and experimental SSO adapter](patcher/AUTHENTICATION_SCHEME.md)

The **Validate and build** workflow builds the native payload with Xcode, runs 35 host tests/static hook checks, packages the Windows GUI and uploads `native-build` and `windows-patcher` artifacts. The **Sign IPA** workflow runs manually, prepares or verifies an unsigned IPA, imports your Apple signing credentials into a temporary keychain, signs its nested code, verifies signatures and uploads the signed IPA. Credentials are supplied as encrypted GitHub Actions secrets.

YouTube IPAs, SDKs, reverse-engineering tool installations, signing credentials and local build outputs are excluded from Git. Supply your own supported decrypted IPA through a direct HTTPS URL. GitHub provides the runner; you provide the Apple certificate and provisioning profile. Signing verification does not prove installation or feature behavior on iOS.

The 0.3.2 authentication experiment uses scoped SSO request identity and native private-keychain adapters, a `sideload-auth` preset and redacted diagnostics. Successful Google login on the target device remains unverified. Older 0.3 and 0.3.1 IPAs are unchanged. The 0.3.1 auth IPA installed and its hooks ran, but login failed with repeated keychain entitlement errors; 0.3.2 device results remain pending.

Run locally:

```powershell
cd patcher
python -m unittest discover -s tests -v
python check_hooks.py
python build.py
python patcher.py patch original.ipa -o sideload-auth-unsigned.ipa --config configs/sideload-auth.json --strip-extensions
```

`build.py` uses Xcode on macOS or the documented local Zig/SDK toolchain on Windows. The compact Objective-C evidence in `patcher/profiles` supports the directly named hook check. Coverage reports were generated from the local ReVanced source tree; place that tree alongside `patcher` to regenerate them. Real-IPA integration needs the original IPA, which is not included.

See [LICENSE](LICENSE). The license covers the original patcher source, not YouTube, SDKs or third-party tools.

Version 0.3.3 adds selected miniplayer gesture, badge, corner, size and background-opacity controls plus a Shorts app-shortcut switch. The 0.3.2 authentication implementation is preserved. See [miniplayer behavior and limits](patcher/MINIPLAYER_SCHEME.md). New options retain native defaults in the ordinary and authentication presets.

[Verified 0.3.3 artifact receipt](patcher/profiles/release-0.3.3.json): the [cloud run](https://github.com/zinfector/Revanced-iOS/actions/runs/37184316117) passed all 35 host tests, built the ARM64 payload and packaged the 79-switch Windows GUI. Real-IPA verification preserves retained original files and executable sections/load commands. These checks do not establish successful device login or UI behavior.
