// SPDX-License-Identifier: MIT
// Explicit native menu choices take precedence over queued startup restoration.
static _Thread_local BOOL RVSpeedBridgeCapture;
static NSDictionary *RVSpeedBridgeReport(void);
static const void *RVSpeedKey=&RVSpeedKey;
static const void *RVSpeedOwnerKey=&RVSpeedOwnerKey;
static NSHashTable *RVSpeedPlayers;
static atomic_uint RVSpeedManagersBound,RVSpeedRateMenuCalls,RVSpeedModelMenuCalls,RVSpeedOwnerMissing;
@interface RVSpeedOwner : NSObject
@property(nonatomic,weak) id player;
@end
@implementation RVSpeedOwner @end
@interface RVSpeedState : NSObject
@property(nonatomic,copy) NSString *video;
@property(nonatomic,strong) id model;
@property(nonatomic) float requested;
@property(nonatomic) float observed;
@property(nonatomic) NSUInteger epoch;
@property(nonatomic) NSUInteger attempts;
@property(nonatomic) NSUInteger nativeChoices;
@property(nonatomic) NSUInteger queuedEpoch;
@property(nonatomic) BOOL queueActive;
@property(nonatomic) BOOL explicitChoice;
@property(nonatomic) BOOL applying;
@property(nonatomic) BOOL restoreConsidered;
@property(nonatomic,copy) NSString *gate;
@property(nonatomic,copy) NSString *source;
@end
@implementation RVSpeedState @end
static RVSpeedState *RVSpeedForPlayer(id player) {
    if (!player) return nil;
    RVSpeedState *state=objc_getAssociatedObject(player,RVSpeedKey);
    if (!state) {
        state=[RVSpeedState new];objc_setAssociatedObject(player,RVSpeedKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        if (!RVSpeedPlayers) RVSpeedPlayers=[NSHashTable weakObjectsHashTable];[RVSpeedPlayers addObject:player];
    }return state;
}
static float RVSpeedFloat(id object,const char *name) {
    return RVCan(object,name,@"f@:") ? ((float(*)(id,SEL))objc_msgSend)(object,sel_registerName(name)) : NAN;
}
static float RVSpeedObserved(id player) { return RVSpeedFloat(RVObject(player,"activePlaybackRateModel"),"activeRate"); }
static NSString *RVSpeedVideo(id player) {
    id video=RVObject(player,"contentVideoID");
    return RVVideoIDValid(video) ? video : nil;
}
static BOOL RVSpeedCurrent(id player,NSString *video) {
    id core=RVPlaybackForPlayer(player);
    return video && [RVSpeedVideo(player) isEqual:video] &&
        [RVObject(core,"contentVideoID") isEqual:video] && core==RVCurrentPlayback && player==RVCurrentPlayer;
}
static void RVSpeedQueue(id player,RVSpeedState *state) {
    if (!state.video || !isfinite(state.requested) || state.requested<.25f || state.requested>4.f) return;
    if (state.queueActive && state.queuedEpoch==state.epoch) return;
    state.queueActive=YES;state.queuedEpoch=state.epoch;
    NSUInteger epoch=state.epoch;__weak id weakPlayer=player;NSString *video=state.video;
    // The native core ignores rate writes in intermediate states. Retry only
    // this captured, current-content intent, through native setters and limits.
    for (NSNumber *delay in @[@0,@100,@350,@1000]) dispatch_after(dispatch_time(DISPATCH_TIME_NOW,delay.longLongValue*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
        id current=weakPlayer;RVSpeedState *pending=objc_getAssociatedObject(current,RVSpeedKey);
        if (!current || pending!=state || state.epoch!=epoch || ![state.video isEqual:video]) return;
        if (!RVSpeedCurrent(current,video)) { state.gate=@"waiting_for_current_player";return; }
        if (!RVOrdinaryContent(RVPlaybackForPlayer(current))) { state.gate=@"waiting_for_ordinary_content";return; }
        float actual=RVSpeedObserved(current);state.observed=actual;
        if (isfinite(actual) && fabsf(actual-state.requested)<.005f) { state.gate=@"native_rate_confirmed";return; }
        if (state.attempts>=4) { state.gate=@"native_rate_not_confirmed_after_retry";return; }
        state.attempts++;state.applying=YES;
        if (state.model && RVCan(current,"setPlaybackRateModel:",@"v@:@")) RVSetObject(current,"setPlaybackRateModel:",state.model);
        else if (RVCan(current,"setPlaybackRate:",@"v@:f")) ((void(*)(id,SEL,float))objc_msgSend)(current,sel_registerName("setPlaybackRate:"),state.requested);
        state.applying=NO;state.observed=RVSpeedObserved(current);
        state.gate=isfinite(state.observed) && fabsf(state.observed-state.requested)<.005f ? @"native_rate_confirmed" : @"awaiting_native_rate_confirmation";
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,1010*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
        if (state.queuedEpoch==epoch) state.queueActive=NO;
    });
}
static void RVSpeedChoose(id player,float rate,id model,NSString *source) {
    if (!NSThread.isMainThread || !RVCompatible || ![player isKindOfClass:NSClassFromString(@"YTPlayerViewController")]) return;
    RVSpeedState *state=RVSpeedForPlayer(player);state.epoch++;state.restoreConsidered=YES;
    state.explicitChoice=YES;state.nativeChoices++;state.source=source;state.attempts=0;
    state.video=RVSpeedVideo(player);state.model=model;state.requested=rate;
    if (!isfinite(rate) || rate<.25f || rate>4.f || !state.video) { state.gate=@"native_choice_preserved_without_retry";return; }
    // Initial native defaults never become a remembered user selection.
    float remembered=model ? RVSpeedFloat(model,"rate") : rate;
    if (RVEnabled(@"remember_speed") && isfinite(remembered) && remembered>=.25f && remembered<=4.f)
        [NSUserDefaults.standardUserDefaults setFloat:remembered forKey:@"RVPort.lastSpeed"];
    state.gate=@"native_choice_captured";RVSpeedQueue(player,state);
}
static void RVSpeedContentReady(id player,NSString *trigger) {
    if (!NSThread.isMainThread) { __weak id weakPlayer=player;dispatch_async(dispatch_get_main_queue(),^{ RVSpeedContentReady(weakPlayer,trigger); });return; }
    NSString *video=RVSpeedVideo(player);if (!video || !RVSpeedCurrent(player,video)) return;
    RVSpeedState *state=RVSpeedForPlayer(player);
    if (![state.video isEqual:video]) {
        state.epoch++;state.video=video;state.model=nil;state.explicitChoice=NO;state.restoreConsidered=NO;state.attempts=0;
    }
    if (state.explicitChoice) { RVSpeedQueue(player,state);return; }
    if (state.restoreConsidered) return;state.restoreConsidered=YES;
    double value=[RVSetting(@"default_speed") doubleValue];
    if (RVEnabled(@"remember_speed")) {
        NSNumber *saved=[NSUserDefaults.standardUserDefaults objectForKey:@"RVPort.lastSpeed"];if (saved) value=saved.doubleValue;
    }
    state.source=trigger;
    if ((!RVEnabled(@"remember_speed") && value==1) || !isfinite(value) || value<.25 || value>4) { state.gate=@"native_default_retained";return; }
    state.requested=(float)value;state.gate=@"startup_restore_queued";state.epoch++;RVSpeedQueue(player,state);
}
static NSDictionary *RVSpeedReport(void) {
    RVSpeedState *state=objc_getAssociatedObject(RVCurrentPlayer,RVSpeedKey);
    float observed=RVSpeedObserved(RVCurrentPlayer);
    return @{@"schema":@2,@"command_bridge":RVSpeedBridgeReport(),@"current_player_bound":@(RVCurrentPlayer!=nil),
        @"content_matches":@(RVSpeedCurrent(RVCurrentPlayer,state.video)),@"source":state.source ?: @"not_observed",
        @"gate":state.gate ?: @"not_observed",@"explicit_native_choice":@(state.explicitChoice),
        @"native_choice_count":@(state.nativeChoices),@"apply_attempts":@(state.attempts),
        @"requested_rate":@(isfinite(state.requested) ? state.requested : 0),
        @"native_rate_available":@(isfinite(observed)),@"native_observed_rate":@(isfinite(observed) ? observed : 0),
        @"native_managers_bound":@(atomic_load(&RVSpeedManagersBound)),
        @"native_rate_menu_callbacks":@(atomic_load(&RVSpeedRateMenuCalls)),@"native_model_menu_callbacks":@(atomic_load(&RVSpeedModelMenuCalls)),
        @"menu_owner_missing":@(atomic_load(&RVSpeedOwnerMissing)),@"native_limits_preserved":@YES,@"device_verified":@NO};
}
static void RVInstallSpeedChoices(void) {
    RVHook(@"YTPlayerViewController",@"setPlaybackRateModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id player,id model) {
            if (RVSpeedBridgeCapture && NSThread.isMainThread) {
                RVSpeedBridgeCapture=NO;
                RVSpeedChoose(player,RVSpeedFloat(model,"effectivePlaybackRate"),model,@"native_granular_picker");
            }
            ((void(*)(id,SEL,id))original)(player,sel,model);
        };
    });
    RVHook(@"YTPlayerOverlayManager",@"initWithOverlayFactory:playerViewController:delegate:",@"@@:@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id manager,id factory,id player,id delegate) {
            id result=((id(*)(id,SEL,id,id,id))original)(manager,sel,factory,player,delegate);
            RVSpeedOwner *owner=[RVSpeedOwner new];owner.player=player;
            if (result && [player isKindOfClass:NSClassFromString(@"YTPlayerViewController")]) atomic_fetch_add(&RVSpeedManagersBound,1);
            objc_setAssociatedObject(result,RVSpeedOwnerKey,owner,OBJC_ASSOCIATION_RETAIN_NONATOMIC);return result;
        };
    });
    RVHook(@"YTPlayerOverlayManager",@"varispeedSwitchController:didSelectRate:",@"v@:@f",NO,^id(IMP original,SEL sel) {
        return ^(id manager,id menu,float rate) {
            RVSpeedOwner *owner=objc_getAssociatedObject(manager,RVSpeedOwnerKey);
            atomic_fetch_add(&RVSpeedRateMenuCalls,1);if (!owner.player) atomic_fetch_add(&RVSpeedOwnerMissing,1);
            RVSpeedChoose(owner.player,rate,nil,@"native_rate_menu");
            ((void(*)(id,SEL,id,float))original)(manager,sel,menu,rate);
        };
    });
    RVHook(@"YTPlayerOverlayManager",@"varispeedSwitchController:didSelectRateModel:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id manager,id menu,id model) {
            RVSpeedOwner *owner=objc_getAssociatedObject(manager,RVSpeedOwnerKey);
            atomic_fetch_add(&RVSpeedModelMenuCalls,1);if (!owner.player) atomic_fetch_add(&RVSpeedOwnerMissing,1);
            RVSpeedChoose(owner.player,RVSpeedFloat(model,"effectivePlaybackRate"),model,@"native_model_menu");
            ((void(*)(id,SEL,id,id))original)(manager,sel,menu,model);
        };
    });
    RVHook(@"YTPlayerViewController",@"playbackController:didActivateVideo:withPlaybackData:",@"v@:@@@",NO,^id(IMP original,SEL sel) {
        return ^(id player,id core,id video,id data) {
            ((void(*)(id,SEL,id,id,id))original)(player,sel,core,video,data);
            RVSpeedContentReady(player,@"content_activation");
        };
    });
}
