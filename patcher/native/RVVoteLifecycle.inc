// SPDX-License-Identifier: MIT
// Native component replacement is independent of the current video's clock.
static NSUInteger RVVoteCompositionRevision;
static BOOL RVVoteLifecycleRefreshing,RVVoteLifecyclePending;
static NSTimeInterval RVVoteLastCompositionRefresh;
static NSDictionary *RVVoteTransitionState;
static void RVVoteCompositionChanged(id component,NSString *reason) {
    int role=RVVoteComponentRole(component);if (!role) return;
    RVVoteRecord(@"component_observed",@{@"component_role":@(role),@"component_template":RVVoteComponentTemplate(component) ?: @"unavailable"});
    if (!RVEnabled(@"return_dislikes")) return;
    if (!NSThread.isMainThread) {
        __weak id weakComponent=component;
        dispatch_async(dispatch_get_main_queue(),^{ if (weakComponent) RVVoteCompositionChanged(weakComponent,reason); });return;
    }
    RVVoteCompositionRevision++;
    if (role==1) RVVoteRecord(@"paired_template_materialized",@{@"paired_template_materialized":@YES,@"composition_revision":@(RVVoteCompositionRevision)});
    else RVVoteRecord(@"composition_changed",@{@"composition_revision":@(RVVoteCompositionRevision),@"composition_reason":reason});
    // Refresh immediately on the main callback, with one queued pass for the
    // native hierarchy attachment that follows initialization. Never wait for
    // playback ticks or a fixed quarter-second background scan delay.
    if (!RVVoteLifecycleRefreshing && RVDislikeHasCount() && NSProcessInfo.processInfo.systemUptime-RVVoteLastCompositionRefresh>.008) {
        RVVoteLifecycleRefreshing=YES;RVVoteLastCompositionRefresh=NSProcessInfo.processInfo.systemUptime;
        RVRefreshElementDislikes();RVVoteLifecycleRefreshing=NO;
    }
    if (RVVoteLifecyclePending) return;RVVoteLifecyclePending=YES;
    NSUInteger videoGeneration=RVDislikeGeneration;
    dispatch_async(dispatch_get_main_queue(),^{
        RVVoteLifecyclePending=NO;
        if (videoGeneration!=RVDislikeGeneration || !RVEnabled(@"return_dislikes") || RVVoteLifecycleRefreshing) return;
        RVVoteLifecycleRefreshing=YES;RVRefreshElementDislikes();RVVoteLifecycleRefreshing=NO;
        RVDislikeCaptureCheckpoint(@"native_component_rebound");
    });
}
static void RVVoteTransitionBegan(NSString *reason) {
    if (!NSThread.isMainThread) return;
    RVVoteTransitionState=@{@"reason":reason,@"video_generation":@(RVDislikeGeneration),
        @"composition_revision_at_start":@(RVVoteCompositionRevision),@"start_uptime_seconds":@(NSProcessInfo.processInfo.systemUptime),@"count_visible":@NO};
}
static void RVVoteCountMounted(RVElementDislikeState *state) {
    if (!NSThread.isMainThread || !state || !state.renderLabel.window || state.renderLabel.hidden) return;
    UILabel *label=state.renderLabel;
    if (label.bounds.size.width<=0 || label.bounds.size.height<=0 || ![state.video isEqual:RVDislikeVideo]) return;
    CGRect rect=label.bounds;UIView *ancestor=label;
    for (NSUInteger i=0;ancestor && i<32;i++) {
        if (ancestor.hidden || ancestor.alpha<=0) return;
        if (ancestor.clipsToBounds && CGRectIsEmpty(CGRectIntersection(rect,ancestor.bounds))) return;
        UIView *next=ancestor.superview;if (!next) break;rect=[ancestor convertRect:rect toView:next];ancestor=next;
    }
    if (CGRectIsEmpty(CGRectIntersection([label convertRect:label.bounds toView:label.window],label.window.bounds))) return;
    BOOL raster=NO;NSMutableArray *layers=[NSMutableArray arrayWithObject:label.layer];
    for (NSUInteger i=0;i<layers.count && i<16;i++) {
        CALayer *layer=layers[i];if (layer.contents) { raster=YES;break; }
        for (CALayer *child in layer.sublayers) if (layers.count<16) [layers addObject:child];
    }
    if (!raster) return;
    if (!RVVoteTransitionState || [RVVoteTransitionState[@"video_generation"] unsignedIntegerValue]!=RVDislikeGeneration || [RVVoteTransitionState[@"count_visible"] boolValue]) return;
    NSMutableDictionary *event=RVVoteTransitionState.mutableCopy;event[@"count_visible"]=@YES;
    event[@"visible_uptime_seconds"]=@(NSProcessInfo.processInfo.systemUptime);event[@"composition_revision_at_mount"]=@(RVVoteCompositionRevision);
    RVVoteTransitionState=event.copy;RVVoteRecord(@"transition_rebound",event);
}
static NSDictionary *RVVoteLifecycleReport(void) {
    return @{@"schema":@1,@"composition_revision":@(RVVoteCompositionRevision),@"video_generation":@(RVDislikeGeneration),
        @"transition":RVVoteTransitionState ?: @{},@"queued_refresh":@(RVVoteLifecyclePending)};
}
static void RVInstallVoteLifecycle(void) {
    RVHook(@"ELMComponent",@"initWithElement:context:treeLocalContext:",@"@@:@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id element,id context,id local) {
            id result=((id(*)(id,SEL,id,id,id))original)(object,sel,element,context,local);
            RVVoteCompositionChanged(result,@"component_created");return result;
        };
    });
    RVHook(@"ELMComponent",@"updateWithElement:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id element) { ((void(*)(id,SEL,id))original)(object,sel,element);RVVoteCompositionChanged(object,@"component_updated"); };
    });
    RVHook(@"ELMComponent",@"updateWithElement:forced:",@"v@:@B",NO,^id(IMP original,SEL sel) {
        return ^(id object,id element,BOOL forced) { ((void(*)(id,SEL,id,BOOL))original)(object,sel,element,forced);RVVoteCompositionChanged(object,@"component_forced_update"); };
    });
    RVHook(@"ELMComponent",@"underlyingComponentDidUpdate",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) { ((void(*)(id,SEL))original)(object,sel);RVVoteCompositionChanged(object,@"native_entity_updated"); };
    });
}
