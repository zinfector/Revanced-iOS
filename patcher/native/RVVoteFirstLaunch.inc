// SPDX-License-Identifier: MIT
// Reproduce the no-blob vote path only during watch request-context construction.
// Native storage, controller construction and command/panel consumers stay native.
static id RVVoteFirstLaunchGroup;
static BOOL RVVoteFirstLaunchReady;
static _Thread_local NSUInteger RVVoteFirstLaunchScope;
static BOOL RVVoteFirstLaunchScoped(void) { return RVVoteFirstLaunchReady && RVVoteFirstLaunchScope>0; }
// Native YTPlayerRequestFactoryImpl uses 0x73; WatchNextServiceImpl uses 0xa1.
static BOOL RVVoteFirstLaunchWatchService(NSInteger service) { return service==0x73 || service==0xa1; }
static BOOL RVSpeedFirstLaunchPolicyActive(void) { return RVVoteFirstLaunchReady; }
static NSMutableDictionary *RVVoteFirstLaunchCounts,*RVVoteFirstLaunchState;
static void RVVoteFirstLaunchCheckpoint(NSString *event,NSDictionary *values) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(UIImage.class) {
        if (!RVVoteFirstLaunchCounts) RVVoteFirstLaunchCounts=[NSMutableDictionary dictionary];
        if (!RVVoteFirstLaunchState) RVVoteFirstLaunchState=[NSMutableDictionary dictionary];
        RVVoteFirstLaunchCounts[event]=@([RVVoteFirstLaunchCounts[event] unsignedLongLongValue]+1);
        [RVVoteFirstLaunchState addEntriesFromDictionary:values ?: @{}];
    }
}
static NSDictionary *RVVoteFirstLaunchReport(void) {
    @synchronized(UIImage.class) {
        NSString *blocked=@"none_detected";
        if (!RVVoteFirstLaunchRequested) blocked=@"disabled_at_process_start";
        else if (!RVVoteFirstLaunchReady) blocked=@"startup_policy_unavailable";
        else if (![RVVoteFirstLaunchCounts[@"watch_scope_entered"] unsignedIntegerValue]) blocked=@"watch_scope_not_observed";
        else if (![RVVoteFirstLaunchCounts[@"request_checked"] unsignedIntegerValue]) blocked=@"request_not_observed";
        else if ([RVVoteFirstLaunchCounts[@"request_cold_present"] unsignedIntegerValue] || ![RVVoteFirstLaunchState[@"request_cold_defaults"] boolValue]) blocked=@"request_cold_configuration_present";
        else if (![RVVoteFirstLaunchCounts[@"serving_context_read"] unsignedIntegerValue]) blocked=@"template_context_not_observed";
        else if (![RVVoteModelCounts[@"paired_template_materialized"] unsignedIntegerValue]) blocked=@"paired_template_not_observed";
        return @{@"schema":@3,@"revision":@"watch-vote-scope-2",@"strategy":@"watch_request_context_defaults_only",
            @"requested_at_start":@(RVVoteFirstLaunchRequested),@"default_group_ready":@(RVVoteFirstLaunchReady),
            @"restart_required":@YES,@"saved_configuration_deleted":@NO,@"account_storage_changed":@NO,
            @"scope":@"player_and_watch_next_context_serialization",@"native_startup_configuration_preserved":@YES,
            @"native_watch_consumer_configuration_preserved":@YES,@"paired_row_device_verified":@NO,
            @"paired_template_seen_this_process":@([RVVoteModelCounts[@"paired_template_materialized"] unsignedIntegerValue]>0),
            @"first_blocked_checkpoint":blocked,@"counts":RVVoteFirstLaunchCounts.copy ?: @{},@"state":RVVoteFirstLaunchState.copy ?: @{}};
    }
}
// Save/restore the exact depth, including nested calls and exceptions. A non-watch
// request temporarily suspends an enclosing watch scope during its own context
// construction; an open player is never used as ownership for browse requests.
static NSUInteger RVVoteFirstLaunchEnter(NSString *route,BOOL watch) {
    NSUInteger previous=RVVoteFirstLaunchScope;
    RVVoteFirstLaunchScope=watch && RVVoteFirstLaunchReady ? previous+1 : 0;
    RVVoteFirstLaunchCheckpoint(RVVoteFirstLaunchScoped() ? @"watch_scope_entered" : @"native_scope_entered",@{@"last_scope_route":route});
    return previous;
}
static void RVVoteFirstLaunchLeave(NSUInteger previous) { RVVoteFirstLaunchScope=previous; }
static void RVInstallVoteFirstLaunch(void) {
    if (RVVoteFirstLaunchRequested) {
        Class cls=NSClassFromString(@"YTIColdConfigGroup");
        // The native load routine itself returns +message when the blob is absent.
        id group=RVObject((id)cls,"message");
        NSData *data=RVObject(group,"data");
        if (cls && [group isKindOfClass:cls] && [data isKindOfClass:NSData.class] && data.length==0) {
            RVVoteFirstLaunchGroup=group;RVVoteFirstLaunchReady=YES;
        }
    }
    BOOL hooksReady=YES;
    for (NSString *className in @[@"YTColdConfigStorage",@"YTColdConfig"]) {
        BOOL installed=RVHook(className,@"coldConfigGroup",@"@@:",NO,^id(IMP original,SEL sel) {
            return ^id(id object) {
                id native=((id(*)(id,SEL))original)(object,sel);
                if (native!=RVVoteFirstLaunchGroup) RVSpeedCaptureColdGroup(native,NO);
                if (RVSpeedNativeConfigurationScope && RVVoteFirstLaunchScoped()) {
                    RVSpeedContextValue(@"original_cold_group_supplied",@{@"original_cold_group_available":@(native!=nil)});
                    return native;
                }
                if (!RVVoteFirstLaunchScoped()) {
                    RVVoteFirstLaunchCheckpoint(@"native_group_preserved",nil);return native;
                }
                RVVoteFirstLaunchCheckpoint(@"active_group_read",@{@"active_group_defaults":@YES});
                return RVVoteFirstLaunchGroup;
            };
        });
        hooksReady=hooksReady && installed;
        RVVoteFirstLaunchCheckpoint(@"hook_installed",@{[className stringByAppendingString:@".coldConfigGroup"]:@(installed)});
    }
    // Keep data/hash consistent only inside the same scoped vote operation.
    for (NSArray *target in @[@[@"YTColdConfigStorage",@"coldConfigGroupHash"],@[@"YTColdConfig",@"configData"],@[@"YTColdConfig",@"storedConfigHashData"]]) {
        NSString *className=target[0],*name=target[1];
        BOOL installed=RVHook(className,name,@"@@:",NO,^id(IMP original,SEL sel) {
            return ^id(id object) {
                if (RVVoteFirstLaunchScoped() && !RVSpeedNativeConfigurationScope) {
                    RVVoteFirstLaunchCheckpoint(@"cold_request_source_defaulted",nil);return nil;
                }
                return ((id(*)(id,SEL))original)(object,sel);
            };
        });
        hooksReady=hooksReady && installed;
        RVVoteFirstLaunchCheckpoint(@"hook_installed",@{[NSString stringWithFormat:@"%@.%@",className,name]:@(installed)});
    }
    RVVoteFirstLaunchReady=RVVoteFirstLaunchReady && hooksReady;
    RVVoteFirstLaunchCheckpoint(@"policy_prepared",@{@"critical_hooks_ready":@(hooksReady),@"default_group_constructed":@(RVVoteFirstLaunchGroup!=nil)});
}
