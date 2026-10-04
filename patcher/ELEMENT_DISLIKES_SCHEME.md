# Native dislike text rendering — 0.3.13

Report 511.txt confirms the 0.3.12 adapter fetches the count, binds the native dislike branch and owns one text node with a positive layout frame. The screenshot shows the native like count but no dislike number. A Yoga frame is insufficient evidence of visible rendering.

Read-only native decompilation identifies the coordinate mismatch: ASDisplayNode +yogaNode enables both Yoga and view flattening. In the native Yoga render callback (0x104712a64), only flattening-enabled nodes accumulate offsets of skipped Yoga ancestors before setFrame. An owned plain ASTextNode previously enabled Yoga only. Its frame could therefore be applied in the flattened render host with button-relative coordinates. The same callback manages render subnodes, so manual addSubnode calls would conflict with native reconciliation.

0.3.13 enables view flattening before appending the owned text node. It inherits typography from the nearest native action ancestors rather than searching only the icon-only dislike branch; fallback colors come from YouTube's page-style palette. Native children, like counts, hit targets and commands retain their original implementation. Reattachment invalidates the native Yoga root. Authentication, SponsorBlock and the previously packaged settings snapshot are retained.

Schema 4 diagnostics distinguish positive Yoga frames, actual render supernodes, loaded text layers, attachment to a visible window, clipping ancestors and geometric visibility. Geometry is bounded to eight owned nodes and records no count text, video IDs, account data or commands. These checks do not prove that pixels were displayed; device confirmation remains outstanding. Fixed button constraints may still require further adaptation if the report shows clipping.

[Device evidence](profiles/device-0.3.12-ryd-render-failure.json) omits authentication and account events. [Native evidence](profiles/element-dislikes-evidence.json) records the renderer contract and source hash. Compilation and normal production packaging are performed; no tests, hook verifier or GUI smoke checks are run at the user's request.
