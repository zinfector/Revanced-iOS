# Native SponsorBlock skip and undo prompts

Status: investigation and implementation scheme for YouTube 21.39.4. No runtime source changes, new IPA, or tests are delivered by this investigation.

Use a fresh native `YTAdSkipView` containing its native `YTQTMAdSkipButton`, hosted inside the owned ordinary-video overlay. Supply SponsorBlock text and actions to that private instance. Manual segments show **Skip sponsor** (or the appropriate category); a confirmed automatic skip shows **Undo skip** in the same position and style. The action seeks within content playback. Native ad renderers, ad-skip delegates, commands, and click tracking remain outside this path.

## Other implementation inspected

The registered Git worktree list currently contains only `Revanced-iOS`. The separate ad-block implementation available locally is the release checkout at `ReVanced/patcher/build/adblock-handoff-release-0.3.24/patcher`, with the preceding merged checkout at `adblock-merge-release-0.3.23/patcher`. This scheme uses the 0.3.24 checkout as its integration baseline, preserving its coordinator-handoff fixes. The older standalone checkout in the main workspace has the same SponsorBlock presentation gap.

In that baseline:

- `native/RVPort.m` implements category behavior, fetching, merged skip intervals, content-clock observation, and automatic seeks. `manual-skip` is excluded from automatic ranges; the global manual switch suppresses automatic seeks. Neither creates an in-player prompt.
- `native/RVServices.inc:RVAddServiceTools` exposes **Skip current SponsorBlock segment**, **Undo last SponsorBlock skip**, and **Jump to SponsorBlock highlight** in the Video tools action sheet. Those menu actions are useful fallbacks, not prompt rendering.
- `hasLastSkip`, `lastSkippedStart`, `lastSkippedEnd`, and the scalar `ignoreSegmentEnd` store only the latest range. Undo remains offered until another reset/skip. The automatic path sets skip state and records counters before invoking a native void-returning seek; it does not distinguish request dispatch from arrival at the target. The undo menu sets its suppression end and clears the action even if seeking fails.
- `RVSponsorRanges` merges intervals into start/end pairs and discards category and member identity. The manual menu uses a merged range across all non-marker categories, so a manual category can be mixed with automatic categories. Prompt text and undo semantics require membership to survive this merge.
- `RVPlayerOverlay` and `RVOwnedPlayerView` already establish a relation to the current player's actual overlay. `RVContentGate` excludes known ads/live playback and unavailable identity/response. It is a starting point, not a sufficient visibility, scrubbing, or route gate.

These are creator-sponsored content segments handled by SponsorBlock. YouTube ad-block coordinator selection is a separate subsystem. A no-op ad coordinator must not be used to model a SponsorBlock segment or suppress its content clock.

## Native control evidence

Read-only Ghidra inspection reused the supplied project with `-readOnly -noanalysis`, decompiled 36 native methods, exported selected ARM64 instructions, and extracted Objective-C ivars and method encodings. The executable SHA-256 is `ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf`. Addresses and analyzed ivar offsets are provenance only; implementation resolves names at runtime and checks the supported app profile and ABIs.

| Native contract | Observed behavior | Implementation consequence |
|---|---|---|
| `YTAdSkipView initWithFrame:` | Creates its own `YTQTMAdSkipButton`, sizes it, initially sets alpha to zero, assigns `ad_skip_button`, and adds it as a child. | Allocate a separate wrapper; obtain only its own child, then give the SponsorBlock control its own accessibility identifier. |
| `_skipAdButton` | Object ivar encoding `@"YTQTMAdSkipButton"`; no public button getter was found on the wrapper. | Resolve the named ivar with `class_getInstanceVariable`, check encoding and actual class, and read with `object_getIvar`. Do not hard-code the analyzed offset or mutate native-owned wrappers. |
| `YTQTMAdSkipButton initWithFrame:` | Uses native skip artwork, theme foreground/background, title styles, padding, image/title layout, exclusive touch, and QTM ink/ripple configuration. No ad delegate or renderer is supplied here. | Reuse this implementation rather than drawing a generic floating box. Runtime-check it is a `UIControl`/`UIButton` and inspect existing targets before adding the private router. |
| `YTQTMAdSkipButton cornerRadius` | ARM64 instructions select 16 points when modern skip buttons are enabled, otherwise zero. The decompiler alone incorrectly suggests a constant zero return. | Let the native subclass choose its own corner treatment; do not flatten it with a custom layer radius. |
| `YTAdSkipView setFullscreen:` | Applies native fullscreen/inline font kinds, modern/heavy-font variants, sizes its child, and updates highlight styling. | Run this after each presentation-mode change; preserve native feature/config choices. |
| `setSkipViewForAdAdIndex:totalAdCount:` | Selects the button, removes the selected accessibility trait, sizes it, and sets alpha to one. Its title is conditional on native renderer/modern-text flags. No ad delegate invocation appears in this method. | On the private wrapper, initialize the ready-to-skip state with neutral indexes, then unconditionally replace title/accessibility text with the local prompt. Never rely on its conditional native ad title. |
| `setSkipViewFromSkipAdRenderer:firstResponder:` | Reads renderer extensions/text/fade transitions and can register the view with native responder targets. | Do not use this method or fabricate `YTISkipAdRenderer` messages for SponsorBlock. |
| `addDidPressSkipAdTarget:action:` | Forwards to the child control's touch-up-inside event. | Bind only the SponsorBlock router on the private instance. Remove it on teardown. |
| Wrapper layout/sizing/hit target | Sizes the child and aligns it bottom-right; modern `hitTargetBounds` expands toward a 48-point minimum. The child constructor also configures a 48-point minimum hit target in the modern branch. | Preserve native sizing and keep the enlarged hit target inside the parent host's tappable bounds. |
| `YTAdVideoPlayerOverlayView layoutSubviews` | Anchors native skip UI at the lower-right of the overlay/video area, adapting to fullscreen safe area, portrait offsets, CTA/survey spacing, peekable controls and side-content frames. | Position against the current visible video region, not the application window or Settings safe area. Copy the placement policy relevant to ordinary content. |
| `YTAdVideoPlayerOverlayViewController didPressSkipAd:forEvent:` | Invokes the ads delegate and can send renderer/button clicks. | Never forward a SponsorBlock tap into this method, `skipAd`, or `didPressSkipAdWithTouchPoint:`. |
| `YTPlayerOverlayManager activateOverlay:updatePlayerView:` | Replaces/wraps active overlay controllers and propagates layout, playback route and video state. | Attach a child to the current content overlay; do not activate an ad overlay or replace the active overlay controller. |

Additional metadata records ordinary-overlay `expandedView`, `videoRect`, engagement/elevated render frames, `playerBar`, fullscreen state, captions, and the current `YTPlayerView.renderingViewFrame`. `videoContentFrame` and `edgePaddingRight` were not declared on `YTMainAppVideoPlayerOverlayView`; do not assume the ad-overlay accessor set exists on the content overlay. `activeOverlay` was also absent from the manager metadata; use the verified owned view path rather than inventing that getter.

The frosted-view wrapper method accepts a separate native styling object. The control constructor alone does not establish frosted parity with every experiment. If the active profile uses frosting, reuse a verified native style provider on the private instance only; otherwise record the native QTM style variant. A missing style contract is a diagnostic limitation, not justification for modifying native global style flags.

## Android behavior to preserve

The local Android source confirms that this feature belongs to SponsorBlock:

- `ui/SkipSponsorButton.java` consumes native skip-ad minimum-height, background, border, and placement resources, but routes taps to `SegmentPlaybackController.onSkipSegmentClicked`, not the YouTube ad-skip action.
- `ui/SponsorBlockViewController.java` separates selecting a segment from showing the control, applies player-type/visibility gates, and hides prompts on unsupported presentations.
- `SegmentPlaybackController.java` supports manual prompts, automatic and once-only skipping, bounded display time, merged undo ranges, stale-video resets, and an undo toast. Undo restores the suppression range before seeking back so the content clock cannot immediately auto-skip again.
- `objects/SponsorSegment.java:getUndoRange` treats highlight jumps specially. Android's automatic undo toast is a custom dialog, so using the iOS native skip control for undo is a requested design adaptation, not a literal UI port.

No new SponsorBlock vote, submission, or view-count request is needed for these controls. Preserve the existing service and privacy behavior.

## Prompt behavior

Use one visible control and the following explicit policy. Durations and the 0.25-second rewind margin below are proposed product defaults, not measurements of native ad behavior.

| Situation | Prompt/action |
|---|---|
| Inside a `manual-skip` interval | Show category-aware **Skip sponsor**, **Skip intro**, etc. Tap seeks to that manual interval's end. |
| Global manual mode | Treat enabled `skip` and `skip-once` intervals as manual candidates too. Do not offer `ignore` or `seekbar-only` ranges. |
| Automatic skip reaches its target | Show **Undo skip** for six seconds of visible foreground presentation. The accessibility label can include the category and skipped duration. |
| Undo is tapped | Return to `max(0, skippedRange.start - 0.25)` to land just before the segment. Suppress automatic skipping for the range while it is replayed. |
| Replaying an undone range | Offer a manual skip control; leaving the range clears its replay suppression. A later deliberate revisit follows normal category policy, including `skip-once`. |
| Manual skip succeeds | May show the same bounded undo control; share the action implementation with automatic skips rather than introducing a second undo mechanism. |
| Highlight point | Manual mode offers **Jump to highlight** before the point, not a zero-duration skip interval. Undo returns to the actual pre-jump time and preserves highlight-applied state to avoid jumping forward again immediately. |
| No eligible range, ignored/marker-only range, unsupported presentation | Hide the control. Retain the existing Video tools actions as fallback. |

Keep member records on merged ranges: UUID/fallback key, category, action and behavior. Merge eligible automatic intervals separately from manual intervals; only overlap/adjacency within the same policy domain can extend a seek. Automatic policy retains its existing precedence when it crosses a manual interval, but the diagnostic record must show what the actual seek covered. Never silently extend a manual tap through a merely nearby automatic segment. A range with multiple categories uses **Skip segment** and a descriptive accessibility label.

A completed automatic seek's undo gets priority briefly over a new manual prompt. New automatic seeks invalidate/replace the old undo; coalesce adjacent/overlapping automatic skips into one transaction only when they occur in the same playback generation and the merge preserves a defensible return point. An unrelated seek or scrub discards the visible undo instead of sending the user back to an old segment.

## State and seek transactions

Introduce `RVSponsorPromptState` associated with the owning playback session/controller. Keep all mutations on the main queue. Store weak controller/player/host identities, playback token (controller + video + CPN), segment revision, presentation serial, prompt mode, target range/member keys, latest observed content time, pending seek, last confirmed skip transaction, replay-suppression interval, and a bounded diagnostic history. Video IDs and CPNs are internal identities and must not appear in copied diagnostic reports.

`RVPlaybackSession.generation` currently also changes for category fetch invalidation; it is not exclusively a playback epoch. Either introduce a separate playback epoch or intentionally invalidate prompts on every such revision. Never allow a stale fetch, timer, button, or same-video replay to target the new session. Settings/segment changes cancel an affected prompt/transaction. Prompt lifetime is separate from the lifetime of historical skip counters.

Implement one `RVSponsorRequestSeek(transaction)` path used by automatic skipping, prompt taps and Video tools. It must:

1. Validate the exact owning player/controller, video/CPN, segment revision, feature/category policy, finite seekable target and ordinary-content state. A manual interval tap must still be inside its interval; an undo must still identify the current confirmed transaction. An absent CPN requires an otherwise stable playback token and no pending transition.
2. Install a pending transaction and any provisional replay suppression before invoking the native seek. The native method can synchronously reenter observers. Disable the control during the request and retain the existing repeated-target throttle.
3. Build `YTSingleVideoTime` through the checked `timeWithTime:CPN:` contract, then invoke the existing `seekToTime:toleranceBefore:toleranceAfter:seekSource:` content seek once. A helper returning YES means the request was issued, not that playback arrived.
4. Confirm arrival from a matching content-clock observation. For a forward skip require crossing the intended end within a bounded target neighborhood; for undo require arrival near the rewind target. Use a documented tolerance accounting for native seek snapping and scheduling, and a bounded timeout, initially three seconds of active observation. Record request and confirmation separately. Reject an external user seek/transition as superseding the transaction rather than mistaking it for a skip success. Do not require playing state for a paused manual seek's time confirmation.
5. On confirmation, commit the last-skip transaction and show bounded undo, or commit replay suppression for undo. On unavailable contract, supersession or timeout, remove pending state, restore provisional suppression, and retain a usable manual action where appropriate. Keep existing counters explicitly labelled seek requests; add confirmed counters separately.

`skip-once` needs a provisional reservation before dispatch to stop reentrant duplicates, committed on confirmation and released on failed/superseded transactions. Undo must set replay suppression before seeking and restore prior state if the request fails. Replace the scalar `ignoreSegmentEnd` with a token-bound interval; the current scalar can suppress unrelated earlier ranges after a backward seek. Suppression lasts through replay, not through the six-second prompt timer. Clear it when playback leaves that interval, changes generation or the user makes an unrelated navigation seek. Preserve explicit highlight suppression separately.

Update the prompt evaluator on accepted segment response, validated content-clock tick (including paused state), player binding, settings changes, content transition/reset, scrub start/end, and overlay presentation changes. Clear or hide presentation state before early returns for ads/live/unknown ownership; otherwise an already mounted control can remain actionable while the native player changes mode. Avoid a timer-driven playback polling loop.

## View ownership, layout and interaction

`RVSponsorPromptHostView` is a transparent child of the verified visible ordinary-video overlay, preferably its native expanded container after confirming its ownership and coordinate system. It owns the fresh native wrapper and an action router. No new app window, modal alert or global overlay is necessary.

Allocate lazily when a valid prompt is needed. Require `RVCompatible`, expected classes and method encodings, the named child ivar encoding, and runtime `UIView`/`UIButton` class checks. Initialize the wrapper, retrieve its own child, set the ready state, set fullscreen styling, then apply local title and accessibility text. Use `sizeThatFits:`/`sizeToFit` after changing title, mode, page style or Dynamic Type. Keep the wrapper's native highlight/layout behavior. Assign identifiers such as `rv_sponsor_skip` and `rv_sponsor_undo` so reports distinguish these controls from real ads.

Bind only the private router through the checked native target/action method. The router reads the current immutable prompt token and revalidates immediately before seeking. Verify the fresh child's existing targets do not invoke ad endpoints; detach the private instance on any unexpected target/contract instead of stripping handlers from a native-owned button. Undo can replace the forward icon with a verified local/native rewind glyph or clear it; its visible text and accessibility label must unambiguously describe going back. Retain native font, padding, shape and hit feedback.

Hook ordinary overlay `layoutSubviews` after the original returns, scoped to `RVOwnedPlayerView`; use controller `viewDidLayoutSubviews` only where needed to retain its presentation context. On each layout:

- Determine the current video presentation rectangle from the checked ordinary-overlay render-frame/videoRect contracts or `YTPlayerView.renderingViewFrame` converted into host coordinates. Validate it against host bounds; use the adjusted engagement/elevated frame when active. Do not reuse an ad-overlay-only selector or treat a screen-sized container as video-sized.
- Anchor at the native skip slot at the lower-right (with the native RTL convention), combining verified skip placement offsets, current safe-area contribution and the visible video rectangle. Apply each inset once. Use the native button size; no fixed width. Relevant ad CTA/survey/peekable layout paths provide evidence for collision handling, but ordinary-content captions, player bar/fullscreen controls and side panels are the actual obstacles here.
- Keep the native expanded hit rectangle inside the host and video bounds, moving the slot up when native bottom controls or a caption region would be covered. If no safe slot exists, hide with `layout_collision`; preserve the tools fallback. Record the chosen coordinate source and adjustment. Native control may be visible when playback chrome fades, like the real ad-skip action, but never while another foreground surface covers the player.
- The host forwards touch hit testing only to the visible prompt's valid enlarged hit rectangle and otherwise returns nil. Verify the ordinary overlay's own `hitTest` contract permits its new child; if it ignores arbitrary children, a narrowly scoped hook can return only the owned prompt hit. Do not capture the whole video area or interfere with scrub, fullscreen, double-tap, captions or back gestures.

Hide/detach for actual YouTube ads, background/offscreen playback, PiP, Cast/AirPlay, live/Reels/feed previews, miniplayer/collapse transitions, scrubbing/fine scrubbing, unsupported route/layout contracts, loading/endscreen or a covering Settings/modal surface. Resume manual eligibility after scrubbing using the new clock position; do not revive stale undo. Rehost on portrait/fullscreen overlay replacement and remove the old child/router. The global `overlay_opacity` feature must not independently dim this control if doing so violates the native skip-action contrast/visibility; document and resolve that interaction during integration.

There is no proof yet that every current overlay experiment forwards child touches, exposes a usable render rectangle, or supplies its frosted style provider. Those are explicit adapter gates, not completed implementation claims.

## Integration sequence

1. Start from the 0.3.24 ad-handoff checkout (or a newer merged baseline preserving it). Add `native/RVSponsorPrompt.inc` for policy/view/router and a bounded diagnostics helper. Do not replace `RVAds.inc` or its coordinator lease lifecycle. Save baseline source hashes in the implementation receipt.
2. Refactor interval selection and seek dispatch in `RVPort.m` into shared helpers retaining category/member provenance, pending transactions and playback tokens. Wire evaluation into the existing accepted clocks and fetch completion without adding network calls.
3. Implement the owned native wrapper adapter and ordinary-overlay hooks in `RVExtras.inc`/the new module. Audit the actual content-overlay hit testing and style provider before enabling the corresponding experiment. Keep contract failures visible in diagnostics.
4. Route `RVServices.inc` manual skip, undo and highlight actions through the same transaction helpers. Retain menu fallback, but evaluate validity at tap time and do not leave an expired prompt transaction exposed as a permanent menu undo.
5. Extend `RVSponsorReport` and `RVSettingsUI.inc` with **Copy SponsorBlock prompt checkpoints**. Existing category keys and the global manual-mode key stay compatible. An optional `sponsor_prompt_duration` can be added later; initial bounded defaults need no extra user setting.
6. When implementation is requested, compile/package using the existing signing-header reservation. Do not run tests while the user's no-tests instruction remains active. Compilation/packaging alone cannot confirm native appearance, touch handling or successful playback seeking.

## Diagnostic checkpoints and device capture

Use revision `sponsor-prompts-1` and the native source fingerprint. Keep existing SponsorBlock fetch/marker reports and ad-block diagnostics distinct. Provide these redacted checkpoints:

| Checkpoint | Diagnostic fields |
|---|---|
| `segment_policy` | loaded segment count, candidate count, selected behavior/category, merged member count, segment revision; no UUIDs/video IDs |
| `playback_eligibility` | controller/player ownership, epoch match, CPN availability/match, content/ad/live state, route, foreground and visibility gate |
| `native_control_contract` | wrapper/button class and ABI checks, named ivar encoding/class, target audit, font/style variant, frosted provider status |
| `prompt_selected` | hidden/manual/highlight/undo/pending, prompt serial, transaction match, remaining visible lifetime, suppression interval active |
| `prompt_layout` | host attached/window-visible, coordinate source, finite/nonzero video rectangle, portrait/fullscreen mode, inset/collision adjustment, effective alpha and hit-target containment |
| `prompt_tap` | tap observed, mode/token revalidated, refusal reason, selected target bucket; never ad renderer/command data |
| `seek_requested` | native time/seek contracts, issued versus refused, source auto/manual/undo/highlight, provisional reservation, reentrant observer count |
| `seek_confirmed` | matching clock observed, target reached, latency, timeout/superseded, skip-once commitment, undo made available |
| `replay_suppression` | installed before rewind, committed/rolled back, segment reentry, attempted immediate reskip blocked, exit/reset |
| `cleanup` | reason hidden/detached, timer token cancelled, old host removed, pending/undo invalidated |

Maintain `last_visible_player` before Settings covers it, its age and generation-match flag, plus a small transition history. `first_blocked_checkpoint` should identify, for example, `no_manual_candidate`, `playing_native_ad`, `native_wrapper_missing`, `button_ivar_contract_mismatch`, `ordinary_overlay_missing`, `prompt_hit_target_unreachable`, `seek_contract_missing`, `seek_not_confirmed`, or `stale_prompt_rejected`. Report issued and confirmed counts separately. Never label the overall feature device-validated from installed hooks alone.

For a future device report: record category/manual settings, a screenshot with the manual prompt, tap result, and the copied checkpoints. For automatic skips, capture the undo prompt, tap it, show playback before the segment and confirm it does not immediately skip again. Also record portrait/fullscreen, paused playback, overlap, same-video replay, a video switch with the prompt visible, Settings coverage, and any competing native controls. These are capture instructions for subsequent device use, not tests executed by this investigation.

Research provenance is in [sponsor-prompt-evidence.json](profiles/sponsor-prompt-evidence.json). Read-only decompilation, assembly and presentation contracts are stored under `ReVanced/patcher/build/sponsor-prompt-investigation`; reproduction scripts are in `ReVanced/analysis/scripts/investigate_sponsor_prompts.py` and `InspectSponsorPromptAssembly.java`.
