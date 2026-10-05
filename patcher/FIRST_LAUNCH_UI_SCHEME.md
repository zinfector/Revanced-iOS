As of 0.3.30, three native speed/registry infrastructure settings preserve their saved native configuration through both getter interfaces. The remaining UI-default/request/template policy below is retained. See [SPEED_CONTEXT_IMPLEMENTATION.md](SPEED_CONTEXT_IMPLEMENTATION.md).

# Keep the first-launch startup path (0.3.22)

The user asked to retain the native paired like/dislike row seen on the first launch after deleting and reinstalling YouTube. Report 1046 is a later launch of 0.3.21: RYD renders its estimate, but the observed components are separate `like_button` / `dislike_button_vm` templates. Its GPB producer hooks did not return an action model. The previous model-only repair therefore did not select the clean-install presentation on this device.

## Implementation

**ReVanced > Return YouTube Dislike > Keep first-launch UI** defaults on. The policy is enabled only when RYD and paired vote buttons are also enabled at process start. Fully terminate and reopen after changing these switches; it never switches the configuration halfway through a session.

The inspected `YTColdConfigStorage +loadColdConfigGroupFromBlobStorage:dataKey:` returns a new empty `YTIColdConfigGroup +message` when no blob exists. This release supplies that same default group on every launch, both at the loader and the active storage/config getters. The loader still reads the saved group for redacted source/effective diagnostics, and the native save routine still preserves new groups on disk. It does not delete app data, configuration files, login/keychain items, preferences, or downloaded template resources.

`configData`, `storedConfigHashData`, and `coldConfigGroupHash` return nil while this process policy is active. This prevents a saved hash from being advertised for a default group, including after a server refresh saves a newer hash. `YTElementUpdateHandler emlTemplateServingContext` returns nil, matching absent startup context; its persisted value is retained. Account configuration, account hashes, hot configuration and other request fields keep their existing paths.

All required hooks have ABI guards. If the empty group cannot be constructed or any required hook cannot be installed, the policy falls back to the original getters as a unit. Native presentation-model normalization remains as an additional supported-model path. Serializer entry diagnostics now distinguish a never-entered boundary from an entered boundary whose bytes did not change.

This is broader than a single vote-layout experiment: it selects **native cold experiment defaults throughout the app**. The exact experiment selecting the alternate action bar has not been identified. This deliberately reproduces the known clean-install cold-config inputs instead of pretending the unused experiment getters select this device's row. Existing authentication, SponsorBlock, settings and native-speed implementations are included unchanged, but device behavior under the new configuration policy still needs confirmation.

It cannot guarantee a specific server-returned row: account/hot configuration, downloaded resources, or a response selected independently of cold configuration can still differ from a clean install. Diagnostics separately report applied startup inputs and actual paired-template observations. No synthetic like count, copied prior video's buttons, C++ arena mutation, or native vote command replacement is introduced.

## Device checkpoints

Install `YouTube-21.39.4-RVPort-0.3.22-SideStore-auth-unsigned.ipa` over the current app using the same signing identity. No delete/reinstall is needed. Open an ordinary video with the row visible, then copy RYD checkpoints. Fully quit, reopen, and capture the same video again. Confirm `ryd-checkpoints-9` and the build fingerprint in the release receipt.

Read `last_visible_watch.vote_model.first_launch_policy` (or `return_youtube_dislike.vote_model.first_launch_policy` in the full report):

| Checkpoint | Expected |
|---|---|
| Requested policy | `requested_at_start=true` |
| Required hooks and empty group | `default_group_ready=true`, `state.policy_ready=true`, `state.critical_hooks_ready=true` |
| Native active configuration | `counts.loaded_group_replaced` or `counts.active_group_read` greater than zero |
| Request configuration | `state.request_cold_defaults=true`, no `counts.request_cold_present` |
| Template context | `counts.serving_context_read` greater than zero, `state.template_context_defaults=true` |
| Actual native paired row | `paired_template_seen_this_process=true`, plus screenshot and existing native-pair/render checkpoints |

`first_blocked_checkpoint` identifies the first unmet stage. `paired_template_not_observed` with all startup stages passing means default startup inputs were applied but they did not produce the paired component; it does not mean the count network request failed. The paired-template observation is process-scoped, not proof that every video's row is paired. Always capture the visible row and check that the rendering snapshot matches the current video.

`vote_model.startup.cold_config_loaded` now reports separate source and effective group fingerprints/lengths. A populated `source_group_bytes` with effective `group_bytes=0` and `first_launch_applied=true` is direct evidence that the later launch bypassed its saved group. The saved file remains intact. No raw configuration, account identity, tokens or URLs are logged.

Compilation and production packaging only. Tests, hook checker, GUI smoke tests and standalone archive tests were not run at the user's request. Device verification remains pending.
