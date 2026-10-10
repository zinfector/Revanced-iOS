#ifndef RV_SOURCE_KERNELS_H
#define RV_SOURCE_KERNELS_H
#include "RVJavaSupport.h"
enum {
    RVJ_SETTING_HIDE_VIDEO_ADS = 0,
    RVJ_SETTING_DISABLE_SHORTS_BACKGROUND_PLAYBACK = 1,
    RVJ_SETTING_DISABLE_AUTO_CAPTIONS = 2,
    RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_CHAPTERS = 3,
    RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_PRECISE_SEEKING = 4,
    RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_SEEK_UNDO = 5,
    RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_TAP_AND_HOLD = 6,
    RVJ_SETTING_DISABLE_HAPTIC_FEEDBACK_ZOOM = 7,
    RVJ_SETTING_DISABLE_FULLSCREEN_AMBIENT_MODE = 8,
    RVJ_SETTING_DISABLE_CHAPTER_SKIP_DOUBLE_TAP = 9,
    RVJ_SETTING_DISABLE_PRECISE_SEEKING_GESTURE = 10,
    RVJ_SETTING_HIDE_INFO_CARDS = 11,
    RVJ_SETTING_HIDE_END_SCREEN_CARDS = 12,
    RVJ_SETTING_HIDE_AUTOPLAY_PREVIEW = 13,
    RVJ_SETTING_HIDE_RELATED_VIDEOS_OVERLAY = 14,
    RVJ_SETTING_HIDE_SEEKBAR = 15,
    RVJ_SETTING_FULLSCREEN_LARGE_SEEKBAR = 16,
    RVJ_SETTING_DISABLE_RESUMING_SHORTS_PLAYER = 17,
    RVJ_SETTING_TAP_TO_SEEK = 18,
    RVJ_SETTING_SLIDE_TO_SEEK = 19,
    RVJ_SETTING_FORCE_AVC_CODEC = 20,
    RVJ_SETTING_DISABLE_PLAYER_POPUP_PANELS = 21,
    RVJ_SETTING_DISABLE_SIGN_IN_TO_TV_POPUP = 22,
    RVJ_SETTING_DISABLE_ROLLING_NUMBER_ANIMATIONS = 23,
    RVJ_SETTING_HIDE_TIMESTAMP = 24,
    RVJ_SETTING_PAUSE_ON_AUDIO_INTERRUPT = 25,
    RVJ_SETTING_COUNT = 26
};
enum { RVJ_PLAYER_NONE, RVJ_PLAYER_HIDDEN, RVJ_PLAYER_INLINE, RVJ_PLAYER_MAXIMIZED, RVJ_PLAYER_FULLSCREEN };
struct RVJContext { bool settings[RVJ_SETTING_COUNT]; bool shorts_open; int32_t player_type; int error;
    bool field_VideoAdsPatch_SHOW_VIDEO_ADS;
    bool field_DisableAutoCaptionsPatch_captionsButtonStatus;
    bool field_DisableFullscreenAmbientModePatch_DISABLE_FULLSCREEN_AMBIENT_MODE;
    int32_t field_DisableFullscreenAmbientModePatch_DIVIDER_ATTRIBUTES_COLOR_SYSTEM_DEFAULT;
    bool field_SlideToSeekPatch_SLIDE_TO_SEEK_DISABLED;
    float field_FixPlaybackSpeedWhilePlayingPatch_DEFAULT_YOUTUBE_PLAYBACK_SPEED;
};
bool rvj_native_feature_enabled(const char *feature, bool enabled);
void rvj_context_init(RVJContext *ctx);
bool rvj_show_video_ads(RVJContext *ctx);
bool rvj_background_allowed(RVJContext *ctx, bool original);
bool rvj_shorts_background_allowed(RVJContext *ctx, bool original);
bool rvj_disable_auto_captions(RVJContext *ctx);
void rvj_set_captions_status(RVJContext *ctx, bool status);
bool rvj_disable_chapter_haptic(RVJContext *ctx);
bool rvj_disable_precise_haptic(RVJContext *ctx);
bool rvj_disable_undo_haptic(RVJContext *ctx);
bool rvj_disable_zoom_haptic(RVJContext *ctx);
bool rvj_disable_haptics(RVJContext *ctx);
int32_t rvj_fullscreen_background_color(RVJContext *ctx, int32_t originalColor);
bool rvj_chapter_double_tap_allowed(RVJContext *ctx, bool original);
bool rvj_precise_gesture_disabled(RVJContext *ctx);
bool rvj_hide_info_cards(RVJContext *ctx);
bool rvj_hide_end_cards(RVJContext *ctx);
bool rvj_hide_autoplay_preview(RVJContext *ctx);
bool rvj_hide_related_overlay(RVJContext *ctx);
bool rvj_hide_seekbar(RVJContext *ctx);
bool rvj_large_seekbar(RVJContext *ctx, bool original);
bool rvj_disable_shorts_resume(RVJContext *ctx);
bool rvj_resume_shorts_allowed(RVJContext *ctx, bool original);
bool rvj_tap_to_seek(RVJContext *ctx);
bool rvj_slide_to_seek_disabled(RVJContext *ctx, bool isDisabled);
bool rvj_block_speed_reset(RVJContext *ctx, float playbackSpeed);
bool rvj_allow_vp9(RVJContext *ctx);
bool rvj_disable_player_popup_panels(RVJContext *ctx);
bool rvj_disable_tv_signin_popup(RVJContext *ctx);
bool rvj_disable_rolling_numbers(RVJContext *ctx);
bool rvj_hide_timestamp(RVJContext *ctx);
bool rvj_pause_on_audio_interrupt(RVJContext *ctx);
RVJInts rvj_kmp_failure(RVJContext *ctx, RVJBytes pattern);
int32_t rvj_kmp_index(RVJContext *ctx, RVJBytes data, RVJBytes pattern, RVJInts failure);
#endif
