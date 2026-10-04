# Native Return YouTube Dislike integration, 0.3.8

The existing Return YouTube Dislike toggle now supplies fetched estimates to the native slim dislike action label and matching main-video entity count/accessibility getters. The standalone badge remains a fallback for other layouts. Service votes still require an explicit action in Video tools.

The local Android implementation intercepts vote and text paths that differ from iOS. Read-only decompilation of the supplied YouTube 21.39.4 binary establishes the iOS paths in [native-dislikes-evidence.json](profiles/native-dislikes-evidence.json):

- `YTSlimVideoScrollableDetailsActionsView` creates action views from supported renderers and identifies the dislike renderer with `slimButton_isDislikeButton`.
- Its delegate setter assigns the action's delegate to the slim action controller. The controller's native `setCell:` reads `delegate.videoId` for the owning video.
- `YTSlimVideoDetailsActionView` creates a `YTFormattedStringLabel`, uses the renderer's default/toggled formatted strings and lays out its native icon and label. Its element-backed variant has a separate rendering path.
- `YTMainVideoEntityModel.dislikeCountString` reads the model's numeric `dislikeCount`. The model exposes its own `videoID`; offline metadata construction uses the numeric count in the native action-bar renderer.

The adapter binds only the verified dislike renderer and action class. It requires a matching video ID before reading the existing active-video estimate, runs cache and UI changes on the main queue, and keeps the original result for off-thread model getters. The existing fetch retains cancellation, generation checks, bounded replies, retry delay and a short cache. Counts must be finite, nonnegative integers no larger than 10^15, including valid zero estimates.

Before native action layout, it updates only the action's own formatted label using the native formatted-string constructor. Native sizing, icon, selected state, fonts and gesture handlers remain responsible for layout. A weak registry invalidates existing controls when the count, video or preference changes. On a mismatch, reuse or disable, it restores the renderer's currently selected native formatted string. Accessibility includes the original action description and identifies the count as an estimate. No global UILabel setter is hooked and no label is added to an unknown element hierarchy.

The diagnostic report has a redacted `return_youtube_dislike` section with native bindings, text/model activity and count/request availability. It contains no video ID, credentials or service identity.

Remaining work: modern segmented/element-backed action text, online Shorts rendering, automatic forwarding of confirmed native votes and rollback handling, and complete native styling. The native like-service status values were traced as 0=like, 1=dislike and 2=remove-like; observer notification alone does not prove a user vote succeeded, so it is not used to send service votes. Development sends no votes or submissions.

Compilation and normal production IPA construction are the available build evidence. No regression, static-hook, GUI smoke or device tests are run, per the user's instruction. Device rendering and restoration remain unverified. Authentication and SponsorBlock sources are unchanged by this adapter.
