// SPDX-License-Identifier: MIT
// Local queue for the MDX next/last commands captured from 21.39.4. Playback,
// insertion notifications, playlist cells and transitions remain native.
// Native panel integration corroborated by the MIT YTKACE reference retained
// in profiles/queue-reference-license.txt. No remote-discovery request is made
// for a recognized local operation; unrelated discovery commands are untouched.
#include "RVQueueParams.inc"
#include <limits.h>
static NSMutableArray *RVQueueLocalItems;
static __weak id RVQueueLocalController,RVQueueLocalPanelController,RVQueueLocalWatch;
static id RVQueueLocalAccount;
static BOOL RVQueueLocalAccountBound;
static NSString *RVQueueLocalFailure;
static atomic_uint RVQueueMDXSeen,RVQueueMDXHandled,RVQueueNativeInserted,RVQueuePanels;
static _Thread_local NSUInteger RVQueueLocalDepth;
static void RVQueueLocalBind(id controller,id account) {
    if (!controller) return;
    if (RVQueueLocalAccountBound && RVQueueLocalAccount!=account && ![RVQueueLocalAccount isEqual:account]) RVQueueLocalItems=nil;
    RVQueueLocalController=controller;RVQueueLocalAccount=account;RVQueueLocalAccountBound=YES;
}

static BOOL RVQueueValidVideo(id value) {
    if (![value isKindOfClass:NSString.class] || [value length]!=11) return NO;
    return [value rangeOfCharacterFromSet:[[NSCharacterSet characterSetWithCharactersInString:
        @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"] invertedSet]].location==NSNotFound;
}
static BOOL RVQueueNumber(id object,const char *setter,NSUInteger value) {
    SEL sel=sel_registerName(setter);
    if (RVCan(object,setter,@"v@:i") && value<=INT_MAX) { ((void(*)(id,SEL,int))objc_msgSend)(object,sel,(int)value);return YES; }
    if (RVCan(object,setter,@"v@:Q")) { ((void(*)(id,SEL,NSUInteger))objc_msgSend)(object,sel,value);return YES; }
    if (RVCan(object,setter,@"v@:q") && value<=NSIntegerMax) { ((void(*)(id,SEL,NSInteger))objc_msgSend)(object,sel,(NSInteger)value);return YES; }
    return NO;
}
static void RVQueueVoid(id object,const char *selector) {
    if (RVCan(object,selector,@"v@:")) ((void(*)(id,SEL))objc_msgSend)(object,sel_registerName(selector));
}
static id RVQueueFormatted(NSString *text) {
    id cls=NSClassFromString(@"YTIFormattedString");
    if (!RVCan(cls,"formattedStringWithString:",@"@@:@")) return nil;
    return ((id(*)(id,SEL,id))objc_msgSend)(cls,sel_registerName("formattedStringWithString:"),text);
}
static id RVQueueMakeVideo(NSString *video,id entry) {
    if (!RVQueueValidVideo(video)) return nil;
    id renderer=[NSClassFromString(@"YTIPlaylistPanelVideoRenderer") new];
    id watch=[NSClassFromString(@"YTIWatchEndpoint") new],command=[NSClassFromString(@"YTICommand") new];
    if (!RVQueueSet(watch,@"setVideoId:",video) || !RVQueueSet(command,@"setWatchEndpoint:",watch)
        || !RVQueueSet(renderer,@"setVideoId:",video) || !RVQueueSet(renderer,@"setNavigationEndpoint:",command)) return nil;
    RVQueueSet(renderer,@"setPlaylistSetVideoId:",video);
    id title=nil,thumbnail=nil;
    if ([RVObject(entry,"videoId") isEqual:video]) { title=RVObject(entry,"title");thumbnail=RVObject(entry,"thumbnail"); }
    if ([title isKindOfClass:NSString.class]) title=RVQueueFormatted(title);
    if (!title) title=RVQueueFormatted(video);
    RVQueueSet(renderer,@"setTitle:",title);if (thumbnail) RVQueueSet(renderer,@"setThumbnail:",thumbnail);
    return renderer;
}
static NSString *RVQueueLocalCurrent(void) {
    id current=RVObject(RVCurrentPlayback,"contentVideoID");
    if (!RVQueueValidVideo(current)) current=RVObject(RVQueueLocalController,"nowPlayingVideoID");
    return RVQueueValidVideo(current) ? current : nil;
}
static NSUInteger RVQueueLocalIndex(NSString *video) {
    if (!video) return NSNotFound;
    for (NSUInteger index=0;index<RVQueueLocalItems.count;index++)
        if ([RVObject(RVQueueLocalItems[index],"videoId") isEqual:video]) return index;
    return NSNotFound;
}
static void RVQueueLocalSeed(void) {
    if (!RVQueueLocalItems) RVQueueLocalItems=[NSMutableArray array];
    if (RVQueueLocalItems.count) return;
    NSArray *items=RVObject(RVQueueLocalController,"playbackQueueItems");
    if ([items isKindOfClass:NSArray.class] && items.count<=500) {
        for (id item in items) {
            id renderer=RVObject(item,"videoRenderer");
            if (RVQueueValidVideo(RVObject(renderer,"videoId")) && [renderer conformsToProtocol:@protocol(NSCopying)])
                [RVQueueLocalItems addObject:[renderer copy]];
        }
    }
    NSString *current=RVQueueLocalCurrent();
    if (current && RVQueueLocalIndex(current)==NSNotFound) {
        id renderer=RVQueueMakeVideo(current,nil);if (renderer) [RVQueueLocalItems insertObject:renderer atIndex:0];
    }
}
static id RVQueueLocalPanel(NSString *current) {
    if (!RVQueueLocalItems.count || RVQueueLocalItems.count>500) return nil;
    id panel=[NSClassFromString(@"YTIPlaylistPanelRenderer") new];
    NSMutableArray *contents=RVObject(panel,"contentsArray");
    if (![contents isKindOfClass:NSMutableArray.class]) return nil;
    NSUInteger selected=RVQueueLocalIndex(current);if (selected==NSNotFound) selected=0;
    for (NSUInteger index=0;index<RVQueueLocalItems.count;index++) {
        id wrapper=[NSClassFromString(@"YTIPlaylistPanelRenderer_PlaylistPanelVideoSupportedRenderers") new];
        id video=[RVQueueLocalItems[index] copy];
        if (RVCan(video,"setSelected:",@"v@:B")) ((void(*)(id,SEL,BOOL))objc_msgSend)(video,sel_registerName("setSelected:"),index==selected);
        RVQueueSet(video,@"setIndexText:",RVQueueFormatted([NSString stringWithFormat:@"%lu",(unsigned long)index+1]));
        if (!RVQueueSet(wrapper,@"setPlaylistPanelVideoRenderer:",video)) return nil;
        [contents addObject:wrapper];
    }
    if (!RVQueueSet(panel,@"setPlaylistId:",@"RVPortQueue") || !RVQueueNumber(panel,"setTotalVideos:",contents.count)
        || !RVQueueNumber(panel,"setCurrentIndex:",selected) || !RVQueueNumber(panel,"setLocalCurrentIndex:",selected)) return nil;
    RVQueueSet(panel,@"setTitle:",@"Queue");RVQueueSet(panel,@"setTitleText:",RVQueueFormatted(@"Queue"));
    if (RVCan(panel,"setIsEditable:",@"v@:B")) ((void(*)(id,SEL,BOOL))objc_msgSend)(panel,sel_registerName("setIsEditable:"),YES);
    return panel;
}
static void RVQueueLocalRefreshPanel(void) {
    if (!RVCan(RVQueueLocalController,"queueCount",@"Q@:")
        || ((NSUInteger(*)(id,SEL))objc_msgSend)(RVQueueLocalController,sel_registerName("queueCount"))!=RVQueueLocalItems.count) return;
    id panel=RVQueueLocalPanel(RVQueueLocalCurrent());
    if (!panel || !RVQueueSet(RVQueueLocalPanelController,@"setPlaylistPanelRenderer:",panel)) return;
    RVQueueVoid(RVQueueLocalPanelController,"refreshPlaylistCollectionViewController");
    RVQueueVoid(RVQueueLocalPanelController,"updateViewForCurrentVideo");
    RVQueueVoid(RVQueueLocalWatch,"showPlaylistMiniBarIfNeeded");
}
static NSDictionary *RVQueueMDXOperation(id command) {
    if (!RVQueueClass(command,@"YTICommand") || !RVCan(command,"getExtension:",@"@@:@")) return nil;
    NSArray *extensions=RVObject(command,"extensionsCurrentlySet");
    if (![extensions isKindOfClass:NSArray.class] || extensions.count>32) return nil;
    NSDictionary *result=nil;BOOL seenMDX=NO;
    for (id extension in extensions) {
        id payload=((id(*)(id,SEL,id))objc_msgSend)(command,sel_registerName("getExtension:"),extension);
        if (!RVQueueClass(payload,@"YTIRequestMdxDeviceDiscoveryCommand")) {
            if (!RVQueueClass(payload,@"YTINavigationEndpointInteractionLoggingExtension")
                && !RVQueueClass(payload,@"YTIServiceEndpointClientScreenExtension")) return nil;
            continue;
        }
        if (seenMDX) return nil;seenMDX=YES;
        NSArray *strategies=RVObject(payload,"requestPollingStrategiesArray");
        // Exactly one queue strategy was captured for each next/last action.
        if (![strategies isKindOfClass:NSArray.class] || strategies.count!=1) return nil;
        id strategy=strategies[0];
        if (!RVQueueClass(strategy,@"YTIMdxDeviceDiscoveryPollingStrategy") || !RVCan(strategy,"discoveryStrategy",@"i@:")
            || RVInt(strategy,"discoveryStrategy")!=4) return nil;
        result=RVQueueDiscoveryParams(RVObject(strategy,"discoveryParams"));
    }
    return result;
}
static BOOL RVQueueLocalHandle(id command,id entry) {
    if (!RVCompatible || !NSThread.isMainThread || RVQueueLocalDepth) return NO;
    NSDictionary *operation=RVQueueMDXOperation(command);if (!operation) return NO;
    atomic_fetch_add(&RVQueueMDXSeen,1);RVQueueLocalDepth++;
    BOOL handled=NO;NSArray *beforeModel=[RVQueueLocalItems copy];
    @try {
        // A casting session keeps the original discovery semantics.
        if (RVBool(RVQueueLocalController,"isMDXRemoteQueue")) { RVQueueLocalFailure=@"casting_queue";return NO; }
        RVQueueLocalSeed();
        if (RVQueueLocalItems.count>=500) { RVQueueLocalFailure=@"queue_limit";return NO; }
        id video=RVQueueMakeVideo(operation[@"video_id"],entry);
        if (!video) { RVQueueLocalFailure=@"renderer_adapter_unavailable";return NO; }
        NSUInteger current=RVQueueLocalIndex(RVQueueLocalCurrent());
        NSUInteger index=[operation[@"play_next"] boolValue] && current!=NSNotFound ? current+1 : RVQueueLocalItems.count;
        [RVQueueLocalItems insertObject:video atIndex:index];
        id panel=RVQueueLocalPanel(RVQueueLocalCurrent());
        if (!panel) { RVQueueLocalFailure=@"panel_adapter_unavailable";return NO; }
        // From here the validated local operation is committed. Even if a
        // native observer throws, never replay it through remote discovery.
        handled=YES;atomic_fetch_add(&RVQueueMDXHandled,1);
        // Use native notifications and queue ownership, not a parallel player.
        id cls=NSClassFromString(@"YTQueueItem");
        if (RVCan(cls,"queueItemWithPlaylistPanelVideoRenderer:",@"@@:@")
            && RVCan(RVQueueLocalController,"insertQueueItems:atIndex:",@"v@:@Q")
            && RVCan(RVQueueLocalController,"queueCount",@"Q@:")) {
            NSUInteger before=((NSUInteger(*)(id,SEL))objc_msgSend)(RVQueueLocalController,sel_registerName("queueCount"));
            BOOL hydrate=before==0 && RVQueueValidVideo(RVQueueLocalCurrent()) && RVQueueLocalItems.count>1;
            NSArray *renderers=hydrate ? RVQueueLocalItems : @[video];
            NSUInteger insertion=hydrate ? 0 : index;
            if (before+renderers.count==RVQueueLocalItems.count && insertion<=before) {
                NSMutableArray *nativeItems=[NSMutableArray array];
                for (id renderer in renderers) {
                    id item=((id(*)(id,SEL,id))objc_msgSend)(cls,sel_registerName("queueItemWithPlaylistPanelVideoRenderer:"),renderer);
                    if (item) [nativeItems addObject:item];
                }
                if (nativeItems.count==renderers.count) {
                    RVQueueVoid(RVQueueLocalController,"recordUserQueueModification");
                    ((void(*)(id,SEL,id,NSUInteger))objc_msgSend)(RVQueueLocalController,sel_registerName("insertQueueItems:atIndex:"),nativeItems,insertion);
                    NSUInteger after=((NSUInteger(*)(id,SEL))objc_msgSend)(RVQueueLocalController,sel_registerName("queueCount"));
                    if (after==before+nativeItems.count) {
                        atomic_fetch_add(&RVQueueNativeInserted,1);
                        if (hydrate) RVQueueNumber(RVQueueLocalController,"setNowPlayingIndex:",RVQueueLocalIndex(RVQueueLocalCurrent()));
                    }
                }
            }
        }
        RVQueueLocalRefreshPanel();RVQueueLocalFailure=nil;
    } @catch (__unused NSException *exception) { RVQueueLocalFailure=@"native_queue_exception"; }
    @finally { if (!handled) RVQueueLocalItems=beforeModel ? [beforeModel mutableCopy] : nil;RVQueueLocalDepth--; }
    return handled;
}
static id RVQueueLocalParsedPanel(id original,id response) {
    if (!RVCompatible || !NSThread.isMainThread || !RVQueueLocalItems.count) return original;
    // Respect a real server playlist. Inject only a missing/local queue panel.
    NSString *playlist=RVObject(original,"playlistId");
    if ([playlist isKindOfClass:NSString.class] && playlist.length && ![playlist isEqual:@"RVPortQueue"]) return original;
    NSString *current=RVObject(RVObject(RVObject(response,"currentVideoEndpoint"),"watchEndpoint"),"videoId");
    if (!RVQueueValidVideo(current)) current=RVQueueLocalCurrent();
    if (current && RVQueueLocalIndex(current)==NSNotFound) {
        id video=RVQueueMakeVideo(current,nil);if (video && RVQueueLocalItems.count<500) [RVQueueLocalItems insertObject:video atIndex:0];
    }
    id panel=RVQueueLocalPanel(current);if (panel) atomic_fetch_add(&RVQueuePanels,1);
    return panel ?: original;
}
static NSDictionary *RVQueueLocalReport(void) {
    return @{@"mdx_queue_operations_recognized":@(atomic_load(&RVQueueMDXSeen)),@"mdx_queue_operations_handled":@(atomic_load(&RVQueueMDXHandled)),
        @"native_insertions_confirmed":@(atomic_load(&RVQueueNativeInserted)),@"local_model_count":@(RVQueueLocalItems.count),
        @"local_panels_returned":@(atomic_load(&RVQueuePanels)),@"native_queue_bound":@(RVQueueLocalController!=nil),
        @"native_panel_bound":@(RVQueueLocalPanelController!=nil),@"last_blocker":RVQueueLocalFailure ?: @"none"};
}
static void RVQueueLocalSync(id controller) {
    if (!NSThread.isMainThread || controller!=RVQueueLocalController || RVQueueLocalDepth || !RVQueueLocalItems.count) return;
    NSArray *items=RVObject(controller,"playbackQueueItems");
    if (![items isKindOfClass:NSArray.class] || items.count>500) return;
    NSMutableArray *updated=[NSMutableArray array];
    for (id item in items) {
        id renderer=RVObject(item,"videoRenderer");
        if (!RVQueueValidVideo(RVObject(renderer,"videoId")) || ![renderer conformsToProtocol:@protocol(NSCopying)]) return;
        [updated addObject:[renderer copy]];
    }
    RVQueueLocalItems=updated;
}
static void RVInstallQueueLocal(void) {
    RVHook(@"MDXRequestDeviceDiscoveryCommandHandlerImpl",@"executeWithCommand:entry:fromView:sender:",@"v@:@@@@",NO,^id(IMP original,SEL sel) {
        return ^(id handler,id command,id entry,id view,id sender) {
            if (!RVQueueLocalHandle(command,entry)) ((void(*)(id,SEL,id,id,id,id))original)(handler,sel,command,entry,view,sender);
        };
    });
    for (NSString *name in @[@"YTCommandRouter",@"YTAccountScopedCommandRouter"]) {
        RVHook(name,@"handleCommand:entry:fromView:sender:",@"B@:@@@@",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id router,id command,id entry,id view,id sender) {
                return RVQueueLocalHandle(command,entry) ? YES : ((BOOL(*)(id,SEL,id,id,id,id))original)(router,sel,command,entry,view,sender);
            };
        });
        RVHook(name,@"handleCommand:entry:fromView:sender:completionBlock:",@"B@:@@@@@?",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id router,id command,id entry,id view,id sender,id completion) {
                if (!RVQueueLocalHandle(command,entry)) return ((BOOL(*)(id,SEL,id,id,id,id,id))original)(router,sel,command,entry,view,sender,completion);
                // Same two-object completion ABI used by the native queue handler.
                if (completion) ((void(^)(id,id))completion)(nil,nil);return YES;
            };
        });
    }
    RVHook(@"YTQueueController",@"initWithAccountID:parentResponder:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id account,id responder) { id result=((id(*)(id,SEL,id,id))original)(object,sel,account,responder);RVQueueLocalBind(result,account);return result; };
    });
    RVHook(@"YTPlaylistPanelController",@"initWithAccountID:parentResponder:queueController:",@"@@:@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id account,id responder,id controller) { id result=((id(*)(id,SEL,id,id,id))original)(object,sel,account,responder,controller);RVQueueLocalPanelController=result;RVQueueLocalBind(controller,account);return result; };
    });
    RVHook(@"YTPlaylistPanelController",@"initWithAccountID:parentResponder:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id account,id responder) { id result=((id(*)(id,SEL,id,id))original)(object,sel,account,responder);RVQueueLocalPanelController=result;return result; };
    });
    RVHook(@"YTDefaultQueueWatchNextResponseParser",@"playlistPanelRendererFromWatchNextResponse:",@"@@:@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id response) { return RVQueueLocalParsedPanel(((id(*)(id,SEL,id))original)(object,sel,response),response); };
    });
    RVHook(@"YTIWatchNextResponse",@"yt_playlistPanelRenderer",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id response) { return RVQueueLocalParsedPanel(((id(*)(id,SEL))original)(response,sel),response); };
    });
    RVHook(@"YTWatchController",@"prepareWithWatchNextResponse:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id watch,id response) { RVQueueLocalWatch=watch;((void(*)(id,SEL,id))original)(watch,sel,response); };
    });
    for (NSString *name in @[@"isQueue",@"hasQueueContents"])
        RVHook(@"YTPlaylistPanelController",name,@"B@:",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id object) {
                if ([RVObject(RVObject(object,"playlistPanelRenderer"),"playlistId") isEqual:@"RVPortQueue"] && RVQueueLocalItems.count) return YES;
                return ((BOOL(*)(id,SEL))original)(object,sel);
            };
        });
    RVHook(@"YTQueueController",@"moveItemAtIndexPath:toIndexPath:userTriggered:",@"v@:@@B",NO,^id(IMP original,SEL sel) {
        return ^(id object,id from,id to,BOOL triggered) { ((void(*)(id,SEL,id,id,BOOL))original)(object,sel,from,to,triggered);RVQueueLocalSync(object); };
    });
    RVHook(@"YTQueueController",@"removeItemAtIndexPath:userTriggered:",@"v@:@B",NO,^id(IMP original,SEL sel) {
        return ^(id object,id path,BOOL triggered) { ((void(*)(id,SEL,id,BOOL))original)(object,sel,path,triggered);RVQueueLocalSync(object); };
    });
    RVHook(@"YTQueueController",@"removeQueueItemAtIndex:",@"v@:Q",NO,^id(IMP original,SEL sel) {
        return ^(id object,NSUInteger index) { ((void(*)(id,SEL,NSUInteger))original)(object,sel,index);RVQueueLocalSync(object); };
    });
    RVHook(@"YTQueueController",@"clearQueue",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) { ((void(*)(id,SEL))original)(object,sel);if (object==RVQueueLocalController && !RVQueueLocalDepth) RVQueueLocalItems=nil; };
    });
}
