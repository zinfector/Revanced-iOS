# SponsorBlock playback repair: 0.3.5

The user reported no highlighted regions or skips on YouTube 21.39.4 with the 0.3.4 SideStore-auth IPA. The supplied diagnostic report shows SponsorBlock and markers enabled, manual mode disabled, the supported profile accepted, and the existing hooks installed. Hook installation alone does not show that the playback callback fires or that a request succeeds. The old report contains no SponsorBlock pipeline state.

The local native analysis identifies a second clock path: `YTPlayerViewController.startObservingStateChangesOnActiveVideo` subscribes directly to the active video's event center. Its callback is `potentiallyMutatedSingleVideo:currentVideoTimeDidChange:`. Version 0.3.4 only hooks the local controller's `singleVideo:currentVideoTimeDidChange:`. Missing this player clock is a concrete implementation gap; the supplied report does not prove it is the sole cause of the device failure.

## Implementation

- Hook both time paths after their native implementations. Marshal observation to the main queue and accept updates only for the bound player's current local controller.
- Resolve `YTPlayerViewController._playbackController` by its native ivar name and exact object type (`@"<YTCorePlaybackController>"`). Require a `YTLocalPlaybackController` value. There is no fixed memory offset or untyped KVC access. Unknown/Cast controllers remain excluded.
- Keep segment requests, skip-once state, manual skipping, undo and markers on the local controller's associated session. The player facade's scalar seek ABI differs from the local controller's `YTSingleVideoTime` ABI; automatic skips continue using the latter through exact ABI guards.
- Use the event's finite time first, with content-time fallback. Reject mismatched event/content CPNs. Fetch for confirmed ordinary content during pause/buffering, but request automatic seeks only while native `isPlayingContentVideo` is true. Ads, live videos, unknown responses and stale ownership remain excluded.
- Cache successful empty responses and HTTP 404 as well as populated responses. Retry failed requests after the existing 30-second throttle. Video/CPN or category/minimum-duration changes cancel requests and invalidate their generation. Old responses cannot replace a newer session's data.
- Find the marker bars inside the bound player's native `playerView.overlayView`. Invalidate matching inline bars directly even if the parent skips child layout. Align marker geometry with the native `barFrame` and raise the noninteractive marker surface above the native progress children. Preview bars outside that overlay remain untouched.

The read-only segment request follows the [SponsorBlock API](https://wiki.sponsor.ajay.app/w/API_Docs), with configured categories and skip/highlight actions. Existing contribution actions still require explicit in-app selections. This repair sends no votes, submissions or service-account mutations during development.

## Diagnostics and scope

Copied Hook diagnostics now include a `sponsorblock` section: binding and response checks, local/player clock counts, the current playback gate, request state, HTTP status, numeric network error code, accepted segment count, automatic-range count, seek requests/targets, and marker layout/rectangle counts. These values contain no video IDs, URLs, CPNs, email addresses or credentials. Counters cover the process lifetime; state fields describe the latest recorded stage. A seek count means the native seek API was called, not that playback completion was independently observed. `device_playback_verified` remains false until there is separate device evidence.

Native method/type anchors and the prior service observation are recorded in [profiles/sponsorblock-evidence.json](profiles/sponsorblock-evidence.json). The previous read-only response for the user's video contained four segments; its reported policies merge intro/self-promotion into 49.312-73.447 seconds and automatically skip sponsor content from 382.982-464.851 seconds. The outro is marker-only. Public service data may change.

No regression tests, Objective-C harnesses, GUI smoke tests or device tests were run for 0.3.5, as explicitly requested by the user. The release is compiled and packaged. Earlier passing test receipts apply to their earlier source versions. Authentication implementation and preference keys are retained; successful login remains user-reported. Device skip/marker behavior is unverified.
