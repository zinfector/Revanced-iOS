// SPDX-License-Identifier: MIT
// Capture-only passive watch trace. Originals, delegates, arguments and return
// values are never changed; no opaque C++ command or live content is exported.
static NSMutableArray *RVDAWatchHistory,*RVDAWatchHooks;
static NSMutableDictionary *RVDAWatchCounts;
static __weak id RVDAWatchOwner,RVDAWatchPlayback;
static NSString *RVDAWatchVideoHash;
static uint64_t RVDAWatchGeneration;
static BOOL RVDAWatchActive(void) {
    if (!RVDAUIDiagnosticsArmed || !NSThread.isMainThread || !RVAdWatchVisible()) return NO;
    NSString *video=RVObject(RVCurrentPlayback,"contentVideoID");
    return RVVideoIDValid(video) && RVOrdinaryContent(RVCurrentPlayback);
}
static void RVDAWatchRecord(NSString *checkpoint,NSDictionary *details) {
    if (!RVDAUIDiagnosticsArmed || !NSThread.isMainThread) return;
    NSString *hash=RVDADiagVideo(RVObject(RVCurrentPlayback,"contentVideoID"));
    if (!RVDAWatchHistory) RVDAWatchHistory=[NSMutableArray array];
    if (!RVDAWatchCounts) RVDAWatchCounts=[NSMutableDictionary dictionary];
    if (RVDAWatchOwner!=RVCurrentWatch || RVDAWatchPlayback!=RVCurrentPlayback || ![hash isEqual:RVDAWatchVideoHash]) {
        RVDAWatchOwner=RVCurrentWatch;RVDAWatchPlayback=RVCurrentPlayback;RVDAWatchVideoHash=hash;RVDAWatchGeneration++;
        if (RVDAWatchHistory.count>=64) [RVDAWatchHistory removeObjectAtIndex:0];
        [RVDAWatchHistory addObject:@{@"checkpoint":@"W00_watch_bound",@"generation":@(RVDAWatchGeneration),@"video_hash":hash,
            @"watch_class":RVCurrentWatch ? NSStringFromClass([RVCurrentWatch class]) : @"absent",@"elapsed_ms":@(MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-RVDAUIStarted)))}];
    }
    RVDAWatchCounts[checkpoint]=@([RVDAWatchCounts[checkpoint] unsignedLongLongValue]+1);
    NSMutableDictionary *event=[@{@"checkpoint":checkpoint,@"generation":@(RVDAWatchGeneration),@"video_hash":hash,
        @"elapsed_ms":@(MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-RVDAUIStarted)))} mutableCopy];
    if (details) [event addEntriesFromDictionary:details];
    if (RVDAWatchHistory.count>=64) [RVDAWatchHistory removeObjectAtIndex:0];
    [RVDAWatchHistory addObject:event.copy];
}
static NSArray *RVDAWatchAncestors(UIView *view) {
    NSMutableArray *result=[NSMutableArray array];
    for (NSUInteger i=0;view && i<12;i++,view=view.superview) {
        NSMutableArray *recognizers=[NSMutableArray array];
        for (UIGestureRecognizer *gesture in view.gestureRecognizers) if (recognizers.count<8)
            [recognizers addObject:@{@"class":NSStringFromClass(gesture.class),@"enabled":@(gesture.enabled),@"state":@(gesture.state),@"cancels_touches":@(gesture.cancelsTouchesInView)}];
        [result addObject:@{@"class":NSStringFromClass(view.class),@"interaction_enabled":@(view.userInteractionEnabled),@"hidden":@(view.hidden),@"alpha":@(view.alpha),@"recognizers":recognizers}];
    }return result;
}
static void RVDAWatchReset(void) {
    RVDAWatchHistory=[NSMutableArray array];RVDAWatchCounts=[NSMutableDictionary dictionary];
    RVDAWatchOwner=nil;RVDAWatchPlayback=nil;RVDAWatchVideoHash=nil;RVDAWatchGeneration=0;
}
static NSDictionary *RVDAWatchReport(void) {
    if (!NSThread.isMainThread) return @{@"blocker":@"watch_snapshot_off_main"};
    NSMutableDictionary *settings=[NSMutableDictionary dictionary];
    for (NSString *key in @[@"return_dislikes",@"feed_ads",@"video_ads",@"sponsorblock",@"disable_chapter_skip",@"disable_double_tap",@"hide_comments",@"hide_layout_components",@"dearrow_titles",@"dearrow_thumbnails"])
        settings[key]=@(RVEnabled(key));
    RVSponsorPromptState *prompt=RVSession(RVCurrentPlayback).promptState;UIView *host=(id)prompt.host;
    NSMutableDictionary *last=[NSMutableDictionary dictionary];
    for (NSDictionary *event in RVDAWatchHistory) last[event[@"checkpoint"]]=event;
    NSDictionary *display=RVDisplayReport();
    NSDictionary *vote=RVVoteModelReport();
    return @{@"schema":@1,@"armed":@(RVDAUIDiagnosticsArmed),@"active_watch":@(RVDAWatchActive()),
        @"generation":@(RVDAWatchGeneration),@"counts":RVDAWatchCounts.copy ?: @{},@"history":RVDAWatchHistory.copy ?: @[],@"last_stages":last,
        @"hooks":RVDAWatchHooks.copy ?: @[],@"settings":settings,@"watch_config_policy":RVVoteFirstLaunchReport(),
        @"vote_model":vote[@"state"] ?: @{},@"vote_counts":vote[@"counts"] ?: @{},
        @"display_ad_counts":display[@"session_events"] ?: @{},@"last_ad_mutation":display[@"structural_element_history"] ?: @[],
        @"sponsor_host":@{@"attached":@(host.superview!=nil),@"hidden":@(!host || host.hidden),@"frame":host ? NSStringFromCGRect(host.frame) : @"absent",@"gate":prompt.gate ?: @"absent",@"hit_forwarded":prompt.values[@"owned_hit_test_forwarded"] ?: @NO},
        @"observer_strategy":@"owned_keyed_native_lifecycle",@"opaque_command_payloads_exported":@NO,
        @"interpretation":@"W01 touch, W02 gesture decision, W03 native tap, W04 command bridge, W05 presentation or seek observed. Text links can intentionally bypass ordinary gestures. Timing is correlation; void returns do not establish success."};
}
static void RVDAWatchHook(NSString *cls,NSString *selector,NSString *abi,RVReplacementFactory factory) {
    BOOL installed=RVDAUIHook(cls,selector,abi,NO,factory);
    if (!RVDAWatchHooks) RVDAWatchHooks=[NSMutableArray array];
    [RVDAWatchHooks addObject:@{@"class":cls,@"selector":selector,@"installed":@(installed),@"abi":abi}];
}
static void RVDAWatchInstall(void) {
    RVDAWatchHook(@"UIWindow",@"sendEvent:",@"v@:@",^id(IMP original,SEL sel) {
        return ^(id window,UIEvent *event) {
            BOOL capture=RVDAWatchActive();
            if (capture && event.type==UIEventTypeTouches) for (UITouch *touch in event.allTouches) {
                if (touch.phase!=UITouchPhaseBegan && touch.phase!=UITouchPhaseEnded && touch.phase!=UITouchPhaseCancelled) continue;
                UIViewController *watch=(id)RVCurrentWatch;
                if (touch.window!=watch.viewIfLoaded.window) continue;
                RVDAWatchRecord(@"W01_touch_received",@{@"phase":@(touch.phase),@"ancestors":RVDAWatchAncestors(touch.view)});
            }
            ((void(*)(id,SEL,id))original)(window,sel,event);
        };
    });
    RVDAWatchHook(@"ELMTouchCommandPropertiesHandler",@"gestureRecognizer:shouldReceiveTouch:",@"B@:@@",^id(IMP original,SEL sel) {
        return ^BOOL(id handler,UIGestureRecognizer *recognizer,id touch) {
            BOOL capture=RVDAWatchActive(),result=((BOOL(*)(id,SEL,id,id))original)(handler,sel,recognizer,touch);
            if (capture) RVDAWatchRecord(@"W02_gesture_decision",@{@"stage":@"should_receive",@"recognizer":NSStringFromClass(recognizer.class),@"accepted":@(result),@"text_link_handoff_possible":@(!result)});return result;
        };
    });
    RVDAWatchHook(@"ELMTouchCommandPropertiesHandler",@"gestureRecognizerShouldBegin:",@"B@:@",^id(IMP original,SEL sel) {
        return ^BOOL(id handler,UIGestureRecognizer *recognizer) {
            BOOL capture=RVDAWatchActive(),result=((BOOL(*)(id,SEL,id))original)(handler,sel,recognizer);
            if (capture) RVDAWatchRecord(@"W02_gesture_decision",@{@"stage":@"should_begin",@"recognizer":NSStringFromClass(recognizer.class),@"accepted":@(result)});return result;
        };
    });
    RVDAWatchHook(@"ELMTouchCommandPropertiesHandler",@"handleTap",@"v@:",^id(IMP original,SEL sel) {
        return ^(id handler) {
            BOOL capture=RVDAWatchActive();if (capture) RVDAWatchRecord(@"W03_native_tap",@{@"path":@"node",@"phase":@"entered"});
            ((void(*)(id,SEL))original)(handler,sel);if (capture) RVDAWatchRecord(@"W03_native_tap",@{@"path":@"node",@"phase":@"returned"});
        };
    });
    RVDAWatchHook(@"ELMTextNode",@"textNode:tappedLinkAttribute:value:atPoint:textRange:",@"v@:@@@{CGPoint=dd}{_NSRange=QQ}",^id(IMP original,SEL sel) {
        return ^(id node,id text,id attribute,id value,CGPoint point,NSRange range) {
            BOOL capture=RVDAWatchActive();if (capture) RVDAWatchRecord(@"W03_native_tap",@{@"path":@"text_link",@"command_data_present":@([value isKindOfClass:NSData.class]),@"phase":@"entered"});
            ((void(*)(id,SEL,id,id,id,CGPoint,NSRange))original)(node,sel,text,attribute,value,point,range);
            if (capture) RVDAWatchRecord(@"W03_native_tap",@{@"path":@"text_link",@"phase":@"returned"});
        };
    });
    RVDAWatchHook(@"YTElementsCommandHandlerImpl",@"executeWithCommand:entry:fromView:sender:completionBlock:",@"v@:@@@@@?",^id(IMP original,SEL sel) {
        return ^(id handler,id command,id entry,id view,id sender,id completion) {
            BOOL capture=RVDAWatchActive();if (capture) RVDAWatchRecord(@"W04_command_dispatched",@{@"phase":@"entered",@"command_present":@(command!=nil),@"entry_present":@(entry!=nil),@"sender_class":sender ? NSStringFromClass([sender class]) : @"absent",@"completion_supplied":@(completion!=nil)});
            ((void(*)(id,SEL,id,id,id,id,id))original)(handler,sel,command,entry,view,sender,completion);
            if (capture) RVDAWatchRecord(@"W04_command_dispatched",@{@"phase":@"returned",@"success_inferred":@NO});
        };
    });
    RVDAWatchHook(@"YTLocalPlaybackController",@"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi",^id(IMP original,SEL sel) {
        return ^(id playback,id time,double before,double after,int source) {
            BOOL capture=RVDAWatchActive() && playback==RVCurrentPlayback;
            ((void(*)(id,SEL,id,double,double,int))original)(playback,sel,time,before,after,source);
            if (capture) RVDAWatchRecord(@"W05_native_outcome",@{@"kind":@"seek_requested",@"source":@(source),@"seek_completed_inferred":@NO});
        };
    });
    RVDAWatchHook(@"UIViewController",@"presentViewController:animated:completion:",@"v@:@B@?",^id(IMP original,SEL sel) {
        return ^(UIViewController *controller,UIViewController *presented,BOOL animated,id completion) {
            BOOL capture=RVDAWatchActive();
            ((void(*)(id,SEL,id,BOOL,id))original)(controller,sel,presented,animated,completion);
            if (capture) RVDAWatchRecord(@"W05_native_outcome",@{@"kind":@"presentation_requested",@"presented_class":presented ? NSStringFromClass(presented.class) : @"absent",@"native_presented_identity_matches":@(controller.presentedViewController==presented && presented!=nil)});
        };
    });
}
