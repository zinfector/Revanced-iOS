# Sideloaded YouTube sign-in: analysis and experimental adapter

Status: **0.3.1 installed and its authentication hooks ran on the user-reported iPhone 17 Pro Max / iOS 27.0, but sign-in failed. The 0.3.2 revision below is awaiting device verification.** The user selected `YouTube-21.39.4-RVPort-0.3.1-SideStore-auth-unsigned.ipa` and supplied its redacted report. No successful login, token refresh, account persistence or playback has been observed.

The report proves profile acceptance, all 12 authentication hooks installed, the application-identifier adapter applied to the expected client/scheme tuple, the original callback scheme registered, and the system ASWebAuthenticationSession path used. It also records 28 failed group probes (`-50`) and repeated native reads with `-34018` (missing entitlement). The old probe combined API failures and missing returned group attributes into the same status, so the report cannot identify the exact probe stage that failed. The browser ended without a callback URL, with system-session code 1 and mapped SSO code -204; these do not expose Google's rejection reason. See [profiles/device-auth-0.3.1.json](profiles/device-auth-0.3.1.json) for the selected non-sensitive evidence.

Version 0.3.2 replaces the fragile probe with YouTube's verified native private-keychain mode, adapts the SSO request user-agent bundle token, and adds redacted auth-advice request checks. The keychain failure is concrete; its causal relationship to Google's rejection is still unproven.

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
| `SSOConfiguration +libraryUserAgentForRequest:` at `0x1045cebe0`, `SSOService +fetcherWithRequest:configuration:` at `0x1045b4a44` | SSO sets its own request User-Agent using the GTM formatter at `0x10076e330`. That formatter reads the installed bundle ID when GTMUserAgentID is absent (as in this IPA). The physical renamed identity can therefore remain in the header even when package_name is adapted. |
| `SSOKeychainHelper +usePrivateKeychain` at `0x10020e624` | A native boolean consulted by query/read/write/delete builders; when true they omit the shared access group. `+queryMatchingID:serviceName:` and `+deleteQueryMatchingID:serviceName:` also skip the group if the getter returns nil. |
| `SSOBundleIdServiceImpl -bundleId` at `0x100c5fad8` | Independently reads the main bundle identifier. Trace its consumers before treating it as a second necessary override. |
| `SSOSafariSignIn -signInWithURL:presentationAnchor:completionHandler:` at `0x104099b04` | Creates and starts **ASWebAuthenticationSession** with the configured callback scheme. A supported system-browser path already exists. Runtime diagnostics must confirm which path this attempt used. |
| `SSOConfiguration -authCallbackURLString` at `0x1045ceb48` | Forms the auth callback from `applicationScheme`. Original Info.plist registers `com.google.sso.755541669657-kbosfavg7pk7sr3849c3tf657hpi5jpd`. Preserve the original matching client/scheme pair. |
| `SSOKeychainHelper +accessGroup` at `0x100202850` | Caches `sharedAccessGroup`. |
| `SSOKeychainHelper +sharedAccessGroup` at `0x1002028d4` | Formats `<computed application prefix>.com.google.common.SSO`. Both format and suffix were decoded from the referenced CFStrings, not inferred from the function name. This group is not necessarily granted by the new signer. |
| `SSOKeychainHelper +queryMatchingID:serviceName:` at `0x10058907c`, `+writeSharedKeychain:error:` at `0x1045cc6d4` | Uses that access group for credential reads/writes unless private-keychain mode applies. A successful browser login can still fail to persist. |

The older 0.3 adapter accepts a re-signed bundle ID for its own binary-profile check, but had no account-authentication identity or keychain adapter. Version 0.3.1 introduced two scoped adapters; 0.3.2 revises their storage and request-identity implementation. Cloud signing derives capabilities from the supplied Apple profiles, and does not restore original Google groups. The 0.3.1 report confirms the installed bundle ID differs from the original; the raw signing identity and entitlements were not collected.

Google documents iOS OAuth registration against bundle ID, with optional team/App Store identity and optional App Check protection. That supports the identity-mismatch hypothesis, but does not prove this client uses App Check or identify the screenshot's exact cause. See [Manage OAuth Clients](https://support.google.com/cloud/answer/15549257). Google separately rejects embedded user agents; use its supported native flow rather than a WKWebView fallback. See [OAuth for native apps](https://developers.google.com/identity/protocols/oauth2/native-app#authorization-errors).

## Implemented design and device experiment

### 1. Add narrowly scoped diagnostics first

`native/RVAuthentication.inc` installs its hooks during `RVStart`, after the supported binary/config check and before ordinary app initialization can construct SSO configuration. Use the existing `RVHook` signature guard; missing or changed methods must remain native and appear in the report.

Record whether the physical installed bundle ID matches the original separately from SSO's reported identifier. Raw bundle/team identifiers are not exported. Record whether the SSO configuration/client/callback tuple matches the original supported profile, which browser entry was used, callback/error presence, and numeric keychain OSStatus. Prefer booleans for group/entitlement comparisons; do not export Apple team identifiers or raw authentication material unnecessarily.

For URLs and requests, allowlist diagnostic fields. Report request host/path classification, presence/match of client ID and package name, callback scheme match, and presence/method of a verifier challenge. Never record full URLs, headers, request bodies, codes, verifier/state/nonce values, cookies, tokens, device challenges, account identifiers, email, passwords or unrestricted NSError descriptions. An error page may never call the app's completion handler; cancellation alone cannot reveal Google's rejection reason. If no sanitized error code is available, retain that uncertainty.

### 2. Adapt the SSO application identifier

The independently controllable experimental `sideload_auth_identity` setting is available in the CLI, GUI and native settings. For the supported YouTube client only, hook `SSOConfiguration -applicationIdentifier` (`@@:`) to report the original `com.google.ios.youtube` to the native SSO request builder. Preserve the original client ID, mediator client ID and registered callback scheme. Check the configuration's client and scheme before applying the override. Delegate unrecognized configurations to the original implementation.

A getter hook covers configurations constructed before the hook and avoids assuming private ivar offsets. It does not modify the physical Info.plist, signed application identifier or provisioning profile. Avoid a process-wide NSBundle override: signing, containers and unrelated SDKs need the real installed identity. Only add an `SSOBundleIdServiceImpl -bundleId` adapter if a traced consumer requires it; its existence alone is insufficient evidence.

### 3. Select the native private keychain

In 0.3.2, `sideload_auth_keychain` makes `SSOKeychainHelper +usePrivateKeychain` (`B@:`) return YES and makes `+accessGroup` / `+sharedAccessGroup` return nil while the supported callback scheme is registered. The verified builders test for a non-nil group and the private-mode flag before adding `kSecAttrAccessGroup`. Security then uses the installed app's default authorized group. Both branches are present in this binary; this does not rely on creating or decoding a temporary probe item.

Read/write/delete and update selection remain native. The `SSOKeychainCore` wrappers record numeric add/read/update/delete statuses and forward their original dictionaries, pointers, results and return values. No account items are read by diagnostics or deleted by the adapter. Adapter-off, an unsupported runtime profile, or an absent registered callback scheme preserves the original getters. Native update queries can retain an actual group returned for an existing authorized item.

Apple explains default-group selection and entitlement enforcement in [kSecAttrAccessGroup](https://developer.apple.com/documentation/security/ksecattraccessgroup); `-34018` is [errSecMissingEntitlement](https://developer.apple.com/documentation/security/errsecmissingentitlement). The adapter cannot import the official YouTube app's account, grant cross-team Google SSO, or preserve private items across a changed signing identity. Folsom and other separate stores require their own evidence if errors remain.

The earlier 0.3.1 discovery probe is removed from production. Historical [YTLite sideloading source](https://github.com/dayanch96/YTLite/blob/main/Sideloading.x) also adapts identity/storage, but uses a broader bundle override. It is an implementation comparison, not proof of compatibility with this IPA.

### 3a. Keep the SSO request user agent consistent

The `SSOService +fetcherWithRequest:configuration:` (`@@:@@`) adapter first invokes the native constructor. For the verified YouTube client/scheme only, on HTTPS production account hosts without URL credentials or a nonstandard port, it examines the native fetcher's request User-Agent. If that string starts with the exact physical installed bundle ID followed by `/`, it replaces that single leading token with `com.google.ios.youtube`. The native version, iSL library/platform suffix, request body, headers other than User-Agent, cookies, challenges, callback, and fetcher behavior are preserved. Unknown configurations, unmatched tokens, missing methods and other hosts retain their native paths. No process-wide NSBundle or browser user-agent hook is installed.

Diagnostics for `/v1/authadvice` expose only booleans for body client/mediator/package/callback matches and challenge/client-state presence. Parsing is bounded and never exports the body or account list. Browser URLs classify the verified SSO library client `936475272427.apps.googleusercontent.com` separately from YouTube's client; a library-client URL must not be treated as proof that a client substitution is needed. The native library client getter is at `0x1045ceacc`. No client IDs are changed by this revision.

### 4. Preserve native authentication checks and isolate browser failures

Keep native server challenge generation, token exchange, callback state/verifier validation and error handling. Do not substitute Safari's user-agent string in an embedded browser or remove challenges. If diagnostics establish an embedded-browser rejection, use the already present system authentication path with the existing completion/callback handling. The screenshot's browser appearance alone is not sufficient to select that branch.

### 5. Build a device experiment with explicit acceptance criteria

The separate `YouTube-21.39.4-RVPort-0.3.2-SideStore-auth-unsigned.ipa` is built from the supported original. It retains the previous 71 features, removes extensions, and enables the two new authentication switches through `configs/sideload-auth.json` (32 enabled switches in total). Defaults and the ordinary expanded preset leave the authentication switches disabled. Build the native payload on the macOS runner, run ABI checks against these exact selectors, and test privacy filtering using synthetic URLs containing secret-like values. Test a configuration constructed before hook installation and native fallbacks for unknown clients or private-mode fallbacks.

On the target device, compare baseline, identity-only, keychain-only and both enabled, using an app restart after changing authentication settings. Success requires completing Google sign-in, account visibility, an authenticated YouTube operation, survival across cold relaunch, token refresh and survival across an ordinary SideStore refresh with the same signing identity. Report attempts and failures rather than silently retrying or clearing user accounts. Playback/features require their own checks.

If the corrected metadata tuple is still rejected, investigate the returned server error and challenge path before another change. A string override cannot satisfy a cryptographic requirement for Google's signing team, app attestation, or a registration policy controlled by Google. An OAuth client registered for our own app can authorize permitted public YouTube API scopes, but has not been shown to supply the private SSO token/services expected by the native YouTube app; it is not a drop-in account-login replacement.

## Evidence files

[profiles/authentication-evidence.json](profiles/authentication-evidence.json) records the binary identity, ABI anchors, source revisions and local research hashes. The full local Ghidra outputs are `build/auth-research-decompiled-annotated.txt`, `build/auth-flow-decompiled-annotated.txt`, `build/auth-advice-decompiled-annotated.txt`, `build/auth-request-builder-decompiled-annotated.txt` and `build/auth-keychain-disassembly.txt`. They are local analysis artifacts, excluded from Git.

The 0.3.2 auth artifact is an unsigned device experiment, not a verified Google login fix. Existing 0.3 and 0.3.1 files are unchanged.

## Testing the new IPA

Install the separately named 0.3.2 SideStore auth IPA with SideStore, which signs it with your account. Keep the same signing identity/bundle identifier when updating if you want existing app data retained. Attempt sign-in once, then test account visibility and cold relaunch. If it fails, close the browser sheet, hold three fingers for one second in YouTube, open **Show hook diagnostics**, and choose **Copy diagnostic report**. The `authentication` section contains bounded redacted events, adapter application and numeric keychain/error statuses. A browser cancellation is not a Google server error code. No passwords, cookies, codes, state/verifier values, tokens, emails, raw URLs or raw keychain groups are included by the authentication reporter.

Both switches are under native settings. Restart after changing them; compare identity-only and keychain-only if needed. The production adapter is exercised by `tests/authentication.m` on macOS using fake SSO classes and a native-query-builder fixture; it never signs in to Google or accesses real credentials during host tests. The shared `RVRuntime.inc` preserves the existing ABI guards and class-local hook behavior.

## Historical 0.3.1 host verification and artifact

The [macOS/Windows workflow](https://github.com/zinfector/Revanced-iOS/actions/runs/37177944021) passed all 33 tests with no skips, including the production Objective-C authentication harness. Xcode built the ARM64 iOS payload; 65 literal/getter-array hook ABIs matched the supplied metadata. The packaged Windows GUI passed with 73 switches. Real-IPA verification retained 11,760 original members byte-identically and removed 1,718 signature/extension members; existing executable load commands, section bytes, chained fixups and ZIP CRC passed.

Previously tested file: `output/YouTube-21.39.4-RVPort-0.3.1-SideStore-auth-unsigned.ipa`, SHA-256 `1481372967024a657c94381324b36a6f90148245adc9b0d80407de63d719bb70`, 140,331,042 bytes. The [release receipt](profiles/release-0.3.1.json) records all three new IPA configurations, payload/GUI hashes and host evidence. Host checks do not prove the device login succeeds.

## 0.3.2 host verification and artifact

The [macOS/Windows workflow](https://github.com/zinfector/Revanced-iOS/actions/runs/37183327384) passed all 33 host tests with no skips, including the production authentication adapter, private-query branch and request-identity/privacy contracts. Xcode built the ARM64 iOS payload, 68 direct hook ABIs match the supplied metadata, and the packaged Windows GUI passed with 73 switches and extension removal selected by default. These checks do not execute Google login or iOS Security on the phone.

Recommended file: `output/YouTube-21.39.4-RVPort-0.3.2-SideStore-auth-unsigned.ipa`, SHA-256 `f1b3e2c752e3187d72b48eb365260cc414bd9921cc19d6a6137f0daae9a59fa0`, 140,332,714 bytes. Real-IPA integration retained 11,760 original members byte-identically, removed 1,718 signature/extension members, and preserved section/LINKEDIT bytes, chained fixups and existing load commands; ZIP CRC checks passed. No `.appex` members remain. Receipt: [profiles/release-0.3.2.json](profiles/release-0.3.2.json).

## User-reported login success

The user subsequently reported, “Login works now,” after the revised auth IPA was supplied. The exact filename was not restated in that message. This is device evidence of successful login, not a test of token refresh, cold-relaunch persistence or every patch. Version 0.3.4 retains the authentication source unchanged while adding the dedicated ReVanced settings pane. [Recorded observation](profiles/device-auth-login-success.json).
