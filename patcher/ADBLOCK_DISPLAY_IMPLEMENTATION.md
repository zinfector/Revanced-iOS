# Display-ad implementation â€” 0.3.28

The patcher now filters the proven Home/feed container and below-player companion paths identified in [the scheme](ADBLOCK_DISPLAY_SCHEME.md). These changes use the existing feed-ads switch, now labelled **Hide feed ads and below-video banners**. Video coordinator policy, authentication, playback speed and vote-counter implementations are retained. The merged release also includes native SponsorBlock prompts from the completed 0.3.26 worktree.

## Implemented boundaries

- **Typed feed content:** 52 profile-proven advertising fields across section-list, item-section, grid and horizontal-list unions. Traversal follows eight known container/union classes, including shelf content. It preserves continuation/header/unknown fields, survivor order and original object identity when unchanged. A per-walk memo preserves shared references to changed ancestors. Budgets and cycle checks remain conservative.
- **Late insertion:** native `insertSections:byPosition:error:` and `insertContents:byPosition:inSection:error:` filter their incoming arrays before forwarding to the original implementation. Native position validation, errors and return values remain native. The analyzed native contracts accept empty arrays through their normal append/prepend operations.
- **Legacy companion banners:** the final `YTWatchNextResultsViewController` setter receives nil for a positively identified ad renderer/advertising union. The original setter still runs once to clear companion state, notify observers and update spacing. Unknown renderers and non-main-thread consumers pass through.
- **Below-player Elements ads:** the exact ad adapter establishes provenance. The optional native responder provider resolves the actual companion consumer; its installed hook and the section/view-model contracts must match before interception. A synchronous scope replaces the exact native section-render operation with `clearSection:` and forwards the original provider update with nil. The outer helper, promises and layout delegate remain native. Scope restoration is exception-safe and deferred calls re-evaluate policy when they execute.
- **Replacement updates:** supported nested containers are filtered before native replacement. A replacement whose root itself is an ad remains unchanged until the native remove/undo/result contract is established; it emits `unsupported_replacement`. No fabricated native result or nil replacement is introduced.
- **Layout observation:** companion state, committed/staged section entry counts, and native inset results are observed after clearing. Insets are not overridden. Managed Home slot binding/disposal is counted without changing slot lifecycle decisions.

Removal requires positive renderer identity or exact ad-adapter provenance. Generic headers, Premium offers, donation/ticket shelves and broad image/text component names are not globally hidden. The pre-existing opaque element rules and their ordinary-content protections remain in place, including vote normalization for surviving content.

## Remaining limits

Unknown opaque Home Elements roots, separate generic header/masthead routes, advertising replacement removal with undo, and teardown of an already-bound Home ad slot remain unsupported. These are explicit diagnostic limits. Typed model coverage and installed hooks do not establish which path a specific device banner uses or prove that every display ad is removed.

There are no new per-surface settings in this release: `feed_ads` remains the master for paid feed cards and below-player banners. After toggling it, reopen the video/feed to refresh already loaded content.

## Specific capture checkpoints

The full report and **Copy ad-block diagnostics** contain `adblock.display_ads` (schema 1, checkpoint version 3). The outer ad report is schema 3 / `adblock-checkpoints-3`. Existing video-ad checkpoints remain available.

When a banner survives, capture while its surface is visible, then open ReVanced settings and copy ad-block diagnostics. The report preserves the most recent visible surface before settings covers it, plus the last native operation and up to 12 context transitions. Feed context generations reset on model loads; session totals remain available if a context retires. These are passive diagnostics; opening the report does not force views, layouts or ad requests.

| Stage | Fields/counters to inspect | Interpretation |
| --- | --- | --- |
| Installation | Outer `hooks`, including feed-field resolved/expected counts and companion/insertion hook booleans | Missing ABI or descriptor prevents that route from being intercepted |
| Model traversal | `container_YTIGridRenderer`, `container_YTIHorizontalListRenderer`, `container_YTIShelfRenderer` | Confirms those containers actually reached traversal |
| Classification/removal | `ad_classified`, `removed_entries`, `last_rule`, `input_entries`, `surviving_entries` | Typed sponsored input was identified and excluded; nested counts are aggregated, not unique visible rows |
| Insertion | `original_insert_sections_forwarded`, `original_insert_contents_forwarded`, native result fields | Original mutation received the filtered arrays and returned its native result |
| Legacy companion | `companion_setter_seen`, `companion_argument_cleared`, `companion_state_cleared` | The setter was reached, ad argument cleared, and backing companion state inspected |
| Elements companion | `elements_companion_helper_seen`, `owner_resolved`, `section_contract_ready`, `native_section_clear_called` | Advertising adapter and exact clearing contracts were found |
| Clear completion | `section_entry_count`, `section_staged_entry_count`, `original_provider_update_forwarded_empty`, `original_layout_helper_completed` | Distinguishes section content, retained companion state and original-helper completion; helper completion alone is not proof of a delegate callback |
| Spacing | `native_insets_observed`, `empty_section_insets_zero` | A removed banner may still reserve space; an absent observation is not a zero inset |
| Opaque/late paths | Session `element_pattern_candidate`, `element_empty_substitution`, `managed_slot_bound`, `unsupported_ad_replacement` | Identifies existing compatibility filtering or routes needing further structural mapping |

`first_blocked_checkpoint` distinguishes `descriptor_missing`, `owner_unknown`, `section_contract_missing`, `non_main_consumer`, `clear_not_observed`, `provider_update_unobserved`, `companion_retained`, `reserved_space`, `unsupported_replacement`, and traversal-budget failures. `inconclusive_no_classified_ad` means no positive ad input was recorded for that context; it does not mean ad removal succeeded. `foreground_context_retired` means the preserved snapshot no longer belongs to the current controller generation.

For useful device evidence, include where the banner appears (Home or below the video), whether it appeared immediately or after scrolling/load-more, and whether a miniplayer transition or relaunch preceded it. Reports export no raw protobufs, URLs, video IDs, slot IDs, credentials or pointer addresses.

## Build status

No regression tests, static-hook test, packaged GUI smoke test, or standalone archive test were run, as instructed. The native source is compiled and the IPAs are made using the production patcher and its normal input/payload checks. The release receipt records the exact cloud source commit and artifact hashes. Banner behavior remains device-unverified until the updated IPA is tried.

For the existing native video-ad strategy, use `YouTube-21.39.4-RVPort-0.3.28-SideStore-auth-native-ads-merged-unsigned.ipa`. Sign and install it with SideStore; it retains the expanded authentication preset and removes extensions.
