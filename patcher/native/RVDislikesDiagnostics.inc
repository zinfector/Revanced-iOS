// SPDX-License-Identifier: MIT
// Passive, bounded checkpoints. Never load a node/view or alter layout to inspect it.
static NSMutableDictionary *RVDislikeServiceState;
static NSDictionary *RVDislikeEventBaseline,*RVDislikeLastVisibleCheckpoint;
static NSMutableArray *RVDislikeCheckpointHistory;
static NSString *RVDislikeCheckpointSignature;
static NSTimeInterval RVDislikeCheckpointTime,RVDislikeLastVisibleTime;
static NSUInteger RVDislikeBaselineGeneration=NSUIntegerMax;

static void RVDislikeServiceValue(NSString *key,id value) {
    if (!NSThread.isMainThread || !RVEnabled(@"diagnostics")) return;
    if (RVDislikeBaselineGeneration!=RVDislikeGeneration) RVDislikeServiceReset();
    if (!RVDislikeServiceState) RVDislikeServiceState=[NSMutableDictionary dictionary];
    RVDislikeServiceState[key]=value;
}
static void RVDislikeServiceReset(void) {
    if (!NSThread.isMainThread || !RVEnabled(@"diagnostics")) return;
    RVDislikeBaselineGeneration=RVDislikeGeneration;
    RVDislikeServiceState=[@{@"fetch_state":@"not_requested",@"http_status":@0,@"network_error_code":@0,
        @"json_parse_error_code":@0,@"response_bytes":@0,@"retry_after_seconds":@0} mutableCopy];
    @synchronized(UIImage.class) { RVDislikeEventBaseline=RVElementDislikeCounts.copy ?: @{}; }
}
static BOOL RVDislikeWatchForeground(void) {
    UIViewController *watch=RVCurrentWatch;
    if (![watch isKindOfClass:UIViewController.class] || !watch.viewIfLoaded.window) return NO;
    UIViewController *ancestor=watch;
    for (NSUInteger i=0;ancestor && i<32;i++,ancestor=ancestor.parentViewController) {
        if (ancestor.presentedViewController && !ancestor.presentedViewController.isBeingDismissed) return NO;
        UINavigationController *nav=ancestor.navigationController;
        if ([nav.viewControllers containsObject:ancestor] && nav.visibleViewController!=ancestor) return NO;
    }
    return YES;
}
static NSString *RVDislikeCheckpointDate(void) {
    static NSISO8601DateFormatter *format;static dispatch_once_t once;
    dispatch_once(&once,^{ format=[NSISO8601DateFormatter new]; });
    return [format stringFromDate:NSDate.date];
}
static NSDictionary *RVDislikeCheckpointSnapshot(NSString *trigger) {
    NSDictionary *element=RVElementDislikeReport();NSDictionary *totals=element[@"counts"];
    NSMutableDictionary *events=[NSMutableDictionary dictionary];
    for (NSString *name in totals) events[name]=@(MAX(0,[totals[name] longLongValue]-[RVDislikeEventBaseline[name] longLongValue]));
    BOOL foreground=RVDislikeWatchForeground();
    BOOL count=RVDislikeHasCount();
    NSDictionary *checks=@{@"feature_enabled":@(RVEnabled(@"return_dislikes")),
        @"playback_video_valid":@(RVVideoIDValid(RVObject(RVCurrentPlayback,"contentVideoID"))),
        @"watch_foreground":@(foreground),@"watch_player_matches":element[@"watch_player_matches"],
        @"count_matches_playback":@(count),@"dislike_icon_seen":@([events[@"dislike_icon_seen"] unsignedLongLongValue]>0),
        @"text_node_created":@([element[@"owned_text_nodes"] unsignedIntegerValue]>0),
        @"text_node_measured":@([element[@"laid_out_text_nodes"] unsignedIntegerValue]>0),
        @"text_node_mounted":@([element[@"render_mounted_nodes"] unsignedIntegerValue]>0),
        @"text_node_loaded":@([element[@"loaded_text_nodes"] unsignedIntegerValue]>0),
        @"text_layer_in_window":@([element[@"render_layers_in_window"] unsignedIntegerValue]>0),
        @"text_raster_contents_present":@([element[@"raster_content_nodes"] unsignedIntegerValue]>0),
        @"text_unclipped":@([element[@"unclipped_text_nodes"] unsignedIntegerValue]>0)};
    NSString *block=@"none_detected";
    for (NSString *key in @[@"feature_enabled",@"playback_video_valid",@"watch_foreground",@"watch_player_matches",
        @"count_matches_playback",@"dislike_icon_seen",@"text_node_created",@"text_node_measured",@"text_node_mounted",
        @"text_node_loaded",@"text_layer_in_window",@"text_raster_contents_present",@"text_unclipped"]) if (![checks[key] boolValue]) { block=key;break; }
    if (count && ![element[@"owned_text_nodes"] unsignedIntegerValue] && (RVNativeDislikeViews.count || RVDislikeActions.count))
        block=@"not_applicable_non_element_path";
    return @{@"captured_at":RVDislikeCheckpointDate(),@"trigger":trigger,@"video_generation":@(RVDislikeGeneration),
        @"scope":@"element_count_path",@"checkpoints":checks,@"first_blocked_checkpoint":block,
        @"service":RVDislikeServiceState.copy ?: @{@"fetch_state":@"not_recorded"},
        @"current_video_events":events,@"event_counter_scope":@"since_current_video_binding",
        @"watch_player_pointer_matches":element[@"watch_player_pointer_matches"],
        @"geometry":element[@"render_geometry"],@"last_adapter_gate":element[@"last_gate"],
        @"legacy_native_actions":@(RVNativeDislikeViews.count),@"inline_accessories":@(RVDislikeActions.count)};
}
static void RVDislikeCaptureCheckpoint(NSString *trigger) {
    if (!NSThread.isMainThread || !RVEnabled(@"diagnostics") || !RVDislikeWatchForeground()) return;
    if (RVDislikeBaselineGeneration!=RVDislikeGeneration) RVDislikeServiceReset();
    NSTimeInterval now=NSDate.timeIntervalSinceReferenceDate;
    if (now-RVDislikeCheckpointTime<.5 && ![trigger isEqual:@"service_response"]) return;
    RVDislikeCheckpointTime=now;
    NSDictionary *snapshot=RVDislikeCheckpointSnapshot(trigger);
    RVDislikeLastVisibleCheckpoint=snapshot;RVDislikeLastVisibleTime=now;
    NSDictionary *checks=snapshot[@"checkpoints"];
    NSString *signature=[NSString stringWithFormat:@"%lu/%@/%@/%@/%@/%@/%@/%@",(unsigned long)RVDislikeGeneration,
        snapshot[@"first_blocked_checkpoint"],snapshot[@"service"][@"fetch_state"],snapshot[@"service"][@"http_status"],
        checks[@"text_node_measured"],checks[@"text_node_mounted"],checks[@"text_node_loaded"],@[checks[@"text_raster_contents_present"],checks[@"text_unclipped"]]];
    if ([signature isEqual:RVDislikeCheckpointSignature]) return;
    RVDislikeCheckpointSignature=signature;
    if (!RVDislikeCheckpointHistory) RVDislikeCheckpointHistory=[NSMutableArray array];
    if (RVDislikeCheckpointHistory.count>=8) [RVDislikeCheckpointHistory removeObjectAtIndex:0];
    [RVDislikeCheckpointHistory addObject:snapshot];
}
static NSDictionary *RVDislikeCheckpointReport(void) {
    BOOL enabled=RVEnabled(@"diagnostics");
    BOOL current=RVDislikeLastVisibleCheckpoint && [RVDislikeLastVisibleCheckpoint[@"video_generation"] unsignedIntegerValue]==RVDislikeGeneration;
    return @{@"schema":@1,@"diagnostics_revision":@"ryd-checkpoints-4",@"build_source_sha256":RV_BUILD_SOURCE_SHA256,
        @"report_captured_at":RVDislikeCheckpointDate(),@"diagnostics_enabled":@(enabled),
        @"video_generation":@(RVDislikeGeneration),@"last_visible_watch_matches_current_video":@(current),
        @"last_visible_watch_age_seconds":RVDislikeLastVisibleCheckpoint ? @(MAX(0,NSDate.timeIntervalSinceReferenceDate-RVDislikeLastVisibleTime)) : NSNull.null,
        @"first_blocked_checkpoint":!enabled ? @"diagnostics_disabled" : current ? RVDislikeLastVisibleCheckpoint[@"first_blocked_checkpoint"] : @"foreground_watch_snapshot_missing",
        @"last_visible_watch":RVDislikeLastVisibleCheckpoint ?: @{},@"checkpoint_history":RVDislikeCheckpointHistory.copy ?: @[],
        @"current_snapshot":enabled ? RVDislikeCheckpointSnapshot(@"report_copy") : @{},
        @"inspection_is_passive":@YES};
}
static NSString *RVDislikeCheckpointSummary(void) {
    NSDictionary *report=RVDislikeCheckpointReport();NSDictionary *service=report[@"last_visible_watch"][@"service"];
    return [NSString stringWithFormat:@"RYD checkpoint: %@\nService: %@ (HTTP %@)\nVisible-player snapshot matches current video: %@\nDiagnostics: ryd-checkpoints-4\n\n",
        report[@"first_blocked_checkpoint"],service[@"fetch_state"] ?: @"not recorded",service[@"http_status"] ?: @0,
        [report[@"last_visible_watch_matches_current_video"] boolValue] ? @"yes" : @"no"];
}
