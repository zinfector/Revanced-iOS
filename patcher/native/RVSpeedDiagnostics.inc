// SPDX-License-Identifier: MIT
// Main-thread, bounded, process-local evidence. Never store URLs or video IDs.
static NSTimeInterval RVSpeedTraceStart;
static NSMutableArray *RVSpeedEvents,*RVSpeedSelections;
static NSMutableDictionary *RVSpeedEventCounts;
static NSUInteger RVSpeedSequence,RVSpeedPickerID,RVSpeedSelectionID;
static NSUInteger RVSpeedReaderSamples;
static BOOL RVSpeedTraceStarted;
static NSDictionary *RVSpeedModelFacts(id model) {
    float rate=RVSpeedFloat(model,"rate"),scalar=RVSpeedFloat(model,"scalar"),effective=RVSpeedFloat(model,"effectivePlaybackRate");
    return @{@"argument_model_present":@(model!=nil),@"argument_base_rate_available":@(isfinite(rate)),
        @"argument_base_rate":@(isfinite(rate) ? rate : 0),@"argument_scalar_available":@(isfinite(scalar)),
        @"argument_scalar":@(isfinite(scalar) ? scalar : 0),@"argument_effective_rate_available":@(isfinite(effective)),
        @"argument_effective_rate":@(isfinite(effective) ? effective : 0)};
}
static NSDictionary *RVSpeedSnapshot(id player) {
    id core=RVPlaybackForPlayer(player),active=RVObject(player,"activePlaybackRateModel");
    id model=RVObject(active,"rateModel");float observed=RVSpeedObserved(player);
    float base=RVSpeedFloat(model,"rate"),scalar=RVSpeedFloat(model,"scalar");
    BOOL stateKnown=RVCan(core,"state",@"i@:");
    int coreState=stateKnown ? ((int(*)(id,SEL))objc_msgSend)(core,sel_registerName("state")) : -1;
    return @{@"player_present":@(player!=nil),@"current_content":@(RVSpeedCurrent(player,RVSpeedVideo(player))),
        @"ordinary_content":@(core && RVOrdinaryContent(core)),@"core_state_known":@(stateKnown),@"core_state":@(coreState),
        @"active_model_present":@(active!=nil),@"rate_model_present":@(model!=nil),
        @"observed_rate_available":@(isfinite(observed)),@"observed_rate":@(isfinite(observed) ? observed : 0),
        @"model_rate":@(isfinite(base) ? base : 0),@"model_scalar":@(isfinite(scalar) ? scalar : 0)};
}
static void RVSpeedCheckpoint(NSString *event,id player,NSDictionary *details) {
    if (!NSThread.isMainThread || !RVEnabled(@"diagnostics")) return;
    if (!RVSpeedTraceStarted) { RVSpeedTraceStarted=YES;RVSpeedTraceStart=NSProcessInfo.processInfo.systemUptime; }
    if (!RVSpeedEvents) { RVSpeedEvents=[NSMutableArray array];RVSpeedSelections=[NSMutableArray array];RVSpeedEventCounts=[NSMutableDictionary dictionary]; }
    RVSpeedEventCounts[event]=@([RVSpeedEventCounts[event] unsignedIntegerValue]+1);
    RVSpeedState *state=objc_getAssociatedObject(player,RVSpeedKey);
    NSMutableDictionary *entry=[RVSpeedSnapshot(player) mutableCopy];[entry addEntriesFromDictionary:details ?: @{}];
    entry[@"event"]=event;entry[@"sequence"]=@(++RVSpeedSequence);
    entry[@"milliseconds_since_hook_install"]=@((long long)((NSProcessInfo.processInfo.systemUptime-RVSpeedTraceStart)*1000));
    entry[@"picker_id"]=@(RVSpeedPickerID);entry[@"selection_id"]=@(state.selectionID);
    entry[@"intent_epoch"]=@(state.epoch);entry[@"source"]=state.source ?: @"not_observed";
    entry[@"requested_rate"]=@(isfinite(state.requested) ? state.requested : 0);
    [RVSpeedEvents addObject:entry];if (RVSpeedEvents.count>256) [RVSpeedEvents removeObjectAtIndex:0];
}
static void RVSpeedBeginSelection(id player) {
    RVSpeedState *state=RVSpeedForPlayer(player);state.selectionID=++RVSpeedSelectionID;
    RVSpeedCheckpoint(@"selection_received",player,RVSpeedModelFacts(state.model));
    if (!RVSpeedSelections) return;
    NSMutableDictionary *selection=[NSMutableDictionary dictionaryWithDictionary:@{@"selection_id":@(state.selectionID),
        @"picker_id":@(RVSpeedPickerID),@"source":state.source ?: @"unknown",@"requested_rate":@(isfinite(state.requested) ? state.requested : 0),
        @"initial":RVSpeedSnapshot(player),@"argument":RVSpeedModelFacts(state.model),@"checkpoints":[NSMutableArray array]}];
    [RVSpeedSelections addObject:selection];if (RVSpeedSelections.count>12) [RVSpeedSelections removeObjectAtIndex:0];
}
static void RVSpeedSelectionReadback(id player,RVSpeedState *state,NSUInteger delay,NSString *outcome) {
    NSMutableDictionary *point=[RVSpeedSnapshot(player) mutableCopy];
    point[@"scheduled_delay_ms"]=@(delay);point[@"outcome"]=outcome;point[@"apply_attempts"]=@(state.attempts);
    point[@"elapsed_since_selection_ms"]=@((long long)((NSProcessInfo.processInfo.systemUptime-state.selectionStarted)*1000));
    RVSpeedCheckpoint(@"selection_readback",player,point);
    for (NSMutableDictionary *selection in RVSpeedSelections) if ([selection[@"selection_id"] unsignedIntegerValue]==state.selectionID) {
        [selection[@"checkpoints"] addObject:point];selection[@"last_outcome"]=outcome;break;
    }
}
static void RVSpeedPickerOpened(id player,NSString *route) {
    RVSpeedPickerID++;RVSpeedReaderSamples=0;
    RVSpeedCheckpoint(@"picker_opened",player,@{@"route":route});
}
static NSDictionary *RVSpeedCheckpointReport(void) {
    NSArray *hooks;@synchronized(RVStatus) { hooks=[RVStatus filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString *line,NSDictionary *bindings) {
        return [line containsString:@"PlaybackRate"] || [line containsString:@"Varispeed"] || [line containsString:@"varispeed"] || [line containsString:@"SpeedSheet"] || [line containsString:@"updateActiveOverlayWithActiveVideo"];
    }]]; }
    NSDictionary *choice=RVSpeedSelections.lastObject;NSString *status=@"no_selection_observed";
    if (choice) {
        NSArray *points=choice[@"checkpoints"];NSDictionary *last=points.lastObject;
        if (!last) status=@"selection_received_without_delayed_readback";
        else if ([last[@"outcome"] isEqual:@"native_rate_confirmed"]) status=@"native_model_confirmed";
        else status=last[@"outcome"] ?: @"readback_not_confirmed";
    }
    return @{@"schema":@1,@"patcher_version":@"0.3.20",@"build_source_sha256":RV_BUILD_SOURCE_SHA256,
        @"session_id":RVSpeedSessionID ?: @"unavailable",@"launch_number":@(RVSpeedLaunchNumber),
        @"enabled":@(RVEnabled(@"diagnostics")),@"clock":@"monotonic_milliseconds_since_hook_install",
        @"event_capacity":@256,@"selection_capacity":@12,@"picker_count":@(RVSpeedPickerID),
        @"selection_count":@(RVSpeedSelectionID),@"events_dropped":@(RVSpeedSequence>256 ? RVSpeedSequence-256 : 0),
        @"latest_selection_status":status,
        @"counts":[RVSpeedEventCounts copy] ?: @{},@"recent_events":[RVSpeedEvents copy] ?: @[],
        @"recent_selections":[RVSpeedSelections copy] ?: @[],@"hooks":hooks ?: @[],@"device_verified":@NO};
}
static NSString *RVSpeedCheckpointSummary(void) {
    RVSpeedState *state=objc_getAssociatedObject(RVCurrentPlayer,RVSpeedKey);float observed=RVSpeedObserved(RVCurrentPlayer);
    return [NSString stringWithFormat:@"Speed: %@\nPicker %lu / choice %lu: %.2fx requested, %@ observed.\nCopy speed checkpoints preserves every choice and 0/100/350/1000 ms readbacks.\n\n",
        state.gate ?: @"No choice observed",(unsigned long)RVSpeedPickerID,(unsigned long)RVSpeedSelectionID,state.requested,
        isfinite(observed) ? [NSString stringWithFormat:@"%.2fx",observed] : @"unavailable"];
}
