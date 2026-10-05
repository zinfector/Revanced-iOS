// SPDX-License-Identifier: MIT
// Explicit native menu choices take precedence over queued startup restoration.
static _Thread_local BOOL RVSpeedBridgeCapture;
static _Thread_local BOOL RVSpeedOverlayCapture;
static NSDictionary *RVSpeedBridgeReport(void);
static id RVSpeedVisiblePlayer(void);
static NSString *RVSpeedSessionID;
static NSUInteger RVSpeedLaunchNumber;
static const void *RVSpeedKey=&RVSpeedKey;
static const void *RVSpeedOwnerKey=&RVSpeedOwnerKey;
static NSHashTable *RVSpeedPlayers;
static atomic_uint RVSpeedManagersBound,RVSpeedRateMenuCalls,RVSpeedModelMenuCalls,RVSpeedOwnerMissing;
@interface RVSpeedOwner : NSObject
@property(nonatomic,weak) id player;
@property(nonatomic,weak) id cachedVideo;
@property(nonatomic) BOOL refreshing;
@end
@implementation RVSpeedOwner @end
@interface RVSpeedState : NSObject
@property(nonatomic,copy) NSString *video;
@property(nonatomic,strong) id model;
@property(nonatomic) float requested;
@property(nonatomic) float observed;
@property(nonatomic) NSUInteger epoch;
@property(nonatomic) NSUInteger selectionID;
@property(nonatomic) NSTimeInterval selectionStarted;
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
#include "RVSpeedDiagnostics.inc"
#include "RVSpeedContext.inc"
static id RVSpeedOwnerPlayer(id manager) {
    RVSpeedOwner *owner=objc_getAssociatedObject(manager,RVSpeedOwnerKey);id player=owner.player;
    return NSThread.isMainThread && RVCompatible && RVSpeedCurrent(player,RVSpeedVideo(player)) &&
        RVOrdinaryContent(RVPlaybackForPlayer(player)) ? player : nil;
}
static void RVSpeedRefreshOverlay(id manager,NSString *trigger) {
    id player=RVSpeedOwnerPlayer(manager),video=RVObject(player,"activeVideo");
    RVSpeedOwner *owner=objc_getAssociatedObject(manager,RVSpeedOwnerKey);
    if (!player || !video || owner.refreshing || !RVCan(manager,"updateActiveOverlayWithActiveVideoCurrentState:",@"v@:@")) return;
    // Populate the native overlay's cache using its native update method.
    // The cache can predate content activation on a fresh install.
    owner.refreshing=YES;
    @try { RVSetObject(manager,"updateActiveOverlayWithActiveVideoCurrentState:",video); }
    @finally { owner.refreshing=NO; }
    RVSpeedCheckpoint(@"overlay_state_refreshed",player,@{@"trigger":trigger,@"cached_video_matches":@(owner.cachedVideo==video)});
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
        if (!current || pending!=state || state.epoch!=epoch || ![state.video isEqual:video]) {
            RVSpeedCheckpoint(@"intent_cancelled",current,@{@"cancelled_epoch":@(epoch),@"scheduled_delay_ms":delay});return;
        }
        if (!RVSpeedCurrent(current,video)) { state.gate=@"waiting_for_current_player";RVSpeedSelectionReadback(current,state,delay.unsignedIntegerValue,state.gate);return; }
        if (!RVOrdinaryContent(RVPlaybackForPlayer(current))) { state.gate=@"waiting_for_ordinary_content";RVSpeedSelectionReadback(current,state,delay.unsignedIntegerValue,state.gate);return; }
        float actual=RVSpeedObserved(current);state.observed=actual;
        if (isfinite(actual) && fabsf(actual-state.requested)<.005f) { state.gate=@"native_rate_confirmed";RVSpeedSelectionReadback(current,state,delay.unsignedIntegerValue,state.gate);return; }
        if (state.attempts>=4) { state.gate=@"native_rate_not_confirmed_after_retry";RVSpeedSelectionReadback(current,state,delay.unsignedIntegerValue,state.gate);return; }
        state.attempts++;state.applying=YES;
        RVSpeedCheckpoint(@"retry_before_native_setter",current,@{@"scheduled_delay_ms":delay,@"attempt":@(state.attempts)});
        if (state.model && RVCan(current,"setPlaybackRateModel:",@"v@:@")) RVSetObject(current,"setPlaybackRateModel:",state.model);
        else if (RVCan(current,"setPlaybackRate:",@"v@:f")) ((void(*)(id,SEL,float))objc_msgSend)(current,sel_registerName("setPlaybackRate:"),state.requested);
        state.applying=NO;state.observed=RVSpeedObserved(current);
        state.gate=isfinite(state.observed) && fabsf(state.observed-state.requested)<.005f ? @"native_rate_confirmed" : @"awaiting_native_rate_confirmation";
        if (delay.unsignedIntegerValue==1000 && ![state.gate isEqual:@"native_rate_confirmed"]) state.gate=@"native_rate_not_confirmed_after_retry";
        RVSpeedSelectionReadback(current,state,delay.unsignedIntegerValue,state.gate);
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
    state.selectionStarted=NSProcessInfo.processInfo.systemUptime;
    RVSpeedBeginSelection(player);
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
        RVSpeedCheckpoint(@"content_generation_changed",player,nil);
        state.epoch++;state.video=video;state.model=nil;state.explicitChoice=NO;state.restoreConsidered=NO;state.attempts=0;state.selectionID=0;
    }
    if (state.explicitChoice) { RVSpeedQueue(player,state);return; }
    if (state.restoreConsidered) return;state.restoreConsidered=YES;
    double value=[RVSetting(@"default_speed") doubleValue];
    if (RVEnabled(@"remember_speed")) {
        NSNumber *saved=[NSUserDefaults.standardUserDefaults objectForKey:@"RVPort.lastSpeed"];if (saved) value=saved.doubleValue;
    }
    state.source=trigger;
    RVSpeedCheckpoint(@"content_ready",player,@{@"trigger":trigger,@"restore_value":@(isfinite(value) ? value : 0)});
    if ((!RVEnabled(@"remember_speed") && value==1) || !isfinite(value) || value<.25 || value>4) { state.gate=@"native_default_retained";return; }
    state.requested=(float)value;state.selectionStarted=NSProcessInfo.processInfo.systemUptime;state.gate=@"startup_restore_queued";state.epoch++;RVSpeedQueue(player,state);
}
static NSDictionary *RVSpeedReport(void) {
    RVSpeedState *state=objc_getAssociatedObject(RVCurrentPlayer,RVSpeedKey);
    float observed=RVSpeedObserved(RVCurrentPlayer);
    return @{@"schema":@3,@"checkpoints":RVSpeedCheckpointReport(),@"current_snapshot":RVSpeedSnapshot(RVCurrentPlayer),@"sheet_context":RVSpeedContextReport(),@"command_bridge":RVSpeedBridgeReport(),@"current_player_bound":@(RVCurrentPlayer!=nil),
        @"content_matches":@(RVSpeedCurrent(RVCurrentPlayer,state.video)),@"source":state.source ?: @"not_observed",
        @"gate":state.gate ?: @"not_observed",@"gate_describes":state.explicitChoice ? @"explicit_user_selection" : @"startup_restoration",@"explicit_native_choice":@(state.explicitChoice),
        @"native_choice_count":@(state.nativeChoices),@"apply_attempts":@(state.attempts),
        @"requested_rate":@(isfinite(state.requested) ? state.requested : 0),
        @"native_rate_available":@(isfinite(observed)),@"native_observed_rate":@(isfinite(observed) ? observed : 0),
        @"native_managers_bound":@(atomic_load(&RVSpeedManagersBound)),
        @"native_rate_menu_callbacks":@(atomic_load(&RVSpeedRateMenuCalls)),@"native_model_menu_callbacks":@(atomic_load(&RVSpeedModelMenuCalls)),
        @"menu_owner_missing":@(atomic_load(&RVSpeedOwnerMissing)),@"native_limits_preserved":@YES,@"device_verified":@NO};
}
static void RVInstallSpeedChoices(void) {
    RVInstallSpeedContext();
    RVSpeedSessionID=NSUUID.UUID.UUIDString;
    RVSpeedLaunchNumber=[NSUserDefaults.standardUserDefaults integerForKey:@"RVPort.speedLaunchNumber"]+1;
    [NSUserDefaults.standardUserDefaults setInteger:RVSpeedLaunchNumber forKey:@"RVPort.speedLaunchNumber"];
    RVSpeedCheckpoint(@"hooks_install_started",nil,nil);
    RVHook(@"YTPlayerOverlayManager",@"updateActiveOverlayWithActiveVideoCurrentState:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id manager,id video) {
            RVSpeedOwner *owner=objc_getAssociatedObject(manager,RVSpeedOwnerKey);owner.cachedVideo=video;
            ((void(*)(id,SEL,id))original)(manager,sel,video);
        };
    });
    RVHook(@"YTPlayerOverlayManager",@"varispeedController",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id manager) {
            RVSpeedOwner *owner=objc_getAssociatedObject(manager,RVSpeedOwnerKey);id player=RVSpeedOwnerPlayer(manager);
            if (player && !owner.refreshing && owner.cachedVideo!=RVObject(player,"activeVideo")) RVSpeedRefreshOverlay(manager,@"varispeed_cache_missing_or_stale");
            return ((id(*)(id,SEL))original)(manager,sel);
        };
    });
    RVHook(@"YTPlayerOverlayManager",@"currentPlaybackRateForVarispeedSwitchController:",@"f@:@",NO,^id(IMP original,SEL sel) {
        return ^float(id manager,id menu) {
            float rate=((float(*)(id,SEL,id))original)(manager,sel,menu);id player=RVSpeedOwnerPlayer(manager);
            BOOL recovered=player && (!isfinite(rate) || rate<=0) && isfinite(RVSpeedObserved(player)) && RVSpeedObserved(player)>0;
            if (recovered) rate=RVSpeedObserved(player);
            if (RVSpeedReaderSamples++<16 || recovered) RVSpeedCheckpoint(@"legacy_rate_read",player,@{@"rate_available":@(isfinite(rate) && rate>0),@"recovered":@(recovered),@"returned_rate":@(isfinite(rate) ? rate : 0)});
            return rate;
        };
    });
    RVHook(@"YTPlayerOverlayManager",@"currentPlaybackRateModelForVarispeedSwitchController:",@"@@:@",NO,^id(IMP original,SEL sel) {
        return ^id(id manager,id menu) {
            id model=((id(*)(id,SEL,id))original)(manager,sel,menu),player=RVSpeedOwnerPlayer(manager);BOOL recovered=NO;
            if (!model && player) { model=RVObject(RVObject(player,"activePlaybackRateModel"),"rateModel");recovered=model!=nil; }
            if (RVSpeedReaderSamples++<16 || recovered) RVSpeedCheckpoint(@"legacy_model_read",player,@{@"model_present":@(model!=nil),@"recovered":@(recovered)});
            return model;
        };
    });
    RVHook(@"YTMainAppVideoPlayerOverlayViewController",@"didPressVarispeed:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id overlay,id sender) {
            id manager=RVObject(overlay,"delegate"),player=RVSpeedOwnerPlayer(manager);
            RVSpeedPickerOpened(player,@"native_overlay_sheet");RVSpeedRefreshOverlay(manager,@"picker_open");
            ((void(*)(id,SEL,id))original)(overlay,sel,sender);
            RVSpeedCheckpoint(@"picker_open_returned",player,nil);
        };
    });
    RVHook(@"YTMainAppVideoPlayerOverlayViewController",@"currentPlaybackRate",@"d@:",NO,^id(IMP original,SEL sel) {
        return ^double(id overlay) {
            double native=((double(*)(id,SEL))original)(overlay,sel);id player=RVSpeedOwnerPlayer(RVObject(overlay,"delegate"));
            BOOL recovered=player && (!isfinite(native) || native<=0) && isfinite(RVSpeedObserved(player)) && RVSpeedObserved(player)>0;
            double effective=recovered ? RVSpeedObserved(player) : native;
            if (RVSpeedReaderSamples++<16 || recovered) RVSpeedCheckpoint(@"overlay_rate_read",player,@{@"native_rate":@(isfinite(native) ? native : 0),@"returned_rate":@(isfinite(effective) ? effective : 0),@"recovered":@(recovered)});
            return effective;
        };
    });
    RVHook(@"YTMainAppVideoPlayerOverlayViewController",@"currentPlaybackRateModel",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id overlay) {
            id model=((id(*)(id,SEL))original)(overlay,sel),player=RVSpeedOwnerPlayer(RVObject(overlay,"delegate"));BOOL recovered=NO;
            if (!model && player) { model=RVObject(RVObject(player,"activePlaybackRateModel"),"rateModel");recovered=model!=nil; }
            if (RVSpeedReaderSamples++<16 || recovered) RVSpeedCheckpoint(@"overlay_model_read",player,@{@"model_present":@(model!=nil),@"recovered":@(recovered)});
            return model;
        };
    });
    RVHook(@"YTMainAppVideoPlayerOverlayViewController",@"setPlaybackRate:",@"v@:d",NO,^id(IMP original,SEL sel) {
        return ^(id overlay,double rate) {
            id player=RVSpeedOwnerPlayer(RVObject(overlay,"delegate"));
            RVSpeedChoose(player,(float)rate,nil,@"native_overlay_rate_picker");RVSpeedCheckpoint(@"overlay_rate_selection",player,@{@"argument_rate":@(isfinite(rate) ? rate : 0)});
            BOOL previous=RVSpeedOverlayCapture;RVSpeedOverlayCapture=YES;
            @try { ((void(*)(id,SEL,double))original)(overlay,sel,rate); }
            @finally { RVSpeedOverlayCapture=previous; }
            RVSpeedCheckpoint(@"overlay_selection_returned",player,nil);
        };
    });
    RVHook(@"YTMainAppVideoPlayerOverlayViewController",@"setPlaybackRateModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id overlay,id model) {
            id player=RVSpeedOwnerPlayer(RVObject(overlay,"delegate"));
            RVSpeedChoose(player,RVSpeedFloat(model,"effectivePlaybackRate"),model,@"native_overlay_model_picker");
            RVSpeedCheckpoint(@"overlay_model_selection",player,RVSpeedModelFacts(model));
            BOOL previous=RVSpeedOverlayCapture;RVSpeedOverlayCapture=YES;
            @try { ((void(*)(id,SEL,id))original)(overlay,sel,model); }
            @finally { RVSpeedOverlayCapture=previous; }
            RVSpeedCheckpoint(@"overlay_selection_returned",player,nil);
        };
    });
    RVHook(@"YTLocalPlaybackController",@"setPlaybackRateModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id core,id model) {
            id player=core==RVCurrentPlayback ? RVCurrentPlayer : nil;
            NSMutableDictionary *details=[RVSpeedModelFacts(model) mutableCopy];
            details[@"actual_core_state_known"]=@(RVCan(core,"state",@"i@:"));
            details[@"actual_core_state"]=@(RVCan(core,"state",@"i@:") ? ((int(*)(id,SEL))objc_msgSend)(core,sel_registerName("state")) : -1);
            RVSpeedCheckpoint(@"core_model_setter_before",player,details);
            ((void(*)(id,SEL,id))original)(core,sel,model);
            details[@"actual_core_state"]=@(RVCan(core,"state",@"i@:") ? ((int(*)(id,SEL))objc_msgSend)(core,sel_registerName("state")) : -1);
            RVSpeedCheckpoint(@"core_model_setter_after",player,details);
        };
    });
    RVHook(@"YTPlayerViewController",@"singleVideo:playbackRateDidChange:",@"v@:@f",NO,^id(IMP original,SEL sel) {
        return ^(id player,id video,float rate) {
            ((void(*)(id,SEL,id,float))original)(player,sel,video,rate);
            RVSpeedCheckpoint(@"native_rate_changed",player,@{@"callback_rate":@(isfinite(rate) ? rate : 0),@"active_video_matches":@(video==RVObject(player,"activeVideo"))});
        };
    });
    RVHook(@"YTPlayerViewController",@"setPlaybackRateModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id player,id model) {
            if (RVSpeedBridgeCapture && NSThread.isMainThread) {
                RVSpeedBridgeCapture=NO;
                RVSpeedChoose(player,RVSpeedFloat(model,"effectivePlaybackRate"),model,@"native_granular_picker");
            }
            RVSpeedCheckpoint(@"player_model_setter_before",player,RVSpeedModelFacts(model));
            ((void(*)(id,SEL,id))original)(player,sel,model);
            RVSpeedCheckpoint(@"player_model_setter_after",player,nil);
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
            if (!RVSpeedOverlayCapture) RVSpeedChoose(owner.player,rate,nil,@"native_rate_menu");
            RVSpeedCheckpoint(@"legacy_rate_delegate",owner.player,@{@"argument_rate":@(isfinite(rate) ? rate : 0)});
            ((void(*)(id,SEL,id,float))original)(manager,sel,menu,rate);
        };
    });
    RVHook(@"YTPlayerOverlayManager",@"varispeedSwitchController:didSelectRateModel:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id manager,id menu,id model) {
            RVSpeedOwner *owner=objc_getAssociatedObject(manager,RVSpeedOwnerKey);
            atomic_fetch_add(&RVSpeedModelMenuCalls,1);if (!owner.player) atomic_fetch_add(&RVSpeedOwnerMissing,1);
            if (!RVSpeedOverlayCapture) RVSpeedChoose(owner.player,RVSpeedFloat(model,"effectivePlaybackRate"),model,@"native_model_menu");
            RVSpeedCheckpoint(@"legacy_model_delegate",owner.player,RVSpeedModelFacts(model));
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
