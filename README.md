# ReVanced iOS

Experimental Windows/Python patcher and ARM64 iOS adapter for the analyzed, decrypted **YouTube 21.39.4** IPA. The source exposes 80 feature switches and a 30-feature expanded preset. This is a partial native port; complete Android parity and full stream/header spoofing are not implemented. Device target: iPhone 17 Pro Max / iOS 27.0. The user now reports successful login; playback, refresh and the new settings UI still require device checks.

- [Patcher usage and build instructions](patcher/README.md)
- [Every local Android patch and its iOS coverage/limits](patcher/COVERAGE.md)
- [Sign an IPA on GitHub's macOS cloud runner](SIGNING.md)
- [Device test checklist](patcher/DEVICE_TESTS.md)
- [Sign-in failure analysis and experimental SSO adapter](patcher/AUTHENTICATION_SCHEME.md)

Commits whose message contains `[skip tests]` skip regression tests, static hook checks and the GUI smoke test; compilation and packaging still run. This is used for 0.3.5 because the user explicitly requested no tests.

The **Validate and build** workflow normally builds the native payload with Xcode, runs 37 host tests/static hook checks, packages the Windows GUI and uploads `native-build` and `windows-patcher` artifacts. The **Sign IPA** workflow runs manually, prepares or verifies an unsigned IPA, imports your Apple signing credentials into a temporary keychain, signs its nested code, verifies signatures and uploads the signed IPA. Credentials are supplied as encrypted GitHub Actions secrets.

YouTube IPAs, SDKs, reverse-engineering tool installations, signing credentials and local build outputs are excluded from Git. Supply your own supported decrypted IPA through a direct HTTPS URL. GitHub provides the runner; you provide the Apple certificate and provisioning profile. Signing verification does not prove installation or feature behavior on iOS.

The 0.3.2 authentication experiment uses scoped SSO request identity and native private-keychain adapters, a `sideload-auth` preset and redacted diagnostics. The user now reports successful login after receiving the revised authentication IPA. Older IPAs are preserved. The earlier 0.3.1 failure and diagnostics are retained as historical evidence; refresh and cold-relaunch persistence remain unverified.

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

Version 0.3.4 adds **YouTube Settings → ReVanced**, with 13 groups, search, 113 runtime preferences, import/export, reset and diagnostics. The working sign-in implementation and existing preferences carry over. [Settings scheme](patcher/SETTINGS_SCHEME.md).

[Verified 0.3.4 artifact receipt](patcher/profiles/release-0.3.4.json): the [cloud run](https://github.com/zinfector/Revanced-iOS/actions/runs/37186801341) passed all 37 host tests, including the production preference/menu bridge harness and CLI export round trip, built the native payload and packaged the Windows GUI. That receipt applies to 0.3.4; its test results do not validate later SponsorBlock changes.

Version 0.3.5 repairs the SponsorBlock playback connection: the native player event-center clock feeds the canonical local-controller session, markers follow the owned native overlay/seekbar track, and diagnostics expose fetch/skip/render stages. See [SponsorBlock scheme](patcher/SPONSORBLOCK_SCHEME.md). No tests were run for this release at the user request; device behavior remains unverified. Use `YouTube-21.39.4-RVPort-0.3.5-SideStore-auth-unsigned.ipa` with SideStore.

[0.3.5 build-only artifact receipt](patcher/profiles/release-0.3.5.json): the [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37188729062) compiled the ARM64 payload and packaged the Windows GUI. Regression tests, static hook checks and the GUI smoke test were skipped. The receipt records all three unsigned IPA hashes and explicitly marks device playback unverified.

Version 0.3.6 corrects the confirmed SponsorBlock response-wrapper rejection and adds native modular-timeline markers. The 0.3.5 device report showed both clocks working but no segment request because the live check targeted the wrong response object. See [the repair evidence](patcher/profiles/sponsorblock-response-evidence.json). Use `YouTube-21.39.4-RVPort-0.3.6-SideStore-auth-unsigned.ipa`; tests remain skipped and corrected device behavior is unverified.

[0.3.6 build-only receipt](patcher/profiles/release-0.3.6.json): the [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37189790284) compiled the native payload and packaged the Windows patcher. Tests, static hook checks and GUI smoke tests were skipped. The three unsigned IPA hashes and exact build source commit are recorded; corrected device skip/marker behavior is unverified.

Version 0.3.7 adds a floating-miniplayer overlay control switch for the owned close/playback buttons and circular backgrounds, with native visibility restoration. Video-tap expansion, progress, badge and ad-skip controls are retained. It is off by default and available in ReVanced settings > Miniplayer and the Windows GUI. The catalog now has 80 switches and 114 runtime preferences. No tests ran at the user request; device behavior remains unverified. [Native evidence and concrete remaining miniplayer scope](patcher/profiles/miniplayer-controls-evidence.json).

[0.3.7 build-only receipt](patcher/profiles/release-0.3.7.json): the [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37190158948) compiled the ARM64 payload and packaged the Windows patcher. Tests, static hook checks and GUI smoke tests were skipped. The three unsigned IPA hashes and exact build source commit are recorded. This version carries the SponsorBlock response-unwrapping correction and modular timeline markers; device behavior remains unverified.

Version 0.3.8 adds video-ID-scoped native dislike estimates for verified slim formatted action labels and main-video entity count/accessibility paths. Native text restores when disabled. The badge and manual service voting remain; modern element buttons, online Shorts and automatic vote forwarding remain unported. Tests are skipped at the user request. [Native paths and limits](patcher/NATIVE_DISLIKES_SCHEME.md).
