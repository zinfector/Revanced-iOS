# Native button count mounting - 0.3.15

The source fingerprint in RYD Report 615 identifies 0.3.14. Its retained foreground watch snapshot clears the service, video binding, icon discovery and text measurement checkpoints. Four root layout passes completed while the text remained without a render parent or loaded layer. This is an actual mounting failure, rather than a Settings-navigation artifact.

Native ASDisplayNode addSubnode: delegates to _insertSubnode:atSubnodeIndex:sublayerIndex:andRemoveSubnode:. The latter refuses a child whose ASNodeContext identity differs from the parent. Node initialization also uses that context to configure locking. The adapter therefore never overwrites a node context or mutates internal C++ memory.

0.3.15 mounts only within an already loaded, nonflattened native dislike button view. It tries native addSubnode when context identities match. If insertion is unavailable/rejected or contexts differ, it preserves the ASTextNode as the existing Yoga sizing child and renders the same attributed text in one owned, noninteractive UILabel in the native button, using the measured button-local frame. The count label does not capture taps; native icons, commands, other children and backgrounds remain intact. Rebinding/removal cleans up only the owned surfaces. Root layout completion and ordinary adapter refresh keep the frame synchronized.

Schema 6 geometry reports mount mode/gate/attempts, native context equality, native text supernode/loading separately from the effective count surface, label ownership and bounds fit. Existing foreground snapshots, callback counters, raster contents, actual layer-to-window ancestry and clipping diagnostics are retained under ryd-checkpoints-3. AS text callbacks may stay zero for the owned UILabel mode, while the effective mount/load/window checkpoints can pass. Raster contents do not prove visible glyphs. Diagnostics inspection remains passive; production mounting is a separate operation.

The device has not verified the new count rendering, constraints or reuse. Authentication and SponsorBlock are retained. Tests, hook verification and GUI smoke checks are skipped at the user's request. [Device evidence](profiles/device-report-615-ryd-mount.json) and [native contract/source evidence](profiles/element-dislikes-evidence.json) are retained.

## 0.3.16 spacing and miniplayer return

Report 635 and its screenshot confirm 0.3.15 mounts and draws the count. Its text begins at x24 with no intended gap: native ASLayoutElementStyleYoga.setSpacingBefore: is a no-op. Use the guarded nine-dimension setMargin: ABI with an eight-point logical start margin on the owned text only; native layout reserves the gap and label uses that frame. No native button padding or like text is changed.

During miniplayer moves the watch player temporarily detaches. Preserve and hide the same-video owned node during that interval rather than deleting it. Bind to the watch player’s actual current video ID rather than a cached player pointer. Appearance, native player setter and watch layout ownership changes trigger four bounded main-thread scans (0, 250, 750, 1500 ms), without a network request or playback interaction. Feature disable/video changes and invalid model reuse still remove owned content. Root remounting also runs with diagnostics off. New diagnostics record margin reservation, suspension/resume counters, transition events and separate player pointer/content matches. Device behavior remains unverified; no tests ran.

## 0.3.17 outer edges and interactive return

Measure the nearest semantic native pair containing exactly two populated icons and native like text. Add an owned logical end margin to mirror the pair’s measured leading inset; keep native padding and like labels unchanged. Skip ambiguous/unmeasured pairs. Diagnostics expose both insets and the owned end margin.

Refresh at native watch will-appear and gesture begin/end, synchronously and at 16/64/250/750 ms. A previously verified same-video model may mount while its player is briefly detached, provided its exact component/element ownership and existing watch-view ancestry still match. Per-frame native watch layout remounts only the small owned-label set. Feature/video/model changes still invalidate owned text. No tests run; device confirmation pending.

## 0.3.18 reload binding

The after-reload Report 728 has count data and a native dislike icon but rejects 48 candidates at semantic_role_missing. Traverse bounded parent and owningComponent links starting at the actual node controller, inspect native key/templateURI plus the existing bounded proto bridge, and expire negative role checks after 250 ms. Positive binding still requires the same element and video. Explicit mismatched video IDs remain rejected. No icon-only fallback or comment-button binding. Diagnostic schema 9 adds semantic_binding_probe with owner classes, role source, proto availability/size and video/parent flags; raw model text, IDs and account data are excluded. Native like labels and arguments are preserved.
