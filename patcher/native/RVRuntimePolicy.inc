// SPDX-License-Identifier: MIT
#include "CapabilityRuntime.h"
#include "RVRuntimeFields.inc"
static NSString *const RVRuntimeSuite=@"org.revanced.runtime-integration";
static NSMutableDictionary *RVRuntimePolicyResults;
static void *RVRuntimePreferences(void) {
    static void *preferences;static dispatch_once_t once;
    dispatch_once(&once,^{ preferences=rv_ios_create_preferences(RVRuntimeSuite.UTF8String); });return preferences;
}
#include "RVRuntimePolicyDispatch.inc"
static BOOL RVRuntimeSourceFeatureEnabled(NSString *feature) {
    @synchronized(RVRuntimeSuite) {
        BOOL result=NO;RVContext context={0,0,10000};
        if (!RVRuntimeDispatch(feature,&context,&result)) return RVSourceFeatureEnabled(feature);
        if (!RVRuntimePolicyResults) RVRuntimePolicyResults=[NSMutableDictionary dictionary];
        NSDictionary *previous=RVRuntimePolicyResults[feature];
        RVRuntimePolicyResults[feature]=@{@"calls":@([previous[@"calls"] unsignedLongLongValue]+1),
            @"result":@(result),@"error":@(context.error),@"steps":@(context.steps),@"provider":@"compiled_generated_ios_preferences"};
        return context.error ? NO : result;
    }
}
static __weak UIView *RVRuntimeRollingView;
static uint64_t RVRuntimeRollingEpoch,RVRuntimeRollingSequence;
static NSMutableArray *RVRuntimeRollingEvents;
static void RVRuntimeObserveRolling(id object,BOOL input,BOOL output,BOOL policy,NSString *selector) {
    if (!NSThread.isMainThread || !atomic_load(&RVDBGActive) || ![object isKindOfClass:UIView.class]) return;
    UIView *view=object;if (RVRuntimeRollingView!=view) { RVRuntimeRollingView=view;RVRuntimeRollingEpoch++; }
    if (!RVRuntimeRollingEvents) RVRuntimeRollingEvents=[NSMutableArray array];
    NSDictionary *layout=nil;id count=RVRuntimeFieldObject(object,@"YTRollingNumberView",@"_updatedCount",&layout);
    NSDictionary *policyResult;@synchronized(RVRuntimeSuite) { policyResult=RVRuntimePolicyResults[@"disable_rolling_numbers"] ?: @{}; }
    NSMutableDictionary *event=[@{@"sequence":@(++RVRuntimeRollingSequence),@"uptime":@(NSProcessInfo.processInfo.systemUptime),
        @"selector":selector,@"incoming_skip":@(input),@"forwarded_skip":@(output),@"policy":@(policy),
        @"policy_error":policyResult[@"error"] ?: @0,@"visible":@(view.window!=nil && !view.hidden && view.alpha>0),
        @"layout":layout ?: @{},@"updated_count_kind":count ? NSStringFromClass([count class]) : @"nil",
        @"player_ownership_proven":@NO,@"visual_animation_verified":@NO} mutableCopy];
    if (RVRuntimeRollingEvents.count>=64) [RVRuntimeRollingEvents removeObjectAtIndex:0];[RVRuntimeRollingEvents addObject:event];
}
