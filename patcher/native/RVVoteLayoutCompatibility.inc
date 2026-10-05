// SPDX-License-Identifier: MIT
// Process identity and frozen startup policy. Four unused experiment overrides are retired.
static NSTimeInterval RVVoteLayoutStarted;
static NSString *RVVoteLayoutSession;
static NSUInteger RVVoteLayoutLaunch;
static BOOL RVVoteFirstLaunchRequested;
static void RVInstallVoteIdentity(void) {
    RVVoteFirstLaunchRequested=RVEnabled(@"return_dislikes") && RVEnabled(@"paired_vote_buttons") && RVEnabled(@"first_launch_ui");
    RVVoteLayoutStarted=NSProcessInfo.processInfo.systemUptime;RVVoteLayoutSession=NSUUID.UUID.UUIDString;
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    RVVoteLayoutLaunch=(NSUInteger)MAX(0,[defaults integerForKey:@"RVPort.VoteLayoutLaunchNumber"])+1;
    [defaults setInteger:(NSInteger)RVVoteLayoutLaunch forKey:@"RVPort.VoteLayoutLaunchNumber"];
}
static NSDictionary *RVVoteLayoutReport(void) {
    return @{@"schema":@2,@"strategy":@"first_launch_defaults_with_native_pair_normalization",@"session_id":RVVoteLayoutSession ?: @"unavailable",
        @"launch_number":@(RVVoteLayoutLaunch),@"native_experiment_overrides_retired":@YES,
        @"first_launch_ui_requested_at_start":@(RVVoteFirstLaunchRequested),@"restart_required_for_policy_changes":@YES,
        @"native_likes_replaced":@NO,@"persistent_experiment_cache_changed":@NO,@"device_layout_verified":@NO};
}
