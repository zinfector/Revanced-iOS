# Native speed context repair and combined release 0.3.30

Report 345 has a valid native 1x model but no recorded speed reads or choices. The user sees a slider stuck at the left; changes neither affect playback nor survive reopening. Its source fingerprint matches 0.3.28. The broad first-launch UI policy defaults the cold configuration used by the native speed adapter and contextual block registry. Read-only native decompilation establishes this interaction; the report does not establish which runtime construction branch failed.

## Repair

The saved native cold group is copied before the first-launch loader substitutes its empty UI-default group. Original active-group reads can supply it if loading happened earlier. Successful native configuration saves refresh the retained copy. No persistent configuration, keychain, account data or UI preference is erased.

Six ABI-checked native getters (three settings through both `YTColdConfig` and their typed interfaces) execute with the saved native group: speed-sheet xplat adapter, playback-rate-selector block registry, and bottom-sheet registry integration. Their native boolean values remain authoritative. Every other cold getter and the existing first-launch request/template policy retain their previous behavior. This is a local infrastructure exception; no true value is imposed on an experiment and no modified configuration/hash is sent to the server.

If the native speed filler factory returns nil, the repair resolves `YTBlockRegistryProvider` from the actual supplied responder. It requires main-thread execution, a loaded current-content Watch player, matching native provider ownership and a valid container before calling the existing native `createInContainer:error:`. It preserves every non-nil original filler and rejects errors/unknown ownership. Native writers, automatic-speed models, limits and synchronous return contracts are retained. A missing provider/container remains native and is reported.

The player-block wrapper is observed as well as the underlying implementation. A synchronous main-thread explicit wrapper selection establishes the same capture scope used by the existing native player-model setter, so C++ dispatch can enter the existing remembered-choice/readback path. Initialization and restore writes never become user preferences. Background calls stay on their native thread and are counted; no synchronous bridge call is transferred between queues.

## Merged implementations

Base: 0.3.29 ad ownership worktree, commit `576526f`, including native display-ad filtering/companion clearing, verified Watch ownership and route checkpoints, plus SponsorBlock skip/undo prompts and native settings/authentication.

Shorts commits `5566cce` and `89cdada` are integrated. The newer typed ad-model walker is retained; Shorts rows/headers are removed independently of the feed-ad setting. Filtering continues to copy GPB models with native setters. The native pivot model removes the Shorts item so remaining tabs are laid out by the native renderer, and every cached view sharing that renderer is refreshed. The later ad ownership and companion lifecycle code is retained.

## Diagnostics

ReVanced > Hook diagnostics > **Copy speed checkpoints** now includes `playback_speed.sheet_context`, revision `speed-context-1`:

- `first_launch_policy_active`, `saved_native_group_available`, `saved_native_group_populated`.
- `state` keys for the six effective native configuration reads.
- Filler/show-command factory result presence, provider/registry/container presence and provider ownership match.
- Native command error presence/code and whether the caller supplied an error output. Error descriptions and raw commands are omitted.
- Wrapper read/selection calls, results per method, returned rate availability, and background-thread counts.
- General Elements context totals explicitly describe **all contexts**, not only speed sheets. They are observations and do not change the context factory.

`command_bridge.off_main_getter_calls` and `off_main_selection_calls` expose the previous main-thread blind spot. `gate_describes` distinguishes startup restoration from explicit selection. When no choice has been observed, the summary says so even if restoring 1x is confirmed. The diagnostic version comes from the release version macro rather than the stale 0.3.20 label.

For a device report after installation, choose 1.25x, leave playback active for two seconds and reopen the native picker. Copy speed checkpoints before resetting to 1x. Useful stages are: effective native flags -> factory/container -> positive initial rate/model -> wrapper/bridge choice -> player/core setter -> matching delayed readbacks. A populated original group and effective flags together distinguish configuration preservation from missing command context; wrapper/background counts distinguish an alternate dispatch path. If the same blank slider persists, copy the full diagnostic report as well for hook-installation and first-launch policy details.

## Build and limits

Production build uses the working Apple Xcode 16.4 / iPhoneOS 18.5 SDK toolchain on macos-15, target arm64-apple-ios17.0, with signing header padding. One SideStore-compatible native-ad IPA is packaged from the merged source and downloaded native artifact. The source fingerprint and compiler provenance are retained in receipts.

No tests, static hook checker, archive self-check or GUI/device smoke tests are run, as requested. Compilation does not establish device success. This repair addresses a statically identified configuration interaction and missing native command context; device confirmation remains pending. Saved configuration absent/empty, unknown providers and native command failures are explicitly reported instead of fabricating a rate or forcing a different player's state.
