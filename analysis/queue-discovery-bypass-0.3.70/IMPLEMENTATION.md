# Queue discovery bypass 0.3.70

Both original next/last discovery requests captured from installed 0.3.69 returned a GetActiveDevices response containing onResponseReceivedCommand.browseEndpoint with browseId SPunlimited and canonicalBaseUrl /premium. The functional queue command is not a second command in those responses. This release bypasses that request for the strictly recognized local queue token and runs the native queue adapter instead.

The command router, MDX handler and native discovery-manager entry now consume recognized local queue operations. Failure to initialize, retrieve metadata or insert does not fall through to the original request. Other discovery shapes and active casting sessions retain their native routes. The visible legacy menu prefers the target and next/last operation in its original token, rather than selecting an unrelated video from a source binding. All recognized entry points use the same native Watch startup/insertion action.

Queue rows now copy a matching full YTIPlaylistPanelVideoRenderer when available. Known feed wrappers are unwrapped; headline, inline nested metadata, bylines, duration and overlays are mapped by type. Current-row seeding reads matching current player details. The raw-video-ID title fallback is removed.

An opaque source with no display model obtains YTIVideoDetails through a separate native YTPlayerService instance. A class-local subclass override ignores handleActionsFromPlayerResponse only for that metadata instance: the normal player service is unaffected. No stream is played by this metadata request. A target-ID match, title and thumbnail are required before insertion; a single pending action has a 12-second deadline and owner/account change checks. Metadata failure reports an error instead of reopening acquisition or inserting an ID-only row.

Native queue startup, ownership, insertion notifications and engagement-panel presentation are inherited from 0.3.69. The queue remains owned by YTQueueController with no parallel item array. Existing rows made by an older installed patch are not retroactively hydrated.

The static native basis is original YouTube iOS 21.39.4 ARM64, UUID 1F61665A-5D4A-3566-8B77-DA821E1D6C17, SHA-256 ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf. Relevant unslid methods:

- 0x103687918: requestScreenDiscoveryForPollingStrategies:.
- 0x100f51cb0 / 0x100f51ea0: GetActiveDevices construction / response-command dispatch.
- 0x101033ec0 / 0x101034058: native Player request / response callback; the latter returns YTPlayerResponse and separately invokes handleActionsFromPlayerResponse:.
- 0x101034a10: normal Player response-action forwarding, omitted only in the private metadata subclass.
- 0x101748360: native queue mutation handler.

Runtime response captures, exact payloads and local Ghidra exports remain in the workspace investigation at ReVanced/analysis/queue-pre-tap-eligibility. Account-specific payloads are not included in this source branch.

Delivery uses the established GitHub macOS/Xcode16.4 compiler workflow and produces a separate unsigned 0.3.70 IPA. Tests, regression harnesses, static-hook tests and archive self-tests are skipped at the user's explicit instruction. Compiler/provenance and production package identity checks remain enabled. This new implementation has not been validated on an installed device.
