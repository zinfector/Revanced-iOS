# Native collapsed queue startup: 0.3.72

The installed 0.3.71 cold-queue path used beginPlaybackOnMinibar YES and pausePlaybackWhenVideoActivates YES. Read-only USB captures showed that the native minibar_delayed_player configuration deliberately left the miniplayer in activation state 2 without a player. Watch preparation and response delivery were deferred until a tap, but adapter seeding required that response within ten seconds. A captured close-and-requeue attempt created a fresh native owner and loaded metadata successfully, then hit the same bootstrap timeout.

## Change

RVQueueStartNativeWatch now checks the three BOOL setter ABIs and sets beginPlaybackOnMinibar NO, prefersPlaybackLayoutStateCollapsed YES, pausePlaybackWhenVideoActivates NO. The native screenDidLoadWithTransition branch at 0x10134b210 therefore opens collapsed playback with shouldAcquirePlayer YES, through native acquisition at 0x10134f800 / 0x10134f2e8 / 0x10134b8b8. It preserves YouTube's inline-player handoff rather than invoking a separate player or disabling deferred-player configuration globally.

The pending action still requires a matching target response and coherent Watch/panel/queue relationship before seeding once. Its fallback current video now comes from that Watch's WatchPlaybackController.contentVideoID, which forwards to its own player (0x10136ea4c). It does not use a feed/inline playback global to identify current Watch content. Owner resolution skips stale candidates whose native Watch layer names another current Watch controller.

## Lifecycle

Named native clearWatchController and didDismissMiniBar methods have guarded void ABI hooks. A tracked Watch clear invalidates adapter presentation, native ownership, bound references and metadata generation. An in-progress bootstrap may legitimately replace an old Watch, so it retains its pending target/deadline and weak layer identity through that handoff. Genuine minibar dismissal cancels pending bootstrap as well. Cleanup never clears the native queue or changes unrelated player globals; native teardown remains responsible for those objects.

Queue owner replacement drops old presentation/cache references and cancels metadata callbacks. Account replacement cancels pending bootstrap too. The panel constructor binds the queue before publishing its panel so invalidation cannot erase the new panel. Explicit queue clear cancels metadata callbacks that could otherwise insert after a user's clear, while retaining the current Watch response for re-seeding a queue alongside a still-loaded video. Cancellation is logical: any already issued native metadata request may finish, but its old generation cannot mutate the new session.

Diagnostics add native_bootstrap_mode, native_session_resets, native_bootstrap_cancelled and queue_metadata_cancelled; the queue revision is native-collapsed-start-8. No video IDs or request payloads are added to logs.

## Evidence and boundaries

Native identity: original YouTube iOS 21.39.4 ARM64, UUID 1F61665A-5D4A-3566-8B77-DA821E1D6C17, SHA-256 ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf. The local workspace's queue-miniplayer-initialization-0.3.71 report retains before/tap/closed/requeue snapshots, native decompilation and timeout report. This source branch contains no runtime account-specific payloads.

Ghidra selector callers establish that didDismissMiniBar is sent by didDismissMiniplayer. clearWatchController also has collapse and navigation callers, so its cleanup preserves an existing bootstrap instead of treating every native controller replacement as a user cancellation.

The native metadata factory fix from 0.3.71, strict queue-token routing, queue insertion, native panel presentation, single pending action and existing deadlines are retained. This release remains unverified on an installed device. It does not claim that restart behavior was reproduced or that all native navigation variants have been observed.

Build: established GitHub macOS-15 / Xcode 16.4 workflow. Tests, regression harnesses, static-hook tests and archive self-checks are skipped per explicit user instruction. Compiler warnings are errors; normal production input, source, payload and packaging identity guards remain enabled. A separate unsigned 0.3.72 IPA is produced and prior IPAs are preserved.
