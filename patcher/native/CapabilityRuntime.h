#ifndef RV_CAPABILITY_RUNTIME_H
#define RV_CAPABILITY_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { int32_t error; uint32_t steps; uint32_t limit; } RVContext;
typedef struct { const uint8_t *data; size_t size; } RVBuffer;

typedef bool (*RVReadBool)(void *, const char *);
uint32_t rv_setting_count(void);
int32_t rv_setting_default_value(uint32_t index);
int32_t rv_find_setting(const char *name);
const char *rv_setting_key(uint32_t index);
int32_t rv_visibility_convert(int32_t visibility);
/* Settings read callbacks use source symbols or persisted keys. */
void *rv_ios_create_preferences(const char *suite_name);
void rv_ios_release_preferences(void *preferences);
int32_t rv_ios_read_bool(void *preferences, const char *name);
int32_t rv_ios_write_bool(void *preferences, const char *name, bool value);
int32_t rv_ios_set_hidden(void *view, bool hidden);
int32_t rv_ios_get_hidden(void *view);
bool rv_bound_policy_rvj_disable_haptics(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_disable_haptics(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_chapter_double_tap_allowed(RVContext *ctx, RVReadBool read_setting, void *user, bool original);
bool rv_ios_policy_rvj_chapter_double_tap_allowed(RVContext *ctx, void *preferences, bool original);
bool rv_bound_policy_rvj_precise_gesture_disabled(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_precise_gesture_disabled(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_hide_info_cards(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_hide_info_cards(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_hide_end_cards(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_hide_end_cards(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_hide_autoplay_preview(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_hide_autoplay_preview(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_hide_related_overlay(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_hide_related_overlay(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_hide_seekbar(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_hide_seekbar(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_disable_shorts_resume(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_disable_shorts_resume(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_tap_to_seek(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_tap_to_seek(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_allow_vp9(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_allow_vp9(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_disable_player_popup_panels(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_disable_player_popup_panels(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_disable_tv_signin_popup(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_disable_tv_signin_popup(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_disable_rolling_numbers(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_disable_rolling_numbers(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_hide_timestamp(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_hide_timestamp(RVContext *ctx, void *preferences);
bool rv_bound_policy_rvj_pause_on_audio_interrupt(RVContext *ctx, RVReadBool read_setting, void *user);
bool rv_ios_policy_rvj_pause_on_audio_interrupt(RVContext *ctx, void *preferences);

#endif
