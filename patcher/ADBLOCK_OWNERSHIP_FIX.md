# Adblock ownership correction — 0.3.29

Report 348 matches the cloud source fingerprint of the delivered merged 0.3.28 payload. All video/feed switches are on, the native coordinator strategy is selected, all ad hooks installed, and all 52 feed advertising-field accessors resolved.

The observed playback failure is **`watch_owner_unknown`**. Native selection records `cached_recreation_requested: false`, retains `YTAdsControlFlowPlaybackCoordinator`, and reports `actual_strategy: native` and `suppression_observed: false` despite positive ad input. This is an eligibility failure before the coordinator handoff. The report does not export the responder chain, so it cannot establish which individual native owner link was missing.

The old implementation only followed `parentResponder`. Read-only native metadata/disassembly establishes separate ownership edges: `YTLocalPlaybackController._delegate`, `YTPlayerViewController._playbackController`, the player's UIKit `parentViewController`, its native event/UI delegates, and `YTWatchPlaybackController._playerViewController`. The supplied Ghidra project was locked by another process; the lock was preserved and the additional research used the original Mach-O directly.

`RVAdWatchOwnership.inc` retains the existing native responder route and adds these exact ownership paths. A fallback player must be the core controller's correctly typed native delegate and must point back to that same core. A Watch parent/delegate must in turn hold that same player. Runtime ivar names/types and getter ABIs are checked; no fixed offsets, arbitrary delegate walk, forced owner class, or unrelated global Watch are used. Reel ownership is rejected before Watch fallback. Existing live/DAI/response checks and the existing native assignment/transition boundaries remain in place.

The playback lease stores weak references to the verified player and Watch view. Passive foreground capture can use that verified view rather than depending entirely on `RVCurrentWatch`. Diagnostic copying never forces a coordinator change. The report separates raw-response and content-video lease mismatches, which were combined in 0.3.28.

## Banner evidence and limits

The same report contains 79 traversed entries, no advertising classifications/removals, no companion clearing callbacks, and one `managed_slot_bound` event. This shows that the observed ad slot bypassed the typed-removal/companion routes. It does **not** reveal its opaque renderer, prove its location, or justify globally hiding generic Elements views.

The new `display_ads.last_managed_slot_checkpoint` records the native adapter class, slot/layout logging and data classes, presence contracts, numeric slot type when its ABI is available, and bind/dispose completion. It exports no raw payloads, tracking IDs, URLs or pointer addresses. Slot lifecycle remains native. A native bind notification occurs after rendering; suppressing only its lifecycle notification is not an established visual removal contract.

Opaque Home/banner roots, mastheads and already-bound ad-slot removal remain unresolved. This release does not claim to fix those banners.

## Checkpoints after installing

Expect **`adblock-checkpoints-4`**, outer schema 4 and display checkpoint version 4.

| Checkpoint | Expected/meaning |
| --- | --- |
| `current_playback.state.watch_ownership` | Redacted responder classes, core-delegate class, player/core and Watch reverse-binding checks |
| `watch_ownership_route` | `player_view_parent`, `player_events_delegate`, `player_ui_delegate`, or the retained native responder route; `unresolved` means no verified Watch path |
| `eligibility_gate` | `ready` for eligible ordinary Watch content; explicit Reel/live/DAI routes remain native |
| `cached_recreation_requested` | True at a native new-playback selection when eligible coordinator blocking and its config contract are available |
| `selected_coordinator_class`, `native_no_op_selected`, `pointer_installed` | The native no-op was selected and installed; hook installation alone is insufficient |
| `lease_raw_response_matches`, `lease_video_matches_content` | Separates response replacement from stale video identity |
| `watch_snapshot_owner_class`, `last_visible_watch` | Verified Watch view available for passive foreground capture |
| `display_ads.last_managed_slot_checkpoint` | Class/presence/type evidence for the opaque banner path |

Capture a remaining ad while its surface is current. For native video blocking, use a fresh video selection after installation so the existing coordinator can be recreated at its normal boundary. Updating a policy while an ad is already active does not replace that coordinator mid-ad.

## One delivery

`package_merged_release.py` now creates only **`YouTube-21.39.4-RVPort-0.3.29-unsigned.ipa`**. It retains all implementations merged into main at 0.3.28, the expanded authentication preset, native coordinator blocking, and extension removal for SideStore. Existing saved preferences still override bundled defaults. Separate unmerged worktree changes are not silently imported.

The established Xcode 16.4 / iPhoneOS18.5 cloud toolchain and exact source/payload provenance checks are required. Regression, hook, GUI, archive self-check and device tests remain disabled under the user's instruction. Compilation and packaging do not verify device behavior.
