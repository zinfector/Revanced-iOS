# Shorts shelves and navigation layout — investigation and scheme

Baseline: the user-confirmed working 0.3.26 Xcode delivery. Investigate first, record this scheme, then implement. No tests are requested or run.

## Why the gaps occur

`RVAdFeed.inc` replaces matched `YTIElementRenderer.elementData` with an empty cell. The default Shorts patterns identify shelf/card components but do not identify the separate generic shelf-header component. Moreover, `RVFilterModel` currently returns immediately when feed-ad filtering is disabled, independently of the Shorts preference. Empty child cells do not remove native enclosing shelf titles or section headers.

`RVExtras.inc` hides the Shorts item view in `YTPivotBarViewController.viewDidAppear:` after YouTube has built the toolbar. Native `YTPivotBarView.updateTabWidths` divides the available width by `_renderer.itemsArray.count`. Merely changing `UIView.hidden` does not change that denominator. `setRenderer:` builds six reusable item slots in renderer order, showing only slots assigned a current item; native layout can compact the slots correctly if its model has fewer entries.

## Evidence

Read-only Ghidra inspection reused the supplied project with `-readOnly -noanalysis`: 24 named methods, then the pivot fetch-completion block and `loadView`. Executable SHA-256: `ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf`. Evidence is in `ReVanced/patcher/build/shorts-layout-investigation` and `shorts-layout-followup`. Protobuf descriptor field tables corroborate `YTIPivotBarRenderer.itemsArray`, supported-item renderer variants, `pivotIdentifier`, native shelf content/title/header fields and section contents.

The fetch-completion block writes the controller's `_renderer` directly, then passes that same renderer to `YTPivotBarView.setRenderer:` and later uses it for logging, item identifiers and selection. Hooking only the controller property setter would miss this path; giving the view a filtered copy alone would misalign controller indices. Therefore the shared renderer's items field must be updated at the view's renderer boundary so all native consumers observe the same order/count.

Android `ShortsFilter.java` distinguishes Shorts identifiers from `shelf_header.e`, with a separate Shorts-content check for that reused header. The iOS adapter should use that identity rule, rather than hide every header or match a localized label alone.

## Implementation scheme

1. Introduce a bounded Shorts classifier. Strong Shorts shelf/card identifiers retain the configured pattern behavior; generic shelf headers require a header-component marker plus a complete Shorts identity string in their protobuf payload. Preserve enclosing ordinary-video/comment contexts. Native wrapper traversal follows only descriptor-proven present fields and known classes, never arbitrary object graphs or absent protobuf children.
2. Remove classified rows from section content arrays before native initial loads, section appends and section-controller construction. Remove an enclosing native shelf/section with its header only when its children prove it is entirely Shorts. Copy changed feed models and preserve unrelated rows/metadata. Activate this path independently of `feed_ads`; retain the guarded element empty-cell substitution for rendering paths outside collection-model filtering.
3. Remove only pivot entries with exact native Shorts identifiers (`FEshorts`, `FEshorts_tab`) before `YTPivotBarView.setRenderer:`. Preserve icon-only/Create and all other entries in their original order. Retain an original-items lease on that renderer and replace only its items field; every native consumer, including count-based layout, then sees the compact model. Restore only a still-owned lease when the setting is disabled; discard stale leases if another producer edits the array.
4. Remove the old post-layout hide hook. Let native `setRenderer:`, width calculation, layout, safe areas, RTL, frosted backgrounds and hit targets operate on the remaining model. Reapply native visual selection after a rebuild. If the removed tab was selected, use native selection of the surviving Home tab (or first normal navigation tab). Prevent a configured Shorts start page from selecting an omitted tab.
5. Refresh existing pivot views on a relevant preference change and preserve selection for other tabs. Feed changes apply to the next refresh/reload. Record hook installation, classified/removal counts, pivot input/output counts, lease restoration/conflicts and selected-tab recovery in a dedicated Shorts checkpoint report.
6. Compile on macOS with Xcode 16.4 / iPhoneOS18.5, targeting ARM64 iOS 17; package the downloaded native artifact on Windows. Reject Zig release payloads and keep tests/archive self-check disabled under the user's instruction. Deliver distinct 0.3.27 IPAs, source and receipts.

Unknown template variants and unavailable native contracts pass through and are reported. The new layout behavior requires device confirmation; the working compiler pipeline is retained.
