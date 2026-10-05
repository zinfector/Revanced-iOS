// SPDX-License-Identifier: MIT
// Proven companion consumers; never suppress the outer layout/promise lifecycle.
static NSString *const RVAdDisplayScopeKey=@"RVPort.DisplayAd.Scope";
static BOOL RVAdDisplaySectionHookReady,RVAdDisplayProviderHookReady;
@interface RVAdDisplayScope : NSObject
@property(nonatomic,strong) id section;
@property(nonatomic,strong) id renderer;
@property(nonatomic,strong) id provider;
@property(nonatomic,strong) RVAdDisplayState *state;
@property(nonatomic) BOOL cleared;
@property(nonatomic) BOOL providerCleared;
@end
@implementation RVAdDisplayScope @end
static RVAdDisplayScope *RVDisplayScope(void) { return NSThread.currentThread.threadDictionary[RVAdDisplayScopeKey]; }
static id RVDisplayCompanionOwner(id section) {
    Class provider=NSClassFromString(@"YTResponderProvider"),results=NSClassFromString(@"YTWatchNextResultsViewController");
    Protocol *protocol=NSProtocolFromString(@"YTCompanionAdResponderProvider");
    if (!NSThread.isMainThread || !protocol || !RVCan(provider,"providerForProtocol:firstResponder:optional:",@"@@:@@B")) return nil;
    id owner=((id(*)(id,SEL,id,id,BOOL))objc_msgSend)(provider,sel_registerName("providerForProtocol:firstResponder:optional:"),protocol,section,YES);
    return results && [owner isKindOfClass:results] ? owner : nil;
}
static BOOL RVDisplayCompanionKind(id renderer,NSString **reason) {
    if (!renderer) return NO;
    NSString *name=NSStringFromClass([renderer class]);
    NSArray *direct=@[@"YTICompanionAdRenderer",@"YTIAppPromoCompanionAdRenderer",@"YTIShoppingCompanionAdRenderer",
        @"YTICompactCompanionAdRenderer",@"YTIMultiItemCompanionAdRenderer",@"YTISuggestedVideosCompanionAdRenderer",@"YTIBelowPlayerAdLayoutRenderer"];
    if ([direct containsObject:name]) { if (reason) *reason=name;return YES; }
    if (![@[@"YTICompanionAdSupportedRenderers",@"YTIPlayerCompanionAdsSupportedRenderers"] containsObject:name]) return NO;
    for (NSString *field in @[@"companionAdRenderer",@"appPromoCompanionAdRenderer",@"shoppingCompanionAdRenderer",
        @"compactCompanionAdRenderer",@"multiItemCompanionAdRenderer",@"suggestedVideosCompanionAdRenderer",@"belowPlayerAdLayoutRenderer",@"elementRenderer"]) {
        NSString *presence=RVAdPresence(field);
        if (RVAdResolve(renderer,presence.UTF8String,@"B@:") && RVBool(renderer,presence.UTF8String)) {
            if (reason) *reason=field;return YES;
        }
    }
    return NO;
}
static void RVDisplayObserveCompanion(id controller,RVAdDisplayState *state) {
    Ivar field=class_getInstanceVariable(NSClassFromString(@"YTWatchNextResultsViewController"),"_companionAd");
    const char *type=field ? ivar_getTypeEncoding(field) : NULL;
    if (type && strcmp(type,"@\"<YTCompanionAdSupportedRenderersProtocol>\"")==0) {
        BOOL cleared=object_getIvar(controller,field)==nil;
        RVDisplayValue(state,@"companion_state_cleared",@(cleared));
        if (!cleared) RVDisplayValue(state,@"blocked_checkpoint",@"companion_retained");
    } else RVDisplayValue(state,@"companion_state_cleared",NSNull.null);
}
static BOOL RVDisplayArraySection(id section,id *model) {
    Class sectionClass=NSClassFromString(@"YTCompanionAdSectionController"),arrayClass=NSClassFromString(@"YTArraySectionViewModel");
    id viewModel=RVObject(section,"viewModel");
    if (model) *model=viewModel;
    return sectionClass && [section isKindOfClass:sectionClass] && arrayClass && [viewModel isKindOfClass:arrayClass] &&
        RVCan(viewModel,"numberOfEntries",@"Q@:") && RVCan(viewModel,"numberOfStagedEntries",@"Q@:");
}
static void RVDisplayObserveEntries(RVAdDisplayScope *scope,id model) {
    NSUInteger entries=((NSUInteger(*)(id,SEL))objc_msgSend)(model,sel_registerName("numberOfEntries"));
    NSUInteger staged=((NSUInteger(*)(id,SEL))objc_msgSend)(model,sel_registerName("numberOfStagedEntries"));
    RVDisplayValue(scope.state,@"section_entry_count",@(entries));RVDisplayValue(scope.state,@"section_staged_entry_count",@(staged));
    if (entries || staged) RVDisplayValue(scope.state,@"blocked_checkpoint",@"clear_not_observed");
}
static void RVInstallDisplayAds(void) {
    RVAdHook(@"YTWatchNextResultsViewController",@"setCompanionAd:layoutID:interactionLoggingAdsClientData:",@"v@:@@@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id renderer,id layoutID,id clientData) {
            RVAdDisplayState *state=RVDisplayState(controller,NO);NSString *reason=nil;
            RVDisplayEvent(state,@"companion_setter_seen",1);RVDisplayValue(state,@"route",@"legacy_companion_setter");
            BOOL ad=RVDisplayCompanionKind(renderer,&reason),block=ad && NSThread.isMainThread && RVEnabled(@"feed_ads");
            if (ad) { RVDisplayValue(state,@"blocked_checkpoint",@"none_detected");RVDisplayEvent(state,@"ad_classified",1);RVDisplayValue(state,@"last_rule",reason); }
            if (ad && !NSThread.isMainThread) RVDisplayValue(state,@"blocked_checkpoint",@"non_main_consumer");
            ((void(*)(id,SEL,id,id,id))original)(controller,sel,block ? nil : renderer,layoutID,clientData);
            RVDisplayEvent(state,@"original_companion_setter_forwarded",1);
            if (block) { RVDisplayEvent(state,@"companion_argument_cleared",1);RVDisplayObserveCompanion(controller,state); }
            else if (renderer && !ad) RVDisplayEvent(state,@"unknown_companion_passthrough",1);
            RVDisplayCapture(state,@"companion_setter");
        };
    });
    RVAdDisplaySectionHookReady=RVAdHook(@"YTAdElementsSectionViewRenderingAPI",@"renderSection:withRenderer:",@"v@:@@",YES,^id(IMP original,SEL sel) {
        return ^(id cls,id section,id renderer) {
            RVAdDisplayScope *scope=RVDisplayScope();id model=nil;
            if (scope && scope.section==section && scope.renderer==renderer && RVDisplayArraySection(section,&model) && RVCan(cls,"clearSection:",@"v@:@")) {
                ((void(*)(id,SEL,id))objc_msgSend)(cls,sel_registerName("clearSection:"),section);
                scope.cleared=YES;scope.state.section=section;
                objc_setAssociatedObject(section,RVAdDisplayKey,scope.state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                RVDisplayEvent(scope.state,@"native_section_clear_called",1);RVDisplayObserveEntries(scope,model);return;
            }
            ((void(*)(id,SEL,id,id))original)(cls,sel,section,renderer);
        };
    });
    RVAdDisplayProviderHookReady=RVAdHook(@"YTWatchNextResultsViewController",@"didUpdateBelowPlayerLayoutWithRenderer:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id renderer) {
            RVAdDisplayScope *scope=RVDisplayScope();BOOL block=scope && scope.cleared && scope.provider==controller && scope.renderer==renderer;
            ((void(*)(id,SEL,id))original)(controller,sel,block ? nil : renderer);
            if (block) { scope.providerCleared=YES;RVDisplayEvent(scope.state,@"original_provider_update_forwarded_empty",1);RVDisplayObserveCompanion(controller,scope.state); }
        };
    });
    RVAdHook(@"YTAdBelowPlayerLayoutRenderingAdapter",@"renderCompanionSection:withRenderer:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id adapter,id section,id renderer) {
            id owner=RVDisplayCompanionOwner(section);RVAdDisplayState *state=RVDisplayState(owner ?: section,NO);id model=nil;
            RVDisplayEvent(state,@"elements_companion_helper_seen",1);RVDisplayValue(state,@"route",@"elements_companion_section");
            // The exact ad adapter establishes provenance, but clearing requires the complete native pair.
            Class element=NSClassFromString(@"YTIElementRenderer"),api=NSClassFromString(@"YTAdElementsSectionViewRenderingAPI");
            BOOL contract=element && [renderer isKindOfClass:element] && RVDisplayArraySection(section,&model);
            Method expected=class_getInstanceMethod(NSClassFromString(@"YTWatchNextResultsViewController"),sel_registerName("didUpdateBelowPlayerLayoutWithRenderer:"));
            Method actual=owner ? class_getInstanceMethod(object_getClass(owner),sel_registerName("didUpdateBelowPlayerLayoutWithRenderer:")) : NULL;
            BOOL providerMatches=actual && expected && method_getImplementation(actual)==method_getImplementation(expected);
            BOOL ready=providerMatches && contract && RVAdDisplaySectionHookReady && RVAdDisplayProviderHookReady && RVCan(api,"clearSection:",@"v@:@");
            RVDisplayValue(state,@"owner_resolved",@(owner!=nil));RVDisplayValue(state,@"section_contract_ready",@(ready));
            if (contract) { RVDisplayValue(state,@"blocked_checkpoint",@"none_detected");RVDisplayEvent(state,@"ad_classified",1); }
            if (!ready) RVDisplayValue(state,@"blocked_checkpoint",owner ? @"section_contract_missing" : @"owner_unknown");
            if (!NSThread.isMainThread) RVDisplayValue(state,@"blocked_checkpoint",@"non_main_consumer");
            BOOL block=RVEnabled(@"feed_ads") && ready && NSThread.isMainThread;
            NSMutableDictionary *thread=NSThread.currentThread.threadDictionary;id previous=thread[RVAdDisplayScopeKey];
            RVAdDisplayScope *scope=nil;
            if (block) {
                scope=[RVAdDisplayScope new];scope.section=section;scope.renderer=renderer;scope.provider=owner;scope.state=state;
                thread[RVAdDisplayScopeKey]=scope;RVDisplayValue(state,@"last_rule",@"below_player_ad_adapter_provenance");
            } else [thread removeObjectForKey:RVAdDisplayScopeKey];
            @try { ((void(*)(id,SEL,id,id))original)(adapter,sel,section,renderer); }
            @finally { if (previous) thread[RVAdDisplayScopeKey]=previous;else [thread removeObjectForKey:RVAdDisplayScopeKey]; }
            RVDisplayEvent(state,@"original_layout_helper_completed",1);
            if (block && !scope.cleared) RVDisplayValue(state,@"blocked_checkpoint",@"clear_not_observed");
            if (block && !scope.providerCleared) RVDisplayValue(state,@"blocked_checkpoint",@"provider_update_unobserved");
            RVDisplayCapture(state,@"elements_companion");
        };
    });
    RVAdHook(@"YTCompanionAdSectionController",@"insetWithInset:",@"{UIEdgeInsets=dddd}@:{UIEdgeInsets=dddd}",NO,^id(IMP original,SEL sel) {
        return ^UIEdgeInsets(id section,UIEdgeInsets input) {
            UIEdgeInsets result=((UIEdgeInsets(*)(id,SEL,UIEdgeInsets))original)(section,sel,input);
            RVAdDisplayState *state=objc_getAssociatedObject(section,RVAdDisplayKey);
            if (state && state.section==section && state.owner && objc_getAssociatedObject(state.owner,RVAdDisplayKey)==state) {
                BOOL zero=result.top==0 && result.left==0 && result.bottom==0 && result.right==0;
                RVDisplayValue(state,@"empty_section_insets_zero",@(zero));RVDisplayEvent(state,@"native_insets_observed",1);
                if (!zero) RVDisplayValue(state,@"blocked_checkpoint",@"reserved_space");RVDisplayCapture(state,@"native_insets");
            }
            return result;
        };
    });
    for (NSString *selector in @[@"didBindSlotData:layoutData:",@"didDisposeSlotData:layoutData:"]) RVAdHook(@"YTAdElementsCellExternallyManagedSlotAdapterImpl",selector,@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id adapter,id slot,id layout) {
            // Observation only: NO from canEnterSlot does not cancel the native bind sequence.
            RVDisplayEvent(nil,[selector hasPrefix:@"didBind"] ? @"managed_slot_bound" : @"managed_slot_disposed",1);
            ((void(*)(id,SEL,id,id))original)(adapter,sel,slot,layout);
        };
    });
}
