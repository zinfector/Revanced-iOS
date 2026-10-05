# Why the vote row changes after a clean installation

Investigation of YouTube 21.39.4, the delivered RVPort 0.3.20 build, and Report
931. The user clarified that “first startup” means the first launch after
**deleting and reinstalling YouTube**. No tests or device simulations were run.
This document records an investigation; it does not announce a new runtime fix.

## Finding

The screenshot shows a different **native YouTube action layout**, not merely a
missing or badly padded RYD label: separate thumb icons replace the paired pill,
and YouTube's native like count appears in the metadata line. The RYD adapter
still draws its dislike estimate inside the native dislike button. Its nearby
typography and pair-spacing assumptions no longer match that layout.

There is a confirmed native mechanism for the first clean launch to differ from
subsequent launches: cold configuration is loaded from disk at initialization,
an empty group is used when no stored group exists, and refreshed groups are
saved for later processes without replacing the current process's group.
YouTube also persists Elements template serving context and sends configuration
data back to its server. Returned action-bar models distinguish paired and
independent vote buttons.

**The exact experiment, server response, or cached template responsible for the
user's switch has not yet been captured.** The persistent configuration/model
explanation fits the reinstall boundary and the observed native redesign, but
Report 931 alone cannot prove which of those inputs selects it. It would be
incorrect to claim that one named Boolean has been established as the cause.

## What Report 931 establishes

| Observation | Interpretation |
|---|---|
| Aggregate source fingerprint `4fb8571f8595dfbd1681d843cd4c8f1f4ee8a835d103fd80fbe36f44585531aa` | Matches the delivered cloud 0.3.20 manifest's `build_source_sha256`. This is the expected build, not an old-payload diagnosis. |
| `diagnostics_revision = ryd-checkpoints-7`, launch number 3 | Later process with the new checkpoints active. The launch counter is patch-owned diagnostics, not YouTube's renderer selector. |
| HTTP 200, `cache_hit`, valid current-video binding | The RYD service and video binding succeeded. |
| Owned text measured, mounted, rasterized, in the window and unclipped | The dislike estimate passed the drawing path. |
| Flattened native icon at x=0, width=24; count at x=32 | The count's measured icon gap is 8 points. |
| `native_pair_detected = false`, native nearby like text not detected | The bounded adapter search did not find its expected two-image/text arrangement. This does not prove there is no native like count elsewhere. |
| All four compatibility hooks installed, each with `reads = 0` | The previous layout override was never consulted on this observed path. Hook installation did not establish layout control. |
| `native_likes_replaced = false` | The RYD label is not a replacement for native likes. |

Use `last_visible_watch`, rather than the live Settings view, to interpret the
captured row. That snapshot reports the current video matches.

The earlier before/after Report 755 pair, both version 0.3.18, also records a
change from `native_neighbor` attributes with a text height of about 14.33 points
to `native_palette` with a height of 33 points. These reports concern different
videos, so their count widths are not a controlled comparison. The attribute
and height differences corroborate a different rendering environment; they do
not identify the layout's experiment or the exact font point size.

## The native launch sequence

| Stage | First process after a clean reinstall | Later processes |
|---|---|---|
| Cold-config storage initialization | With no app-local blob, receives an empty `YTIColdConfigGroup`; generated/default behavior applies. | Loads and parses the previously saved group. |
| Active cold-config group | Cached for the process. | Cached for this process, potentially using different persisted values. |
| Server refresh during playback | Saves a new group and hash on disk. The inspected save path does not swap the active group's pointer. | Can save another group for a later launch. |
| Request context | Can have absent/default cold configuration data initially. | Can send configuration data from the loaded group. |
| Elements serving context | Can fall back to the resource loader when stored context is absent. | Can load a persisted serving context when its feature gates are enabled. |
| Native action model/template | Can select the default paired presentation. | Can select a different presentation based on configuration and returned models/resources. This last selection is the causal hypothesis still to measure on the device. |

This is a process-boundary mechanism, not an inherent “launch two” condition in
the RYD code. Later launches do not have to be identical forever: YouTube can
persist newer configuration and resource information again. Installing an
update while retaining app data also differs from deleting/reinstalling.
App deletion should not be treated as clearing every possible server or
keychain state; the relevant confirmed mechanism here is app-local storage.

### Static evidence from the supplied binary/database

- `YTColdConfigStorage -initWithBlobStorageBlocking:hotConfig:userDefaultsInjector:`
  at `0x1000e5150` loads the cold group and hash into storage at initialization.
- `+loadColdConfigGroupFromBlobStorage:dataKey:` at `0x1000e574c` returns
  `[YTIColdConfigGroup message]` when storage is absent or parsing fails.
- Storage `-coldConfigGroup` at `0x100192f44` returns its group pointer at offset
  `+8`; `YTColdConfig -coldConfigGroup` at `0x1000e4f7c` lazily caches that group.
- `YTGlobalConfigsApplier -loadGlobalConfigs:` at `0x100f0b29c` processes newly
  serialized groups from responses. `-deserializeAndSendColdConfigGroup:newColdHash:`
  at `0x100f0b548` first obtains the current group and then saves the new group.
- `YTColdConfigStorage -saveToBlobStoreColdConfig:hash:error:` at `0x100f02e44`
  writes group data and the hash; it changes the hash pointer at `+0x38`, not the
  active group at `+8`.
- `YTInnerTubeContextFactory -clientInfoWithSendDeviceIdentifier:serviceType:`
  at `0x10024e10c` fills `configInfo.coldConfigData`, `coldHashData`, and
  `hotHashData` for its supported service types. This gives the server an input
  that can differ after a configuration has been saved and reloaded.
- `YTElementUpdateHandler -emlTemplateServingContext` at `0x10019c0ec`, with
  initialization helper `0x10019c178`, can read `diskCacheServerContext` from the
  defaults service when SRS persistence and the relevant cold-config gate are
  enabled. Empty context falls back to `ELMResourceLoader getServingContext`.
- `-onDiskCacheServingContextUpdated:cacheExpirationTimeSecs:` at `0x100e92fc8`
  can persist a new context. This is evidence of a second persistence route,
  not proof that the user's vote template was downloaded or changed.

Read-only exports are in `build/vote-startup-storage-annotated.txt`,
`vote-config-update-pipeline-annotated.txt`, `vote-client-context-annotated.txt`,
and `vote-cold-proto-descriptors-annotated.txt`. The companion JSON records
addresses, report observations and evidence fingerprints.

## Why the native counter looks different

The IPA contains distinct templates under
`Payload/YouTube.app/mainapp_filegroup/_srs_resources_eml_bundle/`:

- `segmented_like_dislike_button` and `segmented_like_dislike_button_inner`;
- `video_action_button_pill`;
- `like_button_with_vm_input` and `dislike_button_vm`;
- `video_action_bar` and `compactify_video_action_bar`.

The compiled `video_action_button_with_vm_input` resource references both the
segmented template and the independent like/dislike templates. These resources
are binary compiled templates; string references establish their dependencies,
not a complete decoding of the branch conditions.

The protobuf descriptors make the distinction explicit. `YTIVideoActionBarButton`
has `buttonViewModel` field 10, `buttonType` field 13 and a visibility entity key
at field 15. Its enum has **LikeButton=3, DislikeButton=4, and
SegmentedLikeDislikeButton=15**. The compactify model supplies `barStyle`,
`buttonsArray`, renderer context and experiments. The segmented model supplies
`likeCountEntity`, dynamic like-count data and separate like/dislike models.
The paired native count is therefore a model/template concern, not a number
that our dislike drawing hook automatically restores.

Native `YTWatchNextResultsViewController` code at `0x101753090` also recognizes a
compactify metadata placeholder in received model contents. The watch filler
has both `getVideoActionBarElement:error:` (`0x10349eebc`) and
`getCompactifyVideoActionBarElement:error:` (`0x10349f344`). This substantiates
multiple native composition paths; the user's exact selected path still needs
a runtime model/template checkpoint.

Our adapter discovers the dislike branch semantically, appends its own Yoga
sizing child and mounts the estimate inside that native button. It does not
move YouTube's like count, replace native thumb assets, create the pill
background or replace native vote commands. Consequently, a native switch to
standalone buttons exposes the new background/icon treatment and can leave
likes in the metadata line while the owned dislike count remains beside its
icon.

The adapter searches nearby native attributed text for matching font/color.
If none is found, it uses a system 14-point medium font scaled by
`UIFontMetrics` and YouTube's text palette. That fallback can differ from
YouTube's template typography. `color_source` records the color decision;
it does not separately prove which font was used. The current report lacks
actual font point size, content-size category and selected typography token.

## Two assumptions the investigation rules out

**Installed getter hooks are not enough.** The four 0.3.20 getters were chosen as
candidates from their names and config implementations. Report 931 records zero
calls, so no evidence connects their overrides to this renderer. More guessed
native getter hooks would repeat the same unsupported approach.

**The scripting binding is not the whole native experiment configuration.**
Resolving previously unnamed stubs shows that
`YTELMGetColdConfigFunctionBinding init` at `0x10050b1e8` takes
`coldConfigGroup.scriptingColdConfig`, sets that in a binding output, and caches
the serialized output. Its `execute:` returns that cached output. The supplied
`YTIScriptingColdConfig` descriptor has only `testValue`, and the scripting hot
config has no described fields. This rules out the preliminary suggestion that
this binding directly serializes all the native vote-layout flags. The confirmed
request `coldConfigData` and returned models remain separate investigation paths.

There is also a limitation in `RVElementBalanceEdges`: its search climbs at most
six ancestors, inspects at most 128 nodes, and expects exactly two
`ASImageNode` objects with `UIImage` content. It stops at a wider subtree with
more than two images. The native like template references Rive/animated icons,
which need not satisfy that image-node assumption. Both a real layout change
and a missed animated native pair can produce `native_pair_detected=false`.
Indeed, earlier paired-layout reports also failed the geometry search. The
screenshot, template/model information and search result must be interpreted
together.

## Checkpoints needed for a decisive comparison

These are proposed **additional** checkpoints, not fields already implemented
in 0.3.20. Capture them passively in order, once during the first reinstall
launch and again after fully terminating and reopening on the same video:

| Checkpoint | Minimal values to record | Question answered |
|---|---|---|
| `cold_config_loaded` | Blob present, parse success, active-group fingerprint, relevant field presence, launch-relative time | Did launch one use defaults while launch two loaded a saved group? |
| `cold_config_saved` | Save success, new-group fingerprint, active-group fingerprint unchanged/changed | Was a different group written during the first process? |
| `request_configuration` | Cold-data present, fingerprint matching active group, cold-hash presence, service type | Did the requests differ without logging request contents? |
| `elements_serving_context` | Source: persisted/resource-loader/absent; opaque context fingerprint; persistence gates | Did a template-serving generation change? |
| `watch_action_model` | Model class, compactify placeholder, `barStyle`, known `buttonType` values, segmented like-count entity presence | Was YouTube given type 15 or separate types 3/4? |
| `watch_vote_template` | Allowlisted native template name, pair/independent role, static/animated/Rive icon kind | Which component was actually constructed? |
| `native_like_location` | Action-button/metadata/absent; count text present boolean and native model source | Did native likes move or fail to render? |
| `count_typography` | Native attributes found, native/fallback font selection, actual point size, text height, content-size category, both outer insets | Why does the estimate's size or spacing differ? |
| `pair_search` | Stop reason, depth/node/image totals, animated icon recognized, semantic pair result | Was the current detector wrong rather than the native pair absent? |

Store bounded history and process-relative ordering, not just a final snapshot.
No raw configuration, account identity, video ID, request URL, token, cookie or
localized text is needed. A locally generated session identifier can relate
events. Cache fingerprints are for equality checks, not a claim that they reveal
an experiment's meaning.

If the action model is type 15 on both launches but icon kind changes, improve
semantic pair recognition and native typography binding. If it changes from
type 15 to separate types 3/4, support that native layout explicitly or prove and
normalize the specific model/config branch that selects it. If template-serving
context changes while models remain identical, inspect that resource generation.

The durable implementation should bind to the current native vote component,
model and typography on each composition/transition, including animated icons.
For a consistently paired pill, the controlling native model/template branch
must be identified first. Resetting all YouTube caches or fabricating a native
like count would not establish that branch. Login, SponsorBlock and the now
working speed picker need no changes for this investigation.

## Build identification correction

The runtime's aggregate `build_source_sha256` is
`4fb8571f8595dfbd1681d843cd4c8f1f4ee8a835d103fd80fbe36f44585531aa`.
The manifest's separate `source_sha256`,
`8fbe4742b3b1c657c00fbcc6107f79d09386263c3da7d222758ce1fb116df907`,
identifies `RVPort.m` alone. The earlier delivery note incorrectly called the
latter the native runtime fingerprint. Report 931 correctly matches the
aggregate field; it should not be rejected for failing to match the single-file
hash.
