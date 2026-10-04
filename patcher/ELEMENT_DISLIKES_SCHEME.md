# Element-based Return YouTube Dislike adapter - 0.3.10

The user report from 0.3.9 has `count_available: true` and zero slim-action bindings/accessories. Its screenshot shows a combined like/dislike pill. The network fetch works; the two slim UIKit adapters never bind this modern action surface. The same report has four SponsorBlock marker rectangles and two automatic ranges. The user separately confirms SponsorBlock works.

0.3.10 adds a separate experimental native element adapter. This preserves the merged 0.3.9 settings UI, working authentication source, existing slim adapters and SponsorBlock implementation.

## Native evidence and binding

`CompositeIconController imageForIcon:` calls `YTIconResolverImpl imageForIcon:color:` and can tint the result into a different image. A thread-local identity carries the resolved icon across this synchronous call; no C++ pointer or fixed function address is read. `YTAppIconSupportProviderImpl delhiResourceMap` returns a static, once-initialized native asset dictionary. Exact ThumbDown/ThumbUp asset prefixes cover the native outline/fill sizes. Toolbar resources independently identify IDs 0x250/0x238 and 0x251/0x239.

`ELMContainerNode`, its controller, owning component, and `ASDisplayNode` expose guarded Objective-C bridges for the element tree. The observer follows Yoga children (including flattened/layer-backed nodes), not only UIView subviews. It considers a single down/up image pair within the active watch hierarchy, outside the player overlay. An owning component must identify the video action bar or segmented like/dislike model. The native `ELMElement protoText` bridge must expose one or more explicit video IDs, all matching the active fetched video ID. Failed proofs leave native controls unchanged. Model text and commands are neither retained nor included in diagnostics; only boolean binding results are cached per element/video.

## Layout and lifetime

A matched dislike branch receives one owned `ASTextNode` through `appendYogaChild:`. Native Yoga measures and lays out the extra text with the button; no native icon, command, hit target, background, or existing label is replaced. Native neighboring text supplies font/color, with Dynamic Type defaults. Compact localized counts and an accessibility estimate are supplied. Native controller updates can rebuild children; only the owned node is reattached. The node is removed when the feature turns off, the video changes, or a binding fails. Original callbacks always run.

The image registry is capped at 64, node scans at 128, view scans at 2048, ancestor walks at 6/12. Model text is rejected above 128 KiB. Color/text updates compare the existing attributed string to avoid triggering repeated layouts. New image loads and native container layout callbacks discover controls created after the count arrives.

## Evidence and limits

[Device failure](profiles/device-0.3.9-ryd-display-failure.json) and [native metadata/source hashes](profiles/element-dislikes-evidence.json) record the basis for this adapter. New diagnostics live at `return_youtube_dislike.element_layout`: icon/pair/semantic-role/video-binding counters and `owned_text_nodes`. They omit model text, target IDs, labels, cookies, tokens and account data.

Compilation and production packaging are permitted; tests remain disabled at the user's request. No tests or development service votes are performed. This adapter is device-unverified. In particular, the failing device's component key and target representation have not yet been confirmed. Missing role or video binding is reported and causes no insertion. Online Shorts and automatic native vote forwarding remain unported.
