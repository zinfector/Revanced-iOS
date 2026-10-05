# ReVanced iOS

Latest release **0.3.31** includes structural Elements banner/feed-ad filtering, native cell fallbacks, verified inline-player ad suppression and detailed checkpoints. It retains the completed 0.3.30 speed and Shorts changes, authentication, RYD, native settings and SponsorBlock. Use the single `YouTube-21.39.4-RVPort-0.3.31-unsigned.ipa` with SideStore. No tests ran; device confirmation is pending. See [implementation and capture checkpoints](patcher/ADBLOCK_ELEMENTS_INLINE_IMPLEMENTATION.md).

Experimental Windows/Python patcher and ARM64 iOS adapter for the analyzed, decrypted **YouTube 21.39.4** IPA. The source exposes 82 feature switches and a 30-feature expanded preset. This is a partial native port; complete Android parity and full stream/header spoofing are not implemented. Device target: iPhone 17 Pro Max / iOS 27.0.

- [Patcher usage and build instructions](patcher/README.md)
- [Every local Android patch and its iOS coverage/limits](patcher/COVERAGE.md)
- [Sign an IPA on GitHub's macOS cloud runner](SIGNING.md)
- [Device test checklist](patcher/DEVICE_TESTS.md)
- [Sign-in failure analysis and experimental SSO adapter](patcher/AUTHENTICATION_SCHEME.md)

The **Validate and build** workflow normally builds the native payload with Xcode, packages the Windows GUI and uploads `native-build` and `windows-patcher` artifacts.

YouTube IPAs, SDKs, reverse-engineering tool installations, signing credentials and local build outputs are excluded from Git. Supply your own supported decrypted IPA through a direct HTTPS URL. GitHub provides the runner; you provide the Apple certificate and provisioning profile. Signing verification does not prove installation or feature behavior on iOS.

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
