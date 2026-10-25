// SPDX-License-Identifier: MIT
// Native queue presentation adapter, scoped to the locally owned native queue.
// 21.39.4 static evidence: YTIWatchNextResponse hasSingleColumnPlaylistPanelRenderer
// (0x1042f7390) checks the nested playlist wrapper oneof, independently of the
// generic yt_playlistPanelRenderer getter. The native factory (0x1042f754c)
// establishes that wrapper with setPlaylist:. See analysis/queue-native-ui/
// presentation/ui-response-annotated.txt. No entitlement or Premium state changes.
static atomic_uint RVQueueResponseCopies,RVQueueResponseDelegated,RVQueueResponseFailures;
static atomic_uint RVQueueResponseCopyFailures,RVQueueResponseShapeFailures;
static atomic_uint RVQueuePresentationHooks,RVQueuePresentationShown;
static atomic_uint RVQueuePresentationRefreshes,RVQueuePresentationRefreshFailures;
static _Thread_local NSUInteger RVQueuePresentationDepth;
static _Thread_local NSUInteger RVQueuePresentationFactoryDepth;
static id RVQueuePresentationLastResponse,RVQueuePresentationAdaptedResponse;
static __weak id RVQueuePresentationOwner;

static id RVQueuePresentationCopy(id object) {
    if (!object || ![object conformsToProtocol:@protocol(NSCopying)]) return nil;
    id copy=[object copy];
    // Every object on the modified branch must be distinct, even for unusual
    // proxy/copy implementations. Original responses remain untouched.
    if (!copy || copy==object || ![copy isKindOfClass:[object class]]) return nil;
    return copy;
}

static id RVQueueNativeInjectResponse(id response) {
    if (!RVCompatible || !NSThread.isMainThread || RVQueuePresentationDepth
        || RVQueueNativeResponseDepth || !RVQueueNativeActive()
        || !RVQueueClass(response,@"YTIWatchNextResponse")) return response;
    RVQueuePresentationDepth++;
    id result=response;
    @try {
        if (!RVQueueNativeAcceptResponse(response)) {
            atomic_fetch_add(&RVQueueResponseDelegated,1);return response;
        }
        id contents=RVObject(response,"contents");
        // Adapt only an existing phone Watch Next branch. Unknown layouts,
        // continuations and mixed renderer oneofs retain the original path.
        if (!RVQueueClass(contents,@"YTIWatchNextSupportedRenderers")
            || !RVQueuePresent(contents,@"singleColumnWatchNextResults")) {
            atomic_fetch_add(&RVQueueResponseDelegated,1);return response;
        }
        id single=RVObject(contents,"singleColumnWatchNextResults");
        if (!RVQueueClass(single,@"YTISingleColumnWatchNextResultsRenderer")) {
            atomic_fetch_add(&RVQueueResponseShapeFailures,1);return response;
        }
        NSString *wrapperName=@"YTISingleColumnWatchNextResultsRenderer_SingleColumnWatchNextPlaylistSupportedRenderers";
        id wrapper=nil;
        if (RVQueuePresent(single,@"playlist")) {
            wrapper=RVObject(single,"playlist");
            if (!RVQueueClass(wrapper,wrapperName) || !RVCan(wrapper,"rendererOneOfCase",@"i@:")
                || (RVInt(wrapper,"rendererOneOfCase")!=0 && RVInt(wrapper,"rendererOneOfCase")!=0x3049158)) {
                atomic_fetch_add(&RVQueueResponseDelegated,1);return response;
            }
        }
        id endpoint=RVObject(response,"currentVideoEndpoint");
        NSString *video=RVObject(RVObject(endpoint,"watchEndpoint"),"videoId");
        if (![video isKindOfClass:NSString.class] || !video.length)
            video=RVObject(RVQueueLocalController,"nowPlayingVideoID");
        id panel=RVQueueNativePanelForVideo(video);
        if (!RVQueueClass(panel,@"YTIPlaylistPanelRenderer")) {
            atomic_fetch_add(&RVQueueResponseShapeFailures,1);return response;
        }
        id responseCopy=RVQueuePresentationCopy(response);
        id contentsCopy=RVQueuePresentationCopy(contents);
        id singleCopy=RVQueuePresentationCopy(single);
        id wrapperCopy=wrapper ? RVQueuePresentationCopy(wrapper) : [NSClassFromString(wrapperName) new];
        if (!responseCopy || !contentsCopy || !singleCopy || !wrapperCopy) {
            atomic_fetch_add(&RVQueueResponseCopyFailures,1);return response;
        }
        // COW at every changed level preserves results, autoplay, tracking,
        // currentVideoEndpoint and all unrelated response fields. GPB setters
        // establish presence and the native playlist renderer oneof naturally.
        if (!RVQueueSet(wrapperCopy,@"setPlaylist:",panel)
            || !RVQueueSet(singleCopy,@"setPlaylist:",wrapperCopy)
            || !RVQueueSet(contentsCopy,@"setSingleColumnWatchNextResults:",singleCopy)
            || !RVQueueSet(responseCopy,@"setContents:",contentsCopy)
            || !RVQueueNumber(responseCopy,"setQueueMode:",4)
            || !RVBool(responseCopy,"hasSingleColumnPlaylistPanelRenderer")
            || RVObject(responseCopy,"singleColumnPlaylistPanelRenderer")!=panel) {
            atomic_fetch_add(&RVQueueResponseShapeFailures,1);return response;
        }
        // Mode 4 is the finite native-client queue mode consumed by the
        // default queue state provider, not a membership/entitlement value.
        result=responseCopy;atomic_fetch_add(&RVQueueResponseCopies,1);
    } @catch (__unused NSException *exception) {
        atomic_fetch_add(&RVQueueResponseFailures,1);result=response;
    } @finally { RVQueuePresentationDepth--; }
    return result;
}

static BOOL RVQueuePresentationOwns(id controller) {
    return RVCompatible && NSThread.isMainThread && controller
        && controller==RVQueueLocalPanelController && RVQueueNativeActive();
}
static BOOL RVQueuePresentationHasResponse(void) {
    return RVQueuePresentationOwner && RVQueuePresentationOwner==RVQueueLocalController
        && RVQueueClass(RVQueuePresentationLastResponse,@"YTIWatchNextResponse");
}
static void RVQueuePresentationCapture(id consumer,id response) {
    if (!RVCompatible || !NSThread.isMainThread || RVQueueNativeResponseDepth
        || !RVQueueClass(response,@"YTIWatchNextResponse")) return;
    id watch=RVQueueClass(consumer,@"YTWatchController") ? consumer : RVObject(consumer,"parentResponder");
    if (RVQueueClass(watch,@"YTWatchController")) RVQueueResolveNativeOwner(watch);
    if (RVQueueClass(watch,@"YTWatchController") && RVQueueLocalPanelController
        && RVObject(watch,"playlistPanelController")==RVQueueLocalPanelController
        && RVObject(RVQueueLocalPanelController,"queueController")==RVQueueLocalController)
        RVQueueLocalWatch=watch;
    if (consumer!=RVQueueLocalPanelController && consumer!=RVQueueLocalWatch
        && consumer!=RVObject(RVQueueLocalWatch,"watchViewController")) return;
    RVQueuePresentationLastResponse=response;RVQueuePresentationAdaptedResponse=nil;
    RVQueuePresentationOwner=RVQueueLocalController;
    // Always leave the native initialization/update stack before completing a
    // first queue action; its watch/panel/queue relationships are now captured.
    dispatch_async(dispatch_get_main_queue(),^{ RVQueueFinishPending(); });
}
static id RVQueuePresentationEngagementRenderer(void) {
    if (!RVQueuePresentationHasResponse() || RVQueuePresentationFactoryDepth) return nil;
    id response=RVQueueNativeResponseDepth ? RVQueuePresentationAdaptedResponse
        : RVQueueNativeInjectResponse(RVQueuePresentationLastResponse);
    if (!response || response==RVQueuePresentationLastResponse) return nil;
    id cls=NSClassFromString(@"YTPlaylistPanelController");
    const char *selector="playlistEngagementPanelSupportedRenderersWithWatchNextResponse:";
    if (!RVCan(cls,selector,@"@@:@")) return nil;
    id renderer=nil;RVQueuePresentationFactoryDepth++;
    @try { renderer=((id(*)(id,SEL,id))objc_msgSend)(cls,sel_registerName(selector),response); }
    @catch (__unused NSException *exception) { atomic_fetch_add(&RVQueueResponseFailures,1); }
    @finally { RVQueuePresentationFactoryDepth--; }
    return renderer;
}
static void RVQueueNativeRefreshPresentation(void) {
    if (!RVCompatible || !NSThread.isMainThread || !RVQueueNativeActive()
        || !RVQueuePresentationHasResponse() || RVQueueNativeResponseDepth) return;
    id response=RVQueueNativeInjectResponse(RVQueuePresentationLastResponse);
    if (response==RVQueuePresentationLastResponse) {
        atomic_fetch_add(&RVQueuePresentationRefreshFailures,1);return;
    }
    RVQueuePresentationAdaptedResponse=response;RVQueueNativeResponseDepth++;
    @try {
        // Register the model through the native responder provider. Do not call
        // setPlaylistPanelRenderer: here: it loads the playlist into the queue
        // and may trigger pending autoplay, including while playback is paused.
        if (!RVCan(RVQueueLocalWatch,"refreshEngagementPanelRenderersWithWatchNextResponse:",@"v@:@")) {
            atomic_fetch_add(&RVQueuePresentationRefreshFailures,1);return;
        }
        ((void(*)(id,SEL,id))objc_msgSend)(RVQueueLocalWatch,
            sel_registerName("refreshEngagementPanelRenderersWithWatchNextResponse:"),response);
        RVQueueVoid(RVQueueLocalPanelController,"refreshPlaylistCollectionViewController");
        RVQueueVoid(RVQueueLocalPanelController,"updateViewForCurrentVideo");
        RVQueueVoid(RVQueueLocalWatch,"showPlaylistMiniBarIfNeeded");
        atomic_fetch_add(&RVQueuePresentationRefreshes,1);
    } @catch (__unused NSException *exception) {
        atomic_fetch_add(&RVQueuePresentationRefreshFailures,1);
    } @finally { RVQueueNativeResponseDepth--; }
}

static NSDictionary *RVQueuePresentationReport(void) {
    return @{@"native_response_copies":@(atomic_load(&RVQueueResponseCopies)),
        @"native_response_delegated":@(atomic_load(&RVQueueResponseDelegated)),
        @"native_response_exceptions":@(atomic_load(&RVQueueResponseFailures)),
        @"native_response_copy_failures":@(atomic_load(&RVQueueResponseCopyFailures)),
        @"native_response_shape_failures":@(atomic_load(&RVQueueResponseShapeFailures)),
        @"native_presentation_hooks_installed":@(atomic_load(&RVQueuePresentationHooks)),
        @"native_owned_playlist_visibility":@(atomic_load(&RVQueuePresentationShown)),
        @"native_presentation_refreshes":@(atomic_load(&RVQueuePresentationRefreshes)),
        @"native_presentation_refresh_failures":@(atomic_load(&RVQueuePresentationRefreshFailures)),
        @"native_presentation_response_bound":@(RVQueuePresentationHasResponse())};
}

static void RVInstallQueuePresentation(void) {
    // Adapt before each original consumer, not from a generic renderer getter.
    // The outermost adapted consumer owns responseDepth; nested native calls
    // see the same coherent response without re-snapshotting the native model.
    NSArray *consumers=@[
        @[@"YTWatchController",@"prepareWithWatchNextResponse:"],
        @[@"YTWatchViewController",@"updateWithWatchNextResponse:"],
        @[@"YTPlaylistPanelController",@"updateWithWatchNextResponse:"],
        @[@"YTPlayerViewController",@"setWatchNextResponse:"]];
    for (NSArray *consumer in consumers) {
        BOOL installed=RVHook(consumer[0],consumer[1],@"v@:@",NO,^id(IMP original,SEL selector) {
            return ^(id object,id response) {
                RVQueuePresentationCapture(object,response);
                BOOL ownedConsumer=object==RVQueueLocalPanelController || object==RVQueueLocalWatch
                    || object==RVObject(RVQueueLocalWatch,"watchViewController");
                id adapted=ownedConsumer ? RVQueueNativeInjectResponse(response) : response;
                BOOL scoped=adapted!=response;
                if (scoped && RVQueuePresentationHasResponse()) RVQueuePresentationAdaptedResponse=adapted;
                if (scoped) RVQueueNativeResponseDepth++;
                @try { ((void(*)(id,SEL,id))original)(object,selector,adapted); }
                @finally {
                    if (scoped) RVQueueNativeResponseDepth--;
                    // Watch setup can construct the panel inside the original
                    // call. Capture again after those relationships exist.
                    RVQueuePresentationCapture(object,response);
                }
            };
        });
        if (installed) atomic_fetch_add(&RVQueuePresentationHooks,1);
    }
    for (NSString *name in @[@"playlistPanelRenderer",@"fullPlaylistPanelRenderer"]) {
        BOOL installed=RVHook(@"YTPlaylistPanelController",name,@"@@:",NO,^id(IMP original,SEL selector) {
            return ^id(id controller) {
                if (RVQueuePresentationOwns(controller)) {
                    id panel=RVQueueNativePanelForVideo(RVObject(RVQueueLocalController,"nowPlayingVideoID"));
                    if (panel) return panel;
                }
                return ((id(*)(id,SEL))original)(controller,selector);
            };
        });
        if (installed) atomic_fetch_add(&RVQueuePresentationHooks,1);
    }
    BOOL rendererInstalled=RVHook(@"YTPlaylistPanelController",@"playlistEngagementPanelSupportedRenderers",@"@@:",NO,^id(IMP original,SEL selector) {
        return ^id(id controller) {
            if (RVQueuePresentationOwns(controller)) {
                id renderer=RVQueuePresentationEngagementRenderer();
                if (renderer) return renderer;
            }
            return ((id(*)(id,SEL))original)(controller,selector);
        };
    });
    if (rendererInstalled) atomic_fetch_add(&RVQueuePresentationHooks,1);
    for (NSString *name in @[@"isQueue",@"hasQueueContents"]) {
        BOOL installed=RVHook(@"YTPlaylistPanelController",name,@"B@:",NO,^id(IMP original,SEL selector) {
            return ^BOOL(id controller) {
                BOOL nativeResult=((BOOL(*)(id,SEL))original)(controller,selector);
                return nativeResult || RVQueuePresentationOwns(controller);
            };
        });
        if (installed) atomic_fetch_add(&RVQueuePresentationHooks,1);
    }
    BOOL installed=RVHook(@"YTPlaylistPanelController",@"shouldShowPlaylist",@"B@:",NO,^id(IMP original,SEL selector) {
        return ^BOOL(id controller) {
            BOOL nativeResult=((BOOL(*)(id,SEL))original)(controller,selector);
            if (nativeResult || !RVCompatible || !NSThread.isMainThread
                || controller!=RVQueueLocalPanelController || !RVQueueNativeActive()) return nativeResult;
            id panel=RVObject(controller,"playlistPanelRenderer");
            // This override covers only our existing live native panel. It
            // cannot turn an absent renderer or unrelated playlist into UI.
            if (RVQueueClass(panel,@"YTIPlaylistPanelRenderer")
                && [RVObject(panel,"playlistId") isEqual:@"RVPortQueue"]) {
                atomic_fetch_add(&RVQueuePresentationShown,1);return YES;
            }
            return nativeResult;
        };
    });
    if (installed) atomic_fetch_add(&RVQueuePresentationHooks,1);
}
