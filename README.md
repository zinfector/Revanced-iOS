# ReVanced iOS

Native iOS port of the local ReVanced YouTube patches for the decrypted **YouTube 21.39.4** IPA, ARM64 and iOS 17 or later. The merged source contains 87 feature switches. Current packaged candidate: **0.3.47**, `YouTube-21.39.4-RVPort-0.3.47-unsigned.ipa`.

[Patcher CLI](patcher/patcher.py) - [Coverage data](patcher/coverage.json) - [Signing](SIGNING.md) - [License](LICENSE)

`patcher/output` contains IPA files only. Windows executables belong in `patcher/dist`; build receipts belong in `patcher/build` or `patcher/profiles`, and source code stays in Git.

Navigation settings now include Home/Subscriptions/You tab hiding, icon-only labels and a Shorts start page, with surviving-tab recovery. The native 3? speed option is converted to a regular speed button, with its upsell action removed and configured range extended to 3?. Working adblocking, SponsorBlock, native vote counts, speed and Shorts changes are retained. [Structured implementation evidence](patcher/profiles/adblock-elements-inline-evidence.json).

On October 5, 2026, the user reported that the adblocker is now working with the 0.3.31 delivery; that implementation is retained in 0.3.47. This confirms the reported device behavior; coverage of every ad format remains unverified.

DeArrow uses the exact service-selected frame timestamp and the upstream random seed when metadata has no frame time. Delayed frame generation retries in bounded bursts while the row is visible, with cooldown recovery instead of permanent deadline failure. Offscreen work pauses, and duplicate requests share a download with independent cancellation. Reports separate fallback HTTP status, requested/returned timestamps, cooldown, native consumption and visibility. Existing title/thumbnail ownership guards and watch-page interaction fixes are retained; unsupported layouts keep originals. The 0.3.47 changes await device confirmation.

## USB diagnostics

1. Install the current IPA and connect your iPhone to Windows by USB. Unlock it and trust the PC if prompted.
2. Open **Settings â†’ ReVanced â†’ USB diagnostics â†’ Enable diagnostics** in YouTube. Note the eight-character pairing code for the first connection on a new PC.
3. Run `patcher/dist/YouTube-USB-Diagnostics.exe` on the PC and enter that code once. Saved pairings survive cable reconnects, foreground transitions and app relaunches. Close the settings alert and reproduce the issue while keeping YouTube foreground.
4. Use the collector to start DeArrow capture, fetch reports, inspect watch checkpoints, or probe the current video's branding service. Full reports are saved as compressed JSON with small checkpoint summaries in `ReVanced/Reports`.

The bridge listens on device loopback and has no session expiry. After the initial opt-in it resumes automatically whenever YouTube is foreground, including after relaunch. Saved computer credentials use the iPhone keychain and Windows current-user DPAPI. Backgrounding pauses collection without removing pairings. Disable diagnostics to stop automatic resumption; use **Forget saved computers** to revoke all saved pairings. The collector pins the saved iPhone's UDID, rediscovers USB device IDs after reconnects, and retries report collection until the connection returns or you press Ctrl+C. It never automatically repeats an ambiguously delivered capture reset or probe. Live memory and native calls require the separate live-debugging opt-in described below.

For direct collection from the workspace:

```powershell
python ReVanced/patcher/usb_diagnostics.py devices
python ReVanced/patcher/usb_diagnostics.py pair
python ReVanced/patcher/usb_diagnostics.py saved
python ReVanced/patcher/usb_diagnostics.py start
python ReVanced/patcher/usb_diagnostics.py fetch --topic dearrow
python ReVanced/patcher/usb_diagnostics.py record --topic dearrow --interval 5
python ReVanced/patcher/usb_diagnostics.py fetch --topic watch
python ReVanced/patcher/usb_diagnostics.py stop
```

The collector uses the installed Apple Mobile Device USB service directly and needs no extra Python packages, WebDriverAgent or developer tunnel. Version 0.3.41 USB report delivery was confirmed by the user's device report. Saved pairing and reconnection behavior, introduced in 0.3.42 and retained here, awaits device confirmation. Pair once when upgrading from 0.3.41 temporary credentials; existing 0.3.42 pairings are retained. Keep the same signing identity and installed app data to preserve pairings across later IPA upgrades. Continuous recording runs until Ctrl+C; `--duration` remains available for a bounded capture.

## Wired live debugger

Enable **Settings -> ReVanced -> USB diagnostics -> Enable live debugging**. Open `patcher/dist/YouTube-Live-Debugger.exe`; it reuses the collector's saved PC/device pairing. Debug mode defaults to off. Keep YouTube foreground. Type `help` for commands, or pass a command and `--args` JSON from PowerShell.

The console provides `status`, `images`, `regions`, `read`, `write`, `rollback`, `allocate`, `free`, `object`, `get`, `items`, `value`, `invoke`, `retain`, `release`, `functions`, `call`, `bindings`, `graph`, `roles`, `refresh`, `events`, `dump` and `watch`. `dump` saves larger snapshots as compressed chunks (up to 16 MiB) in the private captures directory. Addresses use hexadecimal strings; native objects use opaque handles scoped to the current row generation and consumer epoch. Read results contain base64 bytes. Writes require `expected_hex` and `hex` (at most 1 KiB), accept already writable data only, and return readback plus a rollback transaction. Concurrent writes are not atomic. Allocation/free operate on agent-owned scratch buffers.

```text
status
bindings
graph {"handle":"binding handle from bindings","cursor":0}
get {"handle":"node handle from graph","selector":"parent"}
value {"handle":"object handle returned by get"}
read {"address":"0xADDRESS","length":64}
write {"address":"0xADDRESS","hex":"0100","expected_hex":"0000"}
rollback {"transaction":"transaction from write"}
functions
call {"function":"mach_timebase_info"}
watch {"interval":1}
recover
```

UI/native jobs run on the app's main queue. The USB worker returns a job ticket immediately, so a stalled main queue can be distinguished from a broken connection. A bounded job ledger and mutation tombstones prevent duplicate execution across reconnects. The last 64 completed results are retained privately for recovery. Pending requests are saved privately under `.tools/live-debugger/pending` using Windows current-user DPAPI. `recover` retrieves outcomes; a restarted process or expired mutation result remains an explicit unknown outcome and is never automatically replayed. The event stream is bounded and exports dropped-event counts. Disabling live debugging invalidates handles/jobs and releases scratch buffers; it preserves PC pairing. Ordinary reports do not gain raw memory or content.

`doctor` reports external debugger prerequisites. `install-lldb-tools` installs pinned pymobiledevice3 into a private environment (Python required). `lldb` accepts JSON `lldb`, `python`, `target`, optional `symbols`, and `local_port` arguments. It starts a localhost-only Apple debugproxy relay over the selected wired iPhone and opens LLDB for breakpoints, backtraces, registers, memory operations and explicit expressions. It requires a Darwin-capable LLDB, Developer Mode, a mounted personalized developer disk image, and app signing with `get-task-allow`. Missing prerequisites leave agent commands available. No signing identity, memory protection or executable pages are changed by the agent. Debug symbols stay in `patcher/build`, outside the IPA output folder. External attachment and new live operations still require device confirmation; no tests were run for this release.

## Porting progress

Status reflects the merged 0.3.47 source. This report covers every declaration in the local YouTube patch inventory: **51 named patches and 62 dependencies/resource wrappers**, plus one iOS-specific feature. Dependencies are listed separately and are not independent user features.

- <img src="assets/progress/green-circle.svg" width="16" height="16" alt="done"> **done**: the selected iOS behavior is implemented. This does not imply complete Android parity or device validation.
- <img src="assets/progress/yellow-circle.svg" width="16" height="16" alt="in-progress"> **in-progress**: a partial implementation exists, a reported defect remains under confirmation, or supporting infrastructure has incomplete scope.
- <img src="assets/progress/red-circle.svg" width="16" height="16" alt="not started"> **not started**: no working iOS implementation; Android-only mechanisms are identified explicitly.

**30 done - 78 in-progress - 6 not started** across the entries below.

## Named patches

| Patch | Description | Progress |
|---|---|:---:|
| Copy video URL | Native Video tools copies a clean URL, with an optional timestamp. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Add more double tap to seek length options | Configurable native double-tap interval implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Disable double tap actions | Native double-tap and two-finger chapter gesture suppression implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Change header | Supplied header PNG supported through native logo hooks. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Hide autoplay preview | Native autonav preview and end-screen view hiding implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Hide end screen cards | Native creator end-screen container hiding implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Hide end screen suggested video | Native autonav end-screen hiding implemented through the preview switch. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Disable sign in to TV popup | Native seamless TV sign-in popup gate disabled. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Hide timestamp | Native time-label visibility and title gates adapted. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Exit fullscreen | Native fullscreen exit on ordinary-content completion implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Open videos fullscreen | Native fullscreen request on ordinary-content activation implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Disable resuming Shorts on startup | Native Shorts resume gate disabled. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Remove background playback restrictions | Native background gates and capability-checked PiP action implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Bypass URL redirects | Public redirect targets validated and unwrapped on the mapped endpoint path. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Loop video | Native seek-to-start and repeat playback for ordinary completed videos implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Hide ads | Typed filtering, structural Elements ad logging, native empty-renderer/cell fallbacks and companion clearing implemented; adblocking reported working on device. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Video ads | Native ad coordinator with verified Watch and inline-preview ownership implemented; adblocking reported working on device. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| Remove viewer discretion dialog | Ordinary warning confirmation implemented; age, login and purchase verification stays native. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Downloads | External downloader share handoff implemented; internal download/offline management remains unported. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Disable haptic feedback | Selected native semantic haptics suppressed; other haptic producers remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Seekbar | Tap seeking, progress hiding/color and precise-seeking suppression implemented; complete styling remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Swipe controls | Fullscreen brightness/volume gestures implemented; gesture arbitration and preference parity remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Disable auto captions | Automatic-caption gates adapted; server-selected caption defaults remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Hide video action buttons | Configured native Elements identifiers filtered; additional action layouts remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Navigation bar | Home/Shorts/Subscriptions/You tab hiding, reversible native icon-only labels and selection recovery implemented; other navigation options remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Hide player overlay buttons | Selected native controls/watermark hidden; additional Android options remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Change form factor | Phone/tablet request-field override implemented; alternate layouts are not guaranteed. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Disable fullscreen ambient mode | Native ambient gates and Metal strength adapted; alternate renderers remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Hide info cards | Native teaser hidden; expanded card surfaces remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Hide player flyout menu items | Configured components and selected Premium quality actions filtered; complete menu mapping remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Disable player popup panels | Automatic engagement-panel response actions filtered; additional presentation paths remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Hide related video overlay | Native fullscreen engagement overlay hiding implemented; layout scope needs confirmation. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Disable rolling number animations | Scoped native animation suppression implemented; asynchronous digit effects may remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Hide Shorts components | Shelf bodies/headers, Shorts ads, navigation and shortcut filtering implemented; player/widget controls remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Miniplayer | Selected native gestures, badges, corners, size, opacity and controls adapted; full type/control parity remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Custom player overlay opacity | Native player-background alpha adapted; Android scrim rendering remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Return YouTube Dislike | Native inline counts, paired vote layout and manual service voting implemented; automatic vote forwarding/Shorts remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Shorts autoplay | Native auto-advance menu/completion behavior adapted; lifecycle interactions need confirmation. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Open Shorts in regular player | Identified Shorts links/commands routed to watch endpoints; other reel navigation remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| SponsorBlock | Category policies, markers, native skip/undo prompts and contribution tools implemented; complete parity remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Spoof app version | Copied request client-version override implemented; executable/header/stream spoofing remains separate. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Change start page | Home/Subscriptions/You/Shorts initial pivot selection and surviving-tab recovery implemented; other destinations/restores remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Alternative thumbnails | [Report 105](patcher/profiles/device-dearrow-report-105.json) identified Home rejection before service dispatch. URI matching, owner scope, screen policies and primary-image roles corrected. Native settings now consume formatting, Casual categories, fallbacks, saturation and original/preview policies. Contribution voting remains unavailable; unknown templates retain originals. [Implementation scope](patcher/profiles/dearrow-options-scheme.json). Device confirmation pending. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Bypass image region restrictions | Configurable HTTPS thumbnail proxy implemented; requires a compatible user-supplied service. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Announcements | Manual announcements reader implemented; service availability and notification scheduling remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Pause on audio interrupt | Pause on native audio interruption implemented; resumption/audio-focus parity remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Spoof device dimensions | Copied request screen/window dimensions adapted; UIKit layout remains native. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Open links externally | External HTTP(S) endpoints open in the system browser; other browser paths remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Disable video codecs | VP9/HDR filtering with fallback formats implemented; complete codec modes remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Video quality | Resolution caps, per-network remembering, advanced menu and Premium filtering implemented; exact selection/parity remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| Playback speed | Native speed/registry repair and diagnostics implemented; 3? uses a regular option with no upsell action and a 3? configured range. Device confirmation pending. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |

## Supporting patches and dependencies

<details>
<summary>All 62 supporting declarations</summary>

| Patch | Description | Progress |
|---|---|:---:|
| `copyVideoURLResourcePatch` | Supports Copy video URL through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `enableSlideToSeekPatch` | Native iOS scrubber drag behavior retained. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `customBrandingPatch` | Display name and supplied icon/header assets supported. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `changeHeaderBytecodePatch` | Supports Change header through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `hideEndScreenCardsResourcePatch` | Supports Hide end screen cards through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `openVideosFullscreenHookPatch` | Supports Open videos fullscreen through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `checkEnvironmentPatch` | Source profile and runtime UUID/version/config compatibility checks implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `enableDebuggingPatch` | Redacted hook and service diagnostics implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `checkWatchHistoryDomainNameResolutionPatch` | Watch-history DNS diagnostic tool implemented. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `loopVideoButtonResourcePatch` | Supports Loop video through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `loopVideoButtonPatch` | Supports Loop video through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `playerControlsResourcePatch` | URL/timestamp actions supplied by native Video tools rather than Android resources. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `hideAdsResourcePatch` | Supports Hide ads through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/green-circle.svg" width="24" height="24" alt="done" title="done"> |
| `downloadsResourcePatch` | Supports Downloads through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `disablePreciseSeekingGesturePatch` | Supports Seekbar through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `enableTapToSeekPatch` | Supports Seekbar through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `hideSeekbarPatch` | Supports Seekbar through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `swipeControlsResourcePatch` | Supports Swipe controls through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `hideLayoutComponentsResourcePatch` | Supports `hideLayoutComponentsPatch` through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `hideLayoutComponentsPatch` | Configured native layout/comment identifiers filtered; Android component parity remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `hideInfocardsResourcePatch` | Supports Hide info cards through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `hideShortsComponentsResourcePatch` | Supports Hide Shorts components through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `miniplayerResourcePatch` | Supports Miniplayer through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `seekbarColorPatch` | Supports Seekbar through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `sponsorBlockResourcePatch` | Supports SponsorBlock through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `themePatch` | Forces verified native dark/light page-style enum and selected UIColor background getters on common/token palettes. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `themeResourcePatch` | Supports `themePatch` through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `toolbarHookPatch` | Native header provider/controller hooks adapted. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `hookClientContextPatch` | Copied native request client-info and verified protobuf setters adapted. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `engagementPanelHookPatch` | Selected automatic engagement-panel actions filtered; other panel paths remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `sharedExtensionPatch` | Native injected dylib replaces the Android DEX extension; broader shared behavior remains adapter-specific. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `fixPlaybackSpeedWhilePlayingPatch` | Supports Playback speed through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `gmsCoreSupportPatch` | Native SSO/keychain equivalent implemented; login reported working, refresh/relaunch persistence remains unconfirmed. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `cronetImageURLHookPatch` | Selected native thumbnail URL accessors adapted; global image interception remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `lithoFilterPatch` | Positive native Elements filtering adapted; Android Litho infrastructure is not transplanted. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `navigationBarHookPatch` | Native pivot model compaction, live preference refresh, label layout and selection restoration integrated; other navigation options remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `playerControlsOverlayVisibilityPatch` | Native control visibility hooks adapted; Video tools lives in settings. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `playerControlsPatch` | Native Video tools/settings replace injected Android overlay controls; complete control parity remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `playerTypeHookPatch` | Guarded native ordinary/live/ad and fullscreen checks. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `sanitizeSharingLinksPatch` | Mapped sharing URLs cleaned; other sharing surfaces remain. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `recyclerViewTreeHookPatch` | Selected native model/view hooks adapted; no Android RecyclerView runtime on iOS. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `settingsResourcePatch` | Native settings rows and catalog replace Android resources; follows the settings integration scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `settingsPatch` | Native ReVanced menu, icon, search, grouped preferences, import/export, reset and diagnostics implemented; device layout/gesture confirmation remains. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `forceOriginalAudioPatch` | Original-track preference adapted; native selection coverage remains incomplete. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `videoInformationPatch` | Native content ID, time, duration and playback ownership accessors adapted. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `playerResponseMethodHookPatch` | Selected native response/accessor and content lifecycle hooks adapted. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `advancedVideoQualityMenuPatch` | Supports Video quality through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `hidePremiumVideoQualityPatch` | Supports Video quality through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `rememberVideoQualityPatch` | Supports Video quality through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `videoQualityButtonResourcePatch` | Supports Video quality through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `videoQualityDialogButtonPatch` | Supports Video quality through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `playbackSpeedButtonResourcePatch` | Supports Playback speed through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `playbackSpeedButtonPatch` | Supports Playback speed through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `customPlaybackSpeedPatch` | Supports Playback speed through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `rememberPlaybackSpeedPatch` | Supports Playback speed through the selected native iOS adapter; inherits its remaining scope. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `videoIdPatch` | Native content identification and generation-checked responses adapted. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |
| `fixBackToExitGesturePatch` | Android system-back fix has no direct iOS equivalent; ReVanced navigation uses a separate native back gesture. | <img src="assets/progress/red-circle.svg" width="24" height="24" alt="not started" title="not started"> |
| `fixContentProviderPatch` | Android ContentProvider authority rewriting does not apply to iOS. | <img src="assets/progress/red-circle.svg" width="24" height="24" alt="not started" title="not started"> |
| `accountCredentialsInvalidTextPatch` | Android GmsCore credential-error text has no corresponding iOS screen. | <img src="assets/progress/red-circle.svg" width="24" height="24" alt="not started" title="not started"> |
| `versionCheckPatch` | Google Play Services checks do not apply; the iOS source/profile check is separate. | <img src="assets/progress/red-circle.svg" width="24" height="24" alt="not started" title="not started"> |
| `spoofVideoStreamsPatch` | Full stream replacement is unimplemented; request-field overrides do not supply it. | <img src="assets/progress/red-circle.svg" width="24" height="24" alt="not started" title="not started"> |
| `userAgentClientSpoofPatch` | Alternate-client transport/header spoofing is unimplemented. | <img src="assets/progress/red-circle.svg" width="24" height="24" alt="not started" title="not started"> |

</details>

## iOS-specific integration

| Patch | Description | Progress |
|---|---|:---:|
| Use compatible dislike layout | Watch-scoped paired-layout compatibility preserves native account/feed startup configuration; both-screen device verification remains pending. | <img src="assets/progress/yellow-circle.svg" width="24" height="24" alt="in-progress" title="in-progress"> |

The patch catalog maps the 87 feature switches to the entries above. The first-launch UI policy is tracked separately because it is an iOS integration feature rather than an Android patch declaration. Per-category SponsorBlock behavior, native skip/undo prompts, inline RYD counts, settings presentation and Shorts toolbar compaction are included in their parent entries.

## Build and diagnostics

Production payloads use **Apple Xcode 16.4 / iPhoneOS 18.5 SDK**, targeting `arm64-apple-ios17.0`, with signing header padding. Current delivery was compiled and packaged without tests; compilation does not confirm feature behavior on a device. Source IPAs, SDKs, tools, credentials and generated artifacts are excluded from Git.

New options are under **Settings > ReVanced > Navigation** and are off by default. At least one content tab stays available. Their device behavior is pending confirmation.

In YouTube, open **Settings > ReVanced > Hook diagnostics** for full and feature-specific reports. Saved app preferences override bundled presets. Reopen the video or restart for settings that affect startup/player configuration.

Configuration defaults are in [patcher/configs/defaults.json](patcher/configs/defaults.json). Source and structured evidence remain in `patcher/` and `patcher/profiles/`. Only this README and the signing guide are published as Markdown; internal documentation stays local.
