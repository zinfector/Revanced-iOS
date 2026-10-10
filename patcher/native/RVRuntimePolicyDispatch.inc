// Generated from existing source-reuse and capability catalogs.
static BOOL RVRuntimePolicyKnown(NSString *feature) { return [@[
    @"disable_haptics",
    @"disable_chapter_skip",
    @"disable_precise_seeking",
    @"hide_info_cards",
    @"hide_end_cards",
    @"hide_autoplay_preview",
    @"hide_related_overlay",
    @"hide_seekbar",
    @"disable_shorts_resume",
    @"tap_to_seek",
    @"disable_vp9",
    @"disable_popup_panels",
    @"disable_tv_popup",
    @"disable_rolling_numbers",
    @"hide_timestamp",
    @"pause_on_interrupt",
] containsObject:feature]; }
static BOOL RVRuntimeDispatch(NSString *feature,RVContext *context,BOOL *result) {
    void *preferences=RVRuntimePreferences();
    if ([feature isEqual:@"disable_haptics"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_CHAPTERS")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_CHAPTERS",enabled)!=0) context->error=4;
        if (rv_ios_read_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_PRECISE_SEEKING")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_PRECISE_SEEKING",enabled)!=0) context->error=4;
        if (rv_ios_read_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_SEEK_UNDO")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_SEEK_UNDO",enabled)!=0) context->error=4;
        if (rv_ios_read_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_TAP_AND_HOLD")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_TAP_AND_HOLD",enabled)!=0) context->error=4;
        if (rv_ios_read_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_ZOOM")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_HAPTIC_FEEDBACK_ZOOM",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_disable_haptics(context,preferences);return YES;
    }
    if ([feature isEqual:@"disable_chapter_skip"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"DISABLE_CHAPTER_SKIP_DOUBLE_TAP")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_CHAPTER_SKIP_DOUBLE_TAP",enabled)!=0) context->error=4;
        *result=!rv_ios_policy_rvj_chapter_double_tap_allowed(context,preferences,true);return YES;
    }
    if ([feature isEqual:@"disable_precise_seeking"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"DISABLE_PRECISE_SEEKING_GESTURE")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_PRECISE_SEEKING_GESTURE",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_precise_gesture_disabled(context,preferences);return YES;
    }
    if ([feature isEqual:@"hide_info_cards"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"HIDE_INFO_CARDS")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"HIDE_INFO_CARDS",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_hide_info_cards(context,preferences);return YES;
    }
    if ([feature isEqual:@"hide_end_cards"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"HIDE_END_SCREEN_CARDS")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"HIDE_END_SCREEN_CARDS",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_hide_end_cards(context,preferences);return YES;
    }
    if ([feature isEqual:@"hide_autoplay_preview"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"HIDE_AUTOPLAY_PREVIEW")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"HIDE_AUTOPLAY_PREVIEW",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_hide_autoplay_preview(context,preferences);return YES;
    }
    if ([feature isEqual:@"hide_related_overlay"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"HIDE_RELATED_VIDEOS_OVERLAY")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"HIDE_RELATED_VIDEOS_OVERLAY",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_hide_related_overlay(context,preferences);return YES;
    }
    if ([feature isEqual:@"hide_seekbar"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"HIDE_SEEKBAR")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"HIDE_SEEKBAR",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_hide_seekbar(context,preferences);return YES;
    }
    if ([feature isEqual:@"disable_shorts_resume"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"DISABLE_RESUMING_SHORTS_PLAYER")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_RESUMING_SHORTS_PLAYER",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_disable_shorts_resume(context,preferences);return YES;
    }
    if ([feature isEqual:@"tap_to_seek"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"TAP_TO_SEEK")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"TAP_TO_SEEK",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_tap_to_seek(context,preferences);return YES;
    }
    if ([feature isEqual:@"disable_vp9"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"FORCE_AVC_CODEC")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"FORCE_AVC_CODEC",enabled)!=0) context->error=4;
        *result=!rv_ios_policy_rvj_allow_vp9(context,preferences);return YES;
    }
    if ([feature isEqual:@"disable_popup_panels"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"DISABLE_PLAYER_POPUP_PANELS")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_PLAYER_POPUP_PANELS",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_disable_player_popup_panels(context,preferences);return YES;
    }
    if ([feature isEqual:@"disable_tv_popup"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"DISABLE_SIGN_IN_TO_TV_POPUP")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_SIGN_IN_TO_TV_POPUP",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_disable_tv_signin_popup(context,preferences);return YES;
    }
    if ([feature isEqual:@"disable_rolling_numbers"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"DISABLE_ROLLING_NUMBER_ANIMATIONS")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"DISABLE_ROLLING_NUMBER_ANIMATIONS",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_disable_rolling_numbers(context,preferences);return YES;
    }
    if ([feature isEqual:@"hide_timestamp"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"HIDE_TIMESTAMP")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"HIDE_TIMESTAMP",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_hide_timestamp(context,preferences);return YES;
    }
    if ([feature isEqual:@"pause_on_interrupt"]) {
        BOOL enabled=RVEnabled(feature);
        if (rv_ios_read_bool(preferences,"PAUSE_ON_AUDIO_INTERRUPT")!=(enabled ? 1 : 0) && rv_ios_write_bool(preferences,"PAUSE_ON_AUDIO_INTERRUPT",enabled)!=0) context->error=4;
        *result=rv_ios_policy_rvj_pause_on_audio_interrupt(context,preferences);return YES;
    }
    return NO;
}
