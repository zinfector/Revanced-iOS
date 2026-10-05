// SPDX-License-Identifier: MIT
// SponsorBlock policy/transactions. Native ad callbacks are never used.
static void RVSPRefreshUI(void);
static void RVSPDetach(RVPlaybackSession *session,NSString *reason);
static NSString *RVSPPresentationGate(void);
static BOOL RVSPIsScrubbing(void);

@interface RVSponsorPromptState : NSObject
@property(nonatomic,weak) id controller;
@property(nonatomic,weak) RVPlaybackSession *session;
@property(nonatomic) NSUInteger generation;
@property(nonatomic) NSUInteger binding;
@property(nonatomic) NSUInteger serial;
@property(nonatomic,copy) NSString *mode;
@property(nonatomic,copy) NSString *gate;
@property(nonatomic,copy) NSString *policy;
@property(nonatomic,strong) NSDictionary *range;
@property(nonatomic,strong) NSDictionary *pending;
@property(nonatomic,strong) NSDictionary *undo;
@property(nonatomic,strong) NSDictionary *replay;
@property(nonatomic,strong) NSDictionary *priorReplay;
@property(nonatomic,strong) NSMutableDictionary *values;
@property(nonatomic,strong) NSMutableArray *history;
@property(nonatomic,weak) UIView *host;
@property(nonatomic) double seconds;
@property(nonatomic) double previousSeconds;
@property(nonatomic) NSTimeInterval clockWall;
@property(nonatomic) NSTimeInterval undoRemaining;
@property(nonatomic) NSTimeInterval undoDeadline;
@property(nonatomic) NSTimeInterval visibleWall;
@property(nonatomic) BOOL clockKnown;
@property(nonatomic) BOOL issuing;
@property(nonatomic) BOOL pendingHighlight;
@property(nonatomic) BOOL detached;
@end
@implementation RVSponsorPromptState @end
static NSDictionary *RVSPLastVisible;
static NSMutableDictionary *RVSPHooks;
static NSUInteger RVSPSerial,RVSPBinding;

static BOOL RVSPBound(RVSponsorPromptState *state) {
    RVPlaybackSession *s=state.session;
    if (!s || s.promptState!=state || state.generation!=s.generation || state.controller!=RVCurrentPlayback ||
        RVPlaybackForPlayer(RVCurrentPlayer)!=state.controller ||
        ![s.videoID isEqual:RVObject(state.controller,"contentVideoID")]) return NO;
    NSString *cpn=RVObject(RVObject(state.controller,"contentVideoCurrentTime"),"CPN") ?: RVObject(state.controller,"contentVideoCPN");
    return !s.cpn || (cpn && [s.cpn isEqual:cpn]);
}
static NSDictionary *RVSPSnapshot(RVSponsorPromptState *state,NSString *event) {
    return @{@"captured_at_epoch":@(NSDate.date.timeIntervalSince1970),@"event":event,
        @"generation":@(state.generation),@"binding":@(state.binding),@"prompt_serial":@(state.serial),@"mode":state.mode ?: @"hidden",
        @"first_blocked_checkpoint":state.gate ?: @"not_observed",@"binding_matches":@(RVSPBound(state)),
        @"pending_seek":@(state.pending!=nil),@"undo_available":@(state.undo && state.undoRemaining>0),
        @"undo_visible_seconds_remaining":@(MAX(0,state.undoRemaining)),@"replay_suppression_active":@(state.replay!=nil),
        @"selected_category":state.range[@"category"] ?: @"none",@"range_member_count":@( [state.range[@"members"] count]),
        @"state":state.values.copy ?: @{}};
}
static void RVSPEvent(RVSponsorPromptState *state,NSString *event) {
    RVSponsorCount([@"prompt_" stringByAppendingString:event]);
    if (!state || !RVEnabled(@"diagnostics")) return;
    if (!state.history) state.history=[NSMutableArray array];
    if (state.history.count>=16) [state.history removeObjectAtIndex:0];
    [state.history addObject:RVSPSnapshot(state,event)];
}
static void RVSPMode(RVSponsorPromptState *state,NSString *mode,NSDictionary *range,NSString *gate) {
    if (![state.mode isEqual:mode] || ![state.range isEqual:range] || ![state.gate isEqual:gate]) {
        state.mode=mode;state.range=range;state.gate=gate;state.serial=++RVSPSerial;
        RVSPEvent(state,@"selection_changed");
    }
}
static void RVSPReset(RVPlaybackSession *session,NSString *reason) {
    if (!NSThread.isMainThread) {
        __weak RVPlaybackSession *weakSession=session;NSUInteger generation=session.generation;
        dispatch_async(dispatch_get_main_queue(),^{ if (weakSession.generation==generation) RVSPReset(weakSession,reason); });return;
    }
    RVSponsorPromptState *state=session.promptState;
    if (state) { state.gate=reason;RVSPEvent(state,@"reset");RVSPDetach(session,reason); }
    session.promptState=nil;session.hasLastSkip=NO;
}
static NSString *RVSPPolicyKey(void) {
    // Include behaviors and global switches independently of the fetch key.
    id behaviors=RVSetting(@"sponsor_behaviors") ?: @{};
    NSArray *keys=[behaviors isKindOfClass:NSDictionary.class] ? [[behaviors allKeys] sortedArrayUsingSelector:@selector(compare:)] : @[];
    NSMutableArray *entries=[NSMutableArray array];
    for (NSString *key in keys) [entries addObject:@[key,behaviors[key]]];
    NSData *data=[NSJSONSerialization dataWithJSONObject:@[@(RVEnabled(@"sponsorblock")),@(RVEnabled(@"sponsorblock_manual")),entries,
        RVSetting(@"sponsor_categories") ?: @[],RVSetting(@"sponsor_min_duration") ?: @0] options:0 error:nil];
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"invalid";
}
static RVSponsorPromptState *RVSPState(RVPlaybackSession *session) {
    NSString *policy=RVSPPolicyKey();
    RVSponsorPromptState *state=session.promptState;
    if (state && (state.generation!=session.generation || ![state.policy isEqual:policy])) {
        RVSPReset(session,@"revision_changed");state=nil;
    }
    if (!state) {
        state=[RVSponsorPromptState new];state.session=session;state.controller=RVCurrentPlayback;
        state.generation=session.generation;state.binding=++RVSPBinding;state.policy=policy;state.values=[NSMutableDictionary dictionary];
        state.mode=@"hidden";state.gate=@"not_observed";state.serial=++RVSPSerial;session.promptState=state;
        if (!session.skippedOnce) session.skippedOnce=[NSMutableSet set];
    }
    return state;
}
static void RVSPSegmentsReplaced(RVPlaybackSession *session) {
    RVSponsorPromptState *old=session.promptState;BOOL valid=RVSPBound(old) && old.clockKnown;
    double seconds=old.seconds;NSTimeInterval wall=old.clockWall;
    RVSPReset(session,@"segments_replaced");
    if (valid) {
        RVSponsorPromptState *state=RVSPState(session);state.seconds=seconds;state.clockWall=wall;state.clockKnown=YES;
    }
}
static NSString *RVSPCategory(NSArray *members) {
    NSMutableSet *categories=[NSMutableSet set];
    for (NSDictionary *m in members) if (m[@"category"]) [categories addObject:m[@"category"]];
    return categories.count==1 ? categories.anyObject : @"multiple";
}
static NSArray *RVSPRanges(RVPlaybackSession *session,BOOL automatic,BOOL allForTools) {
    NSMutableArray *input=[NSMutableArray array];
    BOOL globalManual=RVEnabled(@"sponsorblock_manual");
    for (NSDictionary *record in session.rawSegments) {
        if (![record[@"action"] isEqual:@"skip"] || ![RVSetting(@"sponsor_categories") containsObject:record[@"category"]]) continue;
        NSString *behavior=RVSponsorBehavior(record[@"category"]);
        if ([@[@"ignore",@"seekbar-only"] containsObject:behavior]) continue;
        BOOL autoPolicy=!globalManual && [@[@"skip",@"skip-once"] containsObject:behavior];
        if (automatic && (!autoPolicy || ([behavior isEqual:@"skip-once"] && [session.skippedOnce containsObject:RVSegmentKey(record)]))) continue;
        NSDictionary *replay=session.promptState.replay;
        BOOL replayMember=replay && [record[@"start"] doubleValue]<[replay[@"end"] doubleValue] &&
            [record[@"end"] doubleValue]>[replay[@"start"] doubleValue];
        BOOL onceRevisited=[behavior isEqual:@"skip-once"] && [session.skippedOnce containsObject:RVSegmentKey(record)];
        if (!automatic && !allForTools && autoPolicy && !replayMember && !onceRevisited) continue;
        if (input.count<512) [input addObject:record];
    }
    [input sortUsingComparator:^NSComparisonResult(NSDictionary *a,NSDictionary *b) { return [a[@"start"] compare:b[@"start"]]; }];
    NSMutableArray *result=[NSMutableArray array];
    for (NSDictionary *record in input) {
        double start=[record[@"start"] doubleValue],end=[record[@"end"] doubleValue];
        NSMutableDictionary *last=result.lastObject;
        if (last && start<=[last[@"end"] doubleValue]) {
            last[@"end"]=@(MAX(end,[last[@"end"] doubleValue]));
            [last[@"members"] addObject:record];last[@"category"]=RVSPCategory(last[@"members"]);
        } else [result addObject:[@{@"start":@(start),@"end":@(end),@"category":record[@"category"],
            @"members":[NSMutableArray arrayWithObject:record],@"action":@"skip"} mutableCopy]];
    }
    return result;
}
static BOOL RVSPContains(NSDictionary *range,double seconds) {
    return range && seconds>=[range[@"start"] doubleValue] && seconds<[range[@"end"] doubleValue]-.08;
}
static BOOL RVSPReplaying(RVSponsorPromptState *state,NSDictionary *range) {
    if (!state.replay) return NO;
    return [range[@"start"] doubleValue]<[state.replay[@"end"] doubleValue] &&
        [range[@"end"] doubleValue]>[state.replay[@"start"] doubleValue];
}
static void RVSPCancelPending(RVSponsorPromptState *state,NSString *reason) {
    if (!state.pending) return;
    state.pending=nil;state.replay=state.priorReplay;state.priorReplay=nil;
    state.pendingHighlight=NO;state.values[@"seek_result"]=reason;RVSPEvent(state,reason);
}
static void RVSPUserSeek(NSString *reason) {
    RVSponsorPromptState *state=RVSession(RVCurrentPlayback).promptState;
    if (!NSThread.isMainThread) {
        __weak RVSponsorPromptState *weakState=state;NSUInteger binding=state.binding;
        dispatch_async(dispatch_get_main_queue(),^{
            RVSponsorPromptState *live=weakState;
            if (live && live.binding==binding && RVSPBound(live)) RVSPUserSeek(reason);
        });return;
    }
    if (!state || state.issuing) return;
    RVSPCancelPending(state,@"seek_superseded");state.undo=nil;state.undoRemaining=0;state.replay=nil;
    state.clockKnown=NO;state.visibleWall=0;state.session.hasLastSkip=NO;
    RVSPMode(state,@"hidden",nil,reason);RVSPDetach(state.session,reason);RVSPEvent(state,@"user_seek_invalidated");
}
static BOOL RVSPRequest(RVSponsorPromptState *state,NSDictionary *range,NSString *source) {
    if (!NSThread.isMainThread || !RVSPBound(state) || !RVEnabled(@"sponsorblock") ||
        !RVOrdinaryContent(state.controller) || state.pending || RVSPIsScrubbing() || !state.clockKnown) return NO;
    if (![state.policy isEqual:RVSPPolicyKey()]) { RVSPReset(state.session,@"policy_changed");return NO; }
    double observed=RVDouble(RVObject(state.controller,"contentVideoCurrentTime"),"time");
    if (!isfinite(observed) || observed<0) return NO;
    state.seconds=observed;
    BOOL undo=[source isEqual:@"undo"],point=[range[@"action"] isEqual:@"poi"];
    if (undo && (!state.undo || ![state.undo isEqual:range] || state.undoRemaining<=0 ||
        NSDate.timeIntervalSinceReferenceDate>=state.undoDeadline)) return NO;
    if (!undo && !point && !RVSPContains(range,state.seconds)) return NO;
    if (point && !undo && state.seconds>=[range[@"start"] doubleValue]) return NO;
    double target=undo ? [range[@"return"] doubleValue] : point ? [range[@"start"] doubleValue] : [range[@"end"] doubleValue];
    double duration=RVDouble(RVCurrentPlayer,"currentVideoTotalMediaTime");
    if (!isfinite(target) || target<0 || (isfinite(duration) && duration>0 && target>duration+.1)) return NO;
    Class times=NSClassFromString(@"YTSingleVideoTime");
    if (!RVCan(times,"timeWithTime:CPN:",@"@@:d@") || !RVCan(state.controller,"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi")) {
        RVSPMode(state,state.mode,state.range,@"seek_contract_missing");RVSPEvent(state,@"seek_contract_missing");return NO;
    }
    NSTimeInterval wall=NSDate.timeIntervalSinceReferenceDate;
    if (wall-state.session.lastSeekWallTime<2 && fabs(target-state.session.lastSeekTarget)<.1) return NO;
    id time=((id(*)(id,SEL,double,id))objc_msgSend)(times,sel_registerName("timeWithTime:CPN:"),target,state.session.cpn);
    if (!time) return NO;
    NSMutableDictionary *transaction=[range mutableCopy];
    transaction[@"source"]=source;transaction[@"target"]=@(target);transaction[@"issued_wall"]=@(wall);
    transaction[@"from"]=@(state.seconds);
    if (!undo) transaction[@"return"]=@(point ? state.seconds : MAX(0,[range[@"start"] doubleValue]-.25));
    state.priorReplay=state.replay;
    if (undo && !point) state.replay=@{@"start":transaction[@"return"],@"end":range[@"end"]};
    state.pending=transaction;state.pendingHighlight=point && !undo;
    state.session.lastSeekWallTime=wall;state.session.lastSeekTarget=target;
    state.undo=nil;state.undoRemaining=0;state.session.hasLastSkip=NO;
    state.values[@"seek_source"]=source;state.values[@"seek_result"]=@"requested";
    state.values[@"seek_target_seconds"]=@(target);state.values[@"native_ad_callbacks_used"]=@NO;
    RVSPMode(state,@"pending",transaction,@"seek_not_confirmed");RVSPEvent(state,@"seek_requested");
    RVSponsorCount(@"seek_requested");
    if (!undo) RVRecordSponsorSkip(state.session,[transaction[@"from"] doubleValue],target);
    state.issuing=YES;state.session.seeking=YES;
    @try { ((void(*)(id,SEL,id,double,double,int))objc_msgSend)(state.controller,
        sel_registerName("seekToTime:toleranceBefore:toleranceAfter:seekSource:"),time,0,0,0); }
    @finally { state.issuing=NO;state.session.seeking=NO; }
    __weak RVSponsorPromptState *weakState=state;
    NSDictionary *pending=state.pending;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),dispatch_get_main_queue(),^{
        RVSponsorPromptState *live=weakState;
        if (live && live.pending==pending && pending && RVSPBound(live)) {
            RVSPCancelPending(live,@"seek_timeout");RVSPMode(live,@"hidden",nil,@"seek_not_confirmed");RVSPRefreshUI();
        }
    });
    RVSPRefreshUI();return YES;
}
static void RVSPConfirm(RVSponsorPromptState *state,double seconds) {
    NSDictionary *pending=state.pending;
    if (!pending) return;
    double target=[pending[@"target"] doubleValue],from=[pending[@"from"] doubleValue];
    BOOL backward=target<from;
    // Require arrival beyond the old position, not merely a pre-seek tick.
    BOOL reached=backward ? seconds>=target-.5 && seconds<=target+1.0 && seconds<from-.08 :
        seconds>=target-.08 && seconds<=target+1.5 && seconds>from+.08;
    if (!reached) return;
    state.pending=nil;state.priorReplay=nil;
    state.values[@"seek_result"]=@"confirmed";
    state.values[@"seek_latency_seconds"]=@(MAX(0,NSDate.timeIntervalSinceReferenceDate-[pending[@"issued_wall"] doubleValue]));
    if ([pending[@"source"] isEqual:@"undo"]) {
        state.undo=nil;state.undoRemaining=0;state.session.hasLastSkip=NO;
        RVSPEvent(state,@"undo_confirmed");
    } else {
        for (NSDictionary *member in pending[@"members"]) if ([RVSponsorBehavior(member[@"category"]) isEqual:@"skip-once"])
            [state.session.skippedOnce addObject:RVSegmentKey(member)];
        if (state.pendingHighlight) state.session.highlightApplied=YES;
        state.undo=pending;state.undoRemaining=UIAccessibilityIsVoiceOverRunning() ? 10 : 6;
        state.undoDeadline=NSDate.timeIntervalSinceReferenceDate+30;
        state.visibleWall=0;state.session.hasLastSkip=YES;
        state.session.lastSkippedStart=[pending[@"return"] doubleValue];state.session.lastSkippedEnd=target;
        RVSPEvent(state,@"seek_confirmed");
        __weak RVSponsorPromptState *weakState=state;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC),dispatch_get_main_queue(),^{
            RVSponsorPromptState *live=weakState;
            if (live.undo==pending && RVSPBound(live)) {
                live.undo=nil;live.undoRemaining=0;live.session.hasLastSkip=NO;
                RVSPEvent(live,@"undo_expired");RVSPRefreshUI();
            }
        });
    }
    state.pendingHighlight=NO;
}
static void RVSPChoose(RVSponsorPromptState *state,BOOL allowAutomatic) {
    if (!RVSPBound(state) || !RVEnabled(@"sponsorblock") || !state.clockKnown) {
        RVSPMode(state,@"hidden",nil,@"playback_binding_missing");return;
    }
    NSString *contentGate=RVContentGate(state.controller);
    if (contentGate || RVSPIsScrubbing()) { RVSPMode(state,@"hidden",nil,contentGate ?: @"scrubbing");return; }
    if (state.undo && NSDate.timeIntervalSinceReferenceDate>=state.undoDeadline) {
        state.undo=nil;state.undoRemaining=0;state.session.hasLastSkip=NO;RVSPEvent(state,@"undo_expired");
    }
    if (state.pending) { RVSPMode(state,@"pending",state.pending,@"seek_not_confirmed");return; }
    if (allowAutomatic && !state.issuing && !state.session.seeking && !RVEnabled(@"sponsorblock_manual") && RVBool(state.controller,"isPlayingContentVideo")) {
        if (!state.session.highlightApplied) for (NSDictionary *p in state.session.rawSegments) {
            if (![p[@"action"] isEqual:@"poi"] || ![@[@"skip",@"skip-once"] containsObject:RVSponsorBehavior(p[@"category"])]) continue;
            if (state.seconds>=[p[@"start"] doubleValue]) { state.session.highlightApplied=YES;break; }
            if (RVSPRequest(state,p,@"auto_highlight")) return;
        }
        for (NSDictionary *r in RVSPRanges(state.session,YES,NO)) if (RVSPContains(r,state.seconds) && !RVSPReplaying(state,r)) {
            if (RVSPRequest(state,r,@"auto")) return;
            break;
        }
    }
    if (state.undo && state.undoRemaining>0) { RVSPMode(state,@"undo",state.undo,@"ready");return; }
    NSArray *manual=RVSPRanges(state.session,NO,NO);
    for (NSDictionary *r in manual) if (RVSPContains(r,state.seconds)) { RVSPMode(state,@"manual",r,@"ready");return; }
    for (NSDictionary *p in state.session.rawSegments) if ([p[@"action"] isEqual:@"poi"] &&
        !state.session.highlightApplied && state.seconds<[p[@"start"] doubleValue] &&
        ![@[@"ignore",@"seekbar-only"] containsObject:RVSponsorBehavior(p[@"category"])] &&
        (RVEnabled(@"sponsorblock_manual") || [RVSponsorBehavior(p[@"category"]) isEqual:@"manual-skip"])) {
        RVSPMode(state,@"highlight",p,@"ready");return;
    }
    RVSPMode(state,@"hidden",nil,@"no_manual_candidate");
}
static void RVSPClock(RVPlaybackSession *session,double seconds) {
    RVSponsorPromptState *state=RVSPState(session);
    if (!RVSPBound(state)) { RVSPReset(session,@"stale_clock");return; }
    NSTimeInterval wall=NSDate.timeIntervalSinceReferenceDate;
    if (state.clockKnown && !state.pending && !state.issuing) {
        double delta=seconds-state.seconds,elapsed=MAX(0,wall-state.clockWall);
        double speed=RVDouble(RVCurrentPlayer,"currentPlaybackRate");if (!isfinite(speed) || speed<=0) speed=4;
        if (delta<-.75 || delta>MAX(2,elapsed*MAX(1,speed)+1.5)) {
            state.undo=nil;state.undoRemaining=0;state.replay=nil;session.hasLastSkip=NO;RVSPEvent(state,@"clock_discontinuity");
        }
    }
    state.previousSeconds=state.seconds;state.seconds=seconds;state.clockWall=wall;state.clockKnown=YES;
    state.values[@"content_time_seconds"]=@(seconds);RVSPConfirm(state,seconds);
    if (state.replay && !state.pending && (seconds<[state.replay[@"start"] doubleValue]-.5 || seconds>=[state.replay[@"end"] doubleValue]-.08)) {
        state.replay=nil;RVSPEvent(state,@"replay_exited");
    }
    RVSPChoose(state,YES);RVSPRefreshUI();
}
static BOOL RVSPAct(RVSponsorPromptState *state,NSUInteger serial,BOOL fromTools) {
    if (!state || serial!=state.serial || !RVSPBound(state)) { RVSPEvent(state,@"stale_tap_rejected");return NO; }
    if (!fromTools && RVSPPresentationGate()) return NO;
    RVSPEvent(state,@"tap");
    return RVSPRequest(state,state.range,[state.mode isEqual:@"undo"] ? @"undo" : [state.mode isEqual:@"highlight"] ? @"manual_highlight" : @"manual");
}
static void RVSPTool(NSString *mode,NSUInteger generation) {
    RVPlaybackSession *session=RVSession(RVCurrentPlayback);
    if (session.generation!=generation || !session.promptState || !RVSPBound(session.promptState)) return;
    RVSponsorPromptState *state=RVSPState(session);
    if ([mode isEqual:@"undo"]) { if (state.undo) RVSPRequest(state,state.undo,@"undo");return; }
    if ([mode isEqual:@"highlight"]) {
        for (NSDictionary *p in session.rawSegments) if ([p[@"action"] isEqual:@"poi"] &&
            ![@[@"ignore",@"seekbar-only"] containsObject:RVSponsorBehavior(p[@"category"])]) { RVSPRequest(state,p,@"manual_highlight");break; }
    } else {
        for (NSDictionary *r in RVSPRanges(session,NO,NO)) if (RVSPContains(r,state.seconds)) { RVSPRequest(state,r,@"manual");return; }
        for (NSDictionary *r in RVSPRanges(session,YES,NO)) if (RVSPContains(r,state.seconds)) { RVSPRequest(state,r,@"manual");return; }
    }
}
static NSDictionary *RVSPReport(void) {
    RVSponsorPromptState *state=RVSession(RVCurrentPlayback).promptState;
    BOOL matches=RVSPBound(state) && RVSPLastVisible && [RVSPLastVisible[@"binding"] unsignedIntegerValue]==state.binding;
    return @{@"schema":@1,@"diagnostics_revision":@"sponsor-prompts-1",@"build_source_sha256":RV_BUILD_SOURCE_SHA256,
        @"enabled":@(RVEnabled(@"sponsorblock")),@"diagnostics_enabled":@(RVEnabled(@"diagnostics")),
        @"first_blocked_checkpoint":!RVEnabled(@"diagnostics") ? @"diagnostics_disabled" :
            !state ? @"playback_not_observed" : !RVSPBound(state) ? @"playback_binding_missing" :
            matches ? RVSPLastVisible[@"first_blocked_checkpoint"] : state.gate ?: @"foreground_snapshot_missing",
        @"hooks":RVSPHooks.copy ?: @{},@"counts":RVSponsorCounts.copy ?: @{},
        @"current_player":state ? RVSPSnapshot(state,@"report_copy") : @{},@"last_visible_player":RVSPLastVisible ?: @{},
        @"last_visible_matches_current_playback":@(matches),
        @"last_visible_age_seconds":RVSPLastVisible ? @(MAX(0,NSDate.date.timeIntervalSince1970-[RVSPLastVisible[@"captured_at_epoch"] doubleValue])) : NSNull.null,
        @"checkpoint_history":state.history.copy ?: @[],@"native_ad_callbacks_used":@NO,@"device_validated":@NO};
}
