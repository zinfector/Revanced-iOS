# Queue metadata request context: 0.3.71

Installed 0.3.70 (source fingerprint abd3e98cda8dc23c2a2d03998f38f0f014cff8ebfd63bef3493436368f70bf22) was captured at YTPlayerService.makePlayerRequest and its private metadata error callback. The submitted YTIPlayerRequest serialized to 13 bytes, containing only videoId. Error callback: NSError, com.google.HTTPStatus, code 400. Its userInfo contains 30 bytes of application/x-protobuf error data; the NSData description begins with the message text Precondition. No successful player response reached metadata parsing.

Read-only Ghidra analysis of the original YouTube iOS 21.39.4 ARM64 (UUID 1F61665A-5D4A-3566-8B77-DA821E1D6C17, SHA-256 ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf) establishes:

- 0x101033ec0, makePlayerRequest:responseBlock:errorBlock:, forwards the existing proto to its request factory; it does not supply missing client context.
- 0x1010336e4, requestForPlayerWithPlayerRequest:URLRequestProperties:enablePlayerResponseCacheKeyRelaxation:playerResponseCacheToken:skipInnertubeCacheLookup:, copies the proto, updates content checks, computes the cache key, and wraps it for transport. It reads existing context rather than building one.
- 0x101033190, innerTubeRequestForPlayerWithVideoID:playlistID:playlistIndex:playbackContext:forOffline:loggingContext:playerParams:proofOfOriginToken:, actually returns YTIPlayerRequest. It supplies active identity/page/incognito and native context provider, client/device context, native content-check defaults, optional playback decorators and optional integrity token.
- 0x101033d38, YTPlayerService.init, obtains that native factory. Runtime ivar metadata confirms _requestFactory, type @"<YTPlayerRequestFactory>", stored factory YTPlayerRequestFactoryImpl.

The patch now obtains this factory through named, type-checked Objective-C ivar metadata and calls its builder with the target video ID, no playlist, index zero, no playback context, forOffline false, and nil logging/player parameters/token. The exact 64-bit/BOOL method ABI is checked before calling. The returned proto must have the same video ID and a present YTIClientInfo context. There is no videoId-only fallback.

Sending, callback handling, queue row mapping, owner checks and the private response-action suppression are inherited from 0.3.70. Diagnostics now distinguish local request construction failures from native errors, retaining only the numeric NSError code and an HTTP-domain flag. No response bodies or account tokens are logged by the patch.

Evidence supports an incomplete request as the cause of this observed rejection. The backend error does not identify a specific missing field. Whether the repaired request succeeds for the account/video and whether subsequent queue insertion succeeds remain unverified until installed-device observation.

Build: established GitHub macOS-15 / Xcode 16.4 workflow, separate unsigned 0.3.71 IPA. No tests, regression harnesses or archive self-checks are run, per user instruction. Production source/payload/input identity checks remain enabled. Runtime captures stay in the local workspace and are not committed.
