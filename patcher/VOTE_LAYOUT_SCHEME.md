# Watch vote layout across launches

Version 0.3.20 keeps the native watch row on a layout compatible with the RYD
counter when **Return YouTube Dislike** is enabled. It targets the native
watch-associated UX and video-action button style experiments. The native speed
logic from 0.3.19 is preserved; the user reports its picker now works.

Report 823 identifies a layout problem, rather than missing RYD data. There is
one measured, mounted, loaded, drawn and unclipped count, but its
`outer_edge_gate` is `native_pair_geometry_unavailable`. The screenshot shows a
separate icon-only action row, a dislike count of 4, and native likes in the
metadata line. The old rendering checkpoints consequently reported
`none_detected` despite a visibly different native layout.

## Implementation and evidence

`RVVoteLayoutCompatibility.inc` installs four ABI-checked BOOL getter hooks before
the application's launch, after the supported-version/UUID checks. The concrete
cross-platform config and aggregate `YTColdConfig` accessors for each of these
two flags return false while RYD is enabled:

- `enableWatchModernTypographyAssociatedUxChangesNative`
- `enableAlignVideoActionButtonStyleWithTetrisButtonStyleNative`

Read-only Ghidra decompilation establishes that these getters and their aliases
read Boolean experiment values from the persisted cold-config group, keyed by
`0x2b81c72` and `0x2b8512f`. The connection between these experiments and the
screenshot's layout is an inference from the flag names and observed redesign.
No direct watch-layout call site has been proved. The next device report must
establish whether the getters are used and whether the paired row returns.

The hooks forward the original native value when RYD is disabled. They do not
rewrite cached experiment data, override global icon assets, alter Elements
migration, fabricate native likes, or change speed flags. YouTube retains its
native commands, icon rendering, accessibility and action hit targets. The
existing video-bound RYD adapter continues to supply only its dislike text.
Changing RYD may require relaunching YouTube because its native layout can be
constructed once per launch.

## Specific device checkpoints

Use `YouTube-21.39.4-RVPort-0.3.20-SideStore-auth-unsigned.ipa`. With Diagnostics
and RYD enabled, open an ordinary video in portrait and leave the row visible
for ten seconds. Save a screenshot and **Settings > ReVanced > Hook diagnostics
> Copy RYD checkpoints**. Repeat after fully closing and reopening the app,
before pausing or resuming the video. No vote is needed.

Check `diagnostics_revision` is `ryd-checkpoints-7` and
`last_visible_watch_matches_current_video` is true. The source fingerprint must
match the release receipt's native build. Interpret `last_visible_watch`, since
the report is copied from Settings.

| Observation | What it establishes |
|---|---|
| `layout_compatibility.launch_number` and `session_id` | Distinguish launches and correlate flag reads with the row captured in that process. |
| Each `flag_reads` entry's `hook_installed` | The exact ABI hook was installed; installation alone says nothing about native use. |
| `reads`, `first_read_ms`, `last_read_ms` | Whether and when YouTube actually requested that experiment. Zero reads means that accessor has not been observed, not that the layout is fixed. |
| `first_native_value`, `last_native_value`, `last_effective_value`, `overrides` | Whether the server's native flag was true and suppressed. With RYD enabled, observed effective values should be false. A native false value needs no override. |
| `count_matches_playback` through `text_unclipped` | Existing service, binding and dislike rendering checkpoints. |
| `native_pair_detected` | A bounded native pair with two image nodes, including the identified dislike icon, was observed around the owned count. |
| `native_like_text_in_pair_detected` | Non-owned native text was observed inside that pair. It does not substitute a guessed like count. |
| `geometry[].icon_geometry_source` and `icon_text_gap_points` | Native icon/count spacing, including flattened icons whose Yoga frame exists without a separate loaded UIKit view. |

The last two checkpoints now participate in `first_blocked_checkpoint`.
`native_pair_detected` false distinguishes a native layout incompatibility from
a drawn-but-missing estimate. If it is true but
`native_like_text_in_pair_detected` is false, investigate the native text branch.
These are bounded observations, not proof that no like text exists elsewhere
on the page. `none_detected` still needs the screenshot to confirm appearance.
History retains eight checkpoint changes, and flag records are bounded to four
accessors. Reports retain no account identity, video IDs, localized native text,
tokens or raw protobuf data.

Local and cloud compilation and production IPA packaging are permitted build
steps. No tests are run. Layout behavior across launches remains device-untested
until the user supplies the next observation.
