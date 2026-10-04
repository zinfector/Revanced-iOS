# ReVanced settings inside YouTube — 0.3.4

Open **YouTube Settings → ReVanced**. The dedicated screen groups all 79 patch switches and 34 additional runtime preferences into 13 areas: Ads, Player, Video quality, Shorts, Miniplayer, Seekbar and gestures, Layout and appearance, Thumbnails, Links and downloads, Return YouTube Dislike, SponsorBlock, Authentication and Advanced. Search covers preference titles, groups and descriptions. The original three-finger one-second hold opens the same screen as a fallback.

Each group contains switches, choice pickers and validated value editors. SponsorBlock categories use switches; category behaviors/colors and per-screen thumbnails have individual override rows. Filter patterns and custom speeds have a line-based editor. The installed app name and configuration schema are packaging metadata, so they are not presented as runtime controls. Custom header images still require rebuilding the IPA with the supplied image.

Changes use the existing `RVPort.<key>` preferences, so settings from earlier versions carry over. Import accepts the iOS patcher's JSON configuration, validates every field before saving, leaves missing settings unchanged and ignores the packaging-only app name. Copy configuration exports the effective settings for import into another installation or the Windows patcher. Reset removes only the known runtime preference overrides and restores the preset bundled in the IPA. It does not delete accounts, credentials, SponsorBlock/RYD identities or unrelated preferences. Android XML preferences are not compatible with this format.

Reopen the affected video/screen after changing response, quality, thumbnails or layout. Restart after changing either authentication switch. Video tools, hook diagnostics, import/export, reset and About are available from the ReVanced root screen. Root and detail screens use native UIKit navigation, Dynamic Type labels, light/dark appearance, iPad popover anchors and a keyboard-aware text editor.

## Native entry strategy and binary evidence

The local Android `misc/settings/SettingsPatch.kt` adds a ReVanced entry and categorized preference screens. The iOS implementation uses verified native settings models rather than modifying Google's server-returned settings data or globally intercepting its collection view.

| Method | Address | Contract |
|---|---|---|
| `YTSettingsViewController -setSectionControllers` | `0x1014578f4` | Builds the final native settings presentation; scoped build wrapper |
| `-viewDidLoad` | `0x10145530c` | Native setup first, then initializes the entry so it can appear before a settings response |
| `-settingsSectionControllers` | `0x101455428` | Returns the category-to-controller dictionary |
| `-setSectionItems:forCategory:title:icon:titleDescription:headerHidden:` | `0x101455cd0` | Constructs a native section; category is an unsigned 64-bit value |
| `-updateSettingsSectionControllersForCategory:withSettingController:sectionItems:headerHidden:` | `0x1014561c0` | Stores the native section under an NSNumber category key |
| `YTSettingsSectionItem +itemWithTitle:titleDescription:accessibilityIdentifier:detailTextBlock:selectBlock:` | `0x10186f214` | Creates the native ReVanced row with a selection block |
| `YTSettingsSectionController -didSelectItemAtIndex:fromView:` | `0x10186eab8` | Calls the selection block with the source view and item index, then tests its Boolean return |
| `YTAppSettingsPresentationData +settingsCategoryOrder` | `0x10149ad6c` | Linear menu category order |
| `YTSettingsGroupData -orderedCategories` / `-type` | `0x101467cfc` / `0x101467cf4` | Grouped menus; group type 2 is Video and audio preferences |
| `YTSettingsViewController -showOrPushViewController:` | `0x10145582c` | Uses the native presentation delegate for split layouts, or pushes through the navigation controller |

The entry uses reserved category `0x52565054`, a native section item, and a weak reference to the originating settings controller. It refuses category collisions and malformed models instead of overwriting another section. The linear/grouped ordering hooks append the category only while the exact YouTube settings controller is building its sections, on the executing thread, and only after verifying ownership of the created section. Grouped menus append it only to group type 2. Native arrays remain unchanged; nested scopes and exceptions restore the prior scope. Other consumers retain their original arrays.

The containing section has no title and hides its header. The verified grouped-menu constructor recognizes this as a single-row action, calls its selection block, and retains the row title **ReVanced**. A titled section would instead route through Google's native category-detail endpoint and add an unnecessary intermediate screen.

The supported binary identity is unchanged: decrypted YouTube 21.39.4, executable SHA-256 `ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf`. All hooks resolve class/selector names and check exact method encodings; fixed addresses are research anchors only. Metadata and local read-only research hashes are recorded in `profiles/settings-evidence.json`.

## Verification limits

The complete UI is compiled for ARM64 iOS with warnings treated as errors. The macOS host harness executes the production preference model and settings bridge with native-shaped fixtures. It checks validation bounds/types, import rejection before mutation, export, reset isolation, search, native section construction, duplicate/collision handling, callback ownership, grouped/linear ordering, other-thread isolation and nested/exception cleanup. It does not render Apple's UIKit or execute the installed YouTube menu. Actual entry visibility, navigation, keyboard/search behavior and preference persistence need the device checks in [DEVICE_TESTS.md](DEVICE_TESTS.md).

The authentication implementation that the user reports now permits login is preserved unchanged. That observation establishes user-reported login success; it does not establish token refresh, cold-relaunch persistence or every patch's behavior.
