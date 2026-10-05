# Native coordinator handoff, 0.3.24

The adapter now follows coordinator ownership beyond the services factory. It accepts a pending policy request from settings changes, content binding and the playback clock, and commits it through native playback selection or a verified internal transition. This addresses the confirmed lifecycle gaps in 0.3.23. Report 1251 does not establish which missing-response subcase occurred on the device; the new report records those separately.

## Implemented paths

- **Initial selection and cached reuse:** `adsPlaybackCoordinatorForNewPlayback` binds the current typed wrapper/raw response. An eligible coordinator request rejects cached compatibility only for that exact receiver, response and current lease. The original native selector and factory construct the no-op coordinator with the original registry and controller delegate. The original caller stores and initializes it; the adapter observes the stored field at `startPlaybackWithVideoResolver:` return.
- **Late response/policy request:** settings, player binding and coalesced clock observations refresh the lease if either wrapper or raw response changes. Requests during active playback remain pending until a native boundary. Clock callbacks allocate no coordinators and make no pointer writes.
- **Internal transition:** the adapter observes whether the retiring coordinator's native `reset` actually returned, and whether it received an autoplay notification. Before `didTransitionToContentSequenceForVideoSequencer:` supplies the incoming video/timeline, it can create a replacement through the native factory, initialize overlay lifecycle and the observed autoplay value, and store the replacement using `object_setIvarWithStrongDefault`. The original transition method then supplies content and timeline and reports activation.
- **Ownership preflight:** resolve `_adsPlaybackCoordinator` by name on the compatible class; require its object encoding, alignment, instance bounds, inclusion in the runtime ARC strong layout, exclusion from the weak layout and agreement with the native getter. Generation, wrapper/raw response, pointer, main thread and inactive-ad checks are repeated after native initialization. There are no fixed runtime addresses or offsets.
- **Rejected candidate:** an unpublished candidate is reset to release native observer/timeline state. The installed old object is retained. Thread scopes restore through `@finally`, including teardown. Native break completion callbacks are preserved; callbacks from an older generation cannot mark a new video's success.

Diagnostics are observers: selection and transition behavior do not depend on the diagnostic switch. Response filtering now also requires exact current response ownership. Authentication, SponsorBlock, vote rendering, speed bridges, presets and feed/Shorts rules are carried forward unchanged, apart from the existing shared player/settings callbacks that request ad-policy evaluation.

## Deferred limits

Replacement is deferred during an active ad, unknown ad state, response/lease mismatch, runtime ownership failure or a transition whose native cleanup was skipped. Live, DAI, Reels and unknown watch ownership remain native. Completed native reset is evidence of cleanup at that boundary; it does not prove all future asynchronous callbacks have drained. Arbitrary mid-ad pointer mutation and synthetic ad completion are not implemented. A deferred request may need opening another video if the current session never reaches a verified boundary.

## Package and device capture

Use **`YouTube-21.39.4-RVPort-0.3.24-SideStore-auth-native-ads-unsigned.ipa`** for this change. SideStore signs it; app extensions are removed. Saved preferences take precedence over the bundled preset. In ReVanced settings, enable video ad removal, select **Native ad coordinator**, and open a new ordinary recorded video. The response-strategy SideStore-auth IPA is also retained as a separate artifact.

If an ad appears, keep the watch page visible, open ReVanced settings and copy **ad-block checkpoints**. Include the IPA filename and whether this was an explicitly opened video or an automatic/internal transition. The report should contain `adblock-checkpoints-2`; `last_visible_watch` preserves the visible-page observation from before opening settings.

| Checkpoint | Meaning |
|---|---|
| `current_controller_has_lease`, `binding_origin` | Distinguishes no controller observation from constructor factory, native selection, late response or internal transition. |
| `wrapper_present`, `wrapper_class`, `player_data_accessor_present`, `player_data_accessor_abi_matches`, `raw_response_class` | Separates a nil factory wrapper, incorrect class, missing/incorrect accessor and nil player data. `eligibility_gate` names the failed stage. |
| `current_response_lease_matches_playback`, `controller_response_matches` | The lease owns the actual current content response. This can be true even if an eager factory had no content. |
| `factory_response_argument_present`, `factory_argument_matches_scope`, `factory_lease_current` | The factory used the same response/generation as selection. A mismatch leaves native construction untouched. |
| `selection_entered`, `cached_recreation_requested`, `cached_compatibility_forced_recreate`, `selection_returned` | Shows whether the native selector and cached compatibility branch ran. |
| `no_op_flag_read`, `native_no_op_selected`, `returned_coordinator_class`, `candidate_original_delegate` | Shows native no-op construction with the original delegate. Construction alone does not establish installation. |
| `transition_reset_observed`, `transition_autoplay_observed`, `handoff_gate` | Explains eligibility for an internal-transition commit, including `transition_cleanup_unproven` and `deferred_active_ad`. |
| `coordinator_ivar_layout_valid`, `coordinator_getter_agrees`, `pointer_installed`, `assignment_origin` | Proves the selected object occupies the controller-owned field. |
| `native_initialization_completed`, `preroll_completed`, `postroll_completed`, `content_clock_progressed`, `clock_cpn_matches` | Separates assignment, native caller completion, native break callbacks and ordinary content progression. CPN values are never exported. |
| `policy_request_pending`, `policy_request_origin`, `first_blocked_checkpoint` | Shows a pending request and the first unresolved stage. `inconclusive_no_ad_input` does not establish ad blocking on an ad-free response. |

Counters appear under `events` in playback snapshots or `session_events` for totals; flags/classes appear under `state`. `current_playback` is the current lease. `last_visible_watch_matches_current_generation` and `last_visible_watch_matches_current_playback` identify whether the retained foreground snapshot belongs to it. History is bounded to 12 snapshots. Reports contain no coordinator addresses, video IDs, raw CPNs, cookies or tokens.

## Build evidence

The runtime compiled locally as ARM64 with warnings treated as errors. The release receipt records the cloud Xcode source fingerprint and artifact hashes when packaging completes. No regression tests, hook checks, GUI smoke tests or output archive self-checks ran, as requested. Device handoff is unverified until a new report confirms the lifecycle checkpoints. The [implementation evidence](profiles/adblock-handoff-implementation-evidence.json) records preserved native sources and research provenance; the [scheme evidence](profiles/adblock-handoff-evidence.json) and [decompilation excerpts](profiles/adblock-handoff-decompiled.txt) establish the analyzed contracts.
