# Miniplayer and Shorts app shortcut adapters — 0.3.3

These are selected equivalents of the local Android miniplayer and Hide Shorts components patches, implemented against the decrypted YouTube 21.39.4 binary. The options are off/native in defaults, expanded and SideStore-auth. The all-candidates preset enables the six new boolean options but retains native numeric defaults. Reopen the miniplayer after changing size or layout preferences. Device behavior remains unverified.

| Configuration | iOS behavior | Limit |
|---|---|---|
| `miniplayer_disable_drag` | Rejects pan starts and suppresses controller/layer pan handlers | Applies to the native floating miniplayer; does not suppress unrelated gestures |
| `miniplayer_disable_horizontal_drag` | Zeros horizontal translation/velocity inside the two miniplayer pan handlers and the controller's explicit delta/velocity forwarding | Vertical movement/dismissal remains native; UIKit getters are scoped to the exact recognizer and executing thread |
| `miniplayer_disable_double_tap` | Suppresses the miniplayer controller's double-tap handler | Separate from full-player double-tap seeking |
| `miniplayer_hide_subtext` | Supplies nil message text and disables the Premium badge flag | Does not hide title/channel labels or the independent native ad badge |
| `miniplayer_square_corners` | Replaces the existing content shape mask with a rectangle after native masking | Retains native masks during collapse/expand transitions and for unexpected content/mask types |
| `miniplayer_min_dimension_points` | Changes the native minimum-dimension constant | 0 preserves native; otherwise 170–480 points, clamped to the current app window's short edge minus 32 points. Too-small/invalid windows retain native. This does not force the complete player frame |
| `miniplayer_overlay_opacity` | Multiplies native alpha for the two circular control-background classes | 0–1, default 1; preserves native fade alpha and does not change video, text or button alpha |
| `hide_shorts_shortcut` | Removes actual shortcut items whose exact type is `com.google.ios.youtube.shorts` | Preserves Search, Subscriptions, Create and unknown types. Cached SpringBoard items can require relaunch |

The prior `classic_miniplayer` experiment toggle remains available. Complete Android miniplayer type variants, expand/close and rewind/forward button preferences, title/channel label suppression and individual widget buttons remain unported. Removing app extensions for SideStore removes entire widgets and other extension integrations.

## Evidence and hook contracts

The input IPA SHA-256 is `37fd59f89d706fb7f93614e12ddb9c09fe3f4a18c1e4609c2b715db6e9f3d88c`; the extracted executable SHA-256 is `ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf`. These anchors come from read-only Ghidra decompilation and extracted Objective-C metadata. Runtime hooks resolve class/selector names and reject incompatible type encodings; they do not use fixed addresses.

| Binary method | Address | Evidence |
|---|---|---|
| `YTWatchFloatingMiniplayerViewController -didPan:` | `0x1015869b0` | Reads recognizer state, translation and velocity, then forwards explicit CGPoint values |
| `-didPanMiniBarWithRecognizer:state:delta:velocity:` | `0x101584ea8` | Native vertical dismissal and horizontal/vertical velocity comparisons; preserve vertical arguments and native fade logic |
| `YTMiniplayerLayerView -didPanMiniplayer:` | `0x1014ea254` | Reads both translation and velocity, including ended-state projection; a position-only override would leave horizontal snapping active |
| `YTWatchFloatingMiniplayerViewController -didDoubleTap:` | `0x101585214` | Native miniplayer expansion/toggle delegation |
| `YTWatchFloatingMiniplayerBadgeView -setMessagingText:showingPremiumBadge:` | `0x10158a338` | Nil clears message/Premium presentation while ad-playing badge handling is independent |
| `YTWatchFloatingMiniplayerWithPersistentControlsView -maskMiniplayerView` | `0x10158d62c` | Builds a content-bounds CAShapeLayer; collapse/expand uses transition-specific geometry |
| `YTWatchMiniplayerConstants +minimumDefaultDimension` | `0x1034aa3e4` | Returns the native 192-point minimum, consumed when constructing layout input |
| `YTQuickActionsController -shortcutItems` | `0x10066a308` | Constructs native app shortcuts with distinct exact type strings |

The Android reference is the local `extensions/youtube/src/main/java/app/revanced/extension/youtube/patches/MiniplayerPatch.java` and `patches/src/main/kotlin/app/revanced/patches/youtube/layout/hide/shorts/HideShortsComponentsPatch.kt`. Android resource IDs, launcher/widget resources and miniplayer variants are not transplanted.

Horizontal getter overrides are active only while the exact recognizer is inside a verified miniplayer pan handler. Nested scopes restore their predecessors, and exceptions clear the scope in `@finally`. Other recognizers and threads retain native getter results. Size reads the verified `YTUIUtils +appBounds` aggregate via NSInvocation, preserves the native result on incompatible/invalid inputs, and shares a tested finite-bound helper. Opacity uses class-local inherited setter overrides, retains an unmodified native-alpha baseline and guards inherited patched implementations against applying the factor twice. No global UIView alpha hook is installed.

## Verification scope

The ARM64 iOS payload builds with warnings treated as errors. The static hook check has 77 metadata matches, zero mismatches and two runtime-resolution entries for the system UIKit pan getters, which are absent from the extracted app metadata. Loop-generated circular-background setter/layout hooks remain outside that static count.

`tests/miniplayer.m` compiles the production runtime and miniplayer include against UIKit-shaped macOS fixtures. It exercises 15 installed hooks, argument/state forwarding, native/profile-disabled fallbacks, nested/exception/cross-thread scope cleanup, independent ad badges, transition masks, finite/window dimension bounds, opacity restoration/inheritance, and exact shortcut filtering/refresh. It verifies production adapter contracts, not Apple's UIKit or the installed YouTube UI. The C geometry/opacity helper also runs on Windows. Actual iPhone layout, gestures, animation and SpringBoard cache behavior require the checks in [DEVICE_TESTS.md](DEVICE_TESTS.md).

The 0.3.2 authentication implementation is retained unchanged. A successful build or miniplayer test does not establish Google sign-in success.
