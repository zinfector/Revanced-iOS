// SPDX-License-Identifier: MIT
// Reproduce the inspected no-blob cold-config path without erasing stored data.
// This is a process-wide default-config policy, not an identified UI experiment.
static id RVVoteFirstLaunchGroup;
static BOOL RVVoteFirstLaunchReady;
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
        else if (![RVVoteFirstLaunchCounts[@"active_group_read"] unsignedIntegerValue] &&
                 ![RVVoteFirstLaunchCounts[@"loaded_group_replaced"] unsignedIntegerValue]) blocked=@"cold_config_path_not_observed";
        else if (![RVVoteFirstLaunchCounts[@"request_checked"] unsignedIntegerValue]) blocked=@"request_not_observed";
        else if ([RVVoteFirstLaunchCounts[@"request_cold_present"] unsignedIntegerValue] || ![RVVoteFirstLaunchState[@"request_cold_defaults"] boolValue]) blocked=@"request_cold_configuration_present";
        else if (![RVVoteFirstLaunchCounts[@"serving_context_read"] unsignedIntegerValue]) blocked=@"template_context_not_observed";
        else if (![RVVoteModelCounts[@"paired_template_materialized"] unsignedIntegerValue]) blocked=@"paired_template_not_observed";
        return @{@"schema":@1,@"strategy":@"process_default_cold_config_and_template_context",
            @"requested_at_start":@(RVVoteFirstLaunchRequested),@"default_group_ready":@(RVVoteFirstLaunchReady),
            @"restart_required":@YES,@"saved_configuration_deleted":@NO,@"account_storage_changed":@NO,
            @"scope":@"cold_defaults_except_native_speed_and_registry_infrastructure",@"paired_row_device_verified":@NO,
            @"paired_template_seen_this_process":@([RVVoteModelCounts[@"paired_template_materialized"] unsignedIntegerValue]>0),
            @"first_blocked_checkpoint":blocked,@"counts":RVVoteFirstLaunchCounts.copy ?: @{},@"state":RVVoteFirstLaunchState.copy ?: @{}};
    }
}
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
                if (RVSpeedNativeConfigurationScope && RVVoteFirstLaunchReady) {
                    id saved=RVSpeedSavedColdGroup();
                    RVSpeedContextValue(@"original_cold_group_supplied",@{@"original_cold_group_available":@(saved!=nil)});
                    return saved ?: native;
                }
                if (!RVVoteFirstLaunchReady) return native;
                RVVoteFirstLaunchCheckpoint(@"active_group_read",@{@"active_group_defaults":@YES});
                return RVVoteFirstLaunchGroup;
            };
        });
        hooksReady=hooksReady && installed;
        RVVoteFirstLaunchCheckpoint(@"hook_installed",@{[className stringByAppendingString:@".coldConfigGroup"]:@(installed)});
    }
    // Keep data/hash consistent, including after native saves a newer hash.
    // Stored groups, hashes and server refreshes are retained for disabling this mode.
    for (NSArray *target in @[@[@"YTColdConfigStorage",@"coldConfigGroupHash"],@[@"YTColdConfig",@"configData"],@[@"YTColdConfig",@"storedConfigHashData"]]) {
        NSString *className=target[0],*name=target[1];
        BOOL installed=RVHook(className,name,@"@@:",NO,^id(IMP original,SEL sel) {
            return ^id(id object) {
                if (RVVoteFirstLaunchReady) {
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
