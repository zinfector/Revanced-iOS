# Coordinator replacement throughout the playback lifecycle

Implemented in 0.3.24: [runtime paths, limits and checkpoint guide](ADBLOCK_HANDOFF_IMPLEMENTATION.md). The original research/design record follows.

Status: implementation scheme for YouTube 21.39.4, based on read-only native research and Adblock Report 1251. No runtime changes or new IPA are delivered by this document. No tests were run.

The design accepts an ad-policy request at any time and replaces the actual controller-owned coordinator when the native playback lifecycle permits it. Initial selection, cached reuse, and internal transitions need separate handling. A request during an active ad or an unverified transition remains pending; arbitrary mutation in the middle of an ad is not established as safe by the current evidence.

## Confirmed ownership and lifecycle

The supplied binary's Objective-C metadata identifies `YTLocalPlaybackController._adsPlaybackCoordinator` as an object ivar with encoding `@"<YTAdsPlaybackCoordinator>"`. The decoded ARC strong-ivar layout includes this slot, establishing strong ownership in the analyzed binary. Its analyzed offset is 496, but the implementation must resolve the ivar by name at runtime. The offset and function addresses in the research evidence are provenance, not runtime constants.

The newly decompiled native methods establish these facts:

| Native method | Observed behavior | Design consequence |
|---|---|---|
| Controller initializer | Can eagerly call `createAdsPlaybackCoordinator` before a content response is available, and stores the returned object in the coordinator ivar. | A missing response at creation is a waiting state, not a permanent binding. |
| `startPlaybackWithVideoResolver:` | Stores content playback data, calls `adsPlaybackCoordinatorForNewPlayback`, stores the returned object, sets overlay lifecycle behavior, and supplies the content video and timeline. | Intercept selection before the native assignment; let the original caller retain, store, and initialize the replacement. |
| `adsPlaybackCoordinatorForNewPlayback` | Checks the cached coordinator's `supportsPlaybackForContentPlayerResponse:`. It returns that cached object or calls `createAdsPlaybackCoordinator`. | Apply policy around the reuse check as well as around factory creation. |
| `adsPlaybackCoordinator` | Reads the stored ivar. Several other methods read the same field directly. | A getter-only override leaves direct consumers using the old coordinator. |
| `videoSequencer:willTransitionToNextContentSequenceWithPlayerTransition:playbackConfig:playbackData:` | Calls `resetWithCurrentVideoSequencer`, installs the incoming playback data, and may notify the existing coordinator of autoplay. It does not invoke new-playback coordinator selection. | Internal transitions need an additional replacement opportunity. |
| `resetWithCurrentVideoSequencer` | Calls the existing coordinator's `reset` only under native flag conditions, clears content playback data, and clears active video references. | Do not assume every transition has cleaned up the old coordinator. Observe actual reset entry/return. |
| `didTransitionToContentSequenceForVideoSequencer:` | Supplies the new content video and timeline directly to the stored coordinator, then reports loaded/activated content to the player delegate. | Complete a verified transition replacement before the original method reads the field. |
| Gapless coordinator `reset` | Removes ad-video observers when registered, stops observing content, cleans timeline/surface state, and clears playback caches/state. | Cleanup is more than releasing the object. Preserve the native reset path. |
| No-op coordinator | `supportsPlaybackForContentPlayerResponse:` returns NO in this binary; `reset` is empty. Native preroll/postroll methods call the original controller delegate with break types 1 and 3. | Do not invent a YES reuse result or synthetic completion events. Preserve native behavior and original delegate identity. |

Report 1251 matches the merged 0.3.23 payload fingerprint. The factory hook ran three times, the selected strategy was coordinator, the latest eligibility gate was `response_unavailable`, and the returned coordinator was `YTGaplessPlaybackCoordinator`. One companion feed ad was removed. The report does not distinguish a null response argument from a failed response class/accessor check. The lifecycle gaps above are confirmed statically; their exact contribution to this device observation still needs the more specific checkpoints below.

## 1. Per-controller ownership and requests

Introduce `RVAdCoordinatorLease`, associated with each typed local playback controller. It stores a weak controller reference, a generation token, desired policy, exact response wrapper/raw identities, weak current coordinator identity, pending reason, and a bounded diagnostic history. Hold candidate and retiring objects strongly only during the handoff transaction.

Resolve `_adsPlaybackCoordinator` on the known controller class with `class_getInstanceVariable`. Require the expected object encoding and exact analyzed app profile. Read with `object_getIvar`; compare its identity with the ABI-checked native `adsPlaybackCoordinator` getter. A mismatch blocks replacement and is recorded. Never patch a raw address, vtable, global services pointer, or ivar offset.

`RVAdRequestCoordinatorPolicy(controller, reason)` may run after settings changes, response arrival, a new content generation, or visible-watch observation. It coalesces repeated requests. Background callers enqueue evaluation on the main queue. The playback clock can request evaluation but cannot allocate, reset, or replace coordinators on each tick.

A generation starts at a content-selection/transition boundary, not at every factory invocation. A factory can occur during controller construction, before content exists. Repeated requests for one generation do not create new objects. Controller identity plus generation and exact response identity prevent an older request from modifying a newly selected video; CPN is an additional check once available, never included verbatim in diagnostics.

The evaluator checks video identity, response ownership, recorded-watch ancestry, non-live/non-DAI flags, desired strategy, native ABI availability, main-thread ownership and handoff phase. `response` and trigger-fallback strategies retain a native coordinator and use the owned response policy. Only the coordinator strategy requests the native no-op coordinator. Disabled features and unsupported/live/DAI/Reels contexts keep native selection.

## 2. Initial playback and cached coordinator reuse

Hook `YTLocalPlaybackController adsPlaybackCoordinatorForNewPlayback` with ABI `@@:`. At entry, obtain the controller's current `contentPlaybackData.playerResponse` and resolve `playerData` using typed, ABI-checked accessors. Record every failed step separately. Build a scoped immutable decision for that exact controller, response, and cached coordinator.

Call the original selection method exactly once under this scope. For an eligible coordinator request, scoped hooks on supported cached coordinator classes can return NO from `supportsPlaybackForContentPlayerResponse:` for the exact cached object and exact response argument. All unrelated receivers/arguments call their originals. This deliberately takes the native recreation branch rather than returning a newly allocated object behind the controller's back.

The existing services-factory hook must use the same lease/decision instead of replacing its state with a new factory-only generation. Its exact `iosPlayerConfig.useNoOpAdsCoordinator` scope stays active through the original factory call. The original factory constructs `YTNoOpAdsPlaybackCoordinator` with the original service registry and controller delegate. Do not instantiate a generic substitute or return nil.

Restore all thread-local scopes with `@finally`, including nested calls. Record requested selection and returned class. The original `startPlaybackWithVideoResolver:` caller then stores and initializes the object. At its exit, observe the named ivar and require it to equal the selected object before reporting `pointer_installed`. Merely returning a no-op object is not an installation checkpoint.

Preserve the native NO compatibility result on an existing no-op coordinator. When the policy is disabled or changes to live/DAI/unknown content, never force its reuse: call native selection without the no-op flag override and observe the resulting native object. Internal transitions need the separate path below because they bypass this selector entirely.

## 3. Internal transitions and late binding

Observe native controller reset/transition methods and the known coordinators' `reset` methods. Track entry, return, nesting and exact receiver identity. A native reset call is a cleanup observation, not proof that every future asynchronous callback has vanished. Retain the old coordinator locally during the handoff and require no active ad and an unchanged generation/pointer.

After the original `willTransition...` method returns, the incoming playback data is installed. Mark a replacement request. At entry to the original `didTransition...` method, the new video has not yet been supplied to the stored coordinator. This is the proposed commit boundary. Before enabling it, require a completed reset for the exact old coordinator in this transition, no active ad, unchanged incoming response, correct thread, and ABI matches for every initialization call. If native flags skipped reset, retain the native object and report `transition_cleanup_unproven`; do not infer safety from a successful method return alone.

Construct a candidate through the original `createAdsPlaybackCoordinator`/services factory with the exact incoming response and scoped decision. Verify the candidate class, original delegate/service ownership, and current lease. Replay native overlay lifecycle initialization only through proven ABI-checked methods. If the outgoing coordinator received the optional `willPlayNewVideoWithAutoplay:` notification, deliver that notification to the replacement once using the incoming native transition's autoplay value. Do not silently omit that lifecycle behavior.

Commit using the named object ivar, with ownership-aware runtime assignment, only after the full preflight. A direct assignment fallback must validate ARC strong/weak layout information and the analyzed strong ownership before using `object_setIvarWithStrongDefault`; its stronger default is not a substitute for proving ownership. No `memcpy`, integer address arithmetic, or unretained pointer stores.

Keep this commit synchronous on the native playback thread, with no asynchronous work between the final identity check and assignment. A nesting guard prevents reentry. Re-read the ivar and native getter. Then let the original `didTransition...` method provide the content video/timeline and native load/activation notifications to the installed coordinator. Do not call native methods while holding the diagnostic lock.

Once the candidate is published, if an unexpected reentrant native action changes ownership, stop overriding and record the mismatch. Do not blindly restore the retired coordinator after side effects or start a second initialization sequence. Native exception propagation is preserved; cleanup scopes and temporary references are released in `@finally`.

For a response arriving late outside these proven boundaries, record a pending request and reevaluate at the next selection/reset/transition. Active-content swapping is a separate capability: it would also require preserving or rebuilding timeline registrations, pending ad interrupts, overlay lifecycle, and already-started break state. Those conditions are not yet established by this research. The first implementation must report `deferred_active_playback` or `deferred_active_ad`, rather than claiming an arbitrary-time pointer write is safe. A user reopening the video gives a native new-playback boundary.

## 4. State machine and callback ownership

```text
unbound -> waiting_for_response -> eligible -> pending_handoff
pending_handoff -> selected_by_native_caller -> installed -> content_progressing
pending_handoff -> cleaned_transition -> candidate_ready -> installed
pending_handoff -> deferred_active_ad / cleanup_unproven / unsupported
any state -> generation_changed -> discard_stale_request
```

Pre/postroll completion still comes from the installed native coordinator's original methods. Do not manually emit break-finished events to compensate for a blocked stage. The controller's stored pointer must equal the completion source. During native reset, preserve legitimate original callbacks. After retirement, record old coordinator callbacks separately; any suppression requires proof that the specific callback is stale and must not advance current playback. Do not install a broad callback dropper as part of this scheme.

The replacement request can be made at any time. The ability to commit remains phase-dependent. Existing login, SponsorBlock, speed, vote presentation and settings hooks stay intact; the new observer hooks chain original calls, never replace the shared clock/session implementations.

## 5. Specific diagnostics

Use revision `adblock-checkpoints-2` and preserve last-visible-watch snapshots even for failed bindings. Keep up to 12 generation-scoped changes and separate session totals. Class names, booleans, ABI match flags and generation counters are sufficient; do not record URLs, video identifiers, CPNs, account details or pointer addresses.

| Checkpoint | Values to record |
|---|---|
| `native_creation` | Origin: constructor, selection, transition, unknown; delegate/parent/response class or nil; response-argument presence. |
| `response_resolution` | Wrapper class match; accessor present; accessor ABI match; returned raw class; controller wrapper identity match; video identity valid. Distinct gates: `wrapper_nil`, `wrapper_class_mismatch`, `player_data_accessor_missing`, `player_data_abi_mismatch`, `player_data_nil`, `raw_class_mismatch`, `response_owner_mismatch`. |
| `policy_eligibility` | Watch ownership, known live/DAI flags, strategy, generation, CPN available/matches when available. |
| `cached_selection` | Selector entered; cached class; compatibility checked; original versus forced decision; exact receiver/response scope matched; factory recreation observed. |
| `candidate_construction` | Factory called; exact config found; scoped flag read; requested/returned class match; original delegate unchanged. |
| `native_assignment` | Selected object equals stored ivar; getter agrees; native initializer path completed. |
| `transition_cleanup` | Transition entered; reset actually called; exact receiver matches; reset returned; ad state known/not playing; pending callbacks observed. |
| `late_commit` | Preconditions passed; generation/pointer unchanged; ownership layout valid; assignment attempted; readback agrees; new native video/timeline initialization observed. |
| `native_completion` | Break type, source equals installed coordinator, current generation, old/reset callback classification. |
| `content_progress` | Bound response/video generation; content state; increasing native content time; unexpected native ad playback. |
| `deferred_request` | Exact reason and age: response missing, active ad/content, cleanup unproven, off-main-thread, ABI/layout missing, generation changed, ownership changed. |

Replace the current top-level `factory_binding_missing` diagnosis when a factory was actually seen: separately report `factory_seen`, `factory_response_matches_current_content`, `current_controller_has_lease`, and the first failed response-resolution step. Report 1251 would then show precisely whether the factory received nil, the wrapper was unsupported, or the raw accessor failed.

## Implementation order and verification limits

First implement the specific resolution diagnostics and lease binding from controller content, then the native selection/reuse route, then the guarded transition commit. Do not make an active-ad pointer replacement part of the first delivery. Capture device reports for first install, later launches, reopening the same video, autoplay/internal transitions, a settings change during playback, and an observed ad attempt. These are future device checkpoints, not tests executed for this scheme.

Research provenance is in [adblock-handoff-evidence.json](profiles/adblock-handoff-evidence.json), with [the 14 native decompilations](profiles/adblock-handoff-decompiled.txt). This document establishes a concrete design and its evidence boundaries; it does not establish device success or universal runtime replacement safety.
