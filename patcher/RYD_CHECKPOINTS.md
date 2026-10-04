# Return YouTube Dislike checkpoints

Report 635 and the screenshot confirm the 0.3.15 count mounts and draws. Its history captures transient player detachment, node removal and delayed recreation after miniplayer return. Version 0.3.16 reserves the missing icon gap and adds lifecycle refreshes. Spacing and restoration still require device confirmation.

Report 541 (0.3.12) has a count matching playback and one measured text node (23 by 14 points), but zero render-mounted, loaded, or in-window text nodes. This narrows the problem to the element text's mounting/rendering path. A measured Yoga frame alone does not establish a rendered label. The report was copied from Settings, so a snapshot from the visible watch page is needed to distinguish the original failure from leaving that page.

The diagnostic update adds `ryd_checkpoints` to the full report and a separate **Copy RYD checkpoints** action. Its revision is `ryd-checkpoints-6`. It inspects existing nodes and layers without loading views, forcing layout, or changing the renderer. The checkpoints remain passive; version 0.3.15 also changes the production mounting path. Compilation does not establish that the counter is visible on a device.

## Capture procedure

1. Install/sign the checkpoint IPA. In Hook diagnostics, confirm **Diagnostics: ryd-checkpoints-6**. The copied report's `build_source_sha256` identifies the compiled native sources; compare it with the delivered build receipt if several builds share a version number.
2. Ensure ReVanced's **Return YouTube Dislike** and **Diagnostics** settings are enabled.
3. Open an ordinary recorded video in portrait, with the like/dislike row visible. Leave it on that page for about 10 seconds. Capture a screenshot of the row before navigating away.
4. Open **Settings > ReVanced > Hook diagnostics > Copy RYD checkpoints**. Hook diagnostics is under the **Tools and configuration** section. Save the copied JSON beside the screenshot.
5. Check `last_visible_watch_matches_current_video`. It should be true. Interpret `last_visible_watch`, rather than `current_snapshot`, for rendering: opening Settings can detach or cover the watch surface. `last_visible_watch_age_seconds` indicates how old the preserved observation is.
6. If the problem remains, repeat after opening a second video. Give the two reports different names. The `video_generation` and bounded `checkpoint_history` show whether a new video was bound and where progress stopped, without including video IDs or protobuf text.

There is no need to tap Like or Dislike to capture this evidence. Zero is a valid RYD count.

## Specific checkpoints

Read `last_visible_watch.checkpoints` in this order. `first_blocked_checkpoint` identifies the earliest failed condition. These checkpoints describe the **element-based count path**; `not_applicable_non_element_path` means a legacy or inline adapter is active instead.

| Checkpoint | Expected on the visible watch page | Failure narrows the investigation to |
|---|---|---|
| `feature_enabled` | true | RYD preference/configuration |
| `playback_video_valid`, `watch_foreground`, `watch_player_matches` | all true | Playback ownership, current watch controller, or capture context |
| `count_matches_playback` | true | Service response, cache, or stale-video rejection |
| `dislike_icon_seen` | true | Native icon identity and watch-tree discovery for this video |
| `text_node_created` | true | Semantic button binding and node creation |
| `text_node_measured` | true | Layout invalidation or text measurement |
| `text_node_mounted` | true | Transfer from Yoga layout into the render tree |
| `text_node_loaded`, `text_layer_in_window` | both true | Native node/layer mounting into the active surface |
| `text_raster_contents_present` | true | Text layer drawing/backing contents, after mounting and window attachment |
| `text_unclipped` | true | Hidden/transparent layers, clipping, or offscreen placement |

`none_detected` means the geometry checks passed; it does not prove the text was visibly drawn. Include the screenshot if the count is still absent.

## Fields that make a failure actionable

- **Request:** `last_visible_watch.service.fetch_state`, `http_status`, `network_error_code`, `json_parse_error_code`, `response_bytes`, `retry_after_seconds`. `loaded` or `cache_hit` plus a matching count clears the data checkpoint. A cache hit need not have made an HTTP request in this session. `network_error`, `http_error`, and `invalid_response` distinguish transport, server, and response-validation failures. Retry delay reflects the existing cooldown; diagnostics do not trigger requests.
- **Button/layout ownership:** Each `geometry` entry has `button_class`, `text_class`, `button_yoga_root_class`, `video_matches_current`, `owned_node_is_yoga_child`, `yoga_parent_is_button`, `owned_node_is_render_child`, and `render_parent_present`. Yoga-child membership with no render parent is the important distinction missing from the older report.
- **Rendering:** `node_loaded`, `button_host_in_window`, `render_layer_in_window`, `frame_x/y/width/height`, `clipping_ancestors`, `geometrically_visible`, and `color_source`. These are observations, not attempts to repair or mount the text.
- **Timing:** `captured_at`, `trigger`, `video_generation`, snapshot age, and the last eight checkpoint changes. Passive sampling is throttled to two snapshots per second, with an additional snapshot on a service response. Event counters are relative to the current video binding rather than totals from earlier videos.

If `foreground_watch_snapshot_missing` appears, return to the video, leave the row visible for 10 seconds, and copy again. If the revision is missing, that report came from a build without these checkpoints. If diagnostics are disabled, enable them and repeat the capture.

This update was built and packaged without running tests. Device behavior and the rendering repair remain unverified.

Version 0.3.14 additionally records root invalidation requests and completed native root layout passes, owned text didLoad and displayDidFinish callbacks, and existing layer raster contents. It does not force mounting, layout, or drawing. Root-layout callbacks are recorded only for the current owned text’s native Yoga root; all original callback results are preserved. Raster contents indicate a backing image, not proof that visible glyph pixels were drawn. Snapshot history preserves foreground failures before opening Settings.

The report version in Report 541 is stale because the older Settings snapshot hardcoded 0.3.12. The checkpoint build explicitly reports 0.3.15 and a generated source fingerprint, so a diagnostic report can be matched to the compiled payload.


In 0.3.15, inspect geometry.mount_mode and mount_gate. `native_text_node` uses native AS mounting. `owned_label_in_native_button` renders a UILabel inside the verified native button; the AS text child remains a sizing proxy. For that mode, native_text_supernode_present and native_text_node_loaded may remain false, and AS text drawing callbacks may stay zero. Effective mount/load/window/clip checkpoints inspect the owned label instead. `label_fits_button_bounds` and clipping_ancestors identify any remaining size constraint. Diagnostics do not force mounting; the production renderer performs its own guarded mount.


Version 0.3.16 records spacing_points (8), spacing_gate (logical_start_margin_reserved), button_width, watch_suspended, suspension_count and resume_count. Transition event counters watch_appeared/watch_player_changed/watch_layout_ownership_changed and transition_refresh show refreshes that require no pause/unpause. watch_player_matches now verifies current content IDs; watch_player_pointer_matches separately describes cached pointer identity. Preserve a screenshot/report after miniplayer return before interacting with playback if the count still disappears.

When icon_gap_measured is true, icon_text_gap_points reports the observed gap using the existing native icon view’s button-local rectangle. This distinguishes a reserved margin from actual displayed geometry; no icon views are loaded for diagnostics.


Version 0.3.17 adds outer_leading_inset, outer_trailing_inset, owned_end_margin and outer_edge_gate. native_outer_edges_balanced means the measured pair’s two outer insets agree within half a point. watch_will_appear/watch_gesture_began/synchronous_transition_refresh distinguish restoration before animation completion. The full diagnostic report includes playback_speed with native menu source, requested/observed rates, attempt count and confirmation gate. Capture it on the first launch after a native speed selection if the rate still fails to change.

Delivered build: `YouTube-21.39.4-RVPort-0.3.17-SideStore-auth-unsigned.ipa`. Expected `build_source_sha256`: `39b45371c1e727d0279e843c14c6b561bfc6493e4d3a3359578c1ce1a96b05d1`. Source commit: `d27b904dfabae3a62fc83a9b3644e48889a5ce04`.

Version 0.3.18 adds semantic_binding_probe. If text_node_created is zero with semantic_role_missing, the probe distinguishes an uninitialized proto from a missing parent/component chain or unsupported role identifier. The full report includes playback_speed.command_bridge. Capture full diagnostics immediately after choosing 1.25 in the native slider on a fresh launch, and again after reload if the behavior differs. Copy RYD diagnostics while the watch row is visible for missing counts. Native like counts are never replaced by invented values.
