# Home and below-player display-ad extension scheme

Status: researched design, 2026-10-05. Target: YouTube 21.39.4, released patcher 0.3.24 at `4469c1982dd9ffec0bea6dadac28ecde30901b77`. This document adds no runtime hooks and builds no new IPA. No tests were run, as requested.

The user reports that video ads appear to be gone and that banners remain below videos and on Home. Static analysis confirms independent display-ad paths that the released filter does not cover. It does not establish which of these paths produced a particular banner on the device. The implementation should record that distinction instead of treating a discovered class as a device-confirmed route.

Evidence: [method metadata and findings](profiles/adblock-display-evidence.json), [56 annotated native method decompilations](profiles/adblock-display-decompiled.txt), and [20 decoded protobuf field tables](profiles/adblock-display-protobuf-fields.json). Analysis used the existing Ghidra project read-only, without reanalysis. Addresses are research provenance, not runtime hook locations; hooks must resolve classes/selectors and check their actual method encodings. Executable SHA-256: `ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf`.

## Confirmed gaps

| Surface/path | Released coverage | Confirmed bypass | Proposed boundary |
| --- | --- | --- | --- |
| Below-player companion | Three player-ad arrays and typed feed fields | Separate `playerCompanionAd`, WatchNext companion payloads, and a native companion setter | Typed payload classification plus original setter with an empty ad argument |
| Below-player Elements banner | Raw `elementData` matching and feed load hooks | Ad adapter renders directly into an array section, then independently updates companion state | Scoped native section clear and companion-state update |
| Home grid/carousel/shelf | Section-list and item-section contents | Promoted grid/horizontal-list fields and shelf nesting are not traversed | Descriptor-backed copy-on-change traversal |
| Later feed updates | Model load and `addSectionsFromArray:` | Insert and replace commands mutate models directly | Filter before each proven native mutation |
| Modern opaque Elements cards | Three byte patterns with protected contexts | No per-cell root identity or advertising provenance | Observe identity/provenance, then apply a narrowly supported removal rule |

The current typed filter has 28 advertising fields across two renderer unions. It visits `YTISectionListRenderer.contentsArray` and `YTIItemSectionRenderer.contentsArray`, but not grid items, horizontal-list items, shelf content, or separate headers. Its opaque compatibility patterns are `text_search_ad`, `ads_video_with_context`, and `carousel_ad`. Widening a substring list alone would leave the direct companion paths untouched and could blank ordinary Elements content.

## What Android contributes

ReVanced's local `AdsFilter.java` identifies ad components using Litho identifier/path context. Its general-ad rules cover carousel ads, ad-with-context cards, brand shelves, image/text/button ad layouts, and watch app promotions. It explicitly protects ordinary home/related videos, comments, and recent-library shelves.

Other categories have stronger conditions: a Premium statement banner requires a root match and growth-promotion evidence; a movie purchase card requires storefront evidence. Shopping, merchandise, paid-promotion disclosures, and fullscreen interstitials have separate handling. These distinctions should carry over to iOS policy, while native iOS ownership and renderer identity supply the classification evidence. Broad Android layout substrings are not a safe replacement for that evidence in opaque iOS protobuf data.

Keep paid display ads enabled for removal under the existing `feed_ads` policy. Proposed settings can subdivide Home/Search sponsored cards and below-player banners. Optional Premium offers, creator merchandise/shopping, movie purchases, and paid-promotion disclosures should remain separate choices, initially off. A channel header, comments header, donation/ticket companion, or generic banner name is not sufficient ad evidence.

## Below-player implementation

### Legacy companion setter

The proven chain is:

`YTBelowPlayerCompanionRenderingAPIImpl startCompanion:withLayoutID:interactionLoggingAdsClientData:` → `YTWatchController startCompanion:withLayoutID:interactionLoggingAdsClientData:` → `loadCompanionAd:layoutID:interactionLoggingAdsClientData:` → `YTWatchNextResultsViewController setCompanionAd:layoutID:interactionLoggingAdsClientData:`.

The final setter stores `_companionAd`, notifies observers, updates the content inset, and scrolls toward a nonempty companion. Its nil branch clears the layout ID and avoids ad scrolling. `YTCompanionAdObserverBehavior companionAdDidChange:interactionLoggingAdsClientData:` clears existing entries when the ad changes and publishes staged changes. This supplies a native clearing contract.

Install an ABI-checked `v@:@@@` hook on the final setter. For a positively classified advertising payload belonging to the current watch context, call its original implementation exactly once with nil as the companion argument. Preserve the other arguments unless further native evidence requires changing them. Preserve ordinary and unknown payloads unchanged. Cover the simpler `setCompanionAd:` forwarding route without causing duplicate original calls.

Do not use `hideCompanionAd`: it scrolls away from a visible companion; it does not clear its model. Do not short-circuit the surface API's pending promise or adapter `startRendering`: those paths also complete native layout lifecycle callbacks. Classify again when a deferred callback reaches the consumer, using the current watch generation.

### Direct Elements section

`YTAdBelowPlayerLayoutRenderingAdapter startRendering` obtains an Elements renderer from ad layout metadata and waits for the companion section if necessary. Its `renderCompanionSection:withRenderer:` helper calls three independent operations:

1. `+[YTAdElementsSectionViewRenderingAPI renderSection:withRenderer:]`.
2. The provider's `didUpdateBelowPlayerLayoutWithRenderer:`.
3. The delegate's `layoutRenderingAdapter:didEnterLayoutForSlot:layout:`.

The class rendering API clears and appends entries in a `YTArraySectionViewModel`. Its separate `+clearSection:` clears entries and pushes staged changes without appending a nil entry. The provider update independently stores `_companionAd`, notifies observers other than `YTCompanionAdObserverBehavior`, updates insets, and scrolls for a nonempty renderer.

Use a synchronous, thread-scoped decision around the original adapter helper. The scope records the exact adapter, current watch context, section, and renderer identities. Within that scope:

- Replace the advertising `+renderSection:withRenderer:` operation with native `+clearSection:` on that exact section.
- Pass nil to the original `didUpdateBelowPlayerLayoutWithRenderer:` for the exact blocked renderer, so companion state and additional observers do not retain the banner.
- Allow the original outer helper to complete its delegate/lifecycle work.

This interception must not affect unrelated callers of the class rendering API. The original helper still runs once, with exception-safe scope cleanup. Async retries must create a fresh scope after validating the current owner; a thread-local decision cannot outlive the synchronous invocation. Native view/model writes stay on the main thread.

The researched companion section reports zero insets when empty and no bottom separator. Observe the resulting empty entry count and geometry; do not globally override insets or force layout. If space remains, record the owning section and inset checkpoint before changing its layout contract.

### Typed payload coverage

Add descriptor-validated classification for:

- `YTIPlayerResponse.playerCompanionAd` and its `YTIPlayerCompanionAdsSupportedRenderers` union.
- `YTIWatchNextResponse.companionAds`, `contentVideoCompanionAds`, and positively advertising panel/layout fields.
- Known advertising companion union members, including app-promo, shopping, compact, multi-item, suggested-video and below-player ad layouts, with category policy applied separately.

An `elementRenderer` becomes positively advertising when its exact container/ad adapter supplies advertising provenance. It is not advertising merely because it is an Elements renderer. `YTIRenderer` has context plus an extension range in its descriptor; guessed direct accessors such as `inFeedAdLayoutRenderer` are not established. Use a proven extension descriptor/provider or leave that generic payload unchanged.

WatchNext ownership is separate from the current raw player-response lease. Do not compare a WatchNext response pointer against the player-response lease and declare it owned. Bind it to its actual watch/feed consumer context.

## Home and feed implementation

### Expand supported typed containers

Extend the existing traversal with a descriptor-backed whitelist:

- `YTIGridRenderer.itemsArray` → `YTIGridSupportedRenderers`: remove positively advertising grid-promoted video/banner and promoted Sparkles fields.
- `YTIHorizontalListRenderer.itemsArray` → `YTIHorizontalListSupportedRenderers`: apply its confirmed promoted/ad-slot fields.
- `YTIShelfRenderer.content` → `YTIShelfSupportedRenderers`: recurse into the confirmed grid and horizontal-list members.

Retain original survivor order, pagination/continuation tokens, unknown protobuf fields, and unchanged object identity. Copy only modified ancestors. Retain the existing cycle guard, depth limit, and work budget. Collapse an empty shell only when it contains no functional header, continuation, command, or other meaningful field. A decoded vertical/expanded shelf name is not enough: those member contracts still need research, so pass them through initially.

Headers and content overlays are separate surfaces. `setupHeaderWithModel:...` confirms that an Elements header can be instantiated independently of contents, but normal chip/search/comments headers use the same boundary. Observe its own identity before adding a targeted header rule. A masthead-ad class exists; whether it produced the user's Home banner remains unverified.

### Cover native command mutations

The following paths bypass current load/append hooks:

| Method | ABI | Contract to preserve |
| --- | --- | --- |
| `insertSections:byPosition:error:` | `B@:@i^@` | Filters incoming section array; original handles prepend/append, validation, error output, and result |
| `insertContents:byPosition:inSection:error:` | `@@:@i@^@` | Filters incoming content array; original updates the native feed model and returns its native result |
| `replaceEntry:withReplacementRenderer:inSectionController:undoKey:operation:` | `@@:@@@@@` | Native replacement, undo bookkeeping and mutation result |

Filter arrays before native mutation, preserving position/error/return semantics. Establish the empty-array contract before suppressing an all-ad command. For replacement, trace the native remove-entry and undo/result contract before replacing an ad with removal; blindly passing nil or returning fabricated success is not justified. Initially observe unsupported replacement cases and report them as uncovered.

Do not drop a section controller after its renderer is already committed to `_sectionRenderers`. Native construction/reuse maps are keyed by renderer identity; late controller omission could desynchronize model and displayed indices.

### Modern Home Elements and managed slots

Add observation at proven ad-metadata providers and cell/root identity boundaries. Record a bounded rule/class enum and context ownership, not raw serialized data. Classify only a cell's own advertising root or exact ad provenance, with protected ordinary-content contexts checked first. Remove through a supported model boundary or proven native empty-renderer contract; do not return nil from an opaque materializer whose C++/Objective-C return ABI is unknown.

The managed-slot code rules out one tempting shortcut: `didEnterExternallyManagedSlot` logs entry before validation, and `didBindSlotData:layoutData:` continues layout notifications afterward. Returning NO from `canEnterSlot:` alone does not cancel all rendering and could split lifecycle state. Prefer removing the ad before binding. Already-bound slot teardown requires additional proof of the native exit/unschedule/dispose sequence; use passive bind/dispose counters until then.

## Context, settings and lifecycle

Home filtering must work without an active video. Associate decisions with the actual feed controller/page and model revision; watch decisions use the watch owner/generation. Keep pointer identities internal, weak where appropriate, and invalidate associations when native ownership changes. Re-evaluate deferred callbacks and replacement models under current settings. Unknown ownership, missing descriptors, unsupported ABI or exhausted traversal budget pass content through and produce a specific diagnostic reason.

Do not modify the working video-ad coordinator to implement display filtering. Keep vote normalization and SponsorBlock on ordinary content paths. A display-ad decision must never remove an entire parent element merely because it includes a like/dislike control or another nested ordinary component.

## Proposed diagnostic checkpoints

Add `adblock.display_ads` schema 1 with display checkpoint version 3. Proposed fields distinguish each stage; these are implementation requirements, not counters already available in 0.3.24.

| Checkpoint | Evidence exported | Failure meaning |
| --- | --- | --- |
| `hooks_ready` | Build fingerprint; hook/descriptor names, ABI matches | Selected implementation is missing or incompatible |
| `surface_seen` | Home/Watch/Search enum; owning context present/current; native route/class enum | Banner route never reached or owner could not be resolved |
| `classified` | Known rule/category, protected-context flag, policy decision | Payload is unknown, protected, or disabled by policy |
| `mutation_forwarded` | Input/matched/removed/survivor counts; original call count; insert/replace/setter route | Decision was made but did not reach a supported mutation |
| `native_clear_observed` | Exact section matched; native clear called; current model entry count | Native consumer retained advertising content |
| `state_cleared` | Companion nil, notification observed, current generation | A secondary provider/observer restored the banner |
| `layout_settled` | Empty section/inset status from passive native layout completion | Content was removed but space remains |
| `lifecycle_preserved` | Original enter/exit and bind/dispose counts by current context | Rendering and lifecycle notifications diverged |

Include counters by legacy companion, Elements companion, grid, horizontal list, shelf and command mutation. Use a `first_blocked_checkpoint` enum such as `policy_off`, `owner_unknown`, `descriptor_missing`, `unknown_ad_root`, `unsupported_replacement`, `budget_exhausted`, `clear_not_observed`, `companion_retained`, `reserved_space`, or `callback_unobserved`. Observed zero and unobserved must remain distinct.

Capture the most recent foreground surface snapshot before diagnostics/settings cover it. Include at most 12 recent context transitions with relative times and generation-current flags, so first launch, later launch, continuation loading and miniplayer return can be distinguished. Export no URLs, raw protobufs, video IDs, cookies, tracking/slot IDs, tokens or credentials. Diagnostics remain passive and do not force view creation, layout, network fetches or gate behavior. Installed hooks and matching counters do not constitute device verification.

## Delivery sequence

1. Implement watch setter/Elements clearing, typed grid/horizontal/shelf traversal, and the proposed checkpoints together. Leave unsupported opaque roots and replacements explicitly reported.
2. Add late insertion filtering once its empty-array contract is traced. Complete replacement removal/undo research, then implement that path.
3. Use a future device report for Home and below-player banners to identify remaining Elements/header/masthead roots. Add narrowly justified rules and managed-slot teardown only where evidence supports them.

Useful future device observations are Home initial load and load-more, below-player banners, miniplayer down/up, and the first launch versus a subsequent launch. The report should identify the last successful checkpoint rather than requiring another blind patch. No tests or new IPA were produced for this scheme.
