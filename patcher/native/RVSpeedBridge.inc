// SPDX-License-Identifier: MIT
// The granular slider calls YTPlayerBlockImpl, not the legacy menu delegate.
// Repair only a missing player during these synchronous, visible-watch calls.
static _Thread_local NSUInteger RVSpeedBridgeScope;
static atomic_uint RVSpeedBridgeCreated,RVSpeedBridgeInitMissing,RVSpeedBridgeGets,RVSpeedBridgeSets;
static atomic_uint RVSpeedBridgeRetrievals,RVSpeedBridgeMissing,RVSpeedBridgeRecovered,RVSpeedBridgeRejected;
static atomic_uint RVSpeedBridgeSubscriptions,RVSpeedBridgeEventVideoMissing;
static NSString *RVSpeedBridgeGate=@"not_observed";
static NSString *RVSpeedBridgeLastCall=@"not_observed";
static float RVSpeedBridgeArgumentRate,RVSpeedBridgeArgumentScalar,RVSpeedBridgeResultRate;
static BOOL RVSpeedBridgeResultHasRate,RVSpeedBridgeCallError;
static id RVSpeedVisiblePlayer(void) {
    if (!NSThread.isMainThread || !RVCompatible || !RVOrdinaryContent(RVCurrentPlayback)) return nil;
    UIViewController *watch=RVCurrentWatch;
    if (![watch isKindOfClass:NSClassFromString(@"YTWatchViewController")] || !watch.isViewLoaded || !watch.view.window) return nil;
    id player=RVObject(watch,"playerViewController");NSString *video=RVSpeedVideo(player);
    return RVSpeedCurrent(player,video) ? player : nil;
}
static NSDictionary *RVSpeedBridgeReport(void) {
    return @{@"schema":@1,@"gate":RVSpeedBridgeGate ?: @"not_observed",
        @"blocks_created":@(atomic_load(&RVSpeedBridgeCreated)),@"initial_player_missing":@(atomic_load(&RVSpeedBridgeInitMissing)),
        @"getter_calls":@(atomic_load(&RVSpeedBridgeGets)),@"selection_calls":@(atomic_load(&RVSpeedBridgeSets)),
        @"player_retrieval_calls":@(atomic_load(&RVSpeedBridgeRetrievals)),@"native_player_missing":@(atomic_load(&RVSpeedBridgeMissing)),
        @"visible_watch_recoveries":@(atomic_load(&RVSpeedBridgeRecovered)),@"recovery_rejected":@(atomic_load(&RVSpeedBridgeRejected)),
        @"rate_subscriptions":@(atomic_load(&RVSpeedBridgeSubscriptions)),@"event_video_missing_at_init":@(atomic_load(&RVSpeedBridgeEventVideoMissing)),
        @"last_call":RVSpeedBridgeLastCall,@"last_call_error":@(RVSpeedBridgeCallError),
        @"argument_rate":@(isfinite(RVSpeedBridgeArgumentRate) ? RVSpeedBridgeArgumentRate : 0),
        @"argument_scalar":@(isfinite(RVSpeedBridgeArgumentScalar) ? RVSpeedBridgeArgumentScalar : 0),
        @"result_rate_available":@(RVSpeedBridgeResultHasRate),
        @"result_rate":@(isfinite(RVSpeedBridgeResultRate) ? RVSpeedBridgeResultRate : 0),
        @"visible_watch_player_ready":@(RVSpeedVisiblePlayer()!=nil)};
}
static void RVInstallSpeedBridge(void) {
    RVHook(@"YTPlayerBlockImpl",@"initWithContext:playerViewController:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id block,id context,id player) {
            atomic_fetch_add(&RVSpeedBridgeCreated,1);if (!player) atomic_fetch_add(&RVSpeedBridgeInitMissing,1);
            return ((id(*)(id,SEL,id,id))original)(block,sel,context,player);
        };
    });
    RVHook(@"YTPlayerBlockImpl",@"retrievePlayerOrFailWithError:",@"@@:^@",NO,^id(IMP original,SEL sel) {
        return ^id(id block,NSError **error) {
            if (!NSThread.isMainThread || !RVSpeedBridgeScope) return ((id(*)(id,SEL,NSError **))original)(block,sel,error);
            atomic_fetch_add(&RVSpeedBridgeRetrievals,1);NSError *nativeError=nil;
            id player=((id(*)(id,SEL,NSError **))original)(block,sel,&nativeError);
            if (player) { RVSpeedBridgeGate=@"native_player_present";return player; }
            atomic_fetch_add(&RVSpeedBridgeMissing,1);player=RVSpeedVisiblePlayer();
            if (player) {
                atomic_fetch_add(&RVSpeedBridgeRecovered,1);RVSpeedBridgeGate=@"visible_watch_player_recovered";
                return player;
            }
            atomic_fetch_add(&RVSpeedBridgeRejected,1);RVSpeedBridgeGate=@"missing_player_without_visible_content";
            if (error) *error=nativeError;return nil;
        };
    });
    for (NSString *name in @[@"getPlaybackRate:error:",@"getPlaybackRateModel:error:",@"getPlayerEventCenter:error:",@"setPlaybackRate:error:",@"setPlaybackRateModel:error:"]) {
        BOOL selection=[name hasPrefix:@"setPlaybackRate"];
        RVHook(@"YTPlayerBlockImpl",name,@"@@:@^@",NO,^id(IMP original,SEL sel) {
            return ^id(id block,id args,NSError **error) {
                if (!NSThread.isMainThread) return ((id(*)(id,SEL,id,NSError **))original)(block,sel,args,error);
                if (selection) atomic_fetch_add(&RVSpeedBridgeSets,1);else atomic_fetch_add(&RVSpeedBridgeGets,1);
                RVSpeedBridgeLastCall=name;
                if (selection) {
                    RVSpeedBridgeArgumentRate=RVSpeedFloat(args,[name isEqual:@"setPlaybackRate:error:"] ? "playbackRate" : "rate");
                    RVSpeedBridgeArgumentScalar=RVSpeedFloat(args,"scalar");
                }
                BOOL previous=RVSpeedBridgeCapture;RVSpeedBridgeScope++;RVSpeedBridgeCapture=selection;
                id result;NSError *nativeError=nil;
                @try { result=((id(*)(id,SEL,id,NSError **))original)(block,sel,args,error ? &nativeError : NULL); }
                @finally { RVSpeedBridgeScope--;RVSpeedBridgeCapture=previous; }
                RVSpeedBridgeCallError=nativeError!=nil;if (error && nativeError) *error=nativeError;
                if ([name isEqual:@"getPlaybackRate:error:"]) {
                    RVSpeedBridgeResultRate=RVSpeedFloat(result,"playbackRate");
                    RVSpeedBridgeResultHasRate=RVBool(result,"hasPlaybackRate") && isfinite(RVSpeedBridgeResultRate);
                } else if ([name isEqual:@"getPlaybackRateModel:error:"]) {
                    RVSpeedBridgeResultRate=RVSpeedFloat(result,"activeRate");
                    RVSpeedBridgeResultHasRate=RVBool(result,"hasActiveRate") && isfinite(RVSpeedBridgeResultRate);
                }
                return result;
            };
        });
    }
    // The native event-center factory receives the recovered player and active
    // video. Preserve its observers, writers, return values and event formats.
    RVHook(@"YTPlaybackEventCenterBlockImpl",@"initWithContext:video:playerViewController:",@"@@:@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id block,id context,id video,id player) {
            if (!video) atomic_fetch_add(&RVSpeedBridgeEventVideoMissing,1);
            return ((id(*)(id,SEL,id,id,id))original)(block,sel,context,video,player);
        };
    });
    for (NSString *name in @[@"subscribeToPlaybackRateChange:writer:error:",@"subscribeToActivePlaybackRateChange:writer:error:"])
        RVHook(@"YTPlaybackEventCenterBlockImpl",name,@"B@:@@^@",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id block,id args,id writer,NSError **error) {
                atomic_fetch_add(&RVSpeedBridgeSubscriptions,1);
                return ((BOOL(*)(id,SEL,id,id,NSError **))original)(block,sel,args,writer,error);
            };
        });
}
