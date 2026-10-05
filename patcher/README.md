# YouTube iOS Patcher

Patches the supported decrypted **YouTube 21.39.4** IPA with the ARM64 native adapter. Requires iOS 17 or later. See the [porting progress checklist](../README.md#porting-progress) for every feature, dependency and remaining scope.

## Current delivery

The merged 0.3.31 candidate is `YouTube-21.39.4-RVPort-0.3.31-unsigned.ipa`, using the SideStore-compatible native-ad preset with extensions removed. Its source and build receipts record the compiler, source fingerprint and artifact hashes. The source includes native settings/authentication, SponsorBlock prompts, display-ad and ownership repairs, Shorts header/toolbar fixes and the native speed-context repair. Structural Elements banner/feed-ad filtering, native cell fallbacks, inline-player suppression and checkpoint revision 5 are included; see [the adblock capture guide](ADBLOCK_ELEMENTS_INLINE_IMPLEMENTATION.md).

Open **YouTube Settings > ReVanced** for grouped preferences, search, configuration import/export/reset, Video tools and diagnostics. A three-finger hold opens the same pane as a fallback. Saved app preferences override bundled configuration. Reopen the video or restart after changing startup, request or player-model settings.

## Configuration

[configs/defaults.json](configs/defaults.json) lists the configuration keys. [COVERAGE.md](COVERAGE.md) and [coverage.json](coverage.json) map the local Android declarations to iOS implementations and limitations; the root checklist reflects the current merged implementation. Source/profile identity checks reject unsupported main executables.

## Windows GUI

The GUI source is `gui.py`. A packaged executable embeds its own native payload; use the build receipt to identify the payload version. Choose an original IPA and a new output path, load a JSON configuration and supply optional branding assets. The GUI removes extensions by default for sideloading. Keeping extensions requires profiles and signing for every retained extension.

## Command line

Python 3.11 or later, standard library only. Run from this directory:

```powershell
python patcher.py inspect "..\Ghidra Project\com.google.ios.youtube_21.39.4_und3fined.ipa"
python patcher.py patch "original.ipa" -o "patched-unsigned.ipa"
python patcher.py patch "original.ipa" -o "sideload-auth-unsigned.ipa" --config configs/sideload-auth.json --strip-extensions
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

## Native build

Release builds use macOS with **Xcode 16.4 / iPhoneOS 18.5 SDK** and target `arm64-apple-ios17.0`. Run from this directory:

```sh
export DEVELOPER_DIR=/Applications/Xcode_16.4.app/Contents/Developer
python build.py
```

`build.py` writes `build/RVPort.dylib`, the source fingerprint and compiler manifest. It reserves Mach-O header padding for signing. The native-only cloud workflow uses the same pinned Xcode toolchain. Windows can package the downloaded Xcode payload; release delivery uses that payload rather than a local Zig build.

`package_merged_release.py` creates the current single IPA with normal production input/source checks and skips the archive self-check in accordance with the no-tests instruction. Its workspace paths target the registered release worktree. Current release work performs compilation and packaging only; no tests or device smoke checks are run.

## Signing and evidence

The injected dylib uses `@executable_path/Frameworks/RVPort.dylib`. Sign the IPA and all nested code before installation; [SIGNING.md](../SIGNING.md) describes the signing workflow. Supply your own supported decrypted IPA and Apple signing credentials. Neither is included in the repository.

Feature-specific diagnostics are available under **ReVanced > Hook diagnostics**. Full reports include effective preferences, hook installation and player/service state; dedicated speed, Shorts, SponsorBlock, RYD and adblock reports narrow failures. Implementation documents and `profiles/` retain the native evidence and previous build receipts.

The [license](../LICENSE) covers the patcher source, not YouTube, SDKs or third-party tools.
