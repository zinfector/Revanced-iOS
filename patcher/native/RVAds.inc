// SPDX-License-Identifier: MIT
// Native ad policy. Response ownership follows content selection and transitions.
#include "RVAdsDiagnostics.inc"
static NSString *const RVAdConfigScope=@"RVPort.AdConfigScope";
static BOOL RVAdNoOpHookReady;
static BOOL RVVideoIDValid(id value);

static BOOL RVAdResolve(id object,const char *name,NSString *abi) {
    if (RVCan(object,name,abi)) return YES;
    Class cls=object_getClass(object);
    if ([cls respondsToSelector:@selector(resolveInstanceMethod:)]) [cls resolveInstanceMethod:sel_registerName(name)];
    return RVCan(object,name,abi);
}
static BOOL RVAdResponsePolicy(RVAdPlaybackState *state,id raw) {
    id current=RVContentResponse(state.controller);
    @synchronized(RVAdPlaybackState.class) {
        return state && state.controller && state.response==raw && state.response==current &&
            RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"] && [state.actual isEqual:@"response"];
    }
}
static id RVAdResolvedObject(id object,const char *name) {
    return RVAdResolve(object,name,@"@@:") ? RVObject(object,name) : nil;
}
static NSString *RVAdEligibility(id delegate,id parent,id wrapper,id raw) {
    Class local=NSClassFromString(@"YTLocalPlaybackController"),response=NSClassFromString(@"YTPlayerResponse"),playerData=NSClassFromString(@"YTIPlayerResponse");
    if (!local || ![delegate isKindOfClass:local]) return @"non_local_delegate";
    if (!wrapper) return @"wrapper_nil";
    if (!response || ![wrapper isKindOfClass:response]) return @"wrapper_class_mismatch";
    if (!RVAdResolve(wrapper,"playerData",@"@@:")) {
        return [wrapper respondsToSelector:sel_registerName("playerData")] ? @"player_data_abi_mismatch" : @"player_data_accessor_missing";
    }
    if (!raw) return @"player_data_nil";
    if (!playerData || ![raw isKindOfClass:playerData]) return @"raw_class_mismatch";
    if (raw!=RVContentResponse(delegate)) return @"response_owner_mismatch";
    if (!RVCan(raw,"isLivePlayback",@"B@:") || !RVCan(wrapper,"isDAICompatible",@"B@:") || !RVCan(raw,"isDAIEnabledPlayback",@"B@:")) return @"content_flags_unknown";
    if (RVBool(raw,"isLivePlayback")) return @"live_passthrough";
    if (RVBool(wrapper,"isDAICompatible")) return @"DAI_passthrough";
    if (RVBool(raw,"isDAIEnabledPlayback")) return @"DAI_passthrough";
    id video=RVObject(wrapper,"videoID");
    if (!RVVideoIDValid(video)) return @"video_identity_unknown";
    id owner=parent ?: delegate;BOOL watch=NO;
    Class watchVC=NSClassFromString(@"YTWatchViewController"),watchController=NSClassFromString(@"YTWatchController");
    for (NSUInteger i=0;owner && i<16;i++) {
        if ([NSStringFromClass([owner class]) hasPrefix:@"YTReel"]) return @"shorts_passthrough";
        watch|=(watchVC && [owner isKindOfClass:watchVC]) || (watchController && [owner isKindOfClass:watchController]);
        id next=RVObject(owner,"parentResponder");if (next==owner) break;owner=next;
    }
    return watch ? @"ready" : @"watch_owner_unknown";
}
static RVAdPlaybackState *RVAdPrepare(id delegate,id parent,id wrapper,NSString *origin,BOOL boundary) {
    Class local=NSClassFromString(@"YTLocalPlaybackController");
    if (!local || ![delegate isKindOfClass:local]) { RVAdEvent(nil,@"non_local_delegate",1);return nil; }
    id raw=RVAdResolvedObject(wrapper,"playerData");
    NSString *gate=RVAdEligibility(delegate,parent,wrapper,raw);
    RVAdPlaybackState *state=objc_getAssociatedObject(delegate,RVAdStateKey);
    id video=RVObject(wrapper,"videoID");
    @synchronized(RVAdPlaybackState.class) {
        // Factory calls can precede content. Generation belongs to content
        // selection/transition, including a replay of an identical response.
        if (!state || boundary || state.wrapper!=wrapper || state.response!=raw) {
            state=[RVAdPlaybackState new];state.controller=delegate;state.response=raw;state.wrapper=wrapper;
            state.video=[video isKindOfClass:NSString.class] ? video : nil;
            state.generation=++RVAdGeneration;state.counts=[NSMutableDictionary dictionary];state.values=[NSMutableDictionary dictionary];
            objc_setAssociatedObject(delegate,RVAdStateKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }
        state.gate=gate;state.requested=RVSetting(@"ad_strategy");
        state.actual=RVEnabled(@"video_ads") && [gate isEqual:@"ready"] ? @"response" : @"native";
        Class rawClass=NSClassFromString(@"YTIPlayerResponse");
        if (rawClass && [raw isKindOfClass:rawClass]) objc_setAssociatedObject(raw,RVAdStateKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    RVAdValue(state,@"binding_origin",origin);RVAdValue(state,@"wrapper_present",@(wrapper!=nil));
    RVAdValue(state,@"wrapper_class",wrapper ? NSStringFromClass([wrapper class]) : @"nil");
    RVAdValue(state,@"raw_response_class",raw ? NSStringFromClass([raw class]) : @"nil");
    RVAdValue(state,@"player_data_accessor_present",@([wrapper respondsToSelector:sel_registerName("playerData")]));
    RVAdValue(state,@"player_data_accessor_abi_matches",@(RVCan(wrapper,"playerData",@"@@:")));
    RVAdValue(state,@"controller_response_matches",@(raw && raw==RVContentResponse(delegate)));
    RVAdValue(state,@"trigger_strategy_falls_back_to_response",@([state.requested isEqual:@"trigger"]));
    return state;
}
static void RVAdRequestCoordinatorPolicy(id controller,NSString *reason);
static void RVAdObserveClock(id controller,id timeObject) {
    if (!NSThread.isMainThread || controller!=RVCurrentPlayback) return;
    RVAdRequestCoordinatorPolicy(controller,@"clock");
    if (!RVEnabled(@"diagnostics")) return;
    RVAdPlaybackState *state=objc_getAssociatedObject(controller,RVAdStateKey);if (!state) return;
    id video=RVObject(controller,"contentVideoID");
    if (![state.video isEqual:video] || !state.response || state.response!=RVContentResponse(controller)) { RVAdCapture(state,@"response_gate");return; }
    RVAdValue(state,@"playing_ad",@(RVBool(controller,"isPlayingAd")));
    id activeTime=RVObject(controller,"contentVideoCurrentTime");
    id activeCPN=RVObject(activeTime,"CPN") ?: RVObject(controller,"contentVideoCPN");
    id eventCPN=RVObject(timeObject,"CPN");
    RVAdValue(state,@"active_cpn_present",@(activeCPN!=nil));RVAdValue(state,@"event_cpn_present",@(eventCPN!=nil));
    RVAdValue(state,@"clock_cpn_matches",@(activeCPN && eventCPN && [activeCPN isEqual:eventCPN]));
    if (activeCPN && eventCPN && ![activeCPN isEqual:eventCPN]) { RVAdEvent(state,@"stale_clock_ignored",1);return; }
    double seconds=RVDouble(timeObject,"time");
    if (!isfinite(seconds)) seconds=RVDouble(activeTime,"time");
    if (isfinite(seconds) && seconds>=0 && RVCan(controller,"isPlayingContentVideo",@"B@:") && RVBool(controller,"isPlayingContentVideo")) {
        @synchronized(RVAdPlaybackState.class) {
            NSNumber *previous=state.values[@"content_time_seconds"];
            if (previous && seconds>previous.doubleValue+.05) RVAdValue(state,@"content_clock_progressed",@YES);
        }
        RVAdValue(state,@"content_time_seconds",@(seconds));
    }
    RVAdCapture(state,@"clock");
}
#include "RVAdCoordinator.inc"
static void RVInstallAds(void) {
    for (NSString *field in @[@"playerAdsArray",@"adPlacementsArray",@"adSlotsArray"]) {
        RVAdHook(@"YTIPlayerResponse",field,@"@@:",NO,^id(IMP original,SEL sel) {
            return ^id(id object) {
                id items=((id(*)(id,SEL))original)(object,sel);
                RVAdPlaybackState *state=objc_getAssociatedObject(object,RVAdStateKey);
                if (![items isKindOfClass:NSArray.class]) return items;
                RVAdEvent(state,@"response_array_read",1);
                if (items && state) { RVAdValue(state,field,@([items count]));RVAdEvent(state,@"response_ad_items_seen",[items count]); }
                if (!RVAdResponsePolicy(state,object)) return items;
                RVAdEvent(state,@"response_ad_items_suppressed",[items count]);
                return [NSMutableArray array];
            };
        });
        RVAdHook(@"YTIPlayerResponse",[field stringByAppendingString:@"_Count"],@"Q@:",NO,^id(IMP original,SEL sel) {
            return ^NSUInteger(id object) {
                NSUInteger count=((NSUInteger(*)(id,SEL))original)(object,sel);
                RVAdPlaybackState *state=objc_getAssociatedObject(object,RVAdStateKey);
                RVAdEvent(state,@"response_count_read",1);
                if (state) RVAdEvent(state,@"response_ad_items_seen",count);
                if (!RVAdResponsePolicy(state,object)) return count;
                RVAdEvent(state,@"response_ad_items_suppressed",count);return 0;
            };
        });
    }
    // Derived queries call the native implementations, which see the same
    // scoped array policy. Never claim an ad-free state for unrelated responses.
    RVAdNoOpHookReady=RVAdHook(@"YTIIosPlayerConfig",@"useNoOpAdsCoordinator",@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id object) {
            NSDictionary *scope=NSThread.currentThread.threadDictionary[RVAdConfigScope];
            if (RVEnabled(@"video_ads") && scope[@"config"]==object) {
                RVAdEvent(scope[@"state"],@"no_op_flag_read",1);return YES;
            }
            return ((BOOL(*)(id,SEL))original)(object,sel);
        };
    });
    RVAdHook(@"YTRealAdsPlayerServices",@"adsPlaybackCoordinatorWithOverlayManager:delegate:parentResponder:contentPlayerResponse:",@"@@:@@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id services,id overlay,id delegate,id parent,id wrapper) {
            if (!NSThread.isMainThread) {
                RVAdEvent(nil,@"factory_off_main_thread_passthrough",1);
                return ((id(*)(id,SEL,id,id,id,id))original)(services,sel,overlay,delegate,parent,wrapper);
            }
            RVAdDecision *decision=NSThread.currentThread.threadDictionary[RVAdSelectionScope];
            if (decision && decision.state.controller==delegate &&
                (decision.wrapper!=wrapper || objc_getAssociatedObject(delegate,RVAdStateKey)!=decision.state)) {
                RVAdPlaybackState *state=decision.state;
                RVAdValue(state,@"factory_response_argument_present",@(wrapper!=nil));RVAdValue(state,@"factory_argument_matches_scope",@NO);
                RVAdValue(state,@"factory_wrapper_class",wrapper ? NSStringFromClass([wrapper class]) : @"nil");
                RVAdValue(state,@"factory_lease_current",@(objc_getAssociatedObject(delegate,RVAdStateKey)==state));
                RVAdValue(state,@"handoff_gate",@"factory_response_argument_mismatch");state.actual=@"native";
                RVAdEvent(state,@"factory_entered",1);RVAdEvent(state,@"factory_argument_mismatch",1);
                id result=((id(*)(id,SEL,id,id,id,id))original)(services,sel,overlay,delegate,parent,wrapper);
                state.coordinator=result;RVAdRegisterCoordinator(result,state);return result;
            }
            RVAdPlaybackState *state=RVAdPrepare(delegate,parent,wrapper,@"factory",NO);
            if (!state) return ((id(*)(id,SEL,id,id,id,id))original)(services,sel,overlay,delegate,parent,wrapper);
            RVAdEvent(state,@"factory_entered",1);
            RVAdValue(state,@"factory_response_argument_present",@(wrapper!=nil));
            RVAdValue(state,@"factory_response_matches_controller",@(state.response && state.response==RVContentResponse(delegate)));
            RVAdValue(state,@"factory_argument_matches_scope",@(!decision || decision.wrapper==wrapper));
            RVAdValue(state,@"factory_lease_current",@(objc_getAssociatedObject(delegate,RVAdStateKey)==state));
            RVAdValue(state,@"factory_origin",decision && decision.state==state ? decision.origin : @"constructor_or_other");
            id raw=state.response;
            // Read inputs once before selecting a policy; the diagnostic reads
            // must not themselves return filtered arrays.
            @synchronized(RVAdPlaybackState.class) { state.actual=@"native"; }
            if (RVEnabled(@"diagnostics")) for (NSString *field in @[@"playerAdsArray",@"adPlacementsArray",@"adSlotsArray"])
                (void)RVObject(raw,field.UTF8String);
            BOOL eligible=NSThread.isMainThread && RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"];
            id config=eligible && [state.requested isEqual:@"coordinator"] ?
                RVAdResolvedObject(RVAdResolvedObject(raw,"playerConfig"),"iosPlayerConfig") : nil;
            BOOL noOp=eligible && [state.requested isEqual:@"coordinator"] && RVAdNoOpHookReady &&
                RVAdResolve(config,"useNoOpAdsCoordinator",@"B@:");
            @synchronized(RVAdPlaybackState.class) { state.actual=eligible ? noOp ? @"coordinator" : @"response" : @"native"; }
            RVAdValue(state,@"coordinator_fallback_to_response",@(eligible && [state.requested isEqual:@"coordinator"] && !noOp));
            NSMutableDictionary *thread=NSThread.currentThread.threadDictionary;id previous=thread[RVAdConfigScope];id result;
            @try {
                if (noOp) thread[RVAdConfigScope]=@{@"config":config,@"state":state};else [thread removeObjectForKey:RVAdConfigScope];
                result=((id(*)(id,SEL,id,id,id,id))original)(services,sel,overlay,delegate,parent,wrapper);
            } @finally { if (previous) thread[RVAdConfigScope]=previous;else [thread removeObjectForKey:RVAdConfigScope]; }
            Class native=NSClassFromString(@"YTNoOpAdsPlaybackCoordinator");
            state.coordinator=result;
            BOOL selected=noOp && native && [result isKindOfClass:native];
            RVAdValue(state,@"native_no_op_selected",@(selected));
            RVAdValue(state,@"returned_coordinator_class",result ? NSStringFromClass([result class]) : @"nil");
            if (selected) RVAdEvent(state,@"no_op_selected",1);
            else if (noOp) { RVAdEvent(state,@"no_op_selection_failed",1);@synchronized(RVAdPlaybackState.class) { state.actual=@"native_selection_failed"; } }
            RVAdRegisterCoordinator(result,state);
            RVAdValue(state,@"candidate_original_delegate",@(state.controller==delegate));
            RVAdCapture(state,@"factory_return");return result;
        };
    });
    RVAdHook(@"YTAdsControlFlowManagerImpl",@"handleActivationForTriggerBundles:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id manager,id bundles) {
            RVAdEvent(nil,@"trigger_activation_native_passthrough",1);
            ((void(*)(id,SEL,id))original)(manager,sel,bundles);
        };
    });
    RVAdHook(@"YTLocalPlaybackController",@"adsPlaybackCoordinator:didFinishBreakWithBreakType:",@"v@:@q",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id coordinator,NSInteger type) {
            RVAdPlaybackState *state=objc_getAssociatedObject(controller,RVAdStateKey);
            ((void(*)(id,SEL,id,NSInteger))original)(controller,sel,coordinator,type);
            if (state && objc_getAssociatedObject(controller,RVAdStateKey)!=state) { RVAdEvent(nil,@"retired_generation_break_callback",1);return; }
            if (!state || state.coordinator!=coordinator) { RVAdEvent(nil,@"unbound_break_callback",1);return; }
            if (type==1 || type==3) {
                if (NSThread.isMainThread && RVAdStoredCoordinator(controller)==coordinator)
                    RVAdVerifyAssignment(controller,@"native_break_callback",NO);
                NSString *event=type==1 ? @"preroll_completed" : @"postroll_completed";
                RVAdEvent(state,event,1);RVAdValue(state,event,@YES);RVAdCapture(state,event);
            }
        };
    });
    NSDictionary *gates=@{@"YTIPlayabilityStatus":@"isPlayableInBackground",@"MLVideo":@"playableInBackground",
        @"YTPlaybackData":@"isPlayableInBackground",@"YTBackgroundabilityPolicyImpl":@"isBackgroundableByUserSettings"};
    for (NSString *cls in gates) RVHook(cls,gates[cls],@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id object) { return RVEnabled(@"background_playback") ? YES : ((BOOL(*)(id,SEL))original)(object,sel); };
    });
    RVInstallAdCoordinatorHandoff();
}
