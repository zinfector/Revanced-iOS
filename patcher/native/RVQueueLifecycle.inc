// SPDX-License-Identifier: MIT
// A cold queue action must first construct YouTube's native watch owner.
// This is one deferred user action, not a second queue model.
static NSDictionary *RVQueuePendingOperation;
static id RVQueuePendingEntry;
static id RVQueuePendingPreviousResponse;
static __weak id RVQueuePendingResponder;
static __weak id RVQueuePendingLayer;
static NSUInteger RVQueuePendingGeneration;
static NSTimeInterval RVQueuePendingDeadline;
static BOOL RVQueuePendingTickScheduled;
static atomic_uint RVQueueBootstrapRequests,RVQueueBootstrapCompleted,RVQueueBootstrapFailures;

static void RVQueueShowMessage(NSString *message,id responder) {
    Class events=NSClassFromString(@"YTToastResponderEvent");
    if (!RVCan(events,"eventWithMessage:firstResponder:",@"@@:@@")) return;
    id event=((id(*)(id,SEL,id,id))objc_msgSend)(events,sel_registerName("eventWithMessage:firstResponder:"),message,responder ?: RVQueueLocalWatch ?: RVCurrentPlayer);
    RVQueueVoid(event,"send");
}
static void RVQueueClearPending(void) {
    RVQueuePendingOperation=nil;RVQueuePendingEntry=nil;RVQueuePendingPreviousResponse=nil;RVQueuePendingResponder=nil;RVQueuePendingLayer=nil;RVQueuePendingDeadline=0;RVQueuePendingTickScheduled=NO;RVQueuePendingGeneration++;
}
static void RVQueueCancelMetadata(void) {
    if (RVQueueMetadataBusy) atomic_fetch_add(&RVQueueMetadataCancelled,1);
    RVQueueMetadataBusy=NO;RVQueueMetadataGeneration++;
}
static void RVQueueInvalidatePresentation(void) {
    RVQueuePresentationLastResponse=nil;RVQueuePresentationAdaptedResponse=nil;RVQueuePresentationOwner=nil;
}
static void RVQueueOwnerChanged(BOOL accountChanged) {
    if (!NSThread.isMainThread) return;
    RVQueueCancelMetadata();RVQueueInvalidatePresentation();RVQueueLocalWatch=nil;RVQueueLocalPanelController=nil;
    // Cold startup may legitimately replace an empty native owner. Keep its
    // pending target check; an account switch cancels that action as well.
    if (accountChanged && RVQueuePendingOperation) { atomic_fetch_add(&RVQueueBootstrapCancelled,1);RVQueueClearPending(); }
}
static void RVQueueResetSession(BOOL preserveBootstrap) {
    RVQueueCancelMetadata();RVQueueInvalidatePresentation();
    if (!preserveBootstrap) {
        if (RVQueuePendingOperation) atomic_fetch_add(&RVQueueBootstrapCancelled,1);
        RVQueueClearPending();
    }
    RVQueueNativeOwned=NO;RVQueueNativeSourcePlaylist=nil;RVQueueLocalFailure=nil;
    RVQueueLocalWatch=nil;RVQueueLocalPanelController=nil;RVQueueLocalController=nil;
    RVQueueLocalAccount=nil;RVQueueLocalAccountBound=NO;atomic_fetch_add(&RVQueueSessionResets,1);
}
static BOOL RVQueueLayerTracksSession(id layer) {
    if (!RVCompatible || !NSThread.isMainThread) return NO;
    id watch=RVObject(layer,"currentWatchController");
    return RVQueuePendingLayer==layer || (RVQueueLocalWatch && RVObject(RVQueueLocalWatch,"parentResponder")==layer)
        || (watch && RVQueueLocalController && RVObject(RVObject(watch,"playlistPanelController"),"queueController")==RVQueueLocalController);
}
static void RVInstallQueueLifecycle(void) {
    RVHook(@"YTWatchLayerViewController",@"clearWatchController",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^(id layer) {
            if (RVQueueLayerTracksSession(layer)) {
                // Native acquisition may clear an old Watch as part of this
                // bootstrap. The real dismissal hook below cancels user close.
                BOOL preserve=RVQueuePendingOperation!=nil;
                if (preserve) RVQueuePendingLayer=layer;
                RVQueueResetSession(preserve);
            }
            ((void(*)(id,SEL))original)(layer,selector);
        };
    });
    RVHook(@"YTWatchLayerViewController",@"didDismissMiniBar",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^(id layer) {
            BOOL tracked=RVQueueLayerTracksSession(layer);
            ((void(*)(id,SEL))original)(layer,selector);
            if (tracked) RVQueueResetSession(NO);
        };
    });
}
static void RVQueuePendingFailure(NSString *reason) {
    id responder=RVQueuePendingResponder;RVQueueClearPending();RVQueueLocalFailure=reason;
    atomic_fetch_add(&RVQueueBootstrapFailures,1);RVQueueShowMessage(@"Unable to open the queue. Please try again.",responder);
}
static void RVQueueSchedulePending(void) {
    if (RVQueuePendingTickScheduled) return;RVQueuePendingTickScheduled=YES;
    NSUInteger generation=RVQueuePendingGeneration;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
        if (RVQueuePendingOperation && generation==RVQueuePendingGeneration) { RVQueuePendingTickScheduled=NO;RVQueueFinishPending(); }
    });
}
static void RVQueueFinishPending(void) {
    if (!NSThread.isMainThread || !RVQueuePendingOperation) return;
    if (RVQueueLocalDepth || RVQueueNativeResponseDepth) { RVQueueSchedulePending();return; }
    if (!RVCompatible || NSProcessInfo.processInfo.systemUptime>RVQueuePendingDeadline) { RVQueuePendingFailure(@"native_watch_bootstrap_timeout");return; }
    id controller=RVQueueResolveNativeOwner(RVQueuePendingResponder);
    if (!controller || !RVQueuePresentationHasResponse()) { RVQueueSchedulePending();return; }
    NSString *target=RVQueuePendingOperation[@"video_id"];
    id endpoint=RVObject(RVQueuePresentationLastResponse,"currentVideoEndpoint");
    NSString *current=RVObject(RVObject(endpoint,"watchEndpoint"),"videoId");
    if (!RVQueueValidVideo(current)) current=RVQueueLocalCurrent();
    if (!RVQueueValidVideo(current)) { RVQueueSchedulePending();return; }
    if (![target isEqual:current]) {
        if (RVQueuePresentationLastResponse==RVQueuePendingPreviousResponse) { RVQueueSchedulePending();return; }
        RVQueuePendingFailure(@"native_watch_bootstrap_target_changed");return;
    }
    if (RVBool(controller,"isMDXRemoteQueue")) { RVQueuePendingFailure(@"native_watch_bootstrap_casting");return; }
    NSUInteger count=RVQueueCount();id source=RVQueuePendingEntry;
    id itemClass=NSClassFromString(@"YTQueueItem");
    if (!count && (!RVCan(controller,"insertQueueItems:atIndex:",@"v@:@Q")
        || !RVCan(controller,"setNowPlayingIndex:",@"v@:Q")
        || !RVCan(itemClass,"queueItemWithPlaylistPanelVideoRenderer:",@"@@:@"))) {
        RVQueuePendingFailure(@"native_watch_bootstrap_seed_adapter_unavailable");return;
    }
    if (count && RVQueueNativeIndex(target)==NSNotFound) { RVQueuePendingFailure(@"native_watch_bootstrap_queue_changed");return; }
    // The transition already opened the first selected video. Seed it once;
    // replaying add-to-queue here would add a duplicate of that same video.
    id item=nil;
    @try {
        id renderer=count ? nil : RVQueueMakeVideo(target,source);
        if (renderer) item=((id(*)(id,SEL,id))objc_msgSend)(itemClass,sel_registerName("queueItemWithPlaylistPanelVideoRenderer:"),renderer);
    } @catch (__unused NSException *exception) { RVQueuePendingFailure(@"native_watch_bootstrap_seed_exception");return; }
    if (!count && !item) { RVQueuePendingFailure(@"native_watch_bootstrap_seed_item_unavailable");return; }
    RVQueueClearPending();RVQueueLocalDepth++;
    @try {
        RVQueueNativeSourcePlaylist=[RVObject(RVObject(controller,"playlistPanel"),"playlistId") copy];
        RVQueueNativeOwned=YES;
        if (!count) {
            ((void(*)(id,SEL,id,NSUInteger))objc_msgSend)(controller,sel_registerName("insertQueueItems:atIndex:"),@[item],0);
            RVQueueNumber(controller,"setNowPlayingIndex:",0);
        }
        if (RVQueueNativeIndex(target)==NSNotFound) {
            RVQueueNativeOwned=NO;RVQueueLocalFailure=@"native_watch_bootstrap_seed_not_inserted";
            atomic_fetch_add(&RVQueueBootstrapFailures,1);return;
        }
        RVQueueLocalFailure=nil;atomic_fetch_add(&RVQueueBootstrapCompleted,1);
        RVQueueNativeRefreshPresentation();
    } @catch (__unused NSException *exception) {
        RVQueueLocalFailure=@"native_watch_bootstrap_exception";atomic_fetch_add(&RVQueueBootstrapFailures,1);
    } @finally { RVQueueLocalDepth--; }
}
static BOOL RVQueueStartNativeWatch(NSDictionary *operation,id entry,id responder) {
    if (!RVCompatible || !NSThread.isMainThread || !RVQueueValidVideo(operation[@"video_id"])) return NO;
    if (!responder) { RVQueueLocalFailure=@"native_watch_bootstrap_responder_missing";return NO; }
    if (RVQueuePendingOperation) { RVQueueShowMessage(@"The queue is opening. Please try again in a moment.",responder);return YES; }
    id command=RVQueueWatchCommand(operation[@"video_id"]);
    id transition=[NSClassFromString(@"YTWatchTransition") alloc];Class events=NSClassFromString(@"YTWatchTransitionResponderEvent");
    if (!command || !RVCan(transition,"initWithNavEndpoint:watchEndpointSource:",@"@@:@i")
        || !RVCan(events,"eventWithTransition:fromView:firstResponder:",@"@@:@@@")) {
        RVQueueLocalFailure=@"native_watch_bootstrap_adapter_unavailable";return NO;
    }
    transition=((id(*)(id,SEL,id,int))objc_msgSend)(transition,sel_registerName("initWithNavEndpoint:watchEndpointSource:"),command,0);
    if (!transition || !RVCan(transition,"setBeginPlaybackOnMinibar:",@"v@:B")
        || !RVCan(transition,"setPrefersPlaybackLayoutStateCollapsed:",@"v@:B")
        || !RVCan(transition,"setPausePlaybackWhenVideoActivates:",@"v@:B")) return NO;
    // Native collapsed playback acquires a player (21.39.4 0x10134b210).
    // Begin-on-minibar instead selects ready-to-resume/deferred acquisition.
    ((void(*)(id,SEL,BOOL))objc_msgSend)(transition,sel_registerName("setBeginPlaybackOnMinibar:"),NO);
    ((void(*)(id,SEL,BOOL))objc_msgSend)(transition,sel_registerName("setPrefersPlaybackLayoutStateCollapsed:"),YES);
    ((void(*)(id,SEL,BOOL))objc_msgSend)(transition,sel_registerName("setPausePlaybackWhenVideoActivates:"),NO);
    id event=((id(*)(id,SEL,id,id,id))objc_msgSend)(events,sel_registerName("eventWithTransition:fromView:firstResponder:"),transition,nil,responder);
    if (!RVCan(event,"send",@"v@:")) return NO;
    RVQueuePendingOperation=[operation copy];RVQueuePendingEntry=entry;RVQueuePendingResponder=responder;
    RVQueuePendingLayer=nil;
    RVQueuePendingPreviousResponse=RVQueuePresentationLastResponse;
    RVQueuePendingDeadline=NSProcessInfo.processInfo.systemUptime+10;RVQueuePendingGeneration++;
    RVQueueLocalFailure=@"native_watch_bootstrap_pending";atomic_fetch_add(&RVQueueBootstrapRequests,1);
    @try { RVQueueVoid(event,"send");RVQueueSchedulePending(); }
    @catch (__unused NSException *exception) { RVQueuePendingFailure(@"native_watch_bootstrap_dispatch_exception"); }
    return YES;
}
