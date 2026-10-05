# Shorts layout implementation — 0.3.27

Built from the user-confirmed working 0.3.26 Xcode source. [The investigation and scheme](SHORTS_LAYOUT_SCHEME.md) were recorded before implementation. No tests were run. The new feed and navigation behavior needs device confirmation.

## Changes

`native/RVShortsLayout.inc` classifies known native Shorts elements and their enclosing shelves/sections. A generic shelf-header component additionally requires a complete `Shorts`, `FEshorts` or `FEshorts_tab` identity string in its protobuf payload. The identity walk is bounded by depth, field and byte limits. Ordinary-video/comment contexts are protected. Unrecognized templates, malformed payloads and unavailable accessors pass through.

`native/RVAdFeed.inc` removes those rows from native content arrays before initial feed loads, appended sections and section-controller construction. All-Shorts native shelves/sections are removed with their associated title/header, while mixed sections retain other rows. Changed feed models are copied; unrelated metadata is preserved. This operates when **Hide Shorts shelves** is enabled even if feed-ad filtering is off. The element substitution path also recognizes guarded Shorts headers for rendering paths outside model filtering.

Navigation compaction occurs at `YTPivotBarView.setRenderer:`. Native fetch completion stores this same model directly on the controller, so replacing only the items field makes view widths, controller item lookup, logging and selected-tab indices agree. Exact `FEshorts`/`FEshorts_tab` entries are omitted, retaining all remaining entry objects/order and icon-only/Create items. Original membership is retained in an associated lease and restored when **Hide Shorts navigation button** is disabled. If another producer replaces or edits the installed array, the stale lease is discarded rather than restoring obsolete membership.

The old post-layout `UIView.hidden` hook is removed. Native code creates the visible slots and divides the available width by the reduced model count; no manual frame adjustment is used. Selection is restored by native identifier. If Shorts was selected, native navigation selects surviving Home or the first ordinary pivot. A Shorts start-page preference uses Home while that navigation entry is hidden. Existing pivot views update when preferences change; feed content needs a refresh/reload.

## Capture a remaining issue

Enable the existing diagnostics preference, then use **Settings → ReVanced → Hook diagnostics → Copy Shorts layout checkpoints**. The full report also contains `shorts_layout`.

- Expect revision `shorts-layout-1` and patcher version 0.3.27 in the full report.
- `feed_row_removed`, `feed_removed_shorts_section`, `feed_removed_shorts_shelf`, `feed_removed_shorts_header` and `element_empty_shorts_header` distinguish model removal from empty-element substitution.
- `generic_header_preserved` or `protected_parent_preserved` show a candidate kept by the identity/mixed-content guard.
- `pivot_renderer` and `pivot_appearance` report native hook installation. `pivot_compacted`, input/output counts and `visible_native_tabs` show model count and native layout frames.
- `items_contract_missing`, `items_write_not_applied`, `no_surviving_tab`, lease conflict/restoration and selection recovery identify navigation failures.

Reports contain navigation identifiers/geometry and counters; they do not expose feed text, video IDs, navigation commands or account data. A checkpoint is diagnostic evidence, not proof of all device behavior.

## Build and delivery

The cloud workflow uses Apple Xcode 16.4 / iPhoneOS18.5 and targets ARM64 iOS 17, retaining the user-confirmed 0.3.26 compiler process. The release packager requires Xcode provenance and matching native source/payload hashes. It injects the downloaded library into the original analyzed YouTube 21.39.4 IPA with extension removal and both SideStore ad-strategy profiles. IPAs remain unsigned. Archive self-check and all tests remain disabled under the user's instruction.
