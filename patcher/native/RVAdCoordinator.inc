// SPDX-License-Identifier: MIT
// Native selection/reuse plus a guarded transition handoff. No fixed addresses.
static NSString *const RVAdSelectionScope=@"RVPort.AdSelectionScope";
static NSString *const RVAdTransitionScope=@"RVPort.AdTransitionScope";
static const void *RVAdOwnerKey=&RVAdOwnerKey;
@interface RVAdDecision : NSObject
@property(nonatomic,strong) RVAdPlaybackState *state;
@property(nonatomic,strong) id wrapper;
@property(nonatomic,strong) id cached;
@property(nonatomic,copy) NSString *origin;
@property(nonatomic) BOOL forceRecreation;
@end
@implementation RVAdDecision @end
@interface RVAdOwner : NSObject
@property(nonatomic,weak) id controller;
@property(nonatomic,weak) RVAdPlaybackState *state;
@property(nonatomic) BOOL selectedByAdapter;
@end
@implementation RVAdOwner @end
@interface RVAdTransition : NSObject
@property(nonatomic,weak) id controller;
@property(nonatomic,strong) id oldCoordinator;
@property(nonatomic) BOOL resetCompleted;
@property(nonatomic) BOOL autoplayObserved;
@property(nonatomic) BOOL autoplay;
@property(nonatomic) NSUInteger resetDepth;
@end
@implementation RVAdTransition @end

static id RVAdControllerWrapper(id controller) {
    return RVAdResolvedObject(RVAdResolvedObject(controller,"contentPlaybackData"),"playerResponse");
}
static BOOL RVAdLayoutContains(Class cls,const uint8_t *layout,ptrdiff_t target) {
    if (!layout) return NO;
    Class parent=class_getSuperclass(cls);
    size_t cursor=parent ? class_getInstanceSize(parent) : 0,size=class_getInstanceSize(cls);
    for (NSUInteger n=0;n<128 && layout[n];n++) {
        cursor+=(layout[n]>>4)*sizeof(id);
        size_t end=cursor+(layout[n]&15)*sizeof(id);
        if (end>size) return NO;
        if (target>=0 && (size_t)target>=cursor && (size_t)target<end) return YES;
        cursor=end;
    }
    return NO;
}
static Ivar RVAdCoordinatorIvar(id controller) {
    Class cls=NSClassFromString(@"YTLocalPlaybackController");
    if (!RVCompatible || !cls || ![controller isKindOfClass:cls]) return NULL;
    Ivar field=class_getInstanceVariable(cls,"_adsPlaybackCoordinator");
    const char *type=field ? ivar_getTypeEncoding(field) : NULL;
    if (!type || strcmp(type,"@\"<YTAdsPlaybackCoordinator>\"")!=0) return NULL;
    ptrdiff_t offset=ivar_getOffset(field);
    if (offset<0 || offset%(ptrdiff_t)sizeof(id) || (size_t)offset+sizeof(id)>class_getInstanceSize(cls) ||
        !RVAdLayoutContains(cls,class_getIvarLayout(cls),offset) ||
        RVAdLayoutContains(cls,class_getWeakIvarLayout(cls),offset)) return NULL;
    return field;
}
static id RVAdStoredCoordinator(id controller) {
    Ivar field=RVAdCoordinatorIvar(controller);
    if (!field || !RVCan(controller,"adsPlaybackCoordinator",@"@@:")) return nil;
    id stored=object_getIvar(controller,field);
    return stored==RVObject(controller,"adsPlaybackCoordinator") ? stored : nil;
}
static void RVAdRegisterCoordinator(id coordinator,RVAdPlaybackState *state) {
    if (!coordinator || !state) return;
    RVAdOwner *owner=[RVAdOwner new];owner.controller=state.controller;owner.state=state;
    owner.selectedByAdapter=[state.actual isEqual:@"coordinator"];
    objc_setAssociatedObject(coordinator,RVAdOwnerKey,owner,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
static BOOL RVAdOwnedNoOp(id controller,id coordinator) {
    RVAdOwner *owner=objc_getAssociatedObject(coordinator,RVAdOwnerKey);
    Class cls=NSClassFromString(@"YTNoOpAdsPlaybackCoordinator");
    return owner.controller==controller && owner.selectedByAdapter && cls && [coordinator isKindOfClass:cls];
}
static BOOL RVAdWantsNoOp(RVAdPlaybackState *state) {
    return RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"] && [state.requested isEqual:@"coordinator"];
}
static void RVAdRetainCurrentPolicy(RVAdPlaybackState *state,id coordinator) {
    state.coordinator=coordinator;
    state.actual=RVAdOwnedNoOp(state.controller,coordinator) ? @"coordinator" :
        RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"] ? @"response" : @"native";
}
static id RVAdNoOpConfig(RVAdPlaybackState *state) {
    if (!RVAdWantsNoOp(state) || !RVAdNoOpHookReady) return nil;
    id config=RVAdResolvedObject(RVAdResolvedObject(state.response,"playerConfig"),"iosPlayerConfig");
    return RVAdResolve(config,"useNoOpAdsCoordinator",@"B@:") ? config : nil;
}
static BOOL RVAdNoActiveAd(id controller,RVAdPlaybackState *state) {
    const char *fields[]={"isPlayingAd","isPlayingAdIntro","isPlayingAdSurvey"};
    for (NSUInteger n=0;n<sizeof(fields)/sizeof(fields[0]);n++) {
        const char *name=fields[n];
        if (!RVCan(controller,name,@"B@:")) { RVAdValue(state,@"handoff_gate",@"ad_state_unknown");return NO; }
        if (RVBool(controller,name)) { RVAdValue(state,@"handoff_gate",@"deferred_active_ad");return NO; }
    }
    return YES;
}
static void RVAdVerifyAssignment(id controller,NSString *origin,BOOL initialized) {
    RVAdPlaybackState *state=objc_getAssociatedObject(controller,RVAdStateKey);if (!state) return;
    Ivar field=RVAdCoordinatorIvar(controller);
    id stored=field ? object_getIvar(controller,field) : nil;
    BOOL getter=RVCan(controller,"adsPlaybackCoordinator",@"@@:") && stored==RVObject(controller,"adsPlaybackCoordinator");
    BOOL matches=stored && getter && stored==state.coordinator;
    RVAdValue(state,@"coordinator_ivar_layout_valid",@(field!=NULL));RVAdValue(state,@"coordinator_getter_agrees",@(getter));
    RVAdValue(state,@"pointer_installed",@(matches));RVAdValue(state,@"assignment_origin",origin);
    if (!matches) { RVAdValue(state,@"handoff_gate",field ? @"native_assignment_mismatch" : @"ivar_layout_unavailable");return; }
    if (initialized) RVAdValue(state,@"native_initialization_completed",@YES);
    RVAdValue(state,@"handoff_gate",RVAdWantsNoOp(state) && !RVAdOwnedNoOp(controller,stored) ? @"no_op_not_installed" : @"native_assignment_verified");
    RVAdEvent(state,@"assignment_verified",1);
    RVAdCapture(state,@"assignment");
}
static void RVAdRequestCoordinatorPolicy(id controller,NSString *reason) {
    if (!controller) return;
    if (!NSThread.isMainThread) {
        __weak id weak=controller;dispatch_async(dispatch_get_main_queue(),^{ if (weak) RVAdRequestCoordinatorPolicy(weak,reason); });return;
    }
    // Native reset/selection can reenter clocks and delegate notifications.
    if (NSThread.currentThread.threadDictionary[RVAdTransitionScope] || NSThread.currentThread.threadDictionary[RVAdSelectionScope]) return;
    RVAdPlaybackState *state=objc_getAssociatedObject(controller,RVAdStateKey);
    NSTimeInterval now=NSDate.timeIntervalSinceReferenceDate;
    if ([reason isEqual:@"clock"] && state && now-state.lastPolicyObservation<.5) return;
    id wrapper=RVAdControllerWrapper(controller);
    id raw=RVAdResolvedObject(wrapper,"playerData");
    if (!state || state.wrapper!=wrapper || state.response!=raw) state=RVAdPrepare(controller,controller,wrapper,@"late_content_response",NO);
    if (!state) return;
    state.lastPolicyObservation=now;
    NSString *requested=RVSetting(@"ad_strategy");
    BOOL changed=![state.requested isEqual:requested];state.requested=requested;
    state.gate=RVAdEligibility(controller,controller,wrapper,state.response,state);
    if (!state.coordinator) {
        id stored=RVAdStoredCoordinator(controller);state.coordinator=stored;
        state.actual=RVAdOwnedNoOp(controller,stored) ? @"coordinator" :
            RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"] && ![requested isEqual:@"coordinator"] ? @"response" : @"native";
    }
    BOOL pending=RVAdWantsNoOp(state) ? !RVAdOwnedNoOp(controller,state.coordinator) : RVAdOwnedNoOp(controller,state.coordinator);
    BOOL wasPending=[state.values[@"policy_request_pending"] boolValue];
    RVAdValue(state,@"policy_request_pending",@(pending));
    if (pending && (!wasPending || changed || [reason isEqual:@"settings_changed"])) {
        RVAdValue(state,@"policy_request_origin",reason);
        NSString *gate=state.values[@"handoff_gate"];
        if (changed || !gate || [gate isEqual:@"ready"] || [gate isEqual:@"native_assignment_verified"])
            RVAdValue(state,@"handoff_gate",@"deferred_until_native_boundary");
        RVAdEvent(state,@"policy_request_deferred",1);
    }
    if (!RVAdOwnedNoOp(controller,state.coordinator) && ![requested isEqual:@"coordinator"])
        state.actual=RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"] ? @"response" : @"native";
}
static void RVAdPolicyChanged(void) { RVAdRequestCoordinatorPolicy(RVCurrentPlayback,@"settings_changed"); }

static BOOL RVAdCandidateContract(id candidate) {
    return RVCan(candidate,"reset",@"v@:") && RVCan(candidate,"startPrerollAdBreak",@"v@:") &&
        RVCan(candidate,"startPostrollAdBreak",@"v@:") && RVCan(candidate,"setContentSingleVideo:playbackTimeline:",@"v@:@@") &&
        RVCan(candidate,"setAdsManageOverlayLifecycleEnabled:",@"v@:B");
}
static void RVAdCommitTransition(id controller) {
    RVAdPlaybackState *state=objc_getAssociatedObject(controller,RVAdStateKey);
    if (!state || !state.transitionPending) return;
    state.transitionPending=NO;
    if (!NSThread.isMainThread) { RVAdValue(state,@"handoff_gate",@"off_main_thread");return; }
    Ivar field=RVAdCoordinatorIvar(controller);id old=RVAdStoredCoordinator(controller);
    if (!field || !RVCan(controller,"adsPlaybackCoordinator",@"@@:") || object_getIvar(controller,field)!=old) {
        RVAdValue(state,@"handoff_gate",@"ivar_layout_or_getter_mismatch");return;
    }
    if (old!=state.retiringCoordinator) { RVAdValue(state,@"handoff_gate",@"transition_pointer_changed");return; }
    BOOL wants=RVAdWantsNoOp(state),ownedNoOp=RVAdOwnedNoOp(controller,old);
    if (wants==ownedNoOp) {
        state.coordinator=old;state.actual=wants ? @"coordinator" : RVEnabled(@"video_ads") && [state.gate isEqual:@"ready"] ? @"response" : @"native";
        RVAdRegisterCoordinator(old,state);RVAdValue(state,@"native_no_op_selected",@(wants));
        if (wants) RVAdEvent(state,@"owned_no_op_reused",1);
        RVAdValue(state,@"handoff_gate",@"ready");return;
    }
    if (state.wrapper!=RVAdControllerWrapper(controller) || !state.response || state.response!=RVContentResponse(controller)) {
        RVAdValue(state,@"handoff_gate",@"transition_response_changed");return;
    }
    if (!state.transitionResetCompleted) { RVAdValue(state,@"handoff_gate",@"transition_cleanup_unproven");return; }
    if (!RVAdNoActiveAd(controller,state)) return;
    if (!RVCan(controller,"shouldUseNewOverlaysAPI",@"B@:") || !RVCan(controller,"createAdsPlaybackCoordinator",@"@@:")) {
        RVAdValue(state,@"handoff_gate",@"candidate_factory_contract_missing");return;
    }
    if (wants && !RVAdNoOpConfig(state)) { RVAdValue(state,@"handoff_gate",@"no_op_config_unavailable");return; }
    NSUInteger generation=state.generation;id wrapper=state.wrapper,raw=state.response;
    RVAdDecision *decision=[RVAdDecision new];decision.state=state;decision.wrapper=wrapper;decision.cached=old;decision.origin=@"transition";
    NSMutableDictionary *thread=NSThread.currentThread.threadDictionary;id previous=thread[RVAdSelectionScope];id candidate=nil;BOOL published=NO;
    @try {
        thread[RVAdSelectionScope]=decision;
        candidate=RVObject(controller,"createAdsPlaybackCoordinator");
        Class noOp=NSClassFromString(@"YTNoOpAdsPlaybackCoordinator");
        BOOL expected=!wants || (noOp && [candidate isKindOfClass:noOp] && RVAdOwnedNoOp(controller,candidate));
        if (!candidate || candidate==old || !expected || !RVAdCandidateContract(candidate) ||
            (state.transitionAutoplayObserved && !RVCan(candidate,"willPlayNewVideoWithAutoplay:",@"v@:B"))) {
            RVAdRetainCurrentPolicy(state,old);RVAdValue(state,@"handoff_gate",@"candidate_contract_or_class_mismatch");return;
        }
        if (objc_getAssociatedObject(controller,RVAdStateKey)!=state || state.generation!=generation || state.wrapper!=wrapper ||
            raw!=RVContentResponse(controller) || object_getIvar(controller,field)!=old || !RVAdNoActiveAd(controller,state)) {
            RVAdRetainCurrentPolicy(state,old);RVAdValue(state,@"handoff_gate",@"candidate_generation_or_pointer_changed");return;
        }
        ((void(*)(id,SEL,BOOL))objc_msgSend)(candidate,sel_registerName("setAdsManageOverlayLifecycleEnabled:"),RVBool(controller,"shouldUseNewOverlaysAPI"));
        if (state.transitionAutoplayObserved)
            ((void(*)(id,SEL,BOOL))objc_msgSend)(candidate,sel_registerName("willPlayNewVideoWithAutoplay:"),state.transitionAutoplay);
        // Revalidate after lifecycle methods: they are native code and may reenter.
        if (objc_getAssociatedObject(controller,RVAdStateKey)!=state || raw!=RVContentResponse(controller) ||
            object_getIvar(controller,field)!=old || !RVAdNoActiveAd(controller,state)) {
            RVAdRetainCurrentPolicy(state,old);RVAdValue(state,@"handoff_gate",@"initialization_reentered");return;
        }
        object_setIvarWithStrongDefault(controller,field,candidate);published=YES;
        state.coordinator=candidate;RVAdValue(state,@"transition_commit_attempted",@YES);
        RVAdEvent(state,@"transition_committed",1);RVAdVerifyAssignment(controller,@"transition_named_ivar",NO);
    } @finally {
        @try {
            // A rejected candidate may already own native observer registrations.
            // Tear down only the unpublished object, never the installed slot.
            if (!published && candidate && candidate!=old && object_getIvar(controller,field)!=candidate &&
                RVCan(candidate,"reset",@"v@:")) {
                ((void(*)(id,SEL))objc_msgSend)(candidate,sel_registerName("reset"));
                RVAdEvent(state,@"discarded_candidate_reset_completed",1);
            }
        } @finally {
            if (previous) thread[RVAdSelectionScope]=previous;else [thread removeObjectForKey:RVAdSelectionScope];
        }
    }
}

static void RVInstallAdCoordinatorHandoff(void) {
    for (NSString *cls in @[@"YTGaplessPlaybackCoordinator",@"YTAdsControlFlowPlaybackCoordinator",@"YTNoOpAdsPlaybackCoordinator",@"YTDAIAdsControlFlowPlaybackCoordinator"]) {
        RVAdHook(cls,@"supportsPlaybackForContentPlayerResponse:",@"B@:@",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id object,id wrapper) {
                RVAdDecision *scope=NSThread.currentThread.threadDictionary[RVAdSelectionScope];
                BOOL exact=scope && scope.cached==object && scope.wrapper==wrapper && scope.state.controller &&
                    objc_getAssociatedObject(scope.state.controller,RVAdStateKey)==scope.state &&
                    wrapper==RVAdControllerWrapper(scope.state.controller);
                if (exact && scope.forceRecreation) {
                    RVAdEvent(scope.state,@"cached_compatibility_forced_recreate",1);return NO;
                }
                BOOL result=((BOOL(*)(id,SEL,id))original)(object,sel,wrapper);
                if (exact) { RVAdEvent(scope.state,@"cached_compatibility_native",1);RVAdValue(scope.state,@"cached_compatibility_result",@(result)); }
                return result;
            };
        });
        RVAdHook(cls,@"reset",@"v@:",NO,^id(IMP original,SEL sel) {
            return ^(id object) {
                RVAdTransition *scope=NSThread.currentThread.threadDictionary[RVAdTransitionScope];
                BOOL exact=scope && scope.oldCoordinator==object;
                if (exact) scope.resetDepth++;
                @try {
                    ((void(*)(id,SEL))original)(object,sel);
                    if (exact && scope.resetDepth==1) scope.resetCompleted=YES;
                } @finally { if (exact) scope.resetDepth--; }
            };
        });
        RVAdHook(cls,@"willPlayNewVideoWithAutoplay:",@"v@:B",NO,^id(IMP original,SEL sel) {
            return ^(id object,BOOL autoplay) {
                RVAdTransition *scope=NSThread.currentThread.threadDictionary[RVAdTransitionScope];
                if (scope && scope.oldCoordinator==object) { scope.autoplayObserved=YES;scope.autoplay=autoplay; }
                ((void(*)(id,SEL,BOOL))original)(object,sel,autoplay);
            };
        });
    }
    RVAdHook(@"YTLocalPlaybackController",@"adsPlaybackCoordinatorForNewPlayback",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id controller) {
            if (!NSThread.isMainThread) { RVAdEvent(nil,@"selection_off_main_thread_passthrough",1);return ((id(*)(id,SEL))original)(controller,sel); }
            id wrapper=RVAdControllerWrapper(controller);
            RVAdPlaybackState *state=RVAdPrepare(controller,controller,wrapper,@"native_selection",YES);
            RVAdDecision *scope=[RVAdDecision new];scope.state=state;scope.wrapper=wrapper;scope.cached=RVAdStoredCoordinator(controller);scope.origin=@"native_selection";
            scope.forceRecreation=RVAdWantsNoOp(state) && RVAdNoOpConfig(state)!=nil;
            RVAdEvent(state,@"selection_entered",1);RVAdValue(state,@"cached_coordinator_class",scope.cached ? NSStringFromClass([scope.cached class]) : @"nil");
            RVAdValue(state,@"cached_recreation_requested",@(scope.forceRecreation));
            NSMutableDictionary *thread=NSThread.currentThread.threadDictionary;id previous=thread[RVAdSelectionScope];id result;
            @try {
                thread[RVAdSelectionScope]=scope;result=((id(*)(id,SEL))original)(controller,sel);
            } @finally { if (previous) thread[RVAdSelectionScope]=previous;else [thread removeObjectForKey:RVAdSelectionScope]; }
            state.coordinator=result;
            if (RVAdOwnedNoOp(controller,result)) state.actual=@"coordinator";
            RVAdValue(state,@"selected_coordinator_class",result ? NSStringFromClass([result class]) : @"nil");
            RVAdValue(state,@"handoff_gate",RVAdWantsNoOp(state) && !RVAdOwnedNoOp(controller,result) ? @"no_op_selection_missing" : @"awaiting_native_assignment");
            RVAdEvent(state,@"selection_returned",1);return result;
        };
    });
    RVAdHook(@"YTLocalPlaybackController",@"startPlaybackWithVideoResolver:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id resolver) {
            ((void(*)(id,SEL,id))original)(controller,sel,resolver);
            if (NSThread.isMainThread) RVAdVerifyAssignment(controller,@"native_start_return",YES);
        };
    });
    RVAdHook(@"YTLocalPlaybackController",@"videoSequencer:willTransitionToNextContentSequenceWithPlayerTransition:playbackConfig:playbackData:",@"v@:@@@@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id sequence,id transition,id config,id data) {
            if (!NSThread.isMainThread) { ((void(*)(id,SEL,id,id,id,id))original)(controller,sel,sequence,transition,config,data);return; }
            RVAdTransition *scope=[RVAdTransition new];scope.controller=controller;scope.oldCoordinator=RVAdStoredCoordinator(controller);
            NSMutableDictionary *thread=NSThread.currentThread.threadDictionary;id previous=thread[RVAdTransitionScope];
            @try {
                thread[RVAdTransitionScope]=scope;((void(*)(id,SEL,id,id,id,id))original)(controller,sel,sequence,transition,config,data);
            } @finally { if (previous) thread[RVAdTransitionScope]=previous;else [thread removeObjectForKey:RVAdTransitionScope]; }
            RVAdPlaybackState *state=RVAdPrepare(controller,controller,RVAdControllerWrapper(controller),@"internal_transition",YES);
            state.retiringCoordinator=scope.oldCoordinator;state.transitionPending=YES;
            state.transitionResetCompleted=scope.resetCompleted || !scope.oldCoordinator;
            state.transitionAutoplayObserved=scope.autoplayObserved;state.transitionAutoplay=scope.autoplay;
            RVAdValue(state,@"transition_reset_observed",@(scope.resetCompleted));RVAdValue(state,@"transition_autoplay_observed",@(scope.autoplayObserved));
            RVAdEvent(state,@"internal_transition_prepared",1);RVAdValue(state,@"handoff_gate",@"awaiting_transition_boundary");
        };
    });
    RVAdHook(@"YTLocalPlaybackController",@"didTransitionToContentSequenceForVideoSequencer:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id sequence) {
            RVAdCommitTransition(controller);
            ((void(*)(id,SEL,id))original)(controller,sel,sequence);
            if (NSThread.isMainThread) {
                RVAdPlaybackState *state=objc_getAssociatedObject(controller,RVAdStateKey);
                if ([state.values[@"handoff_gate"] isEqual:@"ready"] || [state.values[@"handoff_gate"] isEqual:@"native_assignment_verified"])
                    RVAdVerifyAssignment(controller,@"native_transition_return",YES);
                RVAdCapture(state,@"transition_return");
            }
        };
    });
}
