"""Generate an auditable mapping of all local YouTube patch declarations, including factories."""
import collections
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parent
SOURCE=ROOT.parent/'revanced-patches-main/patches/src/main/kotlin/app/revanced/patches/youtube'
# status, configuration keys, implemented behavior and concrete remaining scope
MAPPING={
'hideAdsPatch':('partial', 'feed_ads', '28 typed ad-presence fields, bounded feed/continuation filtering, protected-context element patterns and native Shorts ad classification.', 'Broad Android identifier/path rules and additional insertion/promotion surfaces are not all mapped; device confirmation pending.'),
'videoAdsPatch':('adapted', 'video_ads', 'Owned recorded-watch response arrays/counts or scoped native no-op coordinator selection; trigger choice falls back to response filtering.', 'Unknown owners, live/DAI and Reels player policies pass through; ad/content transitions require device confirmation; no stream replacement.'),
'copyVideoURLPatch':('adapted','copy_video_url','Native Tools menu copies a clean video URL, optionally with timestamp.','Uses an iOS menu instead of the Android overlay buttons.'),
'removeViewerDiscretionDialogPatch':('partial','remove_discretion_dialog','Auto-confirms positively identified ordinary content warnings.','Age, login, rental and interstitial verification remain native; this does not unlock restricted videos.'),
'addMoreDoubleTapToSeekLengthOptionsPatch':('adapted','double_tap_seconds','Overrides the native seek-interval class method, 0 preserves native behavior.','Configured through JSON; disabling double-tap takes precedence.'),
'disableDoubleTapActionsPatch':('adapted','disable_double_tap disable_chapter_skip','Suppresses native double-tap and two-finger chapter gestures.','Needs device gesture tests.'),
'downloadsPatch':('partial','external_downloads','Native share-sheet handoff of the current video URL.','Requires a compatible installed extension; no internal media downloader, offline storage, DRM or subscription authorization.'),
'disableHapticFeedbackPatch':('partial','disable_haptics','Suppresses two native semantic-haptic entry points.','Other UIKit/system or direct haptic producers are not globally intercepted.'),
'seekbarPatch':('partial','tap_to_seek hide_seekbar disable_precise_seeking seekbar_color','Tap-to-seek, progress hiding, fine-scrubber gesture suppression and color override.','Native slide-to-seek is retained. Android heatmap/chapter styling and full seekbar geometry are not reproduced.'),
'swipeControlsPatch':('partial','swipe_controls','Fullscreen vertical swipes change brightness on the left and volume on the right.','MPVolumeView slider discovery and gesture arbitration need device tests; Android gesture preferences are not all reproduced.'),
'disableAutoCaptionsPatch':('partial','disable_auto_captions','Disables automatic-on-mute/start caption gates.','Does not disable manually selected captions; server-side caption defaults may still differ.'),
'changeHeaderPatch':('adapted','custom_header','Loads supplied RVHeader.png through native logo provider/controller hooks.','Requires --header-image; no bundled ReVanced artwork.'),
'hideVideoActionButtonsPatch':('partial','hide_action_buttons action_patterns','Removes positively matching element components.','Only configured iOS identifiers; Android buttons and resources differ.'),
'navigationBarPatch':('partial','hide_home_navigation hide_shorts_navigation hide_subscriptions_navigation hide_library_navigation hide_navigation_labels start_page','Native pivot model compaction for Home/Shorts/Subscriptions/You, reversible icon-only tab labels, immediate preference refresh and surviving-content-tab selection.','Create/Notifications toolbar relocation, button animations, narrow buttons and Android translucency options remain unported. New navigation options require device confirmation.'),
'hidePlayerOverlayButtonsPatch':('partial','hide_cast_button hide_captions_button hide_autoplay_button hide_previous_next hide_watermark','Hides selected native controls and watermark.','Does not reproduce every Android button option.'),
'changeFormFactorPatch':('partial','spoof_form_factor form_factor','Copies request client-info and sets the verified phone/tablet protobuf enum.','Server response may change; no guaranteed Android/tablet UI or automotive implementation.'),
'hideAutoplayPreviewPatch':('adapted','hide_autoplay_preview','Hides native autonav preview and end-screen views.','Does not disable autoplay itself.'),
'hideEndScreenCardsPatch':('adapted','hide_end_cards','Hides the native creator end-screen container.','Server-rendered layouts outside this native container need checks.'),
'hideEndScreenSuggestedVideoPatch':('adapted','hide_autoplay_preview','Hides native autonav end-screen and background.','Shares the preview toggle; Android and iOS end-screen structures differ.'),
'disableFullscreenAmbientModePatch':('partial','disable_ambient','Disables ambient gates and forces native Metal ambient strength to zero.','Other server-selected ambient renderers may require additional hooks.'),
'hideInfoCardsPatch':('partial','hide_info_cards','Hides native teaser container.','Card endpoints and all expanded card surfaces are not removed.'),
'hidePlayerFlyoutMenuItemsPatch':('partial','hide_flyout_items flyout_patterns hide_premium_quality','Positive component filters and native Premium-labelled quality-action filtering.','Does not map every Android flyout item; Premium title filtering is locale/controller dependent.'),
'disablePlayerPopupPanelsPatch':('partial','disable_popup_panels','Filters automatic show-engagement-panel response actions.','Manual panels stay native; other response/presentation paths need runtime evidence.'),
'hideRelatedVideoOverlayPatch':('partial','hide_related_overlay','Hides native fullscreen engagement overlay.','Container may include more than related videos; needs layout checks.'),
'disableRollingNumberAnimationsPatch':('partial','disable_rolling_numbers','Preserves native number-rendering arguments and suppresses UIKit/Core Animation effects in a scoped transaction.','Custom asynchronous digit animations may still run; native like text is not replaced. Device confirmation is outstanding.'),
'hideShortsComponentsPatch':('partial','hide_shorts shorts_ads hide_shorts_navigation hide_shorts_shortcut','Shelf/cell filtering, native Shorts ad-model suppression, pivot hiding and exact-type native Shorts app-shortcut filtering.','Individual Shorts player controls and widget buttons remain unported. Removing IPA extensions removes entire widgets rather than individual controls; shortcut cache updates can require relaunch.'),
'disableSignInToTVPopupPatch':('adapted','disable_tv_popup','Disables native seamless TV sign-in presentation gate.','Other Cast/device prompts are separate.'),
'hideTimestampPatch':('adapted','hide_timestamp','Hides native time labels and disables duration/current-title label gates.','Other compact/server-rendered timestamp surfaces need device checks.'),
'miniplayerPatch':('partial','classic_miniplayer miniplayer_disable_drag miniplayer_disable_horizontal_drag miniplayer_disable_double_tap miniplayer_hide_subtext miniplayer_square_corners miniplayer_hide_overlay_buttons miniplayer_min_dimension_points miniplayer_overlay_opacity','Native floating-miniplayer experiment toggle plus verified controller/layer gesture gates, message/Premium badge suppression, stable rectangular content masks, bounded minimum dimension, circular control-background opacity, and owned floating close/playback button hiding with native visibility restoration.','Selected iOS equivalents: title/channel labels, complete Android type variants, separate expand/close/rewind/forward preferences and all server UI surfaces remain unported. This binary expands by video tap and has no separate rewind/forward control pair; the overlay toggle hides close and the playback action group. Disabled-on-collapse requires a verified collapse boundary, since activateMiniBar is also called during player fetching. Opacity applies only to circular backgrounds; transitions preserve native masks. Size is a minimum, not a forced frame. Requires device verification; see MINIPLAYER_SCHEME.md.'),
'exitFullscreenPatch':('adapted','exit_fullscreen_end','Exits native fullscreen after playback completes.','Looping takes precedence; ordinary content only.'),
'openVideosFullscreenPatch':('adapted','open_videos_fullscreen','Requests native fullscreen on ordinary video activation.','Live/ads fail closed; transition timing needs device checks.'),
'customPlayerOverlayOpacityPatch':('partial','overlay_opacity','Sets native player background alpha when below 1.','Does not reproduce Android scrim rendering; choose before patching or restart after configuration changes.'),
'returnYouTubeDislikePatch':('partial', 'return_dislikes paired_vote_buttons ryd_voting', 'Cached RYD estimates with native count rendering; supported standalone watch vote models normalized to complete native paired controls, preserving native like entities and commands; semantic animated-icon pair binding and native-font inheritance; component-driven transition refresh; manual RYD service votes.', '0.3.21 requires device confirmation on clean install and subsequent launches. Opaque shared models, absent native like entities and unsupported visibility/state contracts retain original native controls. Automatic service voting stays disabled.'),
'shortsAutoplayPatch':('partial','shorts_autoplay','Enables native auto-advance menu gate and requests next reel at completion.','Native completion/loop timing and interactions with regular-player routing need device tests.'),
'openShortsInRegularPlayerPatch':('partial','open_shorts_regular','Rewrites positively identified Shorts links and reel commands into watch endpoints.','Does not replace every already-playing reel, touch gesture or server navigation path.'),
'sponsorBlockPatch':('partial','sponsorblock sponsorblock_manual sponsorblock_contribute sponsorblock_markers sponsor_behaviors sponsor_colors sponsor_min_duration','Player event-center clock and canonical local-controller session; typed response unwrapping and inline/modular native timeline rendering; filtered category fetch, overlap merging, per-category skip/skip-once/manual/seekbar-only/ignore, highlight jump and undo, native markers/colors, quality/category votes, reviewed range/point submissions, local request counters, service statistics/VIP display and public username control.','The supplied 0.3.5 device report confirms both clocks fire but the shared live check rejects the response wrapper before fetching. The 0.3.6 correction retained in 0.3.7 unwraps playerResponse.playerData for the live check and renders the native modular timeline as well as the legacy inline bar. Corrected skip/marker behavior remains device-unverified. Tests were not run for this release at the user request. Own patch configuration can be copied; Android/desktop preference translation, service-identity migration, privileged moderation actions and complete native overlay styling are not reproduced. The local Android client does not implement API chapter/mute/full-video actions either.'),
'spoofAppVersionPatch':('partial','spoof_app_version client_version','Overrides clientVersion on copied serialized request client-info.','Does not change executable identity, clientName, headers or guarantee old UI/working streams.'),
'changeStartPagePatch':('partial','start_page','Selects a supported Home/Subscriptions/You/Shorts initial pivot and recovers hidden or unavailable tabs using actual surviving renderer items.','Other Android browse destinations and force-on-every-launch behavior remain unported.'),
'disableResumingShortsOnStartupPatch':('adapted','disable_shorts_resume','Disables native Shorts resume configuration gate.','Cold-launch restore behavior needs device testing.'),
'alternativeThumbnailsPatch':('partial','alternative_thumbnails dearrow_thumbnails fast_thumbnail_stills thumbnail_frame thumbnail_modes dearrow_url','Verified still variants with fallback; DeArrow cache URL and server redirect fallback; asynchronous bounded image-header probes; home/subscriptions/library/visible-watch/search mode filtering.','Original URLs remain until availability is confirmed; an already loaded cell may need refresh/reuse. Native lifecycle context classification and service/image rendering still need device tests. Custom titles are not supplied by this Android thumbnail patch.'),
'bypassImageRegionRestrictionsPatch':('partial','thumbnail_proxy thumbnail_proxy_url','Routes recognized video-thumbnail URLs through a user-configured HTTPS endpoint with a url query parameter.','Requires a compatible proxy returning an image; no default service, general image interception or guaranteed region bypass.'),
'announcementsPatch':('partial','announcements','User-initiated plain-text reader of the ReVanced YouTube announcement endpoint.','No background notification scheduler; endpoint availability is external and the current endpoint fetch was unavailable during development.'),
'pauseOnAudioInterruptPatch':('partial','pause_on_interrupt','Pauses playback on AVAudioSession interruption began.','Does not suppress native automatic resumption or reproduce Android audio-focus semantics.'),
'removeBackgroundPlaybackRestrictionsPatch':('adapted','background_playback picture_in_picture','Overrides native user/background gates, with a PiP Tools action retaining the native capability check.','No hardware capability, DRM, server authorization or subscription is added.'),
'spoofDeviceDimensionsPatch':('partial','spoof_dimensions screen_width_points screen_height_points','Sets verified Int32 screen/window dimensions in copied request client-info.','Request-only override; does not resize UIKit or guarantee Android layouts.'),
'bypassURLRedirectsPatch':('adapted','bypass_redirects','Unwraps public YouTube redirect q/url targets after HTTP(S) validation.','Only the mapped URL-endpoint path; authentication URLs are preserved.'),
'openLinksExternallyPatch':('partial','open_links_external','Sends external HTTP(S) URL endpoints to the system browser.','Other in-app browser paths and Android default browser controls are not mapped.'),
'loopVideoPatch':('adapted','loop_video','Seeks ordinary completed playback to zero and plays again using native controller APIs.','Not live/ads; suppresses normal finish only when guarded methods are available.'),
'disableVideoCodecsPatch':('partial','disable_hdr disable_vp9','Filters positively identified VP9 MIME strings and PQ/HLG transfer-characteristic formats, retaining fallback choices.','Does not implement the Android stream-spoof dependency or all codec selection modes.'),
'videoQualityPatch':('partial','default_quality wifi_quality cellular_quality remember_quality advanced_quality_menu hide_premium_quality','Global/Wi-Fi/cellular resolution caps, per-network remembered quality label, direct advanced native menu and Premium action hiding.','Cap is not exact format selection. Network changes apply when formats are rebuilt; unknown paths use the global policy. Native custom button, all locale/controller variants and unavailable format unlock are not reproduced.'),
'playbackSpeedPatch':('partial','default_speed remember_speed custom_speed_menu custom_speeds','Default/remembered rate plus native Tools custom-rate picker, 0.25–4x.','Android limits above 4x, speed dialog styling and all rate-change sources are not reproduced.'),
'customBrandingPatch':('adapted','app_name','Changes display name and supplied 120/180/152 PNG legacy icon entries.','CLI assets must be valid PNGs; device icon caching and signing need checks.'),
'hideLayoutComponentsPatch':('partial','hide_layout_components layout_patterns hide_comments comment_patterns','Positive element-data filter rules and comments patterns.','Android Litho identifiers and dozens of layout preferences are not directly portable; iOS identifiers need runtime evidence.'),
'themePatch':('partial','theme theme_light_background theme_dark_background','Forces verified native dark/light page-style enum and selected UIColor background getters on common/token palettes.','Only positively typed palette colors are overridden; complete Android resource/splash palette parity and every server-selected surface are not reproduced.'),
'sanitizeSharingLinksPatch':('partial','sanitize_sharing_links','Removes identified public YouTube share-tracking query parameters; own copy action emits clean URLs.','Does not intercept every native share producer.'),
'forceOriginalAudioPatch':('partial','force_original_audio','Selects a positively identified .4 original audio track through the native switch controller.','Original-track availability and source enum behavior need device tests; unmatched tracks remain unchanged.'),
'enableDebuggingPatch':('adapted','diagnostics','Native hook installation/mismatch log and in-app diagnostics.','Android application debug/resource flags are not copied.'),
'checkWatchHistoryDomainNameResolutionPatch':('adapted','watch_history_dns','Manual off-thread s.youtube.com DNS diagnostic.','Does not modify DNS or verify history upload/account synchronization.'),
'checkEnvironmentPatch':('adapted','schema','Exact source hash/profile checks in patcher and runtime UUID/version/config compatibility checks.','No Android installer/environment heuristics.'),
'spoofVideoStreamsPatch':('blocked','','No iOS stream replacement implementation.','Requires a verified request/response protobuf adapter, account/token handling, stream URL and signature validation, expiry management and player format integration. Client-field overrides do not implement this.'),
'userAgentClientSpoofPatch':('blocked','','No alternate-client transport/header spoofing.','No verified iOS Cronet/header request interception; changing headers alone can desynchronize client/auth/playback requests.'),
'gmsCoreSupportPatch':('partial','sideload_auth_identity sideload_auth_keychain','Scoped native SSO application-identifier and request-user-agent adaptation, native private-keychain mode, and redacted authentication diagnostics.','Android GmsCore/Binder/account services are not transplanted. The 0.3.1 auth IPA installs and its hooks run, but device sign-in failed with repeated keychain entitlement errors. The user reports successful login with the revised 0.3.2 storage/user-agent adapters retained in later builds. Token refresh and persistence remain unverified. It cannot satisfy cryptographic signing-team/attestation requirements.'),
'accountCredentialsInvalidTextPatch':('android_only','','No Android GmsCore error-text rewrite.','The Android account-credential screen does not apply to the iOS app.'),
'fixContentProviderPatch':('android_only','','No Android ContentProvider manifest rewrite.','iOS has no Android ContentProvider authority.'),
'fixBackToExitGesturePatch':('android_only','','No Android system-back gesture fix.','iOS navigation/back behavior differs.'),
'versionCheckPatch':('android_only','','No Google Play Services version checks.','Patcher uses its iOS source profile and minimum OS checks.'),
'enableSlideToSeekPatch':('native_existing','','Native scrubber drag behavior retained.','No Android gesture flag is needed; device tests should check it alongside tap-to-seek.'),
}
ALIASES={
'disablePreciseSeekingGesturePatch':'seekbarPatch','enableTapToSeekPatch':'seekbarPatch','hideSeekbarPatch':'seekbarPatch','seekbarColorPatch':'seekbarPatch',
'changeHeaderBytecodePatch':'changeHeaderPatch','themeResourcePatch':'themePatch','advancedVideoQualityMenuPatch':'videoQualityPatch',
'hidePremiumVideoQualityPatch':'videoQualityPatch','rememberVideoQualityPatch':'videoQualityPatch','videoQualityDialogButtonPatch':'videoQualityPatch',
'playbackSpeedButtonPatch':'playbackSpeedPatch','customPlaybackSpeedPatch':'playbackSpeedPatch','rememberPlaybackSpeedPatch':'playbackSpeedPatch',
'fixPlaybackSpeedWhilePlayingPatch':'playbackSpeedPatch','loopVideoButtonPatch':'loopVideoPatch','openVideosFullscreenHookPatch':'openVideosFullscreenPatch',
}
RESOURCE_PARENTS={
'hideAdsResourcePatch':'hideAdsPatch','copyVideoURLResourcePatch':'copyVideoURLPatch','downloadsResourcePatch':'downloadsPatch',
'swipeControlsResourcePatch':'swipeControlsPatch','hideEndScreenCardsResourcePatch':'hideEndScreenCardsPatch',
'hideLayoutComponentsResourcePatch':'hideLayoutComponentsPatch','hideInfocardsResourcePatch':'hideInfoCardsPatch',
'hideShortsComponentsResourcePatch':'hideShortsComponentsPatch','miniplayerResourcePatch':'miniplayerPatch','sponsorBlockResourcePatch':'sponsorBlockPatch',
'loopVideoButtonResourcePatch':'loopVideoPatch','playerControlsResourcePatch':'copyVideoURLPatch',
'settingsResourcePatch':'settingsPatch','videoQualityButtonResourcePatch':'videoQualityPatch','playbackSpeedButtonResourcePatch':'playbackSpeedPatch',
}
INFRASTRUCTURE={
'toolbarHookPatch':'Header provider/controller hooks instead of Android toolbar injection.',
'hookClientContextPatch':'Native YTInnerTubeContextFactory client-info copy and verified protobuf field setters.',
'engagementPanelHookPatch':'Selected native automatic engagement-panel action filtering.',
'sharedExtensionPatch':'Injected RVPort.dylib; no Android DEX extension.',
'cronetImageURLHookPatch':'Selected GPB thumbnail URL accessors; no global iOS Cronet image interceptor.',
'lithoFilterPatch':'Positive element-data filters; Android Litho runtime is not transplanted.',
'navigationBarHookPatch':'Selected native pivot controller hooks.',
'playerControlsOverlayVisibilityPatch':'Selected native overlay layout and guarded Tools attachment.',
'playerControlsPatch':'Native Tools menu and settings switches instead of Android injected resource controls.',
'playerTypeHookPatch':'Guarded native ordinary/live/ad and fullscreen checks.',
'recyclerViewTreeHookPatch':'Selected native view/model hooks; no Android RecyclerView traversal.',
'settingsPatch':'Dedicated ReVanced entry and YouTube-styled host/native settings cells, embedded ReVanced icon, content-level search, 13 groups covering all 80 switches and 34 runtime values, validated editors/import/export, reset and diagnostics. Existing preference keys and gesture fallback are retained.',
'videoInformationPatch':'Native content ID, time, duration and CPN accessors.',
'playerResponseMethodHookPatch':'Selected player response/accessor and content-load/time hooks.',
'videoIdPatch':'Guarded native video ID accessors and generation-checked service responses.',
}
def generate():
    named={r['id']:r for r in json.loads((ROOT.parent/'analysis/evidence/android_patch_inventory.json').read_text(encoding='utf-8'))}
    rows=[]
    for path in sorted(SOURCE.rglob('*.kt')):
        for match in re.finditer(r'\bval\s+(\w+Patch)\s*=\s*(\w+)\s*(?:\(|\{)',path.read_text(encoding='utf-8')):
            ident=match[1];parent=ALIASES.get(ident) or RESOURCE_PARENTS.get(ident)
            item=MAPPING.get(ident) or MAPPING.get(parent)
            if item:
                status,keys,behavior,limits=item
                if ident in RESOURCE_PARENTS:status='resource_adapter'
            elif ident in INFRASTRUCTURE or parent in INFRASTRUCTURE:
                status='infrastructure_adapter';keys='';behavior=INFRASTRUCTURE.get(ident) or INFRASTRUCTURE[parent];limits='Version-specific subset; runtime ABI checks and device tests remain necessary.'
            else:raise ValueError('Unmapped declaration: '+ident)
            rows.append(dict(id=ident,name=named.get(ident,{}).get('name',ident),named_inventory=ident in named,
                source=str(path.relative_to(ROOT.parent)).replace('\\','/'),line=path.read_text(encoding='utf-8')[:match.start()].count('\n')+1,
                factory=match[2],status=status,config_keys=keys.split(),implementation=behavior,limits=limits,parent=parent,device_validated=False))
    if set(named)-{r['id'] for r in rows}:raise ValueError('Named inventory missing from coverage')
    counts=dict(collections.Counter(r['status'] for r in rows))
    report={'patcher_version':'0.3.21','scope':'All val *Patch declarations found in the local YouTube Kotlin tree, including private resources, unnamed subpatches and shared factories. Shared implementations outside this tree are represented by their YouTube wrapper.',
        'device_validated':False,'named_inventory_count':len(named),'declaration_count':len(rows),'status_counts':counts,'patches':rows}
    (ROOT/'coverage.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    lines=['# YouTube iOS port coverage — 0.3.15','',
        'All implemented entries are **experimental and untested on an iPhone**. Static ABI matches and archive verification do not establish runtime behavior or full ReVanced parity.','',
        f'The local tree contains {len(rows)} patch declarations, including the {len(named)} named patches in the original inventory. Declarations include dependencies and private resources; they are not {len(rows)} independent user features.','',
        '`adapted` = an iOS implementation of the selected behavior; `partial` = implemented subset with explicit remaining scope; `blocked` = no working port; `android_only` = Android mechanism absent on iOS; `native_existing` = retain the native iOS behavior; resource/infrastructure entries support the selected adapters.','',
        'Two wrappers remain blocked: full stream replacement and alternate-client transport/header spoofing. Request version/dimension/form-factor overrides do not provide either. Complete UI parity needs device evidence for additional iOS component IDs, gestures and server-rendered surfaces.','',
        '## Named patches','', '| Android patch | Status / configuration | iOS behavior and limits |','|---|---|---|']
    for r in rows:
        if r['named_inventory']:
            lines.append('| '+r['name']+' | '+r['status']+'; '+', '.join('`'+k+'`' for k in r['config_keys'])+' | '+r['implementation']+' '+r['limits']+' |')
    lines.extend(['','## Factory wrappers, dependencies and resources','','| Declaration | Status | Mapping / remaining scope |','|---|---|---|'])
    for r in rows:
        if not r['named_inventory']:lines.append('| `'+r['id']+'` | '+r['status']+' | '+r['implementation']+' '+r['limits']+' |')
    lines.extend(['','The machine-readable [coverage.json](coverage.json) contains each source path and declaration line. Hook evidence: [hook-check.json](build/hook-check.json); read its scope field. Service, gesture, icon, quality, player-transition and signing/device checks remain outstanding.',''])
    (ROOT/'COVERAGE.md').write_text('\n'.join(lines),encoding='utf-8')
    print(json.dumps({'declarations':len(rows),'named':len(named),'statuses':counts}))
    return report
if __name__=='__main__':generate()
