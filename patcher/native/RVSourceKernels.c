#include "RVSourceKernels.h"
#include <string.h>
void rvj_context_init(RVJContext *ctx) {
    ctx->field_VideoAdsPatch_SHOW_VIDEO_ADS = (!ctx->settings[RVJ_SETTING_HIDE_VIDEO_ADS]);
    ctx->field_DisableAutoCaptionsPatch_captionsButtonStatus = false;
    ctx->field_DisableFullscreenAmbientModePatch_DISABLE_FULLSCREEN_AMBIENT_MODE = ctx->settings[RVJ_SETTING_DISABLE_FULLSCREEN_AMBIENT_MODE];
    ctx->field_DisableFullscreenAmbientModePatch_DIVIDER_ATTRIBUTES_COLOR_SYSTEM_DEFAULT = -16777216;
    ctx->field_SlideToSeekPatch_SLIDE_TO_SEEK_DISABLED = (!ctx->settings[RVJ_SETTING_SLIDE_TO_SEEK]);
    ctx->field_FixPlaybackSpeedWhilePlayingPatch_DEFAULT_YOUTUBE_PLAYBACK_SPEED = 1.0f;
}
// VideoAdsPatch.shouldShowAds at source line 13
bool rvj_show_video_ads(RVJContext *ctx)
{
    (void)ctx;
    return ctx->field_VideoAdsPatch_SHOW_VIDEO_ADS;
}

// BackgroundPlaybackPatch.isBackgroundPlaybackAllowed at source line 13
bool rvj_background_allowed(RVJContext *ctx, bool original)
{
    (void)ctx;
    if (original)
    {
        return true;
    }
    if (ctx->shorts_open)
    {
        return false;
    }
    int32_t current = ctx->player_type;
    return ((!(current == RVJ_PLAYER_NONE || current == RVJ_PLAYER_HIDDEN)) && (current != RVJ_PLAYER_INLINE));
}

// BackgroundPlaybackPatch.isBackgroundShortsPlaybackAllowed at source line 39
bool rvj_shorts_background_allowed(RVJContext *ctx, bool original)
{
    (void)ctx;
    return (!ctx->settings[RVJ_SETTING_DISABLE_SHORTS_BACKGROUND_PLAYBACK]);
}

// DisableAutoCaptionsPatch.disableAutoCaptions at source line 13
bool rvj_disable_auto_captions(RVJContext *ctx)
{
    (void)ctx;
    return (ctx->settings[RVJ_SETTING_DISABLE_AUTO_CAPTIONS] && (!ctx->field_DisableAutoCaptionsPatch_captionsButtonStatus));
}

// DisableAutoCaptionsPatch.setCaptionsButtonStatus at source line 20
void rvj_set_captions_status(RVJContext *ctx, bool status)
{
    (void)ctx;
    ctx->field_DisableAutoCaptionsPatch_captionsButtonStatus = status;
}

// DisableHapticFeedbackPatch.disableChapterVibrate at source line 14
bool rvj_disable_chapter_haptic(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_CHAPTERS];
}

// DisableHapticFeedbackPatch.disablePreciseSeekingVibrate at source line 21
bool rvj_disable_precise_haptic(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_PRECISE_SEEKING];
}

// DisableHapticFeedbackPatch.disableSeekUndoVibrate at source line 28
bool rvj_disable_undo_haptic(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_SEEK_UNDO];
}

// DisableHapticFeedbackPatch.disableZoomVibrate at source line 44
bool rvj_disable_zoom_haptic(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_ZOOM];
}

// DisableHapticFeedbackPatch.disableVibrate at source line 65
bool rvj_disable_haptics(RVJContext *ctx)
{
    (void)ctx;
    return ((((ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_CHAPTERS] && ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_PRECISE_SEEKING]) && ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_SEEK_UNDO]) && ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_TAP_AND_HOLD]) && ctx->settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_ZOOM]);
}

// DisableFullscreenAmbientModePatch.getFullScreenBackgroundColor at source line 18
int32_t rvj_fullscreen_background_color(RVJContext *ctx, int32_t originalColor)
{
    (void)ctx;
    if (ctx->field_DisableFullscreenAmbientModePatch_DISABLE_FULLSCREEN_AMBIENT_MODE)
    {
        return ctx->field_DisableFullscreenAmbientModePatch_DIVIDER_ATTRIBUTES_COLOR_SYSTEM_DEFAULT;
    }
    return originalColor;
}

// DisableDoubleTapActionsPatch.disableDoubleTapChapters at source line 13
bool rvj_chapter_double_tap_allowed(RVJContext *ctx, bool original)
{
    (void)ctx;
    return (original && (!ctx->settings[RVJ_SETTING_DISABLE_CHAPTER_SKIP_DOUBLE_TAP]));
}

// DisablePreciseSeekingGesturePatch.isGestureDisabled at source line 7
bool rvj_precise_gesture_disabled(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_PRECISE_SEEKING_GESTURE];
}

// HideInfoCardsPatch.hideInfoCardsMethodCall at source line 14
bool rvj_hide_info_cards(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_HIDE_INFO_CARDS];
}

// HideEndScreenCardsPatch.hideEndScreenCards at source line 21
bool rvj_hide_end_cards(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_HIDE_END_SCREEN_CARDS];
}

// HideAutoplayPreviewPatch.hideAutoplayPreview at source line 10
bool rvj_hide_autoplay_preview(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_HIDE_AUTOPLAY_PREVIEW];
}

// HideRelatedVideoOverlayPatch.hideRelatedVideoOverlay at source line 10
bool rvj_hide_related_overlay(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_HIDE_RELATED_VIDEOS_OVERLAY];
}

// HideSeekbarPatch.hideSeekbar at source line 10
bool rvj_hide_seekbar(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_HIDE_SEEKBAR];
}

// HideSeekbarPatch.useFullscreenLargeSeekbar at source line 17
bool rvj_large_seekbar(RVJContext *ctx, bool original)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_FULLSCREEN_LARGE_SEEKBAR];
}

// DisableResumingStartupShortsPlayerPatch.disableResumingStartupShortsPlayer at source line 11
bool rvj_disable_shorts_resume(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_RESUMING_SHORTS_PLAYER];
}

// DisableResumingStartupShortsPlayerPatch.disableResumingStartupShortsPlayer at source line 18
bool rvj_resume_shorts_allowed(RVJContext *ctx, bool original)
{
    (void)ctx;
    return (original && (!ctx->settings[RVJ_SETTING_DISABLE_RESUMING_SHORTS_PLAYER]));
}

// TapToSeekPatch.tapToSeekEnabled at source line 7
bool rvj_tap_to_seek(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_TAP_TO_SEEK];
}

// SlideToSeekPatch.isSlideToSeekDisabled at source line 9
bool rvj_slide_to_seek_disabled(RVJContext *ctx, bool isDisabled)
{
    (void)ctx;
    if (!isDisabled)
    {
        return false;
    }
    return ctx->field_SlideToSeekPatch_SLIDE_TO_SEEK_DISABLED;
}

// FixPlaybackSpeedWhilePlayingPatch.playbackSpeedChanged at source line 11
bool rvj_block_speed_reset(RVJContext *ctx, float playbackSpeed)
{
    (void)ctx;
    if ((playbackSpeed == ctx->field_FixPlaybackSpeedWhilePlayingPatch_DEFAULT_YOUTUBE_PLAYBACK_SPEED) && (ctx->player_type == RVJ_PLAYER_MAXIMIZED || ctx->player_type == RVJ_PLAYER_FULLSCREEN))
    {
        (void)0;
        return true;
    }
    return false;
}

// DisableVideoCodecsPatch.allowVP9 at source line 22
bool rvj_allow_vp9(RVJContext *ctx)
{
    (void)ctx;
    return (!ctx->settings[RVJ_SETTING_FORCE_AVC_CODEC]);
}

// DisablePlayerPopupPanelsPatch.disablePlayerPopupPanels at source line 10
bool rvj_disable_player_popup_panels(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_PLAYER_POPUP_PANELS];
}

// DisableSignInToTVPopupPatch.disableSignInToTvPopup at source line 11
bool rvj_disable_tv_signin_popup(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_SIGN_IN_TO_TV_POPUP];
}

// DisableRollingNumberAnimationsPatch.disableRollingNumberAnimations at source line 10
bool rvj_disable_rolling_numbers(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_DISABLE_ROLLING_NUMBER_ANIMATIONS];
}

// HideTimestampPatch.hideTimestamp at source line 7
bool rvj_hide_timestamp(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_HIDE_TIMESTAMP];
}

// PauseOnAudioInterruptPatch.shouldPauseOnAudioInterrupt at source line 15
bool rvj_pause_on_audio_interrupt(RVJContext *ctx)
{
    (void)ctx;
    return ctx->settings[RVJ_SETTING_PAUSE_ON_AUDIO_INTERRUPT];
}

// FilterGroup$ByteArrayFilterGroup.createFailurePattern at source line 151
RVJInts rvj_kmp_failure(RVJContext *ctx, RVJBytes pattern)
{
    (void)ctx;
    int32_t patternLength = pattern.length;
    RVJInts failure = rvj_ints_new(ctx, patternLength);
    {
        int32_t i = 1;
        int32_t j = 0;
        for (; !ctx->error && (i < patternLength); i = rvj_add(i, 1))
        {
            while (!ctx->error && ((j > 0) && (rvj_byte_at(ctx, pattern, j) != rvj_byte_at(ctx, pattern, i))))
            {
                j = rvj_int_at(ctx, failure, rvj_sub(j, 1));
            }
            if (rvj_byte_at(ctx, pattern, j) == rvj_byte_at(ctx, pattern, i))
            {
                j = rvj_add(j, 1);
            }
            rvj_int_set(ctx, failure, i, j);
        }
    }
    return failure;
}

// FilterGroup$ByteArrayFilterGroup.indexOf at source line 133
int32_t rvj_kmp_index(RVJContext *ctx, RVJBytes data, RVJBytes pattern, RVJInts failure)
{
    (void)ctx;
    int32_t patternLength = pattern.length;
    {
        int32_t i = 0;
        int32_t j = 0;
        int32_t dataLength = data.length;
        for (; !ctx->error && (i < dataLength); i = rvj_add(i, 1))
        {
            while (!ctx->error && ((j > 0) && (rvj_byte_at(ctx, pattern, j) != rvj_byte_at(ctx, data, i))))
            {
                j = rvj_int_at(ctx, failure, rvj_sub(j, 1));
            }
            if (rvj_byte_at(ctx, pattern, j) == rvj_byte_at(ctx, data, i))
            {
                j = rvj_add(j, 1);
            }
            if (j == patternLength)
            {
                return rvj_add(rvj_sub(i, patternLength), 1);
            }
        }
    }
    return -1;
}

// Generated native feature bindings.
bool rvj_native_feature_enabled(const char *feature, bool enabled) {
    if (!feature) return enabled;
    RVJContext ctx = {0};
    (void)ctx;
    if (strcmp(feature, "disable_vp9") == 0) {
        ctx.settings[RVJ_SETTING_FORCE_AVC_CODEC] = enabled;
        return !rvj_allow_vp9(&ctx);
    }
    if (strcmp(feature, "hide_info_cards") == 0) {
        ctx.settings[RVJ_SETTING_HIDE_INFO_CARDS] = enabled;
        return rvj_hide_info_cards(&ctx);
    }
    if (strcmp(feature, "hide_end_cards") == 0) {
        ctx.settings[RVJ_SETTING_HIDE_END_SCREEN_CARDS] = enabled;
        return rvj_hide_end_cards(&ctx);
    }
    if (strcmp(feature, "hide_autoplay_preview") == 0) {
        ctx.settings[RVJ_SETTING_HIDE_AUTOPLAY_PREVIEW] = enabled;
        return rvj_hide_autoplay_preview(&ctx);
    }
    if (strcmp(feature, "hide_related_overlay") == 0) {
        ctx.settings[RVJ_SETTING_HIDE_RELATED_VIDEOS_OVERLAY] = enabled;
        return rvj_hide_related_overlay(&ctx);
    }
    if (strcmp(feature, "hide_seekbar") == 0) {
        ctx.settings[RVJ_SETTING_HIDE_SEEKBAR] = enabled;
        return rvj_hide_seekbar(&ctx);
    }
    if (strcmp(feature, "tap_to_seek") == 0) {
        ctx.settings[RVJ_SETTING_TAP_TO_SEEK] = enabled;
        return rvj_tap_to_seek(&ctx);
    }
    if (strcmp(feature, "disable_shorts_resume") == 0) {
        ctx.settings[RVJ_SETTING_DISABLE_RESUMING_SHORTS_PLAYER] = enabled;
        return rvj_disable_shorts_resume(&ctx);
    }
    if (strcmp(feature, "disable_precise_seeking") == 0) {
        ctx.settings[RVJ_SETTING_DISABLE_PRECISE_SEEKING_GESTURE] = enabled;
        return rvj_precise_gesture_disabled(&ctx);
    }
    if (strcmp(feature, "disable_chapter_skip") == 0) {
        ctx.settings[RVJ_SETTING_DISABLE_CHAPTER_SKIP_DOUBLE_TAP] = enabled;
        return !rvj_chapter_double_tap_allowed(&ctx, true);
    }
    if (strcmp(feature, "disable_haptics") == 0) {
        ctx.settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_CHAPTERS] = enabled;
        ctx.settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_PRECISE_SEEKING] = enabled;
        ctx.settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_SEEK_UNDO] = enabled;
        ctx.settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_TAP_AND_HOLD] = enabled;
        ctx.settings[RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_ZOOM] = enabled;
        return rvj_disable_haptics(&ctx);
    }
    if (strcmp(feature, "disable_popup_panels") == 0) {
        ctx.settings[RVJ_SETTING_DISABLE_PLAYER_POPUP_PANELS] = enabled;
        return rvj_disable_player_popup_panels(&ctx);
    }
    if (strcmp(feature, "disable_tv_popup") == 0) {
        ctx.settings[RVJ_SETTING_DISABLE_SIGN_IN_TO_TV_POPUP] = enabled;
        return rvj_disable_tv_signin_popup(&ctx);
    }
    if (strcmp(feature, "disable_rolling_numbers") == 0) {
        ctx.settings[RVJ_SETTING_DISABLE_ROLLING_NUMBER_ANIMATIONS] = enabled;
        return rvj_disable_rolling_numbers(&ctx);
    }
    if (strcmp(feature, "hide_timestamp") == 0) {
        ctx.settings[RVJ_SETTING_HIDE_TIMESTAMP] = enabled;
        return rvj_hide_timestamp(&ctx);
    }
    if (strcmp(feature, "pause_on_interrupt") == 0) {
        ctx.settings[RVJ_SETTING_PAUSE_ON_AUDIO_INTERRUPT] = enabled;
        return rvj_pause_on_audio_interrupt(&ctx);
    }
    return enabled;
}
