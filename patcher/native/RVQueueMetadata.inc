// SPDX-License-Identifier: MIT
// Queue rows consume actual native models. Opaque Elements sources use the
// native Player service for videoDetails, without dispatching response actions.
static atomic_uint RVQueueMetadataRequests,RVQueueMetadataLoaded,RVQueueMetadataFailures;
static id RVQueueMetadataFind(NSString *video,id source,NSUInteger depth,NSUInteger *budget,NSHashTable *seen) {
    if (!source || !depth || !*budget || [seen containsObject:source]) return nil;
    --*budget;[seen addObject:source];
    BOOL matching=[RVObject(source,"videoId") isEqual:video];
    if (matching && (RVQueueClass(source,@"YTIPlaylistPanelVideoRenderer")
        || RVQueueClass(source,@"YTIVideoRenderer") || RVQueueClass(source,@"YTICompactVideoRenderer")
        || RVQueueClass(source,@"YTIVideoWithContextRenderer") || RVQueueClass(source,@"YTIInlinePlaybackRenderer")
        || RVQueueClass(source,@"YTIVideoDetails"))) return source;
    for (NSString *field in @[@"item",@"firstSubmessage",@"videoRenderer",@"compactVideoRenderer",
        @"playlistPanelVideoRenderer",@"inlinePlaybackRenderer",@"videoWithContextRenderer",
        @"playerData",@"playerResponse",@"contentPlayerResponse",@"videoDetails"]) {
        id child=RVObject(source,field.UTF8String);
        if (!child || child==source) continue;
        id found=RVQueueMetadataFind(video,child,depth-1,budget,seen);if (found) return found;
    }
    return nil;
}
static id RVQueueMetadataSource(NSString *video,id entry) {
    NSUInteger budget=64;NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    id source=RVQueueMetadataFind(video,entry,7,&budget,seen);
    if (source) return source;
    for (id owner in @[RVCurrentPlayback ?: NSNull.null,RVCurrentPlayer ?: NSNull.null]) {
        source=RVQueueMetadataFind(video,owner,5,&budget,seen);if (source) return source;
    }
    return nil;
}
static id RVQueueMetadataTitle(id source) {
    if (RVQueueClass(source,@"YTIVideoWithContextRenderer")) return RVObject(source,"headline");
    if (RVQueueClass(source,@"YTIInlinePlaybackRenderer"))
        return RVObject(RVObject(RVObject(source,"metadataRenderer"),"inlinePlaybackMetadataRenderer"),"title");
    return RVObject(source,"title");
}
static BOOL RVQueueMetadataReady(id source) {
    id title=RVQueueMetadataTitle(source),thumbnail=RVObject(source,"thumbnail");
    return title && (![title isKindOfClass:NSString.class] || [title length]) && RVQueueClass(thumbnail,@"YTIThumbnailDetails");
}
static id RVQueueMetadataRow(NSString *video,id entry) {
    id source=RVQueueMetadataSource(video,entry);
    if (!RVQueueMetadataReady(source)) return nil;
    if (RVQueueClass(source,@"YTIPlaylistPanelVideoRenderer") && [source conformsToProtocol:@protocol(NSCopying)])
        return [source copy];
    id renderer=[NSClassFromString(@"YTIPlaylistPanelVideoRenderer") new];
    id command=RVQueueWatchCommand(video);
    if (!renderer || !command || !RVQueueSet(renderer,@"setVideoId:",video)
        || !RVQueueSet(renderer,@"setNavigationEndpoint:",command)) return nil;
    RVQueueSet(renderer,@"setPlaylistSetVideoId:",video);
    id title=RVQueueMetadataTitle(source);
    if ([title isKindOfClass:NSString.class]) title=RVQueueFormatted(title);
    if (!RVQueueSet(renderer,@"setTitle:",title)
        || !RVQueueSet(renderer,@"setThumbnail:",RVObject(source,"thumbnail"))) return nil;
    if (RVQueueClass(source,@"YTIVideoDetails")) {
        id author=RVObject(source,"author");
        if ([author isKindOfClass:NSString.class] && [author length]) {
            id byline=RVQueueFormatted(author);RVQueueSet(renderer,@"setShortBylineText:",byline);RVQueueSet(renderer,@"setLongBylineText:",byline);
        }
        double seconds=NAN;
        if (RVCan(source,"lengthSeconds",@"Q@:")) seconds=(double)((uint64_t(*)(id,SEL))objc_msgSend)(source,sel_registerName("lengthSeconds"));
        else if (RVCan(source,"lengthSeconds",@"q@:")) seconds=(double)((int64_t(*)(id,SEL))objc_msgSend)(source,sel_registerName("lengthSeconds"));
        if (RVQueuePresent(source,@"lengthSeconds") && isfinite(seconds) && seconds>=0 && !RVBool(source,"isLiveContent")
            && RVCan(NSString.class,"yt_shortDurationWithDuration:",@"@@:d")) {
            id duration=((id(*)(id,SEL,double))objc_msgSend)(NSString.class,sel_registerName("yt_shortDurationWithDuration:"),seconds);
            if ([duration isKindOfClass:NSString.class]) RVQueueSet(renderer,@"setLengthText:",RVQueueFormatted(duration));
        }
    } else {
        id metadata=RVQueueClass(source,@"YTIInlinePlaybackRenderer")
            ? RVObject(RVObject(source,"metadataRenderer"),"inlinePlaybackMetadataRenderer") : source;
        for (NSString *field in @[@"shortBylineText",@"longBylineText",@"videoInfo",@"lengthText"]) {
            id value=RVObject(metadata,field.UTF8String) ?: RVObject(source,field.UTF8String);
            if (RVQueueClass(value,@"YTIFormattedString")) {
                NSString *setter=[NSString stringWithFormat:@"set%@%@:",[[field substringToIndex:1] uppercaseString],[field substringFromIndex:1]];
                RVQueueSet(renderer,setter,value);
            }
        }
        id overlays=RVObject(source,"thumbnailOverlaysArray");
        if ([overlays isKindOfClass:NSArray.class] && [overlays count]<=32) RVQueueSet(renderer,@"setThumbnailOverlaysArray:",overlays);
    }
    return renderer;
}
static void RVQueueMetadataIgnoreActions(id object,SEL selector,id response) {}
static id RVQueueMetadataService(void) {
    static Class cls;static dispatch_once_t once;
    dispatch_once(&once,^{
        Class base=NSClassFromString(@"YTPlayerService");
        Method method=base ? class_getInstanceMethod(base,sel_registerName("handleActionsFromPlayerResponse:")) : NULL;
        if (!method || ![RVSignature(method) isEqualToString:@"v@:@"]) return;
        Class fresh=objc_allocateClassPair(base,"RVQueueMetadataPlayerService",0);
        if (!fresh) return;
        if (!class_addMethod(fresh,sel_registerName("handleActionsFromPlayerResponse:"),(IMP)RVQueueMetadataIgnoreActions,method_getTypeEncoding(method))) { objc_disposeClassPair(fresh);return; }
        objc_registerClassPair(fresh);cls=fresh;
    });
    return cls ? [cls new] : nil;
}
static BOOL RVQueueFetchMetadata(NSString *video,void (^completion)(id)) {
    id service=RVQueueMetadataService(),request=[NSClassFromString(@"YTIPlayerRequest") new];
    const char *selector="makePlayerRequest:responseBlock:errorBlock:";
    if (!service || !RVQueueSet(request,@"setVideoId:",video) || !RVCan(service,selector,@"v@:@@?@?")) return NO;
    atomic_fetch_add(&RVQueueMetadataRequests,1);
    void (^finish)(id)=^(id response) {
        dispatch_async(dispatch_get_main_queue(),^{
            // The native request callback weakly references its service; retain
            // this dedicated instance until its final callback has returned.
            (void)[service class];
            id source=RVQueueMetadataSource(video,response);
            if (RVQueueMetadataReady(source)) atomic_fetch_add(&RVQueueMetadataLoaded,1);
            else { source=nil;atomic_fetch_add(&RVQueueMetadataFailures,1); }
            completion(source);
        });
    };
    void (^success)(id,id)=^(id response,id cacheContext) { finish(response); };
    void (^failure)(id)=^(id error) { finish(nil); };
    ((void(*)(id,SEL,id,id,id))objc_msgSend)(service,sel_registerName(selector),request,success,failure);
    return YES;
}
