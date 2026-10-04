# YouTube iOS Patcher 0.3.15

Version 0.3.15 addresses the mounting failure confirmed by foreground RYD Report 615. It mounts count text inside the existing native dislike button, using native subnodes when their contexts match and an owned noninteractive UILabel fallback otherwise. Foreground checkpoints distinguish the rendering modes and remaining constraints. New count display remains device-unverified. Authentication, SponsorBlock and the packaged parallel settings UI are retained. No tests run.

A Windows GUI and Python CLI that inject a native adapter into the **analyzed, decrypted YouTube 21.39.4 ARM64 IPA**. The patcher has 80 feature switches plus configurable speed, network quality, SponsorBlock policies, thumbnails, gesture, theme, branding and request fields. The original IPA is preserved; existing outputs are refused.

**Experimental: the user now reports successful Google login on the target iPhone. Playback, token refresh and the new settings UI still require device checks.** The output is unsigned and must be re-signed, including RVPort.dylib and retained extensions, before installation. Compilation, hook metadata matches and archive checks establish the patch artifact's structure; they do not establish playback behavior.

GitHub source checkout: build/output/dist files are generated locally or available through Actions artifacts. See the repository's `SIGNING.md` for manual cloud signing. The `cloud_*.py` scripts use your Apple signing credentials; GitHub does not supply a certificate. Twelve additional signing/download tests cover certificate/profile matching, expiry, bundle prefixes, wildcard entitlement refusal, nested signing order, validated archive extraction and download integrity. Authentication, miniplayer and settings tests bring the host suite to 37 tests; the three native Objective-C harnesses require macOS.

See [AUTHENTICATION_SCHEME.md](AUTHENTICATION_SCHEME.md) for the Android GmsCore comparison, verified iOS SSO request/keychain path and revised experimental 0.3.2 sign-in adapter. The older 0.3 and 0.3.1 IPAs remain unchanged; the user reports successful device login; refresh and persistence remain unverified.

The 0.3.1 auth artifact installed and its identity hook ran on the target device, but login failed and repeated keychain reads returned missing-entitlement errors. Version 0.3.2 uses the native private keychain, corrects the SSO request user-agent identity, and adds redacted auth-advice diagnostics; the user now reports login success on the target device.

For SideStore, use `output/YouTube-21.39.4-RVPort-0.3.15-SideStore-auth-unsigned.ipa`. It uses the macOS-built payload, removes extensions and enables the two independently switchable authentication adapters alongside the expanded preset. SideStore signs and installs it. The ordinary defaults/expanded presets leave these adapters disabled. The ordinary default/expanded IPAs also retain six app extensions. Use the `SideStore-auth` file for sideloading; it includes the expanded features plus authentication adapters.

Version 0.3.3 adds miniplayer drag/horizontal-drag/double-tap switches, message/Premium badge hiding, square corners, minimum dimension, circular-background opacity, and a Shorts app-shortcut switch. These options are off/native in the default, expanded and SideStore-auth presets; configure them in the GUI or native settings. The 0.3.2 authentication adapter is preserved. See [MINIPLAYER_SCHEME.md](MINIPLAYER_SCHEME.md) for exact behavior and limits.

Version 0.3.4 adds **YouTube Settings → ReVanced**, with 13 preference groups, search, 113 runtime controls, configuration import/export, reset, Video tools and diagnostics. Existing saved preferences and the working authentication implementation carry over. See [SETTINGS_SCHEME.md](SETTINGS_SCHEME.md) for the native integration and verification scope. The three-finger hold opens the same pane as a fallback.

Version 0.3.5 connects SponsorBlock to the native player event-center clock, shares the local playback-controller session with marker rendering, and aligns markers to the native seekbar track. It adds a redacted `sponsorblock` diagnostic section. No tests were run for 0.3.5 at the user's request; compilation and IPA construction do not establish device behavior. See [SPONSORBLOCK_SCHEME.md](SPONSORBLOCK_SCHEME.md).

## Deliverables

- `output/YouTube-21.39.4-RVPort-0.3.15-SideStore-auth-unsigned.ipa`: expanded preset plus both authentication adapters; extensions removed.
- `output/YouTube-21.39.4-RVPort-0.3.15-unsigned.ipa` and `output/YouTube-21.39.4-RVPort-0.3.15-expanded-unsigned.ipa`: default/expanded configurations with authentication adapters disabled and six extensions retained; require a signer that remaps and signs every extension.
- `dist/YouTube-iOS-Patcher.exe`: standalone Windows GUI with scrolling feature selection, JSON config loading and optional PNG branding.
- `output/YouTube-21.39.4-RVPort-0.3-unsigned.ipa`: default configuration; video ads and background playback enabled, additional features available in native settings.
- `output/YouTube-21.39.4-RVPort-0.3-expanded-unsigned.ipa`: 30-feature experimental preset from `configs/expanded.json`.
- [COVERAGE.md](COVERAGE.md) and [coverage.json](coverage.json): mapping and limitations for all 113 local YouTube patch declarations, including the 51 named patches and shared factories/dependencies.
- `build/release-manifest.json` and `build/release-manifest-0.3.9.json`: build hashes, sizes and test-skip status; previous receipts are preserved separately. Public receipt: [profiles/release-0.3.9.json](profiles/release-0.3.9.json).

The old 0.1, 0.2, 0.3, 0.3.1 and 0.3.2 unsigned IPAs are retained separately. Supported source identity is in `profiles/youtube-21.39.4.json`; other binaries are refused. This targets iOS 17 or later, thin ARM64, with an unencrypted main executable. It does not add server authorization or credentials.

## Implemented adapters

The original ad/background/SponsorBlock/feed/speed/quality hooks are extended with:

- Native PiP gates and start action, clean URL/timestamp copying, downloader share handoff, looping, fullscreen transitions and interruption handling.
- Custom/remembered playback speed; direct advanced quality menu; remembered resolution cap with separate Wi-Fi/cellular policies; selected Premium action hiding; original audio preference; positively identified VP9 and HDR format filtering.
- Player/control hiding, ambient and automatic-panel suppression, captions-on-mute/start gates, haptics, rolling numbers, Shorts startup/advance/routing options and selected native miniplayer behavior.
- Double-tap interval override and gesture suppression, tap-to-seek, fullscreen brightness/volume swipes, progress/timestamp hiding, progress color and scrim opacity.
- Positive component filtering for feed/action/flyout/comments/custom layout patterns; selected Shorts navigation and initial page overrides.
- Video-frame and DeArrow thumbnails with bounded image availability probes, per-screen thumbnail modes, faster still variants, configurable HTTPS thumbnail proxying, public redirect unwrapping, external URL endpoint opening and sharing-parameter cleanup.
- Native dark/light style and selected palette background colors, supplied header PNG and display-name/legacy-icon branding.
- Verified request client-version, phone/tablet enum and screen/window dimension overrides on copied protobuf client-info.
- Native RYD dislike text and an inline element-backed watch-action estimate, plus manual service votes; SponsorBlock per-category skip/skip-once/manual/marker-only/ignore policies, seekbar markers, highlight jumps, autoskip/manual skip/undo, minimum duration, manual segment/category votes and reviewed submissions; announcements reader; watch-history DNS diagnostic.
- SponsorBlock account lookup and manual username changes, local estimated seek statistics, and configuration/diagnostic report copying for device feedback.

Read the per-patch limits in [COVERAGE.md](COVERAGE.md). Downloads are a share handoff requiring a compatible installed extension. RYD service votes are separate from your YouTube account's votes. Thumbnail proxying requires your own compatible endpoint. Age/login/rental verification remains native. Full stream replacement and alternate-client transport/header spoofing are **not implemented**; request-field overrides are not equivalents. Android GmsCore services are not transplanted; 0.3.2 instead adapts selected native SSO behavior. ContentProvider and system-back fixes have no corresponding mechanism in this IPA. Many UI patches implement a selected subset, not complete Android preference or resource parity.

In the app, open **YouTube Settings → ReVanced**, or hold three fingers for one second as a fallback. The pane contains grouped switches and value editors, search, import/export and reset. **Copy configuration** is on the root screen; **Copy diagnostic report** is under Hook diagnostics. Reports include hook statuses and effective configuration without service identities. Video tools are available from the root ReVanced settings screen. SponsorBlock Tools exposes category behavior/color controls and manual contribution actions. Reopen the video or restart after changes affecting request construction/player models or network quality policy; not every feature updates an existing player immediately.

## Windows GUI

Run `dist/YouTube-iOS-Patcher.exe`, select the original IPA and a new output path, select features, then create the patched IPA. `Load config` supports the full JSON configuration. `Branding` accepts a display name, header PNG and optional icons. A phone icon needs both 120x120 and 180x180 PNGs; an iPad icon accepts 152x152. PNGs must be 8-bit RGB/RGBA, non-interlaced, at most 2048x2048 and 5 MB; the patcher validates them without resizing.

The GUI removes app extensions by default for sideloading. Clear that checkbox only when your signer remaps each retained extension ID to the renamed main app prefix and signs each with a matching profile. Removing extensions also removes their widgets, notification helpers, external sharing and migration integration.

The GUI packages Python, its supported profile and the built dylib. It does not package Jadx, Ghidra, SDKs or the source IPA.

## Command line

Python 3.11 or later, standard library only. Run from this directory:

```powershell
python patcher.py inspect "..\Ghidra Project\com.google.ios.youtube_21.39.4_und3fined.ipa"
python patcher.py patch "original.ipa" -o "patched-unsigned.ipa"
python patcher.py patch "original.ipa" -o "sideload-auth-unsigned.ipa" --config configs/sideload-auth.json --strip-extensions
python patcher.py verify "patched-unsigned.ipa"
```

`--features` replaces the default enabled set:

```powershell
python patcher.py patch "original.ipa" -o "custom-unsigned.ipa" --features video_ads,background_playback,sponsorblock,picture_in_picture,return_dislikes,copy_video_url,custom_speed_menu --speed 1.25 --quality 1080
python patcher.py patch "original.ipa" -o "branded-unsigned.ipa" --app-name "My YouTube" --header-image header.png --icon-120 icon120.png --icon-180 icon180.png
```

`configs/defaults.json` lists every key. `configs/expanded.json` enables 30 selected adapters; this is a test preset, not a device-qualified recommendation. `configs/all-candidates.json` and `--all-features` enable all boolean candidates. Some choices conflict: loop takes precedence over exit-at-end; disabling double-tap overrides its interval; manual SponsorBlock suppresses autoskip; hiding Shorts/seekbar controls changes where their other options apply. Header/proxy toggles require supplied assets/an endpoint. Do not interpret the all-candidates configuration as a coherent everyday preset.

JSON examples for non-boolean options:

```json
{
  "double_tap_seconds": 15,
  "custom_speeds": [0.5, 1, 1.25, 1.5, 2, 3],
  "overlay_opacity": 0.6,
  "seekbar_color": "#FF3300",
  "theme": "dark",
  "start_page": "FEsubscriptions",
  "thumbnail_frame": 2,
  "thumbnail_modes": {"home": "dearrow-stills", "player": "stills", "library": "original"},
  "sponsor_behaviors": {"sponsor": "skip", "intro": "skip-once", "poi_highlight": "manual-skip"},
  "sponsor_colors": {"sponsor": "#00D400"},
  "sponsor_min_duration": 1,
  "wifi_quality": 1080,
  "cellular_quality": 480,
  "theme_light_background": "#FFFFFF",
  "theme_dark_background": "#101010",
  "thumbnail_proxy_url": "https://your-image-proxy.example/image",
  "client_version": "21.39.4",
  "form_factor": "tablet",
  "screen_width_points": 1920,
  "screen_height_points": 1080
}
```

The thumbnail proxy endpoint receives a URL-encoded `url` query parameter and must return the image bytes. No default proxy is supplied. DeArrow has a separate `dearrow_url`; modes require the relevant master feature switches. Still/DeArrow candidates replace originals only after a successful bounded image probe; first display may keep the original until a cell refresh. Live/unknown thumbnail variants retain their original image. Context detection and image-cache refresh need device verification.

`default_quality` is a resolution cap with fallback, not a guarantee of an exact quality or access to unavailable formats. Network quality values use `-1` to inherit and `0` for Auto; unknown network paths use the global policy. Explicit remembered route selections override configured caps. `sponsor_behaviors` supports `skip`, `skip-once`, `manual-skip`, `seekbar-only` and `ignore`; highlight points jump to the point rather than skip a range. Local statistics count seek requests and estimated skipped time, not confirmed playback time saved. Palette color overrides affect selected getters, not every app surface. `layout_patterns`/`action_patterns`/`flyout_patterns`/`comment_patterns` are positive UTF-8 identifiers in iOS element-data; arbitrary broad patterns can hide unrelated surfaces.

`--ad-strategy response|trigger|coordinator` chooses one strategy. `--strip-extensions` removes .appex bundles under both `PlugIns` and `Extensions`. Select it for CLI sideloading builds; the CLI otherwise retains extensions for a signer that remaps and signs them. The GUI selects extension removal by default. `verify` checks unsigned patch artifacts; a signer modifies code and signatures, so it is not a verifier for subsequently signed IPAs.

## Injection and evidence

The patcher appends `LC_LOAD_DYLIB` within existing zero-filled Mach-O header padding, removes the main executable's obsolete `LC_CODE_SIGNATURE` command, preserves section and LINKEDIT bytes, preserves existing library ordinals and checks chained fixups. It refuses section relocation, insufficient padding, encrypted/unsupported sources, duplicate injection and output overwrite. Archive signature directories/profiles are removed; payload/config/branding hashes are recorded and verified. Publication uses exclusive creation through a same-filesystem hard link (NTFS is supported).

Every installed hook checks the Objective-C type encoding at runtime. Missing methods, incompatible ABIs and unsupported runtime identity leave those hooks disabled. Runtime UUID/version checks allow re-signers to change the bundle ID. The prior static [hook-check.json](build/hook-check.json) is historical 0.3.4 evidence and was not rerun for 0.3.5; directly named hooks can be checked separately; loop-generated hooks, inherited methods and dynamic GPB accessors still require runtime diagnostics. Extracted classes and additional Ghidra decompilations are retained in `../analysis/evidence` and `build/extras-*`.

Services follow the official [SponsorBlock API](https://wiki.sponsor.ajay.app/w/API_Docs) and [RYD API schema](https://returnyoutubedislikeapi.com/swagger/v1/swagger.json). Counts are cached, requests bounded and stale video replies rejected. Contribution POSTs only occur after explicit in-app user actions; development checks do not submit votes or segments. RYD proof-of-work runs off the UI thread and has a local work/time cap. Service identities are random local IDs in app preferences, not YouTube credentials. Announcements use the endpoint/schema from the local Android implementation; endpoint availability was not verified successfully. Audio handling uses [AVAudioSession interruption notifications](https://developer.apple.com/documentation/avfaudio/avaudiosession/interruptionnotification).

DeArrow request parameters and fallback follow the [official thumbnail cache implementation](https://github.com/ajayyy/DeArrowThumbnailCache/blob/master/app.py). Read-only host probes returned valid JPEG still and DeArrow WebP headers for an 11-character public video ID; see `build/thumbnail-service-check.json`. This verifies those host responses only. Network quality reads the passive [Apple Network path monitor](https://developer.apple.com/documentation/network/nw_path_monitor_set_update_handler(_:_:)); it does not send connectivity requests.

## Build and verify

```powershell
python build.py
python -m unittest discover -s tests -v
python check_hooks.py
python coverage.py
python package_windows.py
python gui.py --self-test build/gui-smoke.json
python integration_check.py "original.ipa" "patched-unsigned.ipa" -o build/verification-0.3.json
```

The existing Windows toolchain uses Zig 0.14.1, Theos iPhoneOS16.5.sdk at commit `0222fd5413cf4b9af096f37b4621afa2688572f7`, and PyInstaller 6.22.3 under `.tools/`. Compilation targets ARM64 iOS 17 and treats warnings as errors. Native source hashes and compiler arguments are in `build/build-manifest.json`. On macOS, `build.py` can use Xcode's iOS SDK. Tool sources: [Zig package](https://pypi.org/project/ziglang/0.14.1/), [Theos SDKs](https://github.com/theos/sdks), [PyInstaller](https://pypi.org/project/pyinstaller/6.22.3/).

19 regression tests cover injection invariants, source/output protection, config bounds, all feature serialization, archive traversal/symlinks, signature removal, supplied PNG/icon validation, branding integrity and legacy configuration verification. Native host tests compile the same C interval/marker and image-header helpers used by the runtime; they cover overlapping/touching intervals, invalid bounds, clipped markers and rejected image responses. 81 directly named hook ABIs match extracted metadata. Two UIKit pan getters require runtime resolution because the system classes are outside the extracted YouTube binary; generated/inherited/dynamic hooks remain outside that static count. Real-IPA integration compares all retained original members, section/LINKEDIT bytes, load commands and ZIP CRCs. These checks do not execute the injected Objective-C adapters on iOS.

Device verification remains: signing and cold launch; hook diagnostics; ad/content/live/Shorts transitions; PiP/background/lock-screen playback; gesture arbitration and fullscreen lifecycle; quality/audio/codec selection; server-rendered layouts; thumbnail failures; service requests, stale responses, manual vote/submit/undo; branding/cache behavior and retained extensions. Further complete UI or stream ports need evidence from those runtime paths.

Device target supplied by the user: iPhone 17 Pro Max on iOS 27.0. See [DEVICE_TESTS.md](DEVICE_TESTS.md) and `build/device-results-template.json`. The 0.3.1 authentication attempt has been observed on this target; login now succeeds according to the user; revised settings, playback, refresh and other feature behavior remain unverified.

For 0.3.5, [profiles/release-0.3.5.json](profiles/release-0.3.5.json) records the successful [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37188729062), artifact hashes and skipped test steps. `release_build_receipt.py` generates this build-only receipt. `release_metadata.py` remains the historical 0.3.4 audit entry point. No 0.3.5 GUI/host/device test result is claimed.

Version 0.3.6 corrects the shared ordinary-video check to read `contentPlaybackData.playerResponse.playerData.isLivePlayback`. The supplied 0.3.5 device report confirmed both clocks worked but this check rejected the response wrapper before any segment fetch. The update also draws markers on the native modular timeline. Authentication and the released 0.3.5 settings UI are retained. Tests remain skipped at the user request, and device skip/marker behavior after this correction remains unverified. [Build-only receipt](profiles/release-0.3.6.json).

Version 0.3.7 adds `miniplayer_hide_overlay_buttons`, exposed in ReVanced settings > Miniplayer and the Windows feature list. It hides the owned close/playback controls and circular backgrounds, retaining native tap expansion, progress and ad-skip. This raises the catalog to 80 switches and 114 runtime preferences. The flag is off in the defaults, expanded and SideStore-auth presets. Authentication and the 0.3.6 SponsorBlock fix carry over unchanged. No tests ran at the user request; device behavior remains unverified.

The [0.3.7 build-only receipt](profiles/release-0.3.7.json) records the successful [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37190158948), artifact hashes and skipped test steps. The recommended SideStore IPA carries the SponsorBlock response-unwrapping correction and modular timeline markers; device behavior remains unverified.

Version 0.3.8 integrates fetched dislike estimates into verified native slim dislike labels and matching main-video entity count/accessibility paths. It restores native text when disabled and rejects a mismatched video ID. The existing badge and manual service voting remain available; element-rendered buttons, online Shorts and automatic vote forwarding remain unported. Authentication and SponsorBlock behavior carry over unchanged. No tests ran. See [NATIVE_DISLIKES_SCHEME.md](NATIVE_DISLIKES_SCHEME.md).

The [0.3.8 build-only receipt](profiles/release-0.3.8.json) records the successful [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37190989459), source commit and artifact hashes. Regression, static-hook and GUI-smoke steps were skipped. Native dislike rendering remains device-unverified.

Version 0.3.9 merges a snapshot of the parallel settings UI and inline RYD work authorized by the user. Settings now use a checked YouTube-styled host, native rows/switches and content-level search with an embedded ReVanced icon. The player-overlay Tools/RYD boxes are removed; Video tools remain in settings, and identified element-backed watch actions gain inline estimates alongside the 0.3.8 native formatted-label adapter. The parallel UI work was still in progress at snapshot time; later shared edits are not claimed as included. No tests ran and device rendering remains unverified. [Merged snapshot](profiles/parallel-ui-snapshot-0.3.9.json).

The [0.3.9 build-only receipt](profiles/release-0.3.9.json) records the successful [merged cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37191721554), exact sources and artifact hashes. The initial Xcode deprecation error in run 37191638164 was corrected with a scoped compatibility call; regression, hook and GUI-smoke steps remained skipped. Device UI, inline RYD and SponsorBlock behavior remain unverified.

[0.3.11 build-only receipt](profiles/release-0.3.11.json): [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37233522778) succeeded. Regression, hook-check and GUI smoke steps were skipped. Use the 0.3.12 SideStore-auth IPA; revised like/dislike rendering remains device-unverified.

[0.3.12 build-only receipt](profiles/release-0.3.12.json): [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37234483316) succeeded with regression, hook and GUI smoke checks skipped. Use the 0.3.12 SideStore-auth IPA. Independent dislike rendering and the settings snapshot remain device-unverified.

[0.3.13 build receipt](profiles/release-0.3.13.json): [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37236403392) succeeded, with regression tests, hook checks and GUI smoke checks skipped. Use `YouTube-21.39.4-RVPort-0.3.15-SideStore-auth-unsigned.ipa` in SideStore. The rendering revision remains device-unverified.

Read the [RYD capture/checkpoint guide](RYD_CHECKPOINTS.md).

[0.3.14 build receipt](profiles/release-0.3.14.json): [cloud build](https://github.com/zinfector/Revanced-iOS/actions/runs/37238785537) succeeded; regression, hook and GUI smoke checks were skipped. RYD checkpoint revision 2 preserves visible watch-page state and observes layout/mounting/loading/drawing without forcing it. Rendering repair remains unverified.
