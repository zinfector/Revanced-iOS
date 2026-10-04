# Sideloaded YouTube sign-in: analysis and experimental adapter

Status: **implemented experimentally in 0.3.1; successful device login remains unverified**. The separately named SideStore auth IPA enables both adapters. Older 0.3 IPAs remain unchanged. The user supplied a device screenshot of Google's account page rejecting sign-in with “Google can't confirm that it's safe.” It proves the app reached account authentication, but does not identify the server error code, prove hook installation, or establish successful playback. The target is the user-reported iPhone 17 Pro Max / iOS 27.0. No signed installed IPA, entitlement dump or sanitized authentication trace has been captured.

The implemented experiment corrects the identity that YouTube's SSO configuration sends and use a keychain group actually granted to the installed app. A browser replacement or an Android microG transplant is not the first experiment supported by this binary.

## What Android ReVanced does

The local shared `misc/gms/GmsCoreSupportPatch.kt` rewrites Google Play Services package strings, permissions and content authorities to the configured GmsCore vendor, normally `app.revanced`. YouTube gets a separate package name, normally `app.revanced.android.youtube`. The manifest carries `app.revanced.android.gms.SPOOFED_PACKAGE_NAME` and `SPOOFED_PACKAGE_SIGNATURE`: the original `com.google.android.youtube` and its certificate SHA-1 `24bb24c05e47e0aefa68a58a766179d9b613a600`. Service availability checks are also adapted and the extension checks that GmsCore is installed.

This is more than renaming the app. In [ReVanced GmsCore's AuthManager](https://github.com/ReVanced/GmsCore/blob/776a8fb7a3887fdee24df854b673dc79df5c9da7/play-services-core/src/main/java/org/microg/gms/auth/AuthManager.java#L332), `PackageSpoofUtils` supplies those metadata values to the token request. [AuthRequest](https://github.com/ReVanced/GmsCore/blob/776a8fb7a3887fdee24df854b673dc79df5c9da7/play-services-base/core/src/main/java/org/microg/gms/auth/AuthRequest.java) serializes them as `app` and `client_sig`. GmsCore manages the Android account/token interface separately from the re-signed YouTube package. This does not mean ReVanced can satisfy every Google server policy or cryptographic attestation.

The reusable idea is an authentication-specific identity/token-storage adapter. Android's AccountManager, Binder services, certificate digest and APK metadata do not directly apply to iOS.

## What the analyzed iOS IPA actually does

Evidence is from the exact supported decrypted YouTube 21.39.4 binary, SHA-256 `ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf`. Ghidra was opened read-only, without reanalysis. Objective-C metadata establishes the signatures; decompilation and decoded message stubs establish the data flow. Addresses below are evidence anchors, not proposed fixed-address hooks.

| Native entry | Evidence and implication |
|---|---|
| `SSOConfiguration -initWithClientID:supportedAccountServices:` at `0x1001fb8dc` | Copies `NSBundle.mainBundle.bundleIdentifier` into the configuration's application identifier. Derives the callback scheme from the supplied client ID. Re-signing can therefore change the reported application identifier without changing the built-in client registration. |
| `SSORPCService` request-auth-advice block at `0x1045ae678` | Builds `client_id`, `mediator_client_id`, **`package_name` from `configuration.applicationIdentifier`**, `redirect_uri` from `authCallbackURLString`, and device/version/service fields. This is the concrete identity mismatch to test. It is not merely a guess about a generic `X-Ios-Bundle-Identifier` header. |
| `SSOBundleIdServiceImpl -bundleId` at `0x100c5fad8` | Independently reads the main bundle identifier. Trace its consumers before treating it as a second necessary override. |
| `SSOSafariSignIn -signInWithURL:presentationAnchor:completionHandler:` at `0x104099b04` | Creates and starts **ASWebAuthenticationSession** with the configured callback scheme. A supported system-browser path already exists. Runtime diagnostics must confirm which path this attempt used. |
| `SSOConfiguration -authCallbackURLString` at `0x1045ceb48` | Forms the auth callback from `applicationScheme`. Original Info.plist registers `com.google.sso.755541669657-kbosfavg7pk7sr3849c3tf657hpi5jpd`. Preserve the original matching client/scheme pair. |
| `SSOKeychainHelper +accessGroup` at `0x100202850` | Caches `sharedAccessGroup`. |
| `SSOKeychainHelper +sharedAccessGroup` at `0x1002028d4` | Formats `<computed application prefix>.com.google.common.SSO`. Both format and suffix were decoded from the referenced CFStrings, not inferred from the function name. This group is not necessarily granted by the new signer. |
| `SSOKeychainHelper +queryMatchingID:serviceName:` at `0x10058907c`, `+writeSharedKeychain:error:` at `0x1045cc6d4` | Uses that access group for credential reads/writes unless private-keychain mode applies. A successful browser login can still fail to persist. |

The older 0.3 adapter accepts a re-signed bundle ID for its own binary-profile check, but had no account-authentication identity or keychain adapter. Version 0.3.1 adds the two scoped adapters described below. Cloud signing derives capabilities from the supplied Apple profiles, and does not restore original Google groups. The installed SideStore-signed identity has not yet been observed.

Google documents iOS OAuth registration against bundle ID, with optional team/App Store identity and optional App Check protection. That supports the identity-mismatch hypothesis, but does not prove this client uses App Check or identify the screenshot's exact cause. See [Manage OAuth Clients](https://support.google.com/cloud/answer/15549257). Google separately rejects embedded user agents; use its supported native flow rather than a WKWebView fallback. See [OAuth for native apps](https://developers.google.com/identity/protocols/oauth2/native-app#authorization-errors).

## Implemented design and device experiment

### 1. Add narrowly scoped diagnostics first

`native/RVAuthentication.inc` installs its hooks during `RVStart`, after the supported binary/config check and before ordinary app initialization can construct SSO configuration. Use the existing `RVHook` signature guard; missing or changed methods must remain native and appear in the report.

Record whether the physical installed bundle ID matches the original separately from SSO's reported identifier. Raw bundle/team identifiers are not exported. Record whether the SSO configuration/client/callback tuple matches the original supported profile, which browser entry was used, callback/error presence, and numeric keychain OSStatus. Prefer booleans for group/entitlement comparisons; do not export Apple team identifiers or raw authentication material unnecessarily.

For URLs and requests, allowlist diagnostic fields. Report request host/path classification, presence/match of client ID and package name, callback scheme match, and presence/method of a verifier challenge. Never record full URLs, headers, request bodies, codes, verifier/state/nonce values, cookies, tokens, device challenges, account identifiers, email, passwords or unrestricted NSError descriptions. An error page may never call the app's completion handler; cancellation alone cannot reveal Google's rejection reason. If no sanitized error code is available, retain that uncertainty.

### 2. Adapt the SSO application identifier

The independently controllable experimental `sideload_auth_identity` setting is available in the CLI, GUI and native settings. For the supported YouTube client only, hook `SSOConfiguration -applicationIdentifier` (`@@:`) to report the original `com.google.ios.youtube` to the native SSO request builder. Preserve the original client ID, mediator client ID and registered callback scheme. Check the configuration's client and scheme before applying the override. Delegate unrecognized configurations to the original implementation.

A getter hook covers configurations constructed before the hook and avoids assuming private ivar offsets. It does not modify the physical Info.plist, signed application identifier or provisioning profile. Avoid a process-wide NSBundle override: signing, containers and unrelated SDKs need the real installed identity. Only add an `SSOBundleIdServiceImpl -bundleId` adapter if a traced consumer requires it; its existence alone is insufficient evidence.

### 3. Use a real, accessible keychain group

The separate experimental `sideload_auth_keychain` setting controls storage adaptation. Discover the installed app's usable default group through a dedicated harmless generic-password probe created without an explicit access group. Request only item attributes, never credential data. Use a unique RVPort service/account marker and safe accessibility; handle not-found/duplicate/error cases, release CF results correctly, and remove only a probe created by this invocation. Never fall back to deleting account items or inventing a team prefix.

Hook the two **present** `SSOKeychainHelper` class getters, `accessGroup` and `sharedAccessGroup` (`@@:`), to return the discovered group when enabled and discovery succeeds. On failure, retain native behavior and report the numeric failure. This covers native reads/writes and the cached accessor while keeping Apple entitlement enforcement intact. Do not add old-version `SSOKeychainCore accessGroup` hooks: that method is absent from this version's metadata. Trace attribution/Folsom storage separately if subsequent diagnostics show another unauthorized group.

Apple specifies that an item added without an explicit group uses the app's default group; an unauthorized group causes an error. See [kSecAttrAccessGroup](https://developer.apple.com/documentation/security/ksecattraccessgroup). This changes storage to the sideloaded app's authorized group; it cannot import the official YouTube app's account or grant cross-team Google SSO. A signing-team or bundle-ID change can also make old private items inaccessible.

There is precedent for adapting `SSOKeychainHelper` in [YTLitePlus source](https://github.com/YTLitePlus/YTLitePlus/blob/9cdde568d624da57a54bc9f32eb9357f2c49027d/YTLitePlus.xm). Its broader NSBundle identity workaround is commented out there. Treat historical workarounds as evidence to investigate, not proof of compatibility with 21.39.4 or iOS 27.

### 4. Preserve native authentication checks and isolate browser failures

Keep native server challenge generation, token exchange, callback state/verifier validation and error handling. Do not substitute Safari's user-agent string in an embedded browser or remove challenges. If diagnostics establish an embedded-browser rejection, use the already present system authentication path with the existing completion/callback handling. The screenshot's browser appearance alone is not sufficient to select that branch.

### 5. Build a device experiment with explicit acceptance criteria

The separate `YouTube-21.39.4-RVPort-0.3.1-SideStore-auth-unsigned.ipa` is built from the supported original. It retains the previous 71 features, removes extensions, and enables the two new authentication switches through `configs/sideload-auth.json` (32 enabled switches in total). Defaults and the ordinary expanded preset leave the authentication switches disabled. Build the native payload on the macOS runner, run ABI checks against these exact selectors, and test privacy filtering using synthetic URLs containing secret-like values. Test a configuration constructed before hook installation and native fallbacks for unknown clients or failed group discovery.

On the target device, compare baseline, identity-only, keychain-only and both enabled, using an app restart after changing authentication settings. Success requires completing Google sign-in, account visibility, an authenticated YouTube operation, survival across cold relaunch, token refresh and survival across an ordinary SideStore refresh with the same signing identity. Report attempts and failures rather than silently retrying or clearing user accounts. Playback/features require their own checks.

If the corrected metadata tuple is still rejected, investigate the returned server error and challenge path before another change. A string override cannot satisfy a cryptographic requirement for Google's signing team, app attestation, or a registration policy controlled by Google. An OAuth client registered for our own app can authorize permitted public YouTube API scopes, but has not been shown to supply the private SSO token/services expected by the native YouTube app; it is not a drop-in account-login replacement.

## Evidence files

[profiles/authentication-evidence.json](profiles/authentication-evidence.json) records the binary identity, ABI anchors, source revisions and local research hashes. The full local Ghidra outputs are `build/auth-research-decompiled-annotated.txt`, `build/auth-flow-decompiled-annotated.txt`, `build/auth-advice-decompiled-annotated.txt`, `build/auth-request-builder-decompiled-annotated.txt` and `build/auth-keychain-disassembly.txt`. They are local analysis artifacts, excluded from Git.

The 0.3.1 auth artifact is an unsigned device experiment, not a verified Google login fix. Existing 0.3 files are unchanged.

## Testing the new IPA

Install the separately named 0.3.1 SideStore auth IPA with SideStore, which signs it with your account. Keep the same signing identity/bundle identifier when updating if you want existing app data retained. Attempt sign-in once, then test account visibility and cold relaunch. If it fails, close the browser sheet, hold three fingers for one second in YouTube, open **Show hook diagnostics**, and choose **Copy diagnostic report**. The `authentication` section contains bounded redacted events, adapter application and numeric keychain/error statuses. A browser cancellation is not a Google server error code. No passwords, cookies, codes, state/verifier values, tokens, emails, raw URLs or raw keychain groups are included by the authentication reporter.

Both switches are under native settings. Restart after changing them; compare identity-only and keychain-only if needed. The production adapter is exercised by `tests/authentication.m` on macOS using fake SSO classes and injected keychain calls; it never signs in to Google or accesses real credentials during host tests. The shared `RVRuntime.inc` preserves the existing ABI guards and class-local hook behavior.

## Host verification and artifact

The [macOS/Windows workflow](https://github.com/zinfector/Revanced-iOS/actions/runs/37177944021) passed all 33 tests with no skips, including the production Objective-C authentication harness. Xcode built the ARM64 iOS payload; 65 literal/getter-array hook ABIs matched the supplied metadata. The packaged Windows GUI passed with 73 switches. Real-IPA verification retained 11,760 original members byte-identically and removed 1,718 signature/extension members; existing executable load commands, section bytes, chained fixups and ZIP CRC passed.

Recommended file: `output/YouTube-21.39.4-RVPort-0.3.1-SideStore-auth-unsigned.ipa`, SHA-256 `1481372967024a657c94381324b36a6f90148245adc9b0d80407de63d719bb70`, 140,331,042 bytes. The [release receipt](profiles/release-0.3.1.json) records all three new IPA configurations, payload/GUI hashes and host evidence. Host checks do not prove the device login succeeds.
