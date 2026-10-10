#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include "CapabilityRuntime.h"

void *rv_ios_create_preferences(const char *suite_name) {
    @autoreleasepool {
        NSString *name = suite_name ? [NSString stringWithUTF8String:suite_name] : @"org.revanced.translated-patches";
        if (!name || ![name length]) return NULL;
        return [[NSUserDefaults alloc] initWithSuiteName:name];
    }
}
void rv_ios_release_preferences(void *preferences) { [(NSUserDefaults *)preferences release]; }
int32_t rv_ios_read_bool(void *preferences, const char *name) {
    @autoreleasepool {
        int32_t index = rv_find_setting(name);
        if (index < 0 || !preferences || ![(id)preferences isKindOfClass:[NSUserDefaults class]]) return -1;
        NSString *key = [NSString stringWithUTF8String:rv_setting_key((uint32_t)index)];
        id value = [(NSUserDefaults *)preferences objectForKey:key];
        if (!value) return rv_setting_default_value((uint32_t)index);
        if (![value isKindOfClass:[NSNumber class]]) return -2;
        return [value boolValue] ? 1 : 0;
    }
}
int32_t rv_ios_write_bool(void *preferences, const char *name, bool value) {
    @autoreleasepool {
        int32_t index = rv_find_setting(name);
        if (index < 0 || !preferences || ![(id)preferences isKindOfClass:[NSUserDefaults class]]) return -1;
        [(NSUserDefaults *)preferences setBool:value forKey:[NSString stringWithUTF8String:rv_setting_key((uint32_t)index)]];
        return 0;
    }
}
int32_t rv_ios_set_hidden(void *view, bool hidden) {
    if (![NSThread isMainThread]) return -3;
    if (!view || ![(id)view isKindOfClass:[UIView class]]) return -1;
    [(UIView *)view setHidden:hidden]; return 0;
}
int32_t rv_ios_get_hidden(void *view) {
    if (![NSThread isMainThread]) return -3;
    if (!view || ![(id)view isKindOfClass:[UIView class]]) return -1;
    return [(UIView *)view isHidden] ? 1 : 0;
}
typedef struct { void *preferences; RVContext *ctx; } RVPreferenceReader;
static bool rv_ios_policy_reader(void *user, const char *name) {
    RVPreferenceReader *reader = (RVPreferenceReader *)user;
    int32_t result = rv_ios_read_bool(reader->preferences, name);
    if (result < 0) { reader->ctx->error = 4; return false; }
    return result != 0;
}
bool rv_ios_policy_rvj_disable_haptics(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_disable_haptics(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_chapter_double_tap_allowed(RVContext *ctx, void *preferences, bool original) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_chapter_double_tap_allowed(ctx, rv_ios_policy_reader, &reader, original);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_precise_gesture_disabled(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_precise_gesture_disabled(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_hide_info_cards(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_hide_info_cards(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_hide_end_cards(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_hide_end_cards(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_hide_autoplay_preview(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_hide_autoplay_preview(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_hide_related_overlay(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_hide_related_overlay(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_hide_seekbar(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_hide_seekbar(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_disable_shorts_resume(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_disable_shorts_resume(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_tap_to_seek(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_tap_to_seek(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_allow_vp9(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_allow_vp9(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_disable_player_popup_panels(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_disable_player_popup_panels(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_disable_tv_signin_popup(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_disable_tv_signin_popup(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_disable_rolling_numbers(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_disable_rolling_numbers(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_hide_timestamp(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_hide_timestamp(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
bool rv_ios_policy_rvj_pause_on_audio_interrupt(RVContext *ctx, void *preferences) {
    if (!ctx || ctx->error) return 0;
    RVPreferenceReader reader = {preferences, ctx};
    bool result = rv_bound_policy_rvj_pause_on_audio_interrupt(ctx, rv_ios_policy_reader, &reader);
    return ctx->error ? 0 : result;
}
