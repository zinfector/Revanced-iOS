# ReVanced iOS

Experimental Windows/Python patcher and ARM64 iOS adapter for the analyzed, decrypted **YouTube 21.39.4** IPA. The source exposes 71 feature switches and a 30-feature expanded preset. This is a partial native port; complete Android parity and full stream/header spoofing are not implemented. Device target: iPhone 17 Pro Max / iOS 27.0, still untested.

- [Patcher usage and build instructions](patcher/README.md)
- [Every local Android patch and its iOS coverage/limits](patcher/COVERAGE.md)
- [Sign an IPA on GitHub's macOS cloud runner](SIGNING.md)
- [Device test checklist](patcher/DEVICE_TESTS.md)

The **Validate and build** workflow builds the native payload with Xcode, runs 31 host tests/static hook checks, packages the Windows GUI and uploads `native-build` and `windows-patcher` artifacts. The **Sign IPA** workflow runs manually, prepares or verifies an unsigned IPA, imports your Apple signing credentials into a temporary keychain, signs its nested code, verifies signatures and uploads the signed IPA. Credentials are supplied as encrypted GitHub Actions secrets.

YouTube IPAs, SDKs, reverse-engineering tool installations, signing credentials and local build outputs are excluded from Git. Supply your own supported decrypted IPA through a direct HTTPS URL. GitHub provides the runner; you provide the Apple certificate and provisioning profile. Signing verification does not prove installation or feature behavior on iOS.

Run locally:

```powershell
cd patcher
python -m unittest discover -s tests -v
python check_hooks.py
python build.py
python patcher.py patch original.ipa -o patched-unsigned.ipa --config configs/expanded.json
```

`build.py` uses Xcode on macOS or the documented local Zig/SDK toolchain on Windows. The compact Objective-C evidence in `patcher/profiles` supports the directly named hook check. Coverage reports were generated from the local ReVanced source tree; place that tree alongside `patcher` to regenerate them. Real-IPA integration needs the original IPA, which is not included.

See [LICENSE](LICENSE). The license covers the original patcher source, not YouTube, SDKs or third-party tools.
