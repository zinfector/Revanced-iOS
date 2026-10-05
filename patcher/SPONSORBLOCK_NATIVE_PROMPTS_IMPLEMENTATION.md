# Native SponsorBlock prompts — 0.3.26

Implemented on the isolated 0.3.24 ad-handoff release baseline. Version 0.3.26 replaces the crashing 0.3.25 Windows/Zig delivery with the established macOS Xcode 16.4 / iPhoneOS18.5 cloud compiler. Device appearance, touch handling, seek behavior and crash resolution remain unverified. No tests were run.

`native/RVSponsorPrompt.inc` implements content-clock policy and transactions. `native/RVSponsorPromptUI.inc` hosts a fresh YouTube `YTAdSkipView`/`YTQTMAdSkipButton` in the owned content overlay. Both are included in the existing native translation unit. This source checkout is at `ReVanced/patcher/build/sponsor-prompts-xcode-release-0.3.26`; existing worktrees were not overwritten.

## Behavior

- Categories set to **Ask to skip** show a category-aware native skip button while inside the segment. The global manual switch also makes automatic categories manual. Marker-only and ignored segments do not produce prompts.
- Automatic and manual content seeks share one request/confirmation path with the existing Video tools actions. Category and member identity survive merging; automatic ranges and manual ranges are selected separately. The tools fallback prefers a manual-policy range and does not merge it through an automatic range.
- Undo appears after a matching content clock reaches the seek target, rather than when the void native seek method is called. It stays available for six visible seconds, or ten with VoiceOver, and has an absolute 30-second lifetime. Covering the player pauses the visible-time budget, but cannot keep an old undo indefinitely.
- Undo returns to the segment start minus 0.25 seconds, clamped to zero. Its replay interval is installed before issuing the native seek, preventing reentrant immediate reskipping. The interval remains through replay and clears on exit, a different user seek, playback/reset or relevant setting changes.
- Manual and automatic highlight jumps use the same transaction path. Highlight undo returns to the pre-jump position and retains the applied-highlight flag to avoid an immediate second jump.
- Pending transactions prevent duplicate/reentrant requests; `skip-once` commits only on confirmation. A five-second wall timeout cancels an unconfirmed request and rolls back provisional replay suppression. Existing skip/time counters still describe requests, while prompt checkpoints distinguish confirmation and timeout.
- A new automatic range can replace the previous undo. Video/CPN ownership, fetch generation, category/behavior revisions and per-prompt serials reject stale callbacks or taps. A separate binding number keeps the last visible snapshot tied to the correct session even when Settings covers it.

## Native control and layout

The adapter resolves the named `_skipAdButton` ivar and expected native classes/ABIs. It creates only its own wrapper and uses a declared initializer so ARC consumes the allocated receiver correctly. The ready state and fullscreen font setup use native methods; local skip/undo text replaces the ad text. Undo uses a rewind glyph. The native class supplies theme, typography, padding, modern/classic shape and ink behavior.

Additional read-only Ghidra inspection confirmed `QTMButton.setupTouchHandling` installs self-targets for **touchDragEnter:forEvent:** and **touchDragExit:forEvent:**. Those ink handlers are allowed by the fresh-control target audit. Unexpected targets/actions block mounting. Only the private SponsorBlock router handles touch-up-inside. No ad renderer is fabricated, no native ad press callback is invoked, and no additional service requests are sent.

Placement uses the current player rendering rectangle or verified ordinary-overlay rectangle, with engagement/elevated frames where available. Native 21.39.4 main-app offsets (6-point trailing and 56-point portrait/44-point landscape bottom offset) are combined with only the safe-area contribution not already excluded by the render rectangle. The control keeps a minimum 48-point hit region, adapts to RTL, and moves above visible native caption labels/player-bar collisions. It hides if the region cannot fit. A scoped ordinary-overlay hit-test hook forwards only the owned, visible prompt hit; the transparent host otherwise passes touches through.

Prompts hide for covered/offscreen/background playback, real ads, live playback, PiP/external routes, mini layout, scrubbing, loading/endscreen surfaces, unavailable contracts or unsafe layout. User seeks and resets invalidate old undo. Overlay rehosting removes the old private control/router. Playback clocks drive policy; the only presentation timer expires undo and never polls the media player.

Frosted-glass style-provider experiments remain unmapped. This build uses the actual native QTM control background and reports that style limitation. Runtime rectangle, visibility and hit-test contracts are guarded but still need device observation. No generic floating-box fallback is installed when the native adapter fails; Video tools remains available.

## Capture a failure

1. Sign/install the 0.3.26 SideStore IPA. Confirm SponsorBlock is enabled. Use **Settings → ReVanced → SponsorBlock → Category actions → Ask to skip**, or enable global manual mode, for a manual prompt. Saved preferences take precedence over bundled defaults.
2. Capture the player screenshot before leaving it. For an automatic skip, tap **Undo skip** and include what happens on returning to the segment.
3. Open **Settings → ReVanced → Hook diagnostics → Copy SponsorBlock prompt checkpoints**. Expect revision `sponsor-prompts-1` and patcher version 0.3.26 in the full report.
4. Include `last_visible_player`, its age and `last_visible_matches_current_playback`, `current_player`, `hooks`, counters and checkpoint history. The copy-time observation can be covered by Settings; the last foreground player snapshot is preserved, including blocked layout/control creation. The historical `last_visible_player` field can therefore describe a player with no visible prompt.

Useful failures include `native_wrapper_contract_missing`, `button_ivar_contract_mismatch`, `unexpected_native_targets`, `presentation_contract_missing`, `video_rectangle_missing`, `layout_collision`, `seek_not_confirmed`, stale taps, and timeout/supersession counts. `owned_hit_test_forwarded`, the tap counter and confirmed/undo-confirmed counters distinguish a displayed button from a working action. A zero tap count before touching the control is expected.

The full diagnostic report includes `sponsorblock_prompts`; the existing SponsorBlock report embeds `native_prompts`. Reports exclude segment UUIDs, video IDs, CPNs and service URLs from this new section. `device_validated` remains false. A `ready` display checkpoint is not proof that a later tap or every ad/segment path works.

## Delivery evidence

The production payload reserves signing load-command header space through the existing `-headerpad,0x4000` build option. Delivery IPAs are unsigned and require the usual signing/install flow. The packaging wrapper skips the archive self-check under the user's no-tests instruction.

The native-only cloud workflow pins the same Xcode 16.4 developer directory and `arm64-apple-ios17.0` target used by the preceding release. The native artifact is downloaded to Windows and injected by the production patcher. This workflow contains no test steps; the release packager requires matching Xcode provenance and source/payload hashes. The 0.3.25 Zig IPA remains historical and is superseded by 0.3.26. Without a crash report, the compiler difference is established but is not a proven crash diagnosis.

Source hashes, preserved 0.3.24 modules, compiler identity, package digests, and verification limits are recorded in `profiles/sponsor-prompts-implementation-evidence.json` and the local build receipt. The new runtime files are the implementation of [the native prompt scheme](SPONSORBLOCK_NATIVE_PROMPTS_SCHEME.md); the historical scheme retains the original investigation and proposed defaults.
