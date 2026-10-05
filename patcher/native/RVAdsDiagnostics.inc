// SPDX-License-Identifier: MIT
// Redacted, bounded ad checkpoints. All shared diagnostic state uses one lock.
@interface RVAdPlaybackState : NSObject
@property(nonatomic,weak) id controller;
@property(nonatomic,weak) id response;
@property(nonatomic,weak) id wrapper;
@property(nonatomic,weak) id coordinator;
@property(nonatomic,weak) id retiringCoordinator;
@property(nonatomic) BOOL transitionPending;
@property(nonatomic) BOOL transitionResetCompleted;
@property(nonatomic) BOOL transitionAutoplayObserved;
@property(nonatomic) BOOL transitionAutoplay;
@property(nonatomic) NSTimeInterval lastPolicyObservation;
@property(nonatomic,copy) NSString *video;
@property(nonatomic,copy) NSString *requested;
@property(nonatomic,copy) NSString *actual;
@property(nonatomic,copy) NSString *gate;
@property(nonatomic) NSUInteger generation;
@property(nonatomic,strong) NSMutableDictionary *counts;
@property(nonatomic,strong) NSMutableDictionary *values;
@end
@implementation RVAdPlaybackState @end
static const void *RVAdStateKey=&RVAdStateKey;
static NSMutableDictionary *RVAdTotals,*RVAdHooks;
static NSMutableArray *RVAdHistory;
static NSDictionary *RVAdLastVisible;
static NSString *RVAdLastSignature;
static NSUInteger RVAdGeneration;
static NSTimeInterval RVAdCaptureTime;

static void RVAdEvent(RVAdPlaybackState *state,NSString *event,NSUInteger amount) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(RVAdPlaybackState.class) {
        if (!RVAdTotals) RVAdTotals=[NSMutableDictionary dictionary];
        RVAdTotals[event]=@([RVAdTotals[event] unsignedLongLongValue]+amount);
        if (state) {
            if (!state.counts) state.counts=[NSMutableDictionary dictionary];
            state.counts[event]=@([state.counts[event] unsignedLongLongValue]+amount);
        }
    }
}
static void RVAdValue(RVAdPlaybackState *state,NSString *key,id value) {
    if (!state || !RVEnabled(@"diagnostics")) return;
    @synchronized(RVAdPlaybackState.class) {
        if (!state.values) state.values=[NSMutableDictionary dictionary];
        state.values[key]=value;
    }
}
static BOOL RVAdHook(NSString *cls,NSString *selector,NSString *abi,BOOL classMethod,RVReplacementFactory factory) {
    BOOL installed=RVHook(cls,selector,abi,classMethod,factory);
    @synchronized(RVAdPlaybackState.class) {
        if (!RVAdHooks) RVAdHooks=[NSMutableDictionary dictionary];
        RVAdHooks[[NSString stringWithFormat:@"%@ %@",cls,selector]]=@(installed);
    }
    return installed;
}
static BOOL RVAdWatchVisible(void) {
    UIViewController *watch=RVCurrentWatch;
    if (![watch isKindOfClass:UIViewController.class] || !watch.viewIfLoaded.window) return NO;
    for (NSUInteger i=0;watch && i<32;i++,watch=watch.parentViewController) {
        if (watch.presentedViewController && !watch.presentedViewController.isBeingDismissed) return NO;
        UINavigationController *nav=watch.navigationController;
        if ([nav.viewControllers containsObject:watch] && nav.visibleViewController!=watch) return NO;
    }
    return YES;
}
static NSDictionary *RVAdSnapshot(RVAdPlaybackState *state,NSString *trigger) {
    @synchronized(RVAdPlaybackState.class) {
        NSDictionary *counts=state.counts.copy ?: @{},*values=state.values.copy ?: @{};
        BOOL input=[counts[@"response_ad_items_seen"] unsignedLongLongValue]>0;
        BOOL suppressed=[counts[@"response_ad_items_suppressed"] unsignedLongLongValue]>0 ||
            ([values[@"native_no_op_selected"] boolValue] && [values[@"pointer_installed"] boolValue] && [state.actual isEqual:@"coordinator"]);
        BOOL progressed=[values[@"content_clock_progressed"] boolValue];
        BOOL ad=[values[@"playing_ad"] boolValue];
        NSString *handoff=values[@"handoff_gate"];
        NSString *block=!RVEnabled(@"video_ads") ? @"feature_disabled" : ![state.gate isEqual:@"ready"] ?
            state.gate ?: @"playback_not_observed" : !input ? @"inconclusive_no_ad_input" : !suppressed ?
            @"suppression_not_observed" : ad ? @"unexpected_ad_playback" : !progressed ? @"content_clock_not_progressing" : @"none_detected";
        if (RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"] && handoff && ![handoff isEqual:@"ready"] &&
            ![handoff isEqual:@"native_assignment_verified"]) block=handoff;
        return @{@"captured_at_epoch":@(NSDate.date.timeIntervalSince1970),@"trigger":trigger,
            @"video_generation":@(state.generation),@"requested_strategy":state.requested ?: @"unknown",
            @"actual_strategy":state.actual ?: @"native",@"eligibility_gate":state.gate ?: @"playback_not_observed",
            @"first_blocked_checkpoint":block,@"positive_response_ad_input":@(input),
            @"suppression_observed":@(suppressed),@"content_clock_progressed":@(progressed),
            @"unexpected_ad_playback":@(ad && suppressed),@"events":counts,@"state":values};
    }
}
static void RVAdCapture(RVAdPlaybackState *state,NSString *trigger) {
    if (!state || !NSThread.isMainThread || !RVEnabled(@"diagnostics") || state.controller!=RVCurrentPlayback ||
        objc_getAssociatedObject(RVCurrentPlayback,RVAdStateKey)!=state || !RVAdWatchVisible()) return;
    NSTimeInterval now=NSDate.timeIntervalSinceReferenceDate;
    if ([trigger isEqual:@"clock"] && now-RVAdCaptureTime<1) return;
    RVAdCaptureTime=now;
    NSDictionary *snapshot=RVAdSnapshot(state,trigger);
    @synchronized(RVAdPlaybackState.class) {
        RVAdLastVisible=snapshot;
        NSString *signature=[NSString stringWithFormat:@"%@/%@/%@/%@/%@",snapshot[@"video_generation"],
            snapshot[@"first_blocked_checkpoint"],snapshot[@"actual_strategy"],snapshot[@"state"][@"preroll_completed"],
            snapshot[@"state"][@"postroll_completed"]];
        if ([signature isEqual:RVAdLastSignature]) return;
        RVAdLastSignature=signature;
        if (!RVAdHistory) RVAdHistory=[NSMutableArray array];
        if (RVAdHistory.count>=12) [RVAdHistory removeObjectAtIndex:0];
        [RVAdHistory addObject:snapshot];
    }
}
static NSDictionary *RVAdReport(void) {
    RVAdPlaybackState *state=objc_getAssociatedObject(RVCurrentPlayback,RVAdStateKey);
    BOOL owned=state && state.response && state.response==RVContentResponse(RVCurrentPlayback) && [state.video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")];
    @synchronized(RVAdPlaybackState.class) {
        BOOL matches=state && RVAdLastVisible && [RVAdLastVisible[@"video_generation"] unsignedIntegerValue]==state.generation;
        NSString *block=!RVEnabled(@"diagnostics") ? @"diagnostics_disabled" : !state ? @"controller_lease_missing" :
            ![state.gate isEqual:@"ready"] ? state.gate ?: @"response_resolution_missing" : !owned ? @"response_owner_mismatch" :
            !matches ? @"foreground_watch_snapshot_missing" : RVAdLastVisible[@"first_blocked_checkpoint"];
        return @{@"schema":@2,@"diagnostics_revision":@"adblock-checkpoints-2",@"build_source_sha256":RV_BUILD_SOURCE_SHA256,
            @"diagnostics_enabled":@(RVEnabled(@"diagnostics")),@"profile_accepted":@(RVCompatible),
            @"video_ads_enabled":@(RVEnabled(@"video_ads")),@"feed_ads_enabled":@(RVEnabled(@"feed_ads")),
            @"shorts_ads_enabled":@(RVEnabled(@"shorts_ads")),@"requested_strategy":RVSetting(@"ad_strategy") ?: @"response",
            @"hooks":RVAdHooks.copy ?: @{},@"event_counter_scope":@"session_totals; playback events reset at content selection or internal transition",
            @"session_events":RVAdTotals.copy ?: @{},@"last_visible_watch":RVAdLastVisible ?: @{},
            @"last_visible_watch_matches_current_playback":@(matches && owned),
            @"last_visible_watch_matches_current_generation":@(matches),@"current_controller_has_lease":@(state!=nil),
            @"factory_seen":@([RVAdTotals[@"factory_entered"] unsignedLongLongValue]>0),
            @"factory_response_matches_current_content":@(owned && [state.values[@"factory_response_matches_controller"] boolValue]),
            @"current_factory_binding_matches_playback":@(owned && [state.counts[@"factory_entered"] unsignedLongLongValue]>0),
            @"current_response_lease_matches_playback":@(owned),
            @"first_blocked_checkpoint":block,
            @"last_visible_watch_age_seconds":RVAdLastVisible ? @(MAX(0,NSDate.date.timeIntervalSince1970-[RVAdLastVisible[@"captured_at_epoch"] doubleValue])) : NSNull.null,
            @"current_playback":state ? RVAdSnapshot(state,@"report_copy") : @{},@"checkpoint_history":RVAdHistory.copy ?: @[],
            @"inspection_is_passive":@YES,@"live_DAI_coverage":@"native_passthrough",
            @"element_rules":@"compatibility_patterns_with_protected_contexts; broad Android rules require native structural mapping",
            @"device_validated":@NO};
    }
}
static NSString *RVAdSummary(void) {
    NSDictionary *report=RVAdReport();
    return [NSString stringWithFormat:@"Ad-block checkpoint: %@\nDiagnostics: adblock-checkpoints-2\n\n",report[@"first_blocked_checkpoint"]];
}
