# Compiler delivery correction — 0.3.26

The user reports that this agent's compiled IPAs crash. The 0.3.25 SponsorBlock delivery used Windows Zig and the Theos iPhoneOS16.5 SDK. Inspection of the separate release checkout and its successful cloud-build receipt established a different delivery process: compile with Apple Xcode on macOS, download the native payload, then inject/package it on Windows.

The comparison baseline is `adblock-handoff-release-0.3.24`, whose native manifest records Xcode 16.4, iPhoneOS18.5.sdk, `xcrun --sdk iphoneos clang`, and `arm64-apple-ios17.0`. The currently registered `adblock-display-release-0.3.25` worktree inherits that cloud workflow but has no new completed build artifact yet. Its ongoing source work was left untouched.

## Replacement artifact

Version 0.3.26 builds the same SponsorBlock prompt implementation with the baseline's Apple compiler/SDK/target. The native-only workflow pins Xcode 16.4 and contains no test steps. [Cloud build 37275905998](https://github.com/zinfector/Revanced-iOS/actions/runs/37275905998) succeeded at payload source commit `207ac3e26a7783f480f992ab31a42d0eb941b1f1`. Compiler provenance records Apple clang 17.0.0 (`clang-1700.0.13.5`), Xcode build 16F6 and iPhoneOS18.5.sdk.

The downloaded library is 1,253,600 bytes, versus 1,497,164 bytes for the reported crashing Zig delivery. Signing load-command header padding is 28,424 zero bytes, and the install name remains `@executable_path/Frameworks/RVPort.dylib`. These are compiler/build differences; file size alone does not explain a crash.

The production patcher packages the downloaded Apple-built library in the original analyzed YouTube 21.39.4 IPA, with the same two SideStore profiles and extension removal as before. Its archive self-check is disabled under the user's no-tests instruction. The new release packaging script rejects non-Xcode payloads, requires the established compiler/SDK provenance, and checks that the payload and local native source hashes match the cloud manifest before injection.

Use the 0.3.26 IPAs in `ReVanced/patcher/output`; they supersede this agent's 0.3.25 Zig IPAs. These outputs remain unsigned and require the normal signing/install flow. Existing user preferences, including ad strategy, still override bundled defaults.

## Evidence limits

No crash report was available. The compiler mismatch is established, but its causal connection to the reported crashes and the replacement's device launch remain unverified. No regression, GUI, hook, archive or device tests were run. Source comparison, manifest inspection, compilation and packaging are the evidence provided.

`profiles/compiler-delivery-correction-0.3.26.json` records both compiler manifests, byte/normalized source differences, payload hashes and cloud provenance. `profiles/sponsor-prompts-implementation-evidence.json` and the output build receipt record the packaged IPA hashes and preserved baseline modules. The source archive contains the corrected workflow and release packager.
