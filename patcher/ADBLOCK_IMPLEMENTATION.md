# Ad-block implementation and checkpoints

The source now implements the supported player, typed-feed, and Shorts adapters from [the port scheme](ADBLOCK_PORT_SCHEME.md). Runtime behavior is not yet confirmed on a device. No tests were run.

## Changes

- `native/RVAds.inc` replaces the old inline player hooks. Response filtering is bound to a response owned by an eligible recorded-watch playback controller. Array getters and their generated count getters agree. Native derived queries remain responsible for monetization/preroll state.
- **Native ad coordinator** (`ad_strategy: coordinator`) invokes the original services factory while temporarily selecting its native no-op branch for the exact config object on the calling thread. Original arguments, registry scope, and native completion callbacks are retained. The scope is restored for nested calls and exceptions. There is no `nil` coordinator replacement.
- Recorded-watch ownership, valid video identity, known non-live flags, and non-DAI response eligibility are required. Reels, live/DAI, unknown owners, missing ABIs, and unrelated/cached responses outside the owning controller pass through. The fallback to response filtering is recorded if a requested no-op strategy cannot resolve its flag/config.
- The saved **trigger** choice uses response filtering. Native trigger activation runs unchanged, including exit/expiration/cleanup work. There is no proven instream-only trigger scope in this target.
- `native/RVAdFeed.inc` checks **28 explicit descriptor-backed ad fields** across two renderer wrapper classes. Presence selectors are resolved/ABI-checked once, then dispatched without repeating ABI parsing for every feed item.
- Initial feed models and native `addSectionsFromArray:` insertion are filtered before the original consumer. Changed GPB branches are copied; ordering, unknown fields, tracking, headers, and continuations are preserved. Recursion and work are bounded. An emptied item section collapses only if its complete serialization equals a blank native message, including unknown fields.
- Element compatibility filtering retains the configured positive patterns and adds Android's protected ordinary-video/comment/library contexts. Native empty-element substitution retains a recursion guard and restores it on exceptions. This does not import the broad Android pattern list without structural evidence.
- Shorts entry filtering uses native `YTReelModel.isAdVideo`, preserving unknown/nonvideo models and the existing native nullable entry contract. It does not bypass the sequence response processor or clear its tokens/actions.
- `native/RVAdsDiagnostics.inc` records bounded, redacted checkpoints. A new factory binding always gets a new generation, even when cached response/video identity is reused. Stale clocks/coordinator callbacks cannot mark a newer playback successful. Real native ad state is observed, never forced false.

Background-playback gates remain installed. Ad options retain their existing keys, so saved configurations still load. `response` remains the standard configuration default; the separate native-coordinator package/config selects `coordinator`. Existing saved in-app preferences take precedence over the IPA's bundled values.

## Capture a useful report

1. Sign/install the ad-block IPA. In **Settings → ReVanced → Ads**, confirm **Hide video ads**, **Hide feed ads**, and **Hide Shorts ads** are enabled. To exercise the new coordinator path, select **Video ad strategy → Native ad coordinator** and open a new recorded video.
2. Leave the player visible for about 10 seconds. If an ad plays or playback stalls, capture a screenshot before leaving that page.
3. Open **Settings → ReVanced → Hook diagnostics → Copy ad-block checkpoints**. Confirm revision `adblock-checkpoints-1` and retain the JSON with the screenshot. The full diagnostic report also includes the `adblock` section.
4. For feed failures, copy a report after loading/scrolling the affected feed. For Shorts failures, copy after the affected sequence. Feed/Shorts counters are session totals, while playback events belong to a factory generation. A relaunch provides fresh session totals for a focused capture.

Use `last_visible_watch` for the player observation and confirm `last_visible_watch_matches_current_playback` is true. Opening Settings can cover/detach the watch view; `current_playback` is the report-copy observation. Snapshot age and the last twelve checkpoint changes are included. The build fingerprint identifies the native sources even if an older and newer IPA share a patcher version.

| Report field/counter | Interpretation |
|---|---|
| `hooks` | Boolean installation status per hook; `feed_fields ...` records resolved versus expected descriptor predicates. Missing or mismatched hooks pass through. |
| `first_blocked_checkpoint: factory_binding_missing` | No matching factory binding for current playback; this isolates an ownership/factory interception gap. |
| `eligibility_gate` | `ready` enables recorded-watch policy. Other values explain live/DAI/Reels/unknown-owner passthrough. |
| `requested_strategy`, `actual_strategy` | Shows native, response, coordinator, or failed native selection. State flags identify compatibility fallbacks. |
| `positive_response_ad_input` | At least one nonempty original ad array/count was observed for this binding. Read counters accumulate repeated observations, not unique ads. |
| `events.no_op_flag_read`, `state.native_no_op_selected`, `state.returned_coordinator_class` | Confirms the scoped getter ran and the original factory returned the intended native object. |
| `events.response_ad_items_suppressed` | Positive original array/count observations replaced by empty/zero results. Does not prove serialized/storage consumers are also filtered. |
| `state.preroll_completed`, `state.postroll_completed`, `content_clock_progressed` | Actual native break callbacks and increasing content time; these separate suppression from a stalled content transition. |
| `unexpected_ad_playback` | Native ad state remained true after a suppression decision. |
| `session_events.feed_removed_<field>` | Which explicit typed ads were removed. `feed_append_*` describes insertion input/survivors and original-call completion. |
| `feed_budget_passthrough`, `feed_model_contract_missing` | A bounded traversal or native model contract prevented filtering a branch. |
| `element_pattern_candidate`, `element_protected_context`, `element_empty_substitution`, `element_empty_contract_missing` | Distinguishes a pattern match, a protected containing component, successful substitution, and an unavailable empty renderer. |
| `shorts_entry_seen`, `shorts_ad_skipped`, `shorts_content_surviving`, `shorts_unknown_entry_passthrough` | Distinguishes ad entries skipped from content and unsupported models retained. |

`inconclusive_no_ad_input` is deliberate: neither an installed hook nor a no-op coordinator on an already ad-free response proves that an ad was blocked. `none_detected` means the observed player checkpoints passed; it is not proof of absence of all network requests or all future ad paths.

## Remaining scope

Live/DAI and ads mixed into content streams retain native behavior. Broad Android Elements identifier/path rules, direct Shorts endpoint/opportunity insertions, and additional renderer-replacement commands still need native structural/consumer evidence. Fullscreen interstitials, Premium/merchandise/shopping promotions, popup ads, and paid-promotion disclosure controls have not been added under the general ad switch.

Feed insertion completion is a datasource checkpoint, not a claim that all final layout spacing is correct. If a gap remains, include the screenshot and report. This build does not spoof the iOS client as Android Automotive, replace streams, or submit ad/service votes.

The production build uses the existing signing header reservation. Delivery is unsigned; use the normal SideStore signing/install flow. Build/package evidence is recorded separately, and device validation remains false.

## Historical standalone delivery

Compilation and both unsigned IPA packages succeeded. No tests were run and device validation remains outstanding. The build fingerprint is `32fd0648acddf908a5c8605bc8deb4a90c9c3bff0d27a585d2fb916b61b61217`. The payload reserves 16384 bytes of signing header padding.

Package paths, SHA-256 digests, source hashes, configuration choices, and verification limits are recorded in [the implementation evidence](profiles/adblock-implementation-evidence.json) and the local `build/adblock-receipt.json`. Both packages contain the same native payload; the native package selects `coordinator` and the fallback package selects `response`. Saved preferences can override either bundled choice.

## Merged 0.3.23 delivery

The historical fingerprint above identifies the original standalone ad-block work. Version 0.3.23 integrates those adapters into the released 0.3.22 runtime, preserving the newer speed, RYD normalization, vote lifecycle and first-launch UI policy. The merged element filter still calls `RVVoteNormalizeElementData` for surviving elements. Native ad diagnostics are included in the full report and have a dedicated copy action.

Use `YouTube-21.39.4-RVPort-0.3.23-SideStore-auth-unsigned.ipa` for the response strategy or `YouTube-21.39.4-RVPort-0.3.23-SideStore-auth-native-ads-unsigned.ipa` for the coordinator strategy. Both configs retain the current authentication and first-launch UI preferences. Saved preferences take priority; select a strategy in Settings and open a new recorded video. Merge provenance is in [adblock-merge-0.3.23.json](profiles/adblock-merge-0.3.23.json); the versioned release receipt records the cloud payload fingerprint and package hashes. Compilation and packaging do not establish new ad behavior on a device. No tests were run.
