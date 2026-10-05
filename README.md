# ReVanced iOS

Native iOS port of the local ReVanced YouTube patches for the decrypted **YouTube 21.39.4** IPA, ARM64 and iOS 17 or later. The merged source contains 82 feature switches. Current packaged candidate: **0.3.31**, `YouTube-21.39.4-RVPort-0.3.31-unsigned.ipa`.

[Patcher usage](patcher/README.md) - [Detailed coverage](patcher/COVERAGE.md) - [Signing](SIGNING.md) - [License](LICENSE)

Structural Elements ad filtering and inline-player suppression are included alongside all completed 0.3.30 work. [Implementation and capture checkpoints](patcher/ADBLOCK_ELEMENTS_INLINE_IMPLEMENTATION.md).

## Porting progress

Status reflects the merged 0.3.31 source. This checklist covers every declaration in the local YouTube patch inventory: **51 named patches and 62 dependencies/resource wrappers**, plus one iOS-specific feature. Dependencies are listed separately and are not independent user features.

- **not started**: no working iOS implementation; Android-only mechanisms are identified explicitly.
- **in-progress**: a partial implementation exists, a reported defect remains under confirmation, or supporting infrastructure has incomplete scope.
- **done**: the selected iOS behavior is implemented. This does not imply complete Android parity or device validation. Checked boxes correspond to this status.

**27 done - 81 in-progress - 6 not started** across the entries below.

## Named patches

- [ ] **in-progress** - Hide ads. Typed filtering, structural Elements ad logging, native empty-renderer/cell fallbacks and companion clearing implemented; unidentified routes and device confirmation remain.
- [ ] **in-progress** - Video ads. Native ad coordinator with verified Watch and inline-preview ownership implemented; device behavior still needs confirmation.
- [x] **done** - Copy video URL. Native Video tools copies a clean URL, with an optional timestamp.
- [ ] **in-progress** - Remove viewer discretion dialog. Ordinary warning confirmation implemented; age, login and purchase verification stays native.
- [x] **done** - Add more double tap to seek length options. Configurable native double-tap interval implemented.
- [x] **done** - Disable double tap actions. Native double-tap and two-finger chapter gesture suppression implemented.
- [ ] **in-progress** - Downloads. External downloader share handoff implemented; internal download/offline management remains unported.
- [ ] **in-progress** - Disable haptic feedback. Selected native semantic haptics suppressed; other haptic producers remain.
- [ ] **in-progress** - Seekbar. Tap seeking, progress hiding/color and precise-seeking suppression implemented; complete styling remains.
- [ ] **in-progress** - Swipe controls. Fullscreen brightness/volume gestures implemented; gesture arbitration and preference parity remain.
- [ ] **in-progress** - Disable auto captions. Automatic-caption gates adapted; server-selected caption defaults remain.
- [x] **done** - Change header. Supplied header PNG supported through native logo hooks.
- [ ] **in-progress** - Hide video action buttons. Configured native Elements identifiers filtered; additional action layouts remain.
- [ ] **in-progress** - Navigation bar. Shorts removed from the native pivot model and remaining tabs relaid out; other navigation options remain.
- [ ] **in-progress** - Hide player overlay buttons. Selected native controls/watermark hidden; additional Android options remain.
- [ ] **in-progress** - Change form factor. Phone/tablet request-field override implemented; alternate layouts are not guaranteed.
- [x] **done** - Hide autoplay preview. Native autonav preview and end-screen view hiding implemented.
- [x] **done** - Hide end screen cards. Native creator end-screen container hiding implemented.
- [x] **done** - Hide end screen suggested video. Native autonav end-screen hiding implemented through the preview switch.
- [ ] **in-progress** - Disable fullscreen ambient mode. Native ambient gates and Metal strength adapted; alternate renderers remain.
- [ ] **in-progress** - Hide info cards. Native teaser hidden; expanded card surfaces remain.
- [ ] **in-progress** - Hide player flyout menu items. Configured components and selected Premium quality actions filtered; complete menu mapping remains.
- [ ] **in-progress** - Disable player popup panels. Automatic engagement-panel response actions filtered; additional presentation paths remain.
- [ ] **in-progress** - Hide related video overlay. Native fullscreen engagement overlay hiding implemented; layout scope needs confirmation.
- [ ] **in-progress** - Disable rolling number animations. Scoped native animation suppression implemented; asynchronous digit effects may remain.
- [ ] **in-progress** - Hide Shorts components. Shelf bodies/headers, Shorts ads, navigation and shortcut filtering implemented; player/widget controls remain.
- [x] **done** - Disable sign in to TV popup. Native seamless TV sign-in popup gate disabled.
- [x] **done** - Hide timestamp. Native time-label visibility and title gates adapted.
- [ ] **in-progress** - Miniplayer. Selected native gestures, badges, corners, size, opacity and controls adapted; full type/control parity remains.
- [x] **done** - Exit fullscreen. Native fullscreen exit on ordinary-content completion implemented.
- [x] **done** - Open videos fullscreen. Native fullscreen request on ordinary-content activation implemented.
- [ ] **in-progress** - Custom player overlay opacity. Native player-background alpha adapted; Android scrim rendering remains.
- [ ] **in-progress** - Return YouTube Dislike. Native inline counts, paired vote layout and manual service voting implemented; automatic vote forwarding/Shorts remain.
- [ ] **in-progress** - Shorts autoplay. Native auto-advance menu/completion behavior adapted; lifecycle interactions need confirmation.
- [ ] **in-progress** - Open Shorts in regular player. Identified Shorts links/commands routed to watch endpoints; other reel navigation remains.
- [ ] **in-progress** - SponsorBlock. Category policies, markers, native skip/undo prompts and contribution tools implemented; complete parity remains.
- [ ] **in-progress** - Spoof app version. Copied request client-version override implemented; executable/header/stream spoofing remains separate.
- [ ] **in-progress** - Change start page. Home/Subscriptions/Library initial pivot selection implemented; other destinations/restores remain.
- [x] **done** - Disable resuming Shorts on startup. Native Shorts resume gate disabled.
- [ ] **in-progress** - Alternative thumbnails. Frame/DeArrow/faster still variants and per-screen probes implemented; lifecycle/service behavior needs confirmation.
- [ ] **in-progress** - Bypass image region restrictions. Configurable HTTPS thumbnail proxy implemented; requires a compatible user-supplied service.
- [ ] **in-progress** - Announcements. Manual announcements reader implemented; service availability and notification scheduling remain.
- [ ] **in-progress** - Pause on audio interrupt. Pause on native audio interruption implemented; resumption/audio-focus parity remains.
- [x] **done** - Remove background playback restrictions. Native background gates and capability-checked PiP action implemented.
- [ ] **in-progress** - Spoof device dimensions. Copied request screen/window dimensions adapted; UIKit layout remains native.
- [x] **done** - Bypass URL redirects. Public redirect targets validated and unwrapped on the mapped endpoint path.
- [ ] **in-progress** - Open links externally. External HTTP(S) endpoints open in the system browser; other browser paths remain.
- [x] **done** - Loop video. Native seek-to-start and repeat playback for ordinary completed videos implemented.
- [ ] **in-progress** - Disable video codecs. VP9/HDR filtering with fallback formats implemented; complete codec modes remain.
- [ ] **in-progress** - Video quality. Resolution caps, per-network remembering, advanced menu and Premium filtering implemented; exact selection/parity remain.
- [ ] **in-progress** - Playback speed. Native speed/registry configuration repair and wrapper diagnostics implemented; reported slider failure awaits device confirmation.

## Supporting patches and dependencies

<details>
<summary>All 62 supporting declarations</summary>

- [ ] **in-progress** - `hideAdsResourcePatch`. Supports Hide ads through the selected native iOS adapter; inherits its remaining scope.
- [x] **done** - `copyVideoURLResourcePatch`. Supports Copy video URL through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `downloadsResourcePatch`. Supports Downloads through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `disablePreciseSeekingGesturePatch`. Supports Seekbar through the selected native iOS adapter; inherits its remaining scope.
- [x] **done** - `enableSlideToSeekPatch`. Native iOS scrubber drag behavior retained.
- [ ] **in-progress** - `enableTapToSeekPatch`. Supports Seekbar through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `hideSeekbarPatch`. Supports Seekbar through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `swipeControlsResourcePatch`. Supports Swipe controls through the selected native iOS adapter; inherits its remaining scope.
- [x] **done** - `customBrandingPatch`. Display name and supplied icon/header assets supported.
- [x] **done** - `changeHeaderBytecodePatch`. Supports Change header through the selected native iOS adapter; inherits its remaining scope.
- [x] **done** - `hideEndScreenCardsResourcePatch`. Supports Hide end screen cards through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `hideLayoutComponentsResourcePatch`. Supports `hideLayoutComponentsPatch` through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `hideLayoutComponentsPatch`. Configured native layout/comment identifiers filtered; Android component parity remains.
- [ ] **in-progress** - `hideInfocardsResourcePatch`. Supports Hide info cards through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `hideShortsComponentsResourcePatch`. Supports Hide Shorts components through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `miniplayerResourcePatch`. Supports Miniplayer through the selected native iOS adapter; inherits its remaining scope.
- [x] **done** - `openVideosFullscreenHookPatch`. Supports Open videos fullscreen through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `seekbarColorPatch`. Supports Seekbar through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `sponsorBlockResourcePatch`. Supports SponsorBlock through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `themePatch`. Forces verified native dark/light page-style enum and selected UIColor background getters on common/token palettes.
- [ ] **in-progress** - `themeResourcePatch`. Supports `themePatch` through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `toolbarHookPatch`. Native header provider/controller hooks adapted.
- [x] **done** - `checkEnvironmentPatch`. Source profile and runtime UUID/version/config compatibility checks implemented.
- [ ] **in-progress** - `hookClientContextPatch`. Copied native request client-info and verified protobuf setters adapted.
- [x] **done** - `enableDebuggingPatch`. Redacted hook and service diagnostics implemented.
- [x] **done** - `checkWatchHistoryDomainNameResolutionPatch`. Watch-history DNS diagnostic tool implemented.
- [ ] **in-progress** - `engagementPanelHookPatch`. Selected automatic engagement-panel actions filtered; other panel paths remain.
- [ ] **in-progress** - `sharedExtensionPatch`. Native injected dylib replaces the Android DEX extension; broader shared behavior remains adapter-specific.
- [ ] **not started** - `fixBackToExitGesturePatch`. Android system-back fix has no direct iOS equivalent; ReVanced navigation uses a separate native back gesture.
- [ ] **not started** - `fixContentProviderPatch`. Android ContentProvider authority rewriting does not apply to iOS.
- [ ] **in-progress** - `fixPlaybackSpeedWhilePlayingPatch`. Supports Playback speed through the selected native iOS adapter; inherits its remaining scope.
- [ ] **not started** - `accountCredentialsInvalidTextPatch`. Android GmsCore credential-error text has no corresponding iOS screen.
- [ ] **in-progress** - `gmsCoreSupportPatch`. Native SSO/keychain equivalent implemented; login reported working, refresh/relaunch persistence remains unconfirmed.
- [ ] **in-progress** - `cronetImageURLHookPatch`. Selected native thumbnail URL accessors adapted; global image interception remains.
- [ ] **in-progress** - `lithoFilterPatch`. Positive native Elements filtering adapted; Android Litho infrastructure is not transplanted.
- [x] **done** - `loopVideoButtonResourcePatch`. Supports Loop video through the selected native iOS adapter; inherits its remaining scope.
- [x] **done** - `loopVideoButtonPatch`. Supports Loop video through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `navigationBarHookPatch`. Native pivot model compaction and selection restoration integrated; other navigation options remain.
- [ ] **in-progress** - `playerControlsOverlayVisibilityPatch`. Native control visibility hooks adapted; Video tools lives in settings.
- [x] **done** - `playerControlsResourcePatch`. URL/timestamp actions supplied by native Video tools rather than Android resources.
- [ ] **in-progress** - `playerControlsPatch`. Native Video tools/settings replace injected Android overlay controls; complete control parity remains.
- [ ] **in-progress** - `playerTypeHookPatch`. Guarded native ordinary/live/ad and fullscreen checks.
- [ ] **not started** - `versionCheckPatch`. Google Play Services checks do not apply; the iOS source/profile check is separate.
- [ ] **in-progress** - `sanitizeSharingLinksPatch`. Mapped sharing URLs cleaned; other sharing surfaces remain.
- [ ] **in-progress** - `recyclerViewTreeHookPatch`. Selected native model/view hooks adapted; no Android RecyclerView runtime on iOS.
- [ ] **in-progress** - `settingsResourcePatch`. Native settings rows and catalog replace Android resources; follows the settings integration scope.
- [ ] **in-progress** - `settingsPatch`. Native ReVanced menu, icon, search, grouped preferences, import/export, reset and diagnostics implemented; device layout/gesture confirmation remains.
- [ ] **not started** - `spoofVideoStreamsPatch`. Full stream replacement is unimplemented; request-field overrides do not supply it.
- [ ] **not started** - `userAgentClientSpoofPatch`. Alternate-client transport/header spoofing is unimplemented.
- [ ] **in-progress** - `forceOriginalAudioPatch`. Original-track preference adapted; native selection coverage remains incomplete.
- [ ] **in-progress** - `videoInformationPatch`. Native content ID, time, duration and playback ownership accessors adapted.
- [ ] **in-progress** - `playerResponseMethodHookPatch`. Selected native response/accessor and content lifecycle hooks adapted.
- [ ] **in-progress** - `advancedVideoQualityMenuPatch`. Supports Video quality through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `hidePremiumVideoQualityPatch`. Supports Video quality through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `rememberVideoQualityPatch`. Supports Video quality through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `videoQualityButtonResourcePatch`. Supports Video quality through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `videoQualityDialogButtonPatch`. Supports Video quality through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `playbackSpeedButtonResourcePatch`. Supports Playback speed through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `playbackSpeedButtonPatch`. Supports Playback speed through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `customPlaybackSpeedPatch`. Supports Playback speed through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `rememberPlaybackSpeedPatch`. Supports Playback speed through the selected native iOS adapter; inherits its remaining scope.
- [ ] **in-progress** - `videoIdPatch`. Native content identification and generation-checked responses adapted.

</details>

## iOS-specific integration

- [ ] **in-progress** - Keep first-launch UI. Native paired-layout startup policy implemented; preserves native speed/registry settings, with remaining device/layout confirmation.

The patch catalog maps the 82 feature switches to the entries above. The first-launch UI policy is tracked separately because it is an iOS integration feature rather than an Android patch declaration. Per-category SponsorBlock behavior, native skip/undo prompts, inline RYD counts, settings presentation and Shorts toolbar compaction are included in their parent entries.

## Build and diagnostics

Production payloads use **Apple Xcode 16.4 / iPhoneOS 18.5 SDK**, targeting `arm64-apple-ios17.0`, with signing header padding. Current delivery was compiled and packaged without tests; compilation does not confirm feature behavior on a device. Source IPAs, SDKs, tools, credentials and generated artifacts are excluded from Git.

In YouTube, open **Settings > ReVanced > Hook diagnostics** for full and feature-specific reports. Saved app preferences override bundled presets. Reopen the video or restart for settings that affect startup/player configuration.

Implementation details and evidence remain in `patcher/` and `patcher/profiles/`; this README tracks current progress rather than release history.
