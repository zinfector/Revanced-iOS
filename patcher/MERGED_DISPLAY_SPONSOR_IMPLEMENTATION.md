# Merged display ads and SponsorBlock prompts — 0.3.28

This release merges the completed `build/sponsor-prompts-xcode-0.3.26` worktree at `207ac3e` with the display-ad implementation at `25607ee`. The shared base is `e0a6197`. The two-parent merge retains both implementations and resolves their shared diagnostics, README and release metadata changes.

Included behavior:

- Typed Home/feed grid, carousel and shelf ad filtering, including native insertions.
- Legacy below-video companion banner clearing and scoped native Elements section/provider clearing.
- Display-ad checkpoint version 3, alongside the existing video-ad coordinator checkpoints.
- Native SponsorBlock Ask-to-skip controls, confirmed Undo, replay suppression and `sponsor-prompts-1` checkpoints from 0.3.26.
- Existing authentication, speed, vote-counter and coordinator-handoff fixes.

SponsorBlock's private native skip control remains available: display filtering targets advertising models and exact companion sections. It does not globally hide ad-named views or controls. The merged full diagnostic report contains both `adblock.display_ads` and `sponsorblock_prompts`; the individual diagnostic-copy actions are retained.

The separate `shorts-layout-release-0.3.27` checkout was still being edited at integration time. Its uncommitted preparation and pending Shorts implementation are not included. That worktree and the original prompt/display worktrees remain intact.

## Build and packaging

The cloud build pins `/Applications/Xcode_16.4.app/Contents/Developer` and records compiler/SDK provenance. Release packaging requires Apple clang, Xcode 16.4 / iPhoneOS18.5, a matching payload hash, and the exact cloud native-source set. Windows/Zig output is not used for this release.

`package_merged_release.py` uses the production patcher's original IPA/profile/config/payload checks. It disables the archive self-check under the user's no-tests instruction. Regression tests, static-hook tests, GUI smoke tests and device tests are not executed. Compilation and packaging do not establish device behavior.

The merged outputs have **`-merged`** in their filenames so they remain distinguishable from other parallel artifacts with the same version. For the native video-ad strategy and SideStore, use:

`YouTube-21.39.4-RVPort-0.3.28-SideStore-auth-native-ads-merged-unsigned.ipa`

It includes the expanded authentication preset and removes app extensions. Sign/install with SideStore. The `SideStore-auth-merged` variant uses response filtering; existing saved ad-strategy preferences still override the bundled preset.

## Capturing remaining issues

For a remaining Home or below-video banner, copy ad-block diagnostics while that surface is current, then inspect `display_ads.last_visible_surface`, `last_native_operation`, classification/removal counts, native section/provider clearing and inset checkpoints. Unknown opaque/header roots and direct advertising replacement removal remain explicit conservative passthrough cases. See [display-ad details](ADBLOCK_DISPLAY_IMPLEMENTATION.md).

For native skip/Undo behavior, copy SponsorBlock prompt checkpoints and inspect `sponsor-prompts-1` ownership, native-control readiness, hit-test/tap, seek-confirmation, timeout and replay-suppression fields. See [the inherited prompt implementation](SPONSORBLOCK_NATIVE_PROMPTS_IMPLEMENTATION.md); its historical 0.3.26 references describe the source worktree, while this combined build reports version 0.3.28.

The merge inputs and resolutions are recorded in `profiles/merged-display-sponsor-source.json`. The release receipt records the actual merged cloud source commit, compiler identity, payload hash and IPA hashes. Display-ad and native-prompt device behavior remain unverified.
