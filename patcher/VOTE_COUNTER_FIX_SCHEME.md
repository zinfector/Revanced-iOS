# Stable native like/dislike counters

Proposed implementation for YouTube 21.39.4, based on Report 931 and the supplied
native binary. This is a scheme, not a completed patch or a new IPA. No tests
were run. The current login, SponsorBlock and playback-speed behavior should be
preserved from the integrated 0.3.20 source snapshot.

## Target behavior and chosen approach

With Return YouTube Dislike enabled, show the native paired vote pill with
YouTube's native like count and the RYD dislike estimate on the first clean
launch, later launches, and when returning from the miniplayer. Use YouTube's
commands, selection state, accessibility and paired template. The estimate must
share the row's typography and balanced outer padding.

Normalize the **local watch action model before native composition**. Support
both inputs: an existing segmented button (type 15), or one standalone like
button (type 3) and one standalone dislike button (type 4). This removes the
counter's dependency on which presentation the server selected after startup.
Configuration tracing remains useful for identifying the experiment, but it is
not a prerequisite for making both model inputs render correctly.

Do not change `buttonType` alone. A type-15 button requires a complete compatible
segmented view model, including its submodels and native entity bindings.
Unsupported models retain their native presentation and use the independent
dislike adapter while reporting the precise unsupported contract.

## 1. Recover and bind to the actual model boundary

The inspected binary has ABI-compatible Objective-C boundaries:

| Candidate | Encoding | Purpose |
|---|---|---|
| `YTBAWatchPageFillerBlock getVideoActionBarElement:error:` | `@32@0:8@16^@24` | Returns an `ELMPBElement` for the ordinary action bar. |
| `YTBAWatchPageFillerBlock getCompactifyVideoActionBarElement:error:` | `@32@0:8@16^@24` | Returns an `ELMPBElement` for the compactify action bar. |
| `ELMComponent initWithElement:context:treeLocalContext:` | `@40@0:8@16@24@32` | Observe the allowlisted vote template/component actually being created. |
| `ELMComponent updateWithElement:` | `v24@0:8@16` | Observe native rematerialization and refresh the bound component. |
| `ELMComponent updateWithElement:forced:` | `v28@0:8@16B24` | Observe the alternate update entry point, with duplicate updates coalesced. |

The filler methods are candidate transformation boundaries, **not yet proved
to execute for the user's row**. Record actual call counts and resulting
template identities. If neither executes, use the observed watch component's
input producer; do not declare a successfully installed but unused hook a fix.

`ELMPBElement` has typed `type`, `properties`, `childElementsArray`, `key`, and
extension ranges. Its nested component/model extension mapping must be recovered
and matched to the supported template before mutation. `ELMElement`, by contrast,
wraps immutable C++ protobuf/arena objects: an Objective-C `ELMPBElement` cannot
simply be substituted into an `ELMComponent` initializer expecting `ELMElement`.
Keep transformations on the typed Objective-C producer/serialization side,
before that C++ conversion. If only the C++ route is active, recover its existing
serialization bridge first; do not write fixed offsets into its arena or ABI.

Use native GPB descriptors and extension accessors to locate the supported model.
Preserve unmodified and unknown fields through native message copying. Do not
patch all `GPBMessage` accessors, hook dynamically generated getters without an
observed implementation, edit `protoText`, or replace compiled template bytes
using string substitution. Proto text can provide bounded diagnostic clues, but
it is not the model mutation interface.

## 2. Normalize the model using existing native count/state data

Recovered descriptor facts provide a concrete data contract:

| Model | Required fields |
|---|---|
| `YTIVideoActionBarButton` | `buttonViewModel` #10, `buttonType` #13, `buttonVisibilityEntityKey` #15. |
| `YTICompactifyVideoActionBarViewModel` | `buttonsArray` #1, `barStyle` #3, `experiments` #4, `rendererContext` #997. |
| `YTISegmentedLikeDislikeButtonViewModel` | Native like/dislike renderers #1/#2, `likeStatusEntityKey` #3, `likeStatusEntity` #4, `likeCountEntity` #7, dynamic-count status/data #8/#9. |
| `YTILikeButtonViewModel` | `toggleButtonViewModel` #1, `likeStatusEntityKey` #3, `likeStatusEntity` #6, animated icon model #15, **`likeCountEntity` #16**. |
| `YTIDislikeButtonViewModel` | `toggleButtonViewModel` #1, `dislikeEntityKey` #2, `likeStatusEntity` #3. |
| `YTILikeCountEntity` | Attributed count strings for liked/disliked/neutral #4/#5/#6; expanded strings #7/#8/#9; numeric variants #12/#13/#14. |

The standalone like model already has a native like-count entity. The renderer
can omit that text in an icon-oriented layout, even when the model carries it.
Copying this entity into the segmented input is preferable to reading a label
elsewhere on the screen or substituting an estimated service like count.
Presence on the user's actual second-launch model still needs its checkpoint.

For an existing type-15 input, retain its native segmented model and bind the
RYD text to its dislike subcomponent. For separate types 3/4:

1. Verify ordinary current-video watch scope, the supported template/model
   schema, exactly one eligible adjacent like/dislike pair in the same action
   bar, and consistent native state. Ambiguous or differently ordered branches
   require their own validated mapping.
2. Copy the action model into a presentation-only model. Do not change the
   cached response, shared entity store, configuration group or network payload.
3. Build the proper segmented renderer envelope using the recovered extension
   descriptor. Supply the original native like and dislike renderers through
   the submodel contracts expected by the segmented template.
4. Carry the native like-count entity, selection-state key/entity, commands,
   animation inputs and supported dynamic-count bindings. If status-specific
   native counts exist, select them through the native state contract.
5. Replace the two eligible presentation entries with one complete type-15
   entry in their original action-bar position. Preserve other action buttons,
   logging context, entity references and accessibility metadata.
6. Preserve both original visibility conditions. If their conjunction cannot be
   represented by the native paired contract, retain the separate entries and
   report `visibility_contract_unsupported` instead of making hidden actions
   visible or silently discarding an entity key.
7. Reuse the bundled native segmented/pill template; adjust only the recovered
   vote-specific style contract if the surrounding compactify bar applies an
   incompatible style. Do not guess a numeric `barStyle` or force the entire
   action bar into an unrelated presentation.

A missing native count is an explicit degraded condition. If the native model
or entity store can resolve it through its existing key, use that binding.
Otherwise keep the native like state, report `native_like_count_unavailable`,
and show only the available estimate. Do not invent zero, scrape localized
metadata text, assume an estimated like count is native, or issue an extra
YouTube request to fabricate a count.

Normalization must be idempotent: passing a normalized type-15 model through
again must not add another pair or label. A toggle-off rebuild uses the retained
original presentation for the current generation. Duplicate native metadata
likes are left to YouTube's metadata template; removing that line is a separate
change and is not required for the paired counter.

## 3. Bind typography and geometry to semantic native components

Replace the current “exactly two ASImageNodes” pair search with references to
the segmented component's like and dislike subcomponents. The independent
fallback obtains the same references from types 3/4. Recognize the native like
role even if its icon is animated/Rive, flattened or has no `UIImage`.

Use this typography order:

1. The native like-count attributed string actually selected by the current
   native state, with its resolved font/color and style.
2. The native paired button's resolved typography token when it exposes one.
3. A documented fallback matching the supported native button text style,
   recording that the fallback was used.

Do not take the first unrelated text node from an arbitrary ancestor. Do not
apply `UIFontMetrics` again to a font already scaled by native typography.
Record actual font point size, native/fallback source and content-size category.

Use native padding/style values where exposed, otherwise measure within the
semantically identified pair. Maintain equal logical outer insets, baseline
alignment and the native icon/text gap; use RTL-aware start/end margins. Measure
the estimate before native layout commits so the pill and neighboring actions
receive the correct intrinsic width. Long counts and large text must participate
in native horizontal scrolling rather than overlap the next action.

Keep the established Yoga sizing child and native-button-owned render surface
when different node contexts prevent native subnode mounting. There is no need
to reintroduce the rejected `addSubnode` approach or enlarge the native icon
image to carry text. Exactly one estimate belongs to each current dislike
component; it must never capture taps or replace the original vote target.

## 4. Rebind on composition and miniplayer transitions

Use a bounded watch-generation state containing the current video identity,
source action-model identity, normalized model, component identity, native
selection/count entity, estimate and layout/style revision. Video identity stays
in memory for binding; diagnostics need only a generation number and equality
checks.

Distinguish video generation from **composition revision**. YouTube can replace
the row while the video and cached estimate remain unchanged. The current
same-video/count refresh key must not suppress updates to a new component or
new typography.

Respond to:

- Action-model arrival or replacement: normalize before native materialization.
- Native component creation/update: bind model/state and attach cached estimate.
- Entity changes from like/dislike/unvote: native like text and icon state update
  through their original contracts; the estimate adapter refreshes its binding.
- Native layout completion: update frames/insets without recursively rebuilding
  the whole watch view.
- Miniplayer expansion: retain valid cached data, attach during composition or
  immediately when the target watch button is available, before its animation
  commits. Do not wait for `viewDidAppear`, a playback tick or pause/unpause.
- Collapse or teardown: detach/hide the owned estimate only with the corresponding
  watch component; retain video data for a valid expansion. Release old component
  references when ownership changes.

Run UI mutations on the main queue. Copy/normalize native models on the producer's
permitted queue before they are published; do not mutate shared models during
rendering. Use a reentrancy guard, a pending update token per composition revision,
weak component references and bounded retained data. Ignore stale asynchronous
responses after a video-generation change.

## 5. Checkpoints that show exactly where the fix fails

Keep startup/context fingerprints from the investigation, plus these ordered
rendering checkpoints. Record event times relative to process start and retain
a bounded history. Scope snapshots to the last visible watch component.

| Checkpoint | Success evidence / failure reason |
|---|---|
| `action_model_observed` | Actual hooked boundary called; source class/template and button types recorded. `hook_unused` distinguishes installation from execution. |
| `video_scope_verified` | Ordinary watch, matching video generation and renderer ownership. |
| `native_like_entity_resolved` | Native count/state entity present; state-specific string or numeric source recorded as a category, without raw text. |
| `pair_model_ready` | Existing or normalized type 15, both native submodels, state and visibility contracts validated; normalization reason recorded. |
| `paired_template_materialized` | Expected native component actually constructed; report the observed template, including unexpected style/independent fallback. |
| `native_like_text_rendered` | Current native count text measured and visible inside the like subcomponent; location recorded separately from metadata. |
| `ryd_count_bound` | Estimate matches the current video generation and composition revision. |
| `typography_resolved` | Native attributes/token or explicit fallback, point size and category. |
| `pair_geometry_balanced` | Measured start/end insets, icon gap, baseline delta, text fits and no clipping. |
| `transition_rebound` | Expansion start, target component bound and first visible count frame ordered in the same composition revision. |
| `vote_command_preserved` | Both original command/state bindings retained; no service vote or native vote is sent merely by binding. |

Separate the first failed model/composition checkpoint from service failure and
geometry failure. An independent fallback with a visible estimate is a declared
degraded presentation, not a false “paired renderer verified” result.

## Implementation sequence

1. Add `RVVoteModel.inc` for typed schema/extension mapping, scoped model
   normalization and native count/state binding. Prove boundary use with the
   new call/template checkpoints.
2. Add `RVVoteLifecycle.inc` for composition revisions, entity changes and
   transition rebinding. Connect it to existing watch/video lifecycle hooks.
3. Refactor `RVElementDislikes.inc` to use semantic model/component references
   for typography and pair geometry while retaining its proven mount path.
4. Extend `RVDislikesDiagnostics.inc` with the ordered checkpoint sequence and
   startup/context observations. Retire the ineffective four-flag override once
   the replacement is active; it has recorded zero reads on the failing path.
5. Expose **Use paired vote buttons** under ReVanced's Return YouTube Dislike
   section, defaulting on when RYD is enabled. Turning it off supports YouTube's
   chosen layout with the improved independent estimate adapter. Changing it
   rebuilds only the current supported watch presentation when possible.
6. Build from the integrated snapshot that includes the working UI, auth,
   SponsorBlock and speed changes. Provide the new unsigned SideStore IPA with
   its aggregate source fingerprint and the checkpoint revision. Do not run
   automated tests under the user's instruction.

The user's device reports establish runtime behavior. Completion requires both
native likes and estimated dislikes in the paired pill on clean and subsequent
launches, balanced padding, working native vote/unvote state, and counts visible
during miniplayer expansion without playback interaction. Until observed, the
release must remain marked device-unverified rather than claiming success from
hook installation or compilation alone.
