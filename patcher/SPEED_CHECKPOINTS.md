# Playback speed checkpoints (0.3.19)

Use ReVanced settings > Hook diagnostics > **Copy speed checkpoints**. The full diagnostic report includes the same data under `playback_speed`. Keep diagnostics enabled. Reports include version, source fingerprint, a random process session ID and a launch number so before/after reports can be distinguished. No video IDs, URLs, credentials or account details are recorded.

For a useful first-launch capture:

1. Open an ordinary video and let it play. Open YouTube's native speed picker.
2. If the initial rate is blank or the slider is at the far left, copy speed checkpoints before closing the app. This captures picker reads even if no selection reaches the player.
3. Select **1.25x**, leave the video playing for at least two seconds, then copy speed checkpoints again. Do this before resetting to 1x or restarting.
4. If restarting changes the behavior, repeat the same sequence and keep both reports. `session_id` must differ and `launch_number` should increase. The source fingerprint should match.

There are two native picker routes. `overlay_*` and `legacy_*` events describe the overlay delegate route; `bridge_*` describes the newer command adapter. Seeing one route without the other is normal. Report 755 before reload used the overlay delegate; after reload used the command adapter. The previous missing-player recovery was not exercised in either report.

| Checkpoint | Expected evidence | What a failure narrows down |
| --- | --- | --- |
| `picker_opened` / `picker_open_returned` | Current ordinary content and available active rate/model | A missing picker-open event means the menu enters through another native path; it does not prove the menu was not opened. |
| `overlay_state_refreshed` | Cache matches the current player's active video | Missing native ownership or a cache/update ABI that could not be resolved. |
| `legacy_rate_read` / `overlay_rate_read` | Positive `returned_rate`; `recovered` says whether a missing native read was repaired | Picker initialized with an empty/invalid rate. |
| `legacy_model_read` / `overlay_model_read` | `model_present: true` | Picker initialized without its native rate/scalar model. |
| `bridge_rate_read_started` / `bridge_call_returned` | Rate getter result available, no native error | Missing command owner, getter result or adapter error. Consult `command_bridge`. |
| `overlay_*_selection` or `bridge_selection_received` | Argument matches the chosen rate | Selection never left the picker, or it sent a different/empty argument. |
| `selection_received` | Intended rate and explicit native source | Selection was not captured for the current player. Invalid/automatic models are preserved without forced retries. |
| `player_model_setter_before` / `after` | Argument and observed rate | Native overlay forwarding failed, or the player has not accepted it. |
| `core_model_setter_before` / `after` | Correct argument and native core state | The player forwarded the choice; the native core is pending, clamping or ignoring it. Local code accepts in state 6, queues in state 1 and ignores intermediate states. State numbers are version-specific. |
| `native_rate_changed` | Callback rate matches the chosen rate | The native event propagated; compare delayed readbacks for a subsequent reset. |
| `selection_readback` | Matching observed rate at 0/100/350/1000 ms | A transient acceptance followed by initialization/restoration, or an unresolved native rejection. |

`recent_selections` preserves the last 12 choices with initial snapshots, actual elapsed times, scheduled delays and outcomes. `native_rate_not_confirmed_after_retry` is a failed final confirmation. `waiting_for_current_player`, `waiting_for_ordinary_content` and `intent_cancelled` distinguish gating from an applied-but-reset rate. Old content intents never apply to another video. `recent_events` contains at most 256 events with sequence numbers and monotonic milliseconds from hook installation; `events_dropped` reports truncation. Native adapter flags are observed, not forced. Native rate confirmation is a model observation, not a media timing measurement.

The 0.3.19 fix refreshes the older overlay's cached native video state through its own update method and recovers missing rate/model reads from the same current ordinary-content player. It captures overlay selections before forwarding, so a missing intermediate menu delegate cannot lose the user's intent. It retains native limits, scalar models and the earlier bounded retry policy. No tests run; first-launch device confirmation remains pending.
