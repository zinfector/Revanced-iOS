// SPDX-License-Identifier: MIT
// Keep native watch vote controls on the paired layout while RYD is enabled.
// These exact BOOL getters read persisted experiment flags in YouTube 21.39.4.
// Their connection to the reported redesign is a hypothesis pending device
// confirmation. Record actual native/effective reads; never claim a pair exists
// merely because a flag hook installed. Do not change global icon or speed flags.
static NSMutableDictionary *RVVoteLayoutFlags;
static NSTimeInterval RVVoteLayoutStarted;
static NSString *RVVoteLayoutSession;
static NSUInteger RVVoteLayoutLaunch;

static NSDictionary *RVVoteLayoutReport(void) {
    NSDictionary *flags;
    @synchronized(NSProcessInfo.class) { flags=[[NSDictionary alloc] initWithDictionary:RVVoteLayoutFlags ?: @{} copyItems:YES]; }
    return @{@"schema":@1,@"strategy":@"native_watch_pair_experiment_compatibility",
        @"enabled":@(RVEnabled(@"return_dislikes")),@"session_id":RVVoteLayoutSession ?: @"unavailable",
        @"launch_number":@(RVVoteLayoutLaunch),@"installed_before_app_launch":@YES,
        @"flag_reads":flags,@"native_likes_replaced":@NO,@"persistent_experiment_cache_changed":@NO,
        @"device_layout_verified":@NO};
}
static void RVInstallVoteLayoutCompatibility(void) {
    RVVoteLayoutStarted=NSProcessInfo.processInfo.systemUptime;
    RVVoteLayoutSession=NSUUID.UUID.UUIDString;
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    RVVoteLayoutLaunch=(NSUInteger)MAX(0,[defaults integerForKey:@"RVPort.VoteLayoutLaunchNumber"])+1;
    [defaults setInteger:(NSInteger)RVVoteLayoutLaunch forKey:@"RVPort.VoteLayoutLaunchNumber"];
    RVVoteLayoutFlags=[NSMutableDictionary dictionary];
    NSArray *contracts=@[
        @[@"YTColdConfigCrossPlatformCoreClientGlobalConfigImpl",@"enableWatchModernTypographyAssociatedUxChangesNative"],
        @[@"YTColdConfig",@"crossPlatformCoreClientGlobalConfigEnableWatchModernTypographyAssociatedUxChangesNative"],
        @[@"YTColdConfigCrossPlatformCoreClientGlobalConfigImpl",@"enableAlignVideoActionButtonStyleWithTetrisButtonStyleNative"],
        @[@"YTColdConfig",@"crossPlatformCoreClientGlobalConfigEnableAlignVideoActionButtonStyleWithTetrisButtonStyleNative"]];
    for (NSArray *contract in contracts) {
        NSString *key=[NSString stringWithFormat:@"%@.%@",contract[0],contract[1]];
        BOOL installed=RVHook(contract[0],contract[1],@"B@:",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id config) {
                BOOL native=((BOOL(*)(id,SEL))original)(config,sel);
                BOOL enabled=RVEnabled(@"return_dislikes");BOOL effective=enabled ? NO : native;
                if (RVEnabled(@"diagnostics")) @synchronized(NSProcessInfo.class) {
                    NSMutableDictionary *record=[RVVoteLayoutFlags[key] mutableCopy] ?: [NSMutableDictionary dictionary];
                    NSUInteger reads=[record[@"reads"] unsignedIntegerValue];
                    double elapsed=MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-RVVoteLayoutStarted));
                    if (!reads) { record[@"first_read_ms"]=@(elapsed);record[@"first_native_value"]=@(native); }
                    record[@"reads"]=@(reads+1);record[@"last_read_ms"]=@(elapsed);
                    record[@"last_native_value"]=@(native);record[@"last_effective_value"]=@(effective);
                    record[@"compatibility_enabled_at_read"]=@(enabled);
                    record[@"native_true_reads"]=@([record[@"native_true_reads"] unsignedIntegerValue]+(native ? 1 : 0));
                    record[@"overrides"]=@([record[@"overrides"] unsignedIntegerValue]+(native!=effective ? 1 : 0));
                    RVVoteLayoutFlags[key]=record.copy;
                }
                return effective;
            };
        });
        @synchronized(NSProcessInfo.class) {
            NSMutableDictionary *record=[RVVoteLayoutFlags[key] mutableCopy] ?: [NSMutableDictionary dictionary];
            record[@"hook_installed"]=@(installed);if (!record[@"reads"]) record[@"reads"]=@0;
            RVVoteLayoutFlags[key]=record.copy;
        }
    }
}
