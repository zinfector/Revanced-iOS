// SPDX-License-Identifier: MIT
// Route the captured MDX next/last operations to YouTube's native queue owner.
// No parallel item array: ordering, selection, removal and deduplication belong
// to YTQueueController.handleQueueModification: (21.39.4: 0x101748360).
#include "RVQueueParams.inc"
#include <limits.h>
static __weak id RVQueueLocalController,RVQueueLocalPanelController,RVQueueLocalWatch;
static id RVQueueLocalAccount;
static BOOL RVQueueLocalAccountBound,RVQueueNativeOwned;
static NSString *RVQueueNativeSourcePlaylist,*RVQueueLocalFailure;
static _Thread_local NSUInteger RVQueueLocalDepth,RVQueueNativeResponseDepth;
static atomic_uint RVQueueMDXSeen,RVQueueMDXHandled,RVQueueNativeConfirmed,RVQueueNativeUnconfirmed;
static atomic_uint RVQueueDecodePayloads,RVQueueDecodeSuccess,RVQueueDecodeStage;
static atomic_uint RVQueueMenuTokens,RVQueueMenuActions,RVQueueMenuActionTaps,RVQueueMenuActionFailures,RVQueueOffMain;

static BOOL RVQueueValidVideo(id value) {
    if (![value isKindOfClass:NSString.class] || [value length]!=11) return NO;
    return [value rangeOfCharacterFromSet:[[NSCharacterSet characterSetWithCharactersInString:
        @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"] invertedSet]].location==NSNotFound;
}
static BOOL RVQueueNumber(id object,const char *setter,NSUInteger value) {
    SEL sel=sel_registerName(setter);
    if (RVCan(object,setter,@"v@:i") && value<=INT_MAX) { ((void(*)(id,SEL,int))objc_msgSend)(object,sel,(int)value);return YES; }
    if (RVCan(object,setter,@"v@:I") && value<=UINT_MAX) { ((void(*)(id,SEL,unsigned int))objc_msgSend)(object,sel,(unsigned int)value);return YES; }
    if (RVCan(object,setter,@"v@:Q")) { ((void(*)(id,SEL,NSUInteger))objc_msgSend)(object,sel,value);return YES; }
    if (RVCan(object,setter,@"v@:q") && value<=NSIntegerMax) { ((void(*)(id,SEL,NSInteger))objc_msgSend)(object,sel,value);return YES; }
    return NO;
}
static void RVQueueVoid(id object,const char *selector) {
    if (RVCan(object,selector,@"v@:")) ((void(*)(id,SEL))objc_msgSend)(object,sel_registerName(selector));
}
static NSUInteger RVQueueCount(void) {
    if (!RVCan(RVQueueLocalController,"queueCount",@"Q@:")) return 0;
    return ((NSUInteger(*)(id,SEL))objc_msgSend)(RVQueueLocalController,sel_registerName("queueCount"));
}
static NSArray *RVQueueNativeItems(void) {
    id items=RVObject(RVQueueLocalController,"playbackQueueItems");
    return [items isKindOfClass:NSArray.class] && [items count]<=500 ? items : nil;
}
static BOOL RVQueueNativeActive(void) { return RVQueueNativeOwned && RVQueueLocalController && RVQueueNativeItems().count>0; }
static void RVQueueLocalBind(id controller,id account) {
    if (!controller) return;
    if ((RVQueueLocalController && RVQueueLocalController!=controller)
        || (RVQueueLocalAccountBound && RVQueueLocalAccount!=account && ![RVQueueLocalAccount isEqual:account])) {
        RVQueueNativeOwned=NO;RVQueueNativeSourcePlaylist=nil;
    }
    RVQueueLocalController=controller;RVQueueLocalAccount=account;RVQueueLocalAccountBound=YES;
}
static NSString *RVQueueLocalCurrent(void) {
    id video=RVObject(RVCurrentPlayback,"contentVideoID");
    if (!RVQueueValidVideo(video)) video=RVObject(RVQueueLocalController,"nowPlayingVideoID");
    return RVQueueValidVideo(video) ? video : nil;
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
static NSUInteger RVQueueNativeIndex(NSString *video) {
    if (!video) return NSNotFound;NSArray *items=RVQueueNativeItems();
    for (NSUInteger index=0;index<items.count;index++)
        if ([RVObject(RVObject(items[index],"videoRenderer"),"videoId") isEqual:video]) return index;
    return NSNotFound;
}
static id RVQueueNativePanelForVideo(NSString *current) {
    if (!RVQueueNativeActive()) return nil;
    NSArray *items=[RVQueueNativeItems() copy];
    id panel=[NSClassFromString(@"YTIPlaylistPanelRenderer") new];
    NSMutableArray *contents=RVObject(panel,"contentsArray");
    if (![contents isKindOfClass:NSMutableArray.class]) return nil;
    NSUInteger selected=RVQueueNativeIndex(current);
    if (selected==NSNotFound && RVCan(RVQueueLocalController,"nowPlayingIndex",@"Q@:"))
        selected=((NSUInteger(*)(id,SEL))objc_msgSend)(RVQueueLocalController,sel_registerName("nowPlayingIndex"));
    if (selected>=items.count) selected=0;
    for (NSUInteger index=0;index<items.count;index++) {
        id original=RVObject(items[index],"videoRenderer");
        if (!RVQueueValidVideo(RVObject(original,"videoId")) || ![original conformsToProtocol:@protocol(NSCopying)]) return nil;
        id video=[original copy];
        if (RVCan(video,"setSelected:",@"v@:B")) ((void(*)(id,SEL,BOOL))objc_msgSend)(video,sel_registerName("setSelected:"),index==selected);
        RVQueueSet(video,@"setIndexText:",RVQueueFormatted([NSString stringWithFormat:@"%lu",(unsigned long)index+1]));
        id wrapper=[NSClassFromString(@"YTIPlaylistPanelRenderer_PlaylistPanelVideoSupportedRenderers") new];
        if (!RVQueueSet(wrapper,@"setPlaylistPanelVideoRenderer:",video)) return nil;
        [contents addObject:wrapper];
    }
    if (!RVQueueSet(panel,@"setPlaylistId:",@"RVPortQueue") || !RVQueueNumber(panel,"setTotalVideos:",items.count)
        || !RVQueueNumber(panel,"setCurrentIndex:",selected) || !RVQueueNumber(panel,"setLocalCurrentIndex:",selected)) return nil;
    RVQueueSet(panel,@"setTitle:",@"Queue");RVQueueSet(panel,@"setTitleText:",RVQueueFormatted(@"Queue"));
    // Native section move/remove callbacks below own local editing; no server
    // edit request is sent with this synthetic playlist identifier.
    if (RVCan(panel,"setIsEditable:",@"v@:B")) ((void(*)(id,SEL,BOOL))objc_msgSend)(panel,sel_registerName("setIsEditable:"),YES);
    return panel;
}
static BOOL RVQueueNativeAcceptResponse(id response) {
    if (!RVQueueNativeActive() || !RVQueueClass(response,@"YTIWatchNextResponse")) return NO;
    id existing=RVObject(response,"singleColumnPlaylistPanelRenderer");
    NSString *playlist=RVObject(existing,"playlistId");
    if ([playlist isKindOfClass:NSString.class] && playlist.length && ![playlist isEqual:@"RVPortQueue"]
        && ![playlist isEqual:RVQueueNativeSourcePlaylist]) return NO;
    id endpoint=RVObject(response,"currentVideoEndpoint");
    NSString *video=RVObject(RVObject(endpoint,"watchEndpoint"),"videoId");
    if (!RVQueueValidVideo(video)) video=RVQueueLocalCurrent();
    return !video || RVQueueNativeIndex(video)!=NSNotFound;
}

#include "RVQueuePresentation.inc"

static NSDictionary *RVQueueMDXOperation(id command) {
    if (!RVQueueClass(command,@"YTICommand") || !RVCan(command,"getExtension:",@"@@:@")) return nil;
    NSArray *extensions=RVObject(command,"extensionsCurrentlySet");
    if (![extensions isKindOfClass:NSArray.class] || extensions.count>32) return nil;
    NSDictionary *result=nil;BOOL seen=NO;
    for (id extension in extensions) {
        id payload=((id(*)(id,SEL,id))objc_msgSend)(command,sel_registerName("getExtension:"),extension);
        if (!RVQueueClass(payload,@"YTIRequestMdxDeviceDiscoveryCommand")) {
            if (!RVQueueClass(payload,@"YTINavigationEndpointInteractionLoggingExtension")
                && !RVQueueClass(payload,@"YTIServiceEndpointClientScreenExtension")) return nil;
            continue;
        }
        if (seen) return nil;seen=YES;
        atomic_fetch_add(&RVQueueDecodePayloads,1);atomic_store(&RVQueueDecodeStage,1);
        NSArray *strategies=RVObject(payload,"requestPollingStrategiesArray");
        if (![strategies isKindOfClass:NSArray.class] || strategies.count!=1) { atomic_store(&RVQueueDecodeStage,11);return nil; }
        id strategy=strategies[0];
        if (!RVQueueClass(strategy,@"YTIMdxDeviceDiscoveryPollingStrategy") || !RVCan(strategy,"discoveryStrategy",@"i@:")
            || RVInt(strategy,"discoveryStrategy")!=4) { atomic_store(&RVQueueDecodeStage,12);return nil; }
        result=RVQueueDiscoveryParams(RVObject(strategy,"discoveryParams"));
        atomic_store(&RVQueueDecodeStage,result ? 2 : 13);
        if (result) atomic_fetch_add(&RVQueueDecodeSuccess,1);
    }
    return result;
}
static BOOL RVQueueNativePerform(NSDictionary *operation,id entry) {
    if (!RVCompatible || !NSThread.isMainThread || RVQueueLocalDepth) return NO;
    if (!RVQueueValidVideo(operation[@"video_id"]) || ![operation[@"play_next"] isKindOfClass:NSNumber.class]) return NO;
    atomic_fetch_add(&RVQueueMDXSeen,1);
    if (!RVQueueLocalController) { RVQueueLocalFailure=@"native_queue_unavailable";return NO; }
    if (RVBool(RVQueueLocalController,"isMDXRemoteQueue")) { RVQueueLocalFailure=@"casting_queue";return NO; }
    id itemClass=NSClassFromString(@"YTQueueItem"),dataClass=NSClassFromString(@"YTQueueModificationNotificationData");
    const char *factory="addToQueueNotificationWithQueueItems:atInsertPosition:predecessorSetVideoID:responseForLogging:";
    if (!RVCan(RVQueueLocalController,"handleQueueModification:",@"v@:@") || !RVCan(RVQueueLocalController,"queueCount",@"Q@:")
        || !RVCan(itemClass,"queueItemWithPlaylistPanelVideoRenderer:",@"@@:@") || !RVCan(dataClass,factory,@"@@:@i@@")) {
        RVQueueLocalFailure=@"native_mutation_adapter_unavailable";return NO;
    }
    NSUInteger before=RVQueueCount();if (before>=500) { RVQueueLocalFailure=@"queue_limit";return NO; }
    NSString *current=RVQueueLocalCurrent();
    if (before && [operation[@"play_next"] boolValue] && (!RVCan(RVQueueLocalController,"nowPlayingIndex",@"Q@:")
        || ((NSUInteger(*)(id,SEL))objc_msgSend)(RVQueueLocalController,sel_registerName("nowPlayingIndex"))>=before)) {
        RVQueueLocalFailure=@"native_current_index_unavailable";return NO;
    }
    RVQueueLocalDepth++;BOOL invoked=NO,previousOwned=RVQueueNativeOwned;NSUInteger expected=before+1;
    @try {
        id video=RVQueueMakeVideo(operation[@"video_id"],entry);
        id item=video ? ((id(*)(id,SEL,id))objc_msgSend)(itemClass,sel_registerName("queueItemWithPlaylistPanelVideoRenderer:"),video) : nil;
        if (!item) { RVQueueLocalFailure=@"native_item_unavailable";return NO; }
        int position=[operation[@"play_next"] boolValue] ? 1 : 2;
        if (!before && !current) position=2; // An empty queue has the same next/last insertion point.
        id data=((id(*)(id,SEL,id,int,id,id))objc_msgSend)(dataClass,sel_registerName(factory),@[item],position,nil,nil);
        if (!data) { RVQueueLocalFailure=@"native_notification_unavailable";return NO; }
        if (!before && current) {
            if (!RVCan(RVQueueLocalController,"insertQueueItems:atIndex:",@"v@:@Q")
                || !RVCan(RVQueueLocalController,"setNowPlayingIndex:",@"v@:Q")) { RVQueueLocalFailure=@"native_seed_adapter_unavailable";return NO; }
            id seedVideo=RVQueueMakeVideo(current,nil);
            id seed=seedVideo ? ((id(*)(id,SEL,id))objc_msgSend)(itemClass,sel_registerName("queueItemWithPlaylistPanelVideoRenderer:"),seedVideo) : nil;
            if (!seed) { RVQueueLocalFailure=@"native_seed_item_unavailable";return NO; }
            if (!previousOwned) RVQueueNativeSourcePlaylist=[RVObject(RVObject(RVQueueLocalController,"playlistPanel"),"playlistId") copy];
            RVQueueNativeOwned=YES;invoked=YES;
            ((void(*)(id,SEL,id,NSUInteger))objc_msgSend)(RVQueueLocalController,sel_registerName("insertQueueItems:atIndex:"),@[seed],0);
            if (RVQueueCount()!=1) { RVQueueLocalFailure=@"native_seed_not_inserted";atomic_fetch_add(&RVQueueNativeUnconfirmed,1);return YES; }
            RVQueueNumber(RVQueueLocalController,"setNowPlayingIndex:",0);expected++;
        }
        if (!previousOwned) RVQueueNativeSourcePlaylist=[RVObject(RVObject(RVQueueLocalController,"playlistPanel"),"playlistId") copy];
        RVQueueNativeOwned=YES;invoked=YES;
        // Native registration uses NSNotification.object == typed data. Invoke
        // the active controller directly; do not broadcast to other accounts.
        NSNotification *notification=[NSNotification notificationWithName:@"YTQueueModificationNotification" object:data];
        ((void(*)(id,SEL,id))objc_msgSend)(RVQueueLocalController,sel_registerName("handleQueueModification:"),notification);
        if (RVQueueCount()==expected && RVQueueNativeIndex(operation[@"video_id"])!=NSNotFound) {
            atomic_fetch_add(&RVQueueNativeConfirmed,1);RVQueueLocalFailure=nil;
        } else { atomic_fetch_add(&RVQueueNativeUnconfirmed,1);RVQueueLocalFailure=@"native_mutation_not_confirmed"; }
        atomic_fetch_add(&RVQueueMDXHandled,1);
        RVQueueNativeRefreshPresentation();
    } @catch (__unused NSException *exception) {
        RVQueueLocalFailure=@"native_queue_exception";
        if (invoked) atomic_fetch_add(&RVQueueNativeUnconfirmed,1);
    } @finally { if (!invoked) RVQueueNativeOwned=previousOwned;RVQueueLocalDepth--; }
    return invoked; // Never replay a possibly committed native operation.
}
static BOOL RVQueueLocalHandle(id command,id entry) {
    if (!RVCompatible || RVQueueLocalDepth) return NO;
    NSDictionary *operation=RVQueueMDXOperation(command);if (!operation) return NO;
    if (!NSThread.isMainThread) {
        atomic_fetch_add(&RVQueueOffMain,1);
        return NO;
    }
    return RVQueueNativePerform(operation,entry);
}
static id RVQueueDirectMenuAction(id action,id renderer,id entry) {
    if (!RVCompatible || !NSThread.isMainThread || !RVQueueRole(renderer)
        || !RVQueueLocalController || RVBool(RVQueueLocalController,"isMDXRemoteQueue")) return action;
    id command=RVObject(renderer,"serviceEndpoint");
    if (!command) command=RVObject(renderer,"navigationEndpoint");
    NSDictionary *operation=RVQueueMDXOperation(command);if (!operation) return action;
    atomic_fetch_add(&RVQueueMenuTokens,1);
    if ([operation[@"play_next"] boolValue]!=[RVQueueRole(renderer) isEqual:@1]
        || !RVQueueClass(action,@"YTActionSheetAction") || !RVCan(action,"setHandler:",@"v@:@?")
        || !RVCan(action,"handler",@"@?@:")) { atomic_fetch_add(&RVQueueMenuActionFailures,1);return action; }
    void (^nativeHandler)(void)=((id(*)(id,SEL))objc_msgSend)(action,sel_registerName("handler"));
    void (^handler)(void)=^{
        void (^perform)(void)=^{
            if (!RVCompatible || RVBool(RVQueueLocalController,"isMDXRemoteQueue")) { if (nativeHandler) nativeHandler();return; }
            atomic_fetch_add(&RVQueueMenuActionTaps,1);
            if (!RVQueueNativePerform(operation,entry)) atomic_fetch_add(&RVQueueMenuActionFailures,1);
        };
        if (NSThread.isMainThread) perform();else dispatch_async(dispatch_get_main_queue(),perform);
    };
    // Replace the native action's callback before the sheet consumes it. This
    // is upstream of responder dispatch, routing and any later acquisition UI.
    ((void(*)(id,SEL,id))objc_msgSend)(action,sel_registerName("setHandler:"),handler);
    atomic_fetch_add(&RVQueueMenuActions,1);return action;
}
static NSDictionary *RVQueueLocalReport(void) {
    NSMutableDictionary *report=[@{@"queue_owner":@"YTQueueController",@"parallel_queue_model":@NO,
        @"mdx_queue_operations_recognized":@(atomic_load(&RVQueueMDXSeen)),@"mdx_queue_operations_handled":@(atomic_load(&RVQueueMDXHandled)),
        @"native_mutations_confirmed":@(atomic_load(&RVQueueNativeConfirmed)),@"native_mutations_unconfirmed":@(atomic_load(&RVQueueNativeUnconfirmed)),
        @"native_queue_count":@(RVQueueCount()),@"native_queue_owned":@(RVQueueNativeOwned),
        @"native_queue_bound":@(RVQueueLocalController!=nil),@"native_panel_bound":@(RVQueueLocalPanelController!=nil),
        @"mdx_decoder_payloads":@(atomic_load(&RVQueueDecodePayloads)),@"mdx_decoder_success":@(atomic_load(&RVQueueDecodeSuccess)),
        @"mdx_decoder_last_stage":@(atomic_load(&RVQueueDecodeStage)),@"queue_commands_off_main_delegated":@(atomic_load(&RVQueueOffMain)),
        @"queue_menu_tokens_decoded":@(atomic_load(&RVQueueMenuTokens)),@"queue_menu_actions_wired":@(atomic_load(&RVQueueMenuActions)),
        @"queue_menu_action_taps":@(atomic_load(&RVQueueMenuActionTaps)),@"queue_menu_action_failures":@(atomic_load(&RVQueueMenuActionFailures)),
        @"last_blocker":RVQueueLocalFailure ?: @"none"} mutableCopy];
    [report addEntriesFromDictionary:RVQueuePresentationReport()];return report;
}
static BOOL RVQueueNativeSection(id section) {
    return RVQueueNativeActive() && [RVObject(RVObject(section,"entry"),"playlistId") isEqual:@"RVPortQueue"];
}
static void RVInstallQueueLocal(void) {
    RVInstallQueuePresentation();
    RVHook(@"YTMenuController",@"menuItemWithServiceItemRenderer:entry:firstResponder:",@"@@:@@@",NO,^id(IMP original,SEL selector) {
        return ^id(id controller,id renderer,id entry,id responder) {
            id action=((id(*)(id,SEL,id,id,id))original)(controller,selector,renderer,entry,responder);
            return RVQueueDirectMenuAction(action,renderer,entry);
        };
    });
    RVHook(@"YTMenuController",@"menuItemWithNavigationItemRenderer:fromView:entry:firstResponder:",@"@@:@@@@",NO,^id(IMP original,SEL selector) {
        return ^id(id controller,id renderer,id view,id entry,id responder) {
            id action=((id(*)(id,SEL,id,id,id,id))original)(controller,selector,renderer,view,entry,responder);
            return RVQueueDirectMenuAction(action,renderer,entry);
        };
    });
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
                if (completion) ((void(^)(id,id))completion)(nil,nil);return YES;
            };
        });
    }
    RVHook(@"YTQueueController",@"initWithAccountID:parentResponder:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id account,id responder) { id result=((id(*)(id,SEL,id,id))original)(object,sel,account,responder);RVQueueLocalBind(result,account);return result; };
    });
    RVHook(@"YTQueueController",@"initWithAccountID:restorableQueueState:parentResponder:",@"@@:@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id account,id state,id responder) { id result=((id(*)(id,SEL,id,id,id))original)(object,sel,account,state,responder);RVQueueLocalBind(result,account);return result; };
    });
    RVHook(@"YTPlaylistPanelController",@"initWithAccountID:parentResponder:queueController:",@"@@:@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id account,id responder,id controller) { id result=((id(*)(id,SEL,id,id,id))original)(object,sel,account,responder,controller);RVQueueLocalPanelController=result;RVQueueLocalBind(controller,account);return result; };
    });
    RVHook(@"YTPlaylistPanelController",@"initWithAccountID:parentResponder:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id account,id responder) { id result=((id(*)(id,SEL,id,id))original)(object,sel,account,responder);RVQueueLocalPanelController=result;return result; };
    });
    RVHook(@"YTQueueController",@"clearQueue",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) { ((void(*)(id,SEL))original)(object,sel);if (object==RVQueueLocalController && !RVQueueLocalDepth && !RVQueueNativeResponseDepth) { RVQueueNativeOwned=NO;RVQueueNativeSourcePlaylist=nil; } };
    });
    // These section callbacks normally submit a remote playlist edit as well
    // as the local mutation. Local queue rows route directly to their owner.
    RVHook(@"YTPlaylistPanelSectionController",@"didMoveEntryAtIndex:toIndexPath:",@"v@:Q@",NO,^id(IMP original,SEL sel) {
        return ^(id section,NSUInteger from,NSIndexPath *to) {
            if (!RVQueueNativeSection(section)) { ((void(*)(id,SEL,NSUInteger,id))original)(section,sel,from,to);return; }
            if (to.length!=2 || from>=RVQueueCount() || [to indexAtPosition:1]>=RVQueueCount()
                || !RVCan(RVQueueLocalController,"moveItemAtIndexPath:toIndexPath:userTriggered:",@"v@:@@B")) return;
            NSUInteger indices[]={0,from};NSIndexPath *source=[NSIndexPath indexPathWithIndexes:indices length:2];
            ((void(*)(id,SEL,id,id,BOOL))objc_msgSend)(RVQueueLocalController,sel_registerName("moveItemAtIndexPath:toIndexPath:userTriggered:"),source,to,YES);
            RVQueueNativeRefreshPresentation();
        };
    });
    RVHook(@"YTPlaylistPanelSectionController",@"didRemoveEntryAtIndexPath:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id section,NSIndexPath *path) {
            if (!RVQueueNativeSection(section)) { ((void(*)(id,SEL,id))original)(section,sel,path);return; }
            if (path.length!=2 || [path indexAtPosition:1]>=RVQueueCount()
                || !RVCan(RVQueueLocalController,"removeItemAtIndexPath:userTriggered:",@"v@:@B")) return;
            ((void(*)(id,SEL,id,BOOL))objc_msgSend)(RVQueueLocalController,sel_registerName("removeItemAtIndexPath:userTriggered:"),path,YES);
            RVQueueNativeRefreshPresentation();
        };
    });
}
