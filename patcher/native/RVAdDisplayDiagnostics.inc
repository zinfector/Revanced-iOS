// SPDX-License-Identifier: MIT
// Consumer-owned display checkpoints. No URLs, payloads, tracking IDs or pointers exported.
@interface RVAdDisplayState : NSObject
@property(nonatomic,weak) id owner;
@property(nonatomic,weak) id section;
@property(nonatomic) NSUInteger generation;
@property(nonatomic,copy) NSString *surface;
@property(nonatomic,strong) NSMutableDictionary *counts;
@property(nonatomic,strong) NSMutableDictionary *values;
@end
@implementation RVAdDisplayState @end
static const void *RVAdDisplayKey=&RVAdDisplayKey;
static NSMutableDictionary *RVAdDisplayTotals;
static NSMutableArray *RVAdDisplayHistory;
static NSDictionary *RVAdDisplayLastVisible,*RVAdDisplayLastNative;
static NSDictionary *RVAdDisplayLastManagedSlot;
static NSMutableArray *RVAdElementHistory,*RVAdManagedSlotHistory;
static void RVDisplayStructuralCheckpoint(NSDictionary *value) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(RVAdDisplayState.class) {
        if (!RVAdElementHistory) RVAdElementHistory=[NSMutableArray array];
        if (RVAdElementHistory.count>=24) [RVAdElementHistory removeObjectAtIndex:0];
        [RVAdElementHistory addObject:value];
    }
}
static __weak RVAdDisplayState *RVAdDisplayLastOwner;
static NSUInteger RVAdDisplayGeneration;
static NSDate *RVAdDisplayStarted;
static RVAdDisplayState *RVDisplayState(id owner,BOOL boundary) {
    if (!owner) return nil;
    @synchronized(RVAdDisplayState.class) {
        RVAdDisplayState *state=objc_getAssociatedObject(owner,RVAdDisplayKey);
        if (!state || boundary) {
            state=[RVAdDisplayState new];state.owner=owner;state.generation=++RVAdDisplayGeneration;
            state.counts=[NSMutableDictionary dictionary];state.values=[NSMutableDictionary dictionary];
            NSString *name=NSStringFromClass([owner class]);
            state.surface=[name containsString:@"Watch"] ? @"watch" : [name containsString:@"Search"] ? @"search" : @"feed";
            objc_setAssociatedObject(owner,RVAdDisplayKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }
        return state;
    }
}
static void RVDisplayEvent(RVAdDisplayState *state,NSString *key,NSUInteger amount) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(RVAdDisplayState.class) {
        if (!RVAdDisplayStarted) RVAdDisplayStarted=NSDate.date;
        if (!RVAdDisplayTotals) RVAdDisplayTotals=[NSMutableDictionary dictionary];
        RVAdDisplayTotals[key]=@([RVAdDisplayTotals[key] unsignedLongLongValue]+amount);
        if (state) state.counts[key]=@([state.counts[key] unsignedLongLongValue]+amount);
    }
}
static void RVDisplayValue(RVAdDisplayState *state,NSString *key,id value) {
    if (!state || !RVEnabled(@"diagnostics")) return;
    @synchronized(RVAdDisplayState.class) { state.values[key]=value; }
}
static NSDictionary *RVDisplaySnapshot(RVAdDisplayState *state,NSString *trigger) {
    @synchronized(RVAdDisplayState.class) {
        NSDictionary *values=state.values.copy ?: @{},*counts=state.counts.copy ?: @{};
        NSString *block=!RVEnabled(@"feed_ads") ? @"policy_off" : !state.owner ? @"owner_missing" :
            values[@"blocked_checkpoint"] ?: ([counts[@"ad_classified"] unsignedLongLongValue] ? @"none_detected" : @"inconclusive_no_classified_ad");
        return @{@"surface":state.surface ?: @"unknown",@"context_generation":@(state.generation),
            @"trigger":trigger,@"captured_at_epoch":@(NSDate.date.timeIntervalSince1970),
            @"counts":counts,@"state":values,@"first_blocked_checkpoint":block};
    }
}
static void RVDisplayCapture(RVAdDisplayState *state,NSString *trigger) {
    if (!state || !NSThread.isMainThread || !RVEnabled(@"diagnostics")) return;
    NSDictionary *snapshot=RVDisplaySnapshot(state,trigger);
    @synchronized(RVAdDisplayState.class) { RVAdDisplayLastNative=snapshot; }
    UIViewController *owner=[state.owner isKindOfClass:UIViewController.class] ? state.owner : nil;
    if (!owner.viewIfLoaded.window || objc_getAssociatedObject(owner,RVAdDisplayKey)!=state) return;
    for (UIViewController *vc=owner;vc;vc=vc.parentViewController) {
        if (vc.presentedViewController && !vc.presentedViewController.isBeingDismissed) return;
        if ([vc.navigationController.viewControllers containsObject:vc] && vc.navigationController.visibleViewController!=vc) return;
    }
    @synchronized(RVAdDisplayState.class) {
        NSDictionary *previous=RVAdDisplayLastVisible;
        RVAdDisplayLastVisible=snapshot;RVAdDisplayLastOwner=state;
        if ([previous[@"context_generation"] isEqual:snapshot[@"context_generation"]] &&
            [previous[@"first_blocked_checkpoint"] isEqual:snapshot[@"first_blocked_checkpoint"]] &&
            [previous[@"state"] isEqual:snapshot[@"state"]]) return;
        if (!RVAdDisplayHistory) RVAdDisplayHistory=[NSMutableArray array];
        if (RVAdDisplayHistory.count>=12) [RVAdDisplayHistory removeObjectAtIndex:0];
        [RVAdDisplayHistory addObject:snapshot];
    }
}
static NSDictionary *RVDisplayReport(void) {
    @synchronized(RVAdDisplayState.class) {
        RVAdDisplayState *state=RVAdDisplayLastOwner;
        BOOL current=state.owner && objc_getAssociatedObject(state.owner,RVAdDisplayKey)==state;
        return @{@"schema":@3,@"checkpoint_version":@5,@"enabled":@(RVEnabled(@"feed_ads")),
            @"last_managed_slot_checkpoint":RVAdDisplayLastManagedSlot ?: @{},
            @"managed_slot_history":RVAdManagedSlotHistory.copy ?: @[],
            @"structural_element_history":RVAdElementHistory.copy ?: @[],
            @"session_events":RVAdDisplayTotals.copy ?: @{},@"last_native_operation":RVAdDisplayLastNative ?: @{},@"last_visible_surface":RVAdDisplayLastVisible ?: @{},
            @"last_visible_context_still_owned":@(current),@"checkpoint_history":RVAdDisplayHistory.copy ?: @[],
            @"session_age_seconds":RVAdDisplayStarted ? @(-RVAdDisplayStarted.timeIntervalSinceNow) : @0,
            @"first_blocked_checkpoint":!RVEnabled(@"diagnostics") ? @"diagnostics_disabled" : !RVEnabled(@"feed_ads") ? @"policy_off" :
                !RVAdDisplayLastVisible ? @"foreground_snapshot_missing" : !current ? @"foreground_context_retired" : RVAdDisplayLastVisible[@"first_blocked_checkpoint"],
            @"unsupported_routes":@[@"element_roots_without_positive_ad_provenance",@"generic_header_or_masthead",@"typed_ad_replacement_removal_undo_contract",@"already_bound_home_slot_teardown"],
            @"inspection_is_passive":@YES,@"device_verified":@NO};
    }
}
