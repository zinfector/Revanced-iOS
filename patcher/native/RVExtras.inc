// SPDX-License-Identifier: MIT
// Additional version-specific adapters. Included in RVPort.m so all hooks share ABI guards.
static NSString *RVDislikeVideo;
static NSNumber *RVDislikeCount;
static NSURLSessionDataTask *RVDislikeTask;
static NSMutableDictionary *RVDislikeCache;
static NSTimeInterval RVDislikeAttempt;
static NSUInteger RVDislikeGeneration;
static void RVRefreshDislikeUI(void);
static void RVElementWatchTransition(id watch,NSString *reason);
static void RVNativeDislikeRefresh(void);
static void RVDislikeServiceValue(NSString *key,id value);
static void RVDislikeServiceReset(void);
static void RVDislikeCaptureCheckpoint(NSString *trigger);
static __weak UIView *RVSegmentBar;
static NSString *RVThumbnailContext=@"home";
static NSString *RVThumbnailPivot=@"home";
static NSHashTable *RVVisibleWatchControllers,*RVVisibleSearchControllers;
static NSObject *RVThumbnailContextLock;
static void RVRefreshThumbnailContext(void) {
    static dispatch_once_t once;
    dispatch_once(&once,^{ RVThumbnailContextLock=[NSObject new];RVVisibleWatchControllers=[NSHashTable weakObjectsHashTable];RVVisibleSearchControllers=[NSHashTable weakObjectsHashTable]; });
    @synchronized(RVThumbnailContextLock) { RVThumbnailContext=RVVisibleWatchControllers.count ? @"player" : RVVisibleSearchControllers.count ? @"search" : RVThumbnailPivot; }
}
static UIView *RVPlayerOverlay(void) {
    id view=RVObject(RVObject(RVCurrentPlayer,"playerView"),"overlayView");
    return [view isKindOfClass:UIView.class] ? view : nil;
}
static BOOL RVOwnedPlayerView(UIView *view) {
    UIView *overlay=RVPlayerOverlay();
    return overlay && (view==overlay || [view isDescendantOfView:overlay]);
}
static void RVSegmentsChanged(void) {
    if (!NSThread.isMainThread) { dispatch_async(dispatch_get_main_queue(),^{ RVSegmentsChanged(); });return; }
    // Native layout can skip its child bars. Invalidate those bars directly,
    // including ones laid out before data-load established playback ownership.
    UIView *root=RVPlayerOverlay();
    if (!root) { [RVSegmentBar setNeedsLayout];return; }
    NSMutableArray<UIView *> *pending=[NSMutableArray arrayWithObject:root];
    Class barClass=NSClassFromString(@"YTInlinePlayerBarView");
    Class modularClass=NSClassFromString(@"YTModularPlayerBarView");
    NSUInteger found=0;
    for (NSUInteger index=0;index<pending.count && index<512;index++) {
        UIView *view=pending[index];
        if ((barClass && [view isKindOfClass:barClass]) || (modularClass && [view isKindOfClass:modularClass])) { found++;[view setNeedsLayout]; }
        for (UIView *child in view.subviews) if (pending.count<512) [pending addObject:child];
    }
    RVSponsorValue(@"marker_bar_candidates",@(found));
}
static const void *RVHideKey=&RVHideKey, *RVSwipeKey=&RVSwipeKey, *RVLoopKey=&RVLoopKey;
static void RVShowTools(UIViewController *presenter);

static BOOL RVVideoIDValid(id value) {
    if (![value isKindOfClass:[NSString class]] || [value length]!=11) return NO;
    NSCharacterSet *allowed=[NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"];
    return [value rangeOfCharacterFromSet:allowed.invertedSet].location==NSNotFound;
}
static void RVVoid(id object,const char *selector) {
    if (RVCan(object,selector,@"v@:")) ((void(*)(id,SEL))objc_msgSend)(object,sel_registerName(selector));
}
static void RVSetBool(id object,const char *selector,BOOL value) {
    if (RVCan(object,selector,@"v@:B")) ((void(*)(id,SEL,BOOL))objc_msgSend)(object,sel_registerName(selector),value);
}
static NSString *RVContentGate(id controller) {
    if (!RVVideoIDValid(RVObject(controller,"contentVideoID"))) return @"video_id_invalid";
    if (!RVCan(controller,"isPlayingAd",@"B@:")) return @"ad_status_unknown";
    if (RVBool(controller,"isPlayingAd")) return @"playing_ad";
    id response=RVContentResponse(controller);
    if (!response) return @"response_unavailable";
    if (!RVCan(response,"isLivePlayback",@"B@:")) return @"live_status_unknown";
    if (RVBool(response,"isLivePlayback")) return @"playing_live";
    return nil;
}
static BOOL RVOrdinaryContent(id controller) { return RVContentGate(controller)==nil; }
static void RVBoolGate(NSString *cls,NSString *selector,NSString *feature,BOOL value) {
    RVHook(cls,selector,@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id object) { return RVEnabled(feature) ? value : ((BOOL(*)(id,SEL))original)(object,sel); };
    });
}
static void RVHideView(id value,BOOL hidden) {
    if (![value isKindOfClass:[UIView class]]) return;
    UIView *view=value;
    NSNumber *baseline=objc_getAssociatedObject(view,RVHideKey);
    if (hidden) {
        if (!baseline) objc_setAssociatedObject(view,RVHideKey,@(view.hidden),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        view.hidden=YES;
    } else if (baseline) {
        view.hidden=baseline.boolValue;objc_setAssociatedObject(view,RVHideKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
}

static BOOL RVExtraReject(NSData *data) {
    NSDictionary *rules=@{@"hide_action_buttons":@"action_patterns",@"hide_flyout_items":@"flyout_patterns",
                          @"hide_comments":@"comment_patterns",@"hide_layout_components":@"layout_patterns"};
    for (NSString *feature in rules) if (RVEnabled(feature) && RVDataMatches(data,RVSetting(rules[feature]))) return YES;
    return NO;
}

// Only public YouTube redirect/share URLs are rewritten. Authentication URLs and other schemes pass through.
static NSURL *RVRewriteURL(NSURL *url) {
    if (![url isKindOfClass:[NSURL class]] || ![@[@"http",@"https"] containsObject:url.scheme.lowercaseString]) return url;
    NSURLComponents *parts=[NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
    NSString *host=parts.host.lowercaseString;
    BOOL youtube=[@[@"youtube.com",@"www.youtube.com",@"m.youtube.com",@"youtu.be"] containsObject:host];
    if (youtube && RVEnabled(@"bypass_redirects") && [parts.path isEqual:@"/redirect"]) {
        for (NSURLQueryItem *item in parts.queryItems) {
            if (![@[@"q",@"url"] containsObject:item.name]) continue;
            NSURL *candidate=[NSURL URLWithString:item.value ?: @""];
            if (candidate.host.length && [@[@"http",@"https"] containsObject:candidate.scheme.lowercaseString] && !candidate.user && !candidate.password) {
                url=candidate;parts=[NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
                host=parts.host.lowercaseString;youtube=[@[@"youtube.com",@"www.youtube.com",@"m.youtube.com",@"youtu.be"] containsObject:host];break;
            }
        }
    }
    if (!youtube) return url;
    if (RVEnabled(@"open_shorts_regular") && [parts.path hasPrefix:@"/shorts/"]) {
        NSString *video=[parts.path componentsSeparatedByString:@"/"].lastObject;
        if (RVVideoIDValid(video)) {
            parts.path=@"/watch";
            NSMutableArray *query=[NSMutableArray arrayWithObject:[NSURLQueryItem queryItemWithName:@"v" value:video]];
            for (NSURLQueryItem *item in parts.queryItems) if (![item.name isEqual:@"v"]) [query addObject:item];
            parts.queryItems=query;
        }
    }
    if (RVEnabled(@"sanitize_sharing_links") && ([parts.path isEqual:@"/watch"] || [parts.path hasPrefix:@"/shorts/"] || [host isEqual:@"youtu.be"])) {
        NSMutableArray *query=[NSMutableArray array];
        for (NSURLQueryItem *item in parts.queryItems) {
            if ([@[@"si",@"feature",@"pp",@"utm_source",@"utm_medium",@"utm_campaign"] containsObject:item.name]) continue;
            [query addObject:item];
        }
        parts.queryItems=query.count ? query : nil;
    }
    return parts.URL ?: url;
}
static NSString *RVReelID(id command) { return RVObject(RVObject(command,"reelWatchEndpoint"),"videoId"); }
static id RVWatchEndpointForReel(id command) {
    NSString *video=RVReelID(command);
    if (!RVVideoIDValid(video)) return nil;
    id endpoint=[NSClassFromString(@"YTIWatchEndpoint") new];
    if (!RVCan(endpoint,"setVideoId:",@"v@:@")) return nil;
    RVSetObject(endpoint,"setVideoId:",video);return endpoint;
}
#include "RVImages.inc"
static NSString *RVThumbnailURL(NSString *value) {
    NSURLComponents *parts=[NSURLComponents componentsWithString:value];
    if (![@[@"i.ytimg.com",@"i1.ytimg.com",@"i2.ytimg.com",@"i3.ytimg.com",@"i4.ytimg.com"] containsObject:parts.host.lowercaseString]) return value;
    NSArray *path=parts.path.pathComponents;
    if (path.count!=4 || ![@[@"vi",@"vi_webp"] containsObject:path[1]] || !RVVideoIDValid(path[2])) return value;
    NSString *filename=path[3];
    NSString *quality=[filename stringByDeletingPathExtension];
    NSRange custom=[quality rangeOfString:@"_custom_"];if (custom.location!=NSNotFound) quality=[quality substringToIndex:custom.location];
    NSDictionary *variants=@{@"default":@"",@"mqdefault":@"mq",@"hqdefault":@"hq",@"sddefault":@"hq720_",@"hq720":@"hq720_",@"maxresdefault":@"maxres"};
    NSString *prefix=variants[quality];
    BOOL stills=prefix && RVEnabled(@"alternative_thumbnails"),dearrowEnabled=prefix && RVEnabled(@"dearrow_thumbnails");
    NSString *context;@synchronized(RVThumbnailContextLock) { context=RVThumbnailContext ?: @"home"; }
    id modes=RVSetting(@"thumbnail_modes");NSString *mode=[modes isKindOfClass:NSDictionary.class] ? modes[context] : nil;
    if ([mode isEqual:@"original"]) { stills=NO;dearrowEnabled=NO; }
    if ([mode isEqual:@"stills"]) dearrowEnabled=NO;
    if ([mode isEqual:@"dearrow"]) stills=NO;
    if (stills) {
        if (RVEnabled(@"fast_thumbnail_stills") && [@[@"sddefault",@"hq720",@"maxresdefault"] containsObject:quality]) prefix=@"sd";
        NSURLComponents *still=[parts copy];still.query=nil;
        NSArray *candidates=[prefix isEqual:@"hq"] ? @[@"hq"] : @[prefix,@"hq"];
        for (NSString *variant in candidates) {
            still.path=[NSString stringWithFormat:@"/vi/%@/%@%d.jpg",path[2],variant,[RVSetting(@"thumbnail_frame") intValue]];
            if (RVImageAvailable(still.string)) { parts=[still copy];break; }
        }
    }
    NSString *result=parts.string ?: value;
    if (dearrowEnabled) {
        NSURLComponents *dearrow=[NSURLComponents componentsWithString:RVSetting(@"dearrow_url")];
        NSMutableArray *query=[dearrow.queryItems mutableCopy] ?: [NSMutableArray array];
        NSURLComponents *fallback=[NSURLComponents componentsWithString:result];fallback.scheme=@"https";fallback.host=@"i.ytimg.com";
        [query addObject:[NSURLQueryItem queryItemWithName:@"videoID" value:path[2]]];
        [query addObject:[NSURLQueryItem queryItemWithName:@"redirectUrl" value:fallback.string]];dearrow.queryItems=query;
        if (RVImageAvailable(dearrow.string)) result=dearrow.string;
    }
    NSString *endpoint=RVSetting(@"thumbnail_proxy_url");
    if (RVEnabled(@"thumbnail_proxy") && [endpoint isKindOfClass:[NSString class]] && endpoint.length) {
        NSURLComponents *proxy=[NSURLComponents componentsWithString:endpoint];
        if ([proxy.scheme isEqual:@"https"] && proxy.host.length && !proxy.user && !proxy.password) {
            NSMutableArray *query=[proxy.queryItems mutableCopy] ?: [NSMutableArray array];
            [query addObject:[NSURLQueryItem queryItemWithName:@"url" value:result]];proxy.queryItems=query;return proxy.string ?: result;
        }
    }
    return result;
}
static void RVInstallLinks(void) {
    RVRefreshThumbnailContext();
    for (NSString *cls in @[@"YTWatchViewController",@"YTSearchViewController",@"YTSearchResultsViewController"]) for (NSString *selector in @[@"viewDidAppear:",@"viewDidDisappear:"]) RVHook(cls,selector,@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id object,BOOL animated) {
            ((void(*)(id,SEL,BOOL))original)(object,sel,animated);
            __weak id weakObject=object;
            dispatch_async(dispatch_get_main_queue(),^{
                id current=weakObject;if (!current) return;
                NSHashTable *visible=[cls isEqual:@"YTWatchViewController"] ? RVVisibleWatchControllers : RVVisibleSearchControllers;
                if ([selector isEqual:@"viewDidAppear:"]) [visible addObject:current];else [visible removeObject:current];RVRefreshThumbnailContext();
                if ([cls isEqual:@"YTWatchViewController"]) {
                    if ([selector isEqual:@"viewDidAppear:"]) RVCurrentWatch=current;
                    else if (RVCurrentWatch==current) RVCurrentWatch=nil;
                    RVSPRefreshUI();RVRefreshDislikeUI();RVElementWatchTransition(current,[selector isEqual:@"viewDidAppear:"] ? @"watch_appeared" : @"watch_disappeared");
                }
            });
        };
    });
    RVHook(@"YTPivotBarViewController",@"setSelectedPivotIdentifier:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id identifier) {
            ((void(*)(id,SEL,id))original)(object,sel,identifier);
            if (![identifier isKindOfClass:NSString.class]) return;
            NSString *context=[identifier isEqual:@"FEsubscriptions"] ? @"subscriptions" : [@[@"FElibrary",@"FEyou"] containsObject:identifier] ? @"library" : @"home";
            dispatch_async(dispatch_get_main_queue(),^{ RVThumbnailPivot=context;RVRefreshThumbnailContext(); });
        };
    });
    for (NSString *selector in @[@"URL",@"url"]) RVHook(@"YTIUrlEndpoint",selector,@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id value=((id(*)(id,SEL))original)(object,sel);
            if (![value isKindOfClass:[NSString class]]) return value;
            return RVRewriteURL([NSURL URLWithString:value]).absoluteString ?: value;
        };
    });
    RVHook(@"YTNavigationCommandHandlerImpl",@"handleUrlEndpoint:event:fromView:",@"B@:@@@",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id object,id endpoint,id event,id view) {
            NSString *value=RVObject(RVObject(endpoint,"URLEndpoint"),"URL");
            NSURL *url=[value isKindOfClass:[NSString class]] ? RVRewriteURL([NSURL URLWithString:value]) : nil;
            NSString *host=url.host.lowercaseString;
            BOOL external=url.host.length && ![@[@"youtube.com",@"www.youtube.com",@"m.youtube.com",@"youtu.be",@"accounts.google.com"] containsObject:host];
            if (RVEnabled(@"open_links_external") && external && [@[@"http",@"https"] containsObject:url.scheme.lowercaseString] && [NSThread isMainThread]) {
                [UIApplication.sharedApplication openURL:url options:@{} completionHandler:nil];return YES;
            }
            return ((BOOL(*)(id,SEL,id,id,id))original)(object,sel,endpoint,event,view);
        };
    });
    RVHook(@"YTICommand",@"hasWatchEndpoint",@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id command) { return RVEnabled(@"open_shorts_regular") && RVWatchEndpointForReel(command) ? YES : ((BOOL(*)(id,SEL))original)(command,sel); };
    });
    RVHook(@"YTICommand",@"hasReelWatchEndpoint",@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id command) { return RVEnabled(@"open_shorts_regular") && RVWatchEndpointForReel(command) ? NO : ((BOOL(*)(id,SEL))original)(command,sel); };
    });
    RVHook(@"YTICommand",@"watchEndpoint",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id command) {
            id replacement=RVEnabled(@"open_shorts_regular") ? RVWatchEndpointForReel(command) : nil;
            return replacement ?: ((id(*)(id,SEL))original)(command,sel);
        };
    });
    for (NSString *selector in @[@"URL",@"url"]) RVHook(@"YTIThumbnailDetails_Thumbnail",selector,@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id value=((id(*)(id,SEL))original)(object,sel);
            if (![value isKindOfClass:[NSString class]]) return value;
            return RVThumbnailURL(value);
        };
    });
}

static void RVExtraSetPlayer(id player,id controller) {
    if (![NSThread isMainThread]) {
        __weak id p=player,c=controller;dispatch_async(dispatch_get_main_queue(),^{ if (p && c) RVExtraSetPlayer(p,c); });return;
    }
    id nativeController=RVPlaybackForPlayer(player);
    BOOL changed=RVCurrentPlayer!=player || RVCurrentPlayback!=nativeController;
    if (changed && RVCurrentPlayback) RVSPReset(RVSession(RVCurrentPlayback),@"player_rebound");
    RVCurrentPlayer=player;RVCurrentPlayback=nativeController;
    RVAdRequestCoordinatorPolicy(nativeController,@"content_player_bound");
    if (changed) {
        RVSponsorCount(@"player_bound");
        RVSponsorValue(@"controller_resolved",@(nativeController!=nil));
        RVSegmentsChanged();
    }
}
static void RVExtraObserve(id controller) {
    if (![NSThread isMainThread]) return;
    if (controller!=RVCurrentPlayback) return;
    static NSTimeInterval lastMarkerLayout;
    NSTimeInterval markerNow=NSDate.timeIntervalSinceReferenceDate;
    if (markerNow-lastMarkerLayout>.5) { lastMarkerLayout=markerNow;RVSegmentsChanged(); }
    NSString *video=RVObject(controller,"contentVideoID");
    if (!RVVideoIDValid(video)) return;
    if (![RVDislikeVideo isEqual:video]) {
        [RVDislikeTask cancel];RVDislikeTask=nil;RVDislikeVideo=video;RVDislikeCount=nil;RVDislikeAttempt=0;RVDislikeGeneration++;
        RVDislikeServiceReset();RVNativeDislikeRefresh();
    }
    RVDislikeCaptureCheckpoint(@"playback_tick");
    if (!RVEnabled(@"return_dislikes")) { [RVDislikeTask cancel];RVDislikeTask=nil;RVNativeDislikeRefresh();RVDislikeServiceValue(@"fetch_state",@"disabled");return; }
    if (!RVOrdinaryContent(controller)) { RVDislikeServiceValue(@"content_gate",RVContentGate(controller) ?: @"unknown");return; }
    RVDislikeServiceValue(@"content_gate",@"ready");
    if (!RVDislikeCache) RVDislikeCache=[NSMutableDictionary dictionary];
    NSDictionary *cached=RVDislikeCache[video];
    NSTimeInterval now=[NSDate timeIntervalSinceReferenceDate];
    if (cached && now-[cached[@"time"] doubleValue]<600) {
        RVDislikeCount=cached[@"count"];RVNativeDislikeRefresh();RVDislikeServiceValue(@"fetch_state",@"cache_hit");
        RVDislikeServiceValue(@"cache_age_seconds",@(MAX(0,now-[cached[@"time"] doubleValue])));return;
    }
    RVDislikeServiceValue(@"retry_after_seconds",@(RVDislikeTask || RVDislikeCount ? 0 : MAX(0,60-(now-RVDislikeAttempt))));
    if (RVDislikeTask || RVDislikeCount || now-RVDislikeAttempt<60) return;
    RVDislikeAttempt=now;
    RVDislikeServiceValue(@"fetch_state",@"requesting");
    NSUInteger generation=RVDislikeGeneration;
    NSURLComponents *url=[NSURLComponents componentsWithString:@"https://returnyoutubedislikeapi.com/votes"];
    url.queryItems=@[[NSURLQueryItem queryItemWithName:@"videoId" value:video]];
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:url.URL];request.timeoutInterval=10;
    NSString *requested=[video copy];
    RVDislikeTask=[[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData *data,NSURLResponse *response,NSError *error) {
        id json=nil;NSError *parseError=nil;
        NSInteger status=[response isKindOfClass:NSHTTPURLResponse.class] ? [(NSHTTPURLResponse *)response statusCode] : 0;
        if (!error && status==200 && data.length<65536 && data.length) json=[NSJSONSerialization JSONObjectWithData:data options:0 error:&parseError];
        NSNumber *count=[json isKindOfClass:[NSDictionary class]] && [json[@"dislikes"] isKindOfClass:[NSNumber class]] ? json[@"dislikes"] : nil;
        if (!isfinite(count.doubleValue) || count.doubleValue<0 || count.doubleValue>1e15 || floor(count.doubleValue)!=count.doubleValue) count=nil;
        dispatch_async(dispatch_get_main_queue(),^{
            if (generation!=RVDislikeGeneration || ![RVDislikeVideo isEqual:requested]) return;
            RVDislikeTask=nil;
            RVDislikeServiceValue(@"http_status",@(status));RVDislikeServiceValue(@"network_error_code",@(error.code));
            RVDislikeServiceValue(@"response_bytes",@(data.length));RVDislikeServiceValue(@"json_parse_error_code",@(parseError.code));
            RVDislikeServiceValue(@"fetch_state",count ? @"loaded" : error ? @"network_error" : status!=200 ? @"http_error" : @"invalid_response");
            if (count) {
                if (RVDislikeCache.count>=100) [RVDislikeCache removeAllObjects];
                RVDislikeCache[requested]=@{@"count":count,@"time":@([NSDate timeIntervalSinceReferenceDate])};RVDislikeCount=count;
                RVNativeDislikeRefresh();
            }
            RVDislikeCaptureCheckpoint(@"service_response");
        });
    }];[RVDislikeTask resume];
}

#include "RVDislikesUI.inc"
#include "RVSpeedBridge.inc"
#include "RVVoteLayoutCompatibility.inc"
#include "RVVoteModel.inc"
#include "RVElementDislikes.inc"
#include "RVVoteLifecycle.inc"
#include "RVNativeDislikes.inc"
#include "RVDislikesDiagnostics.inc"

static NSURL *RVCurrentURL(BOOL timestamp) {
    NSString *video=RVObject(RVCurrentPlayback,"contentVideoID");
    if (!RVVideoIDValid(video)) return nil;
    double seconds=RVDouble(RVCurrentPlayer,"currentVideoMediaTime");
    NSString *value=[NSString stringWithFormat:@"https://www.youtube.com/watch?v=%@",video];
    if (timestamp && isfinite(seconds) && seconds>=0) value=[value stringByAppendingFormat:@"&t=%lld",(long long)floor(seconds)];
    return [NSURL URLWithString:value];
}
#include "RVSponsorPromptUI.inc"
#include "RVServices.inc"
static void RVShowTools(UIViewController *presenter) {
    UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"Video tools" message:nil preferredStyle:UIAlertControllerStyleActionSheet];
    if (RVEnabled(@"return_dislikes")) {
        menu.message=RVDislikeCount ? [NSString stringWithFormat:@"Return YouTube Dislike estimate: %@",[NSNumberFormatter localizedStringFromNumber:RVDislikeCount numberStyle:NSNumberFormatterDecimalStyle]] : @"Dislike count unavailable or still loading.";
    }
    RVAddServiceTools(menu,presenter);
    if (RVEnabled(@"copy_video_url")) for (NSNumber *timestamp in @[@NO,@YES]) {
        [menu addAction:[UIAlertAction actionWithTitle:timestamp.boolValue ? @"Copy URL at current time" : @"Copy video URL" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            NSURL *url=RVCurrentURL(timestamp.boolValue);if (url) UIPasteboard.generalPasteboard.string=url.absoluteString;
        }]];
    }
    if (RVEnabled(@"external_downloads")) [menu addAction:[UIAlertAction actionWithTitle:@"Share URL to external downloader" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSURL *url=RVCurrentURL(NO);if (!url) return;
        UIActivityViewController *share=[[UIActivityViewController alloc] initWithActivityItems:@[url] applicationActivities:nil];
        share.popoverPresentationController.sourceView=presenter.view;share.popoverPresentationController.sourceRect=CGRectMake(CGRectGetMidX(presenter.view.bounds),CGRectGetMidY(presenter.view.bounds),1,1);
        [presenter presentViewController:share animated:YES completion:nil];
    }]];
    if (RVEnabled(@"picture_in_picture")) [menu addAction:[UIAlertAction actionWithTitle:@"Start Picture in Picture" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        if (RVBool(RVCurrentPlayer,"isPictureInPicturePossible")) { [presenter dismissViewControllerAnimated:YES completion:^{ RVVoid(RVCurrentPlayer,"switchToPip"); }]; }
    }]];
    if (RVEnabled(@"custom_speed_menu")) [menu addAction:[UIAlertAction actionWithTitle:@"Playback speedâ€¦" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        UIAlertController *speeds=[UIAlertController alertControllerWithTitle:@"Speed" message:nil preferredStyle:UIAlertControllerStyleAlert];
        for (NSNumber *rate in RVSetting(@"custom_speeds")) [speeds addAction:[UIAlertAction actionWithTitle:[NSString stringWithFormat:@"%@x",rate] style:UIAlertActionStyleDefault handler:^(UIAlertAction *choice) {
            RVSpeedChoose(RVCurrentPlayer,rate.floatValue,nil,@"revanced_video_tools");
            if (RVCan(RVCurrentPlayer,"setPlaybackRate:",@"v@:f")) ((void(*)(id,SEL,float))objc_msgSend)(RVCurrentPlayer,sel_registerName("setPlaybackRate:"),rate.floatValue);
        }]];
        [speeds addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[presenter presentViewController:speeds animated:YES completion:nil];
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    menu.popoverPresentationController.sourceView=presenter.view;menu.popoverPresentationController.sourceRect=CGRectMake(CGRectGetMidX(presenter.view.bounds),CGRectGetMidY(presenter.view.bounds),1,1);
    [presenter presentViewController:menu animated:YES completion:nil];
}

@interface RVSeekbarTap : NSObject <UIGestureRecognizerDelegate>
- (void)tap:(UITapGestureRecognizer *)gesture;
@end
static UIColor *RVSponsorColor(NSString *category) {
    NSDictionary *defaults=@{@"sponsor":@"#00D400",@"selfpromo":@"#FFFF00",@"interaction":@"#CC00FF",@"intro":@"#00FFFF",@"outro":@"#0202ED",@"preview":@"#008FD6",@"music_offtopic":@"#FF9900",@"filler":@"#7300FF",@"hook":@"#3957FF",@"poi_highlight":@"#FF1684"};
    id configured=RVSetting(@"sponsor_colors");NSString *value=[configured isKindOfClass:NSDictionary.class] ? configured[category] : nil;
    if (![value isKindOfClass:NSString.class] || value.length!=7) value=defaults[category] ?: @"#00D400";
    unsigned int color=0;[[NSScanner scannerWithString:[value substringFromIndex:1]] scanHexInt:&color];
    return [UIColor colorWithRed:((color>>16)&255)/255.0 green:((color>>8)&255)/255.0 blue:(color&255)/255.0 alpha:.8];
}
static void RVLayoutSponsorMarkers(UIView *bar) {
    static const void *key=&key;
    UIView *surface=objc_getAssociatedObject(bar,key);
    BOOL owned=RVOwnedPlayerView(bar);
    RVPlaybackSession *session=RVSession(RVCurrentPlayback);
    BOOL active=owned && RVOrdinaryContent(RVCurrentPlayback) && RVEnabled(@"sponsorblock") && RVEnabled(@"sponsorblock_markers") && [session.videoID isEqual:RVObject(RVCurrentPlayback,"contentVideoID")];
    RVSponsorCount(@"marker_layout");
    if (owned) {
        RVSegmentBar=bar;RVSponsorCount(@"owned_marker_layout");
        RVSponsorValue(@"marker_active",@(active));RVSponsorValue(@"marker_rectangles",@0);
        RVSponsorValue(@"marker_view_kind",[bar isKindOfClass:NSClassFromString(@"YTModularPlayerBarView")] ? @"modular" : @"inline");
    }
    if (!surface && active) { surface=[UIView new];surface.userInteractionEnabled=NO;surface.clipsToBounds=YES;[bar addSubview:surface];objc_setAssociatedObject(bar,key,surface,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
    surface.hidden=!active;if (!active) return;
    double duration=RVDouble(bar,"totalTime");
    NSString *durationSource=@"bar";
    if (!isfinite(duration) || duration<=0) { duration=RVDouble(RVObject(bar,"playingState"),"totalTimeSec");durationSource=@"playing_state"; }
    if (!isfinite(duration) || duration<=0) { duration=RVDouble(RVCurrentPlayer,"currentVideoTotalMediaTime");durationSource=@"player"; }
    RVSponsorValue(@"marker_duration_source",durationSource);
    RVSponsorValue(@"marker_duration_valid",@(isfinite(duration) && duration>0));
    if (!isfinite(duration) || duration<=0) { surface.hidden=YES;return; }
    CGRect track=bar.bounds;
    if (RVCan(bar,"barFrame",@"{CGRect={CGPoint=dd}{CGSize=dd}}@:"))
        track=((CGRect(*)(id,SEL))objc_msgSend)(bar,sel_registerName("barFrame"));
    if (!isfinite(track.origin.x) || !isfinite(track.origin.y) || !isfinite(track.size.width) || track.size.width<=0) { surface.hidden=YES;return; }
    surface.frame=CGRectMake(track.origin.x,fmax(CGRectGetMinY(bar.bounds),track.origin.y-1),track.size.width,4);
    [bar bringSubviewToFront:surface];
    surface.layer.sublayers=nil;
    NSMutableDictionary<NSString *,UIBezierPath *> *paths=[NSMutableDictionary dictionary];
    NSUInteger rectangles=0;
    for (NSDictionary *segment in session.rawSegments) {
        NSString *category=segment[@"category"];
        if ([RVSponsorBehavior(category) isEqual:@"ignore"]) continue;
        double left,width;
        if (!RVMarkerBounds((RVInterval){[segment[@"start"] doubleValue],[segment[@"end"] doubleValue]},duration,surface.bounds.size.width,&left,&width)) continue;
        UIBezierPath *path=paths[category];if (!path) { path=[UIBezierPath bezierPath];paths[category]=path; }
        [path appendPath:[UIBezierPath bezierPathWithRect:CGRectMake(left,0,width,4)]];
        rectangles++;
    }
    for (NSString *category in paths) { CAShapeLayer *layer=[CAShapeLayer layer];layer.frame=surface.bounds;layer.path=paths[category].CGPath;layer.fillColor=RVSponsorColor(category).CGColor;[surface.layer addSublayer:layer]; }
    RVSponsorValue(@"marker_rectangles",@(rectangles));
}
@implementation RVSeekbarTap
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gesture {
    return RVEnabled(@"tap_to_seek") && RVOrdinaryContent(RVCurrentPlayback) && !RVEnabled(@"hide_seekbar");
}
- (void)tap:(UITapGestureRecognizer *)gesture {
    if (gesture.state!=UIGestureRecognizerStateEnded || !RVEnabled(@"tap_to_seek") || !RVOrdinaryContent(RVCurrentPlayback)) return;
    UIView *bar=gesture.view;double width=bar.bounds.size.width,duration=RVDouble(RVCurrentPlayer,"currentVideoTotalMediaTime");
    CGPoint point=[gesture locationInView:bar];
    if (width>0 && isfinite(duration) && duration>0 && point.x>=0 && point.x<=width) RVSeekSeconds(duration*(point.x/width));
}
@end
static void RVAttachBarTap(UIView *view) {
    static const void *key=&key;
    if (objc_getAssociatedObject(view,key)) return;
    RVSeekbarTap *target=[RVSeekbarTap new];UITapGestureRecognizer *tap=[[UITapGestureRecognizer alloc] initWithTarget:target action:@selector(tap:)];
    tap.delegate=target;tap.cancelsTouchesInView=NO;[view addGestureRecognizer:tap];objc_setAssociatedObject(view,key,target,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@interface RVSwipeController : NSObject <UIGestureRecognizerDelegate>
@property(nonatomic,weak) UIView *view;
@property(nonatomic,strong) MPVolumeView *volume;
@property(nonatomic) float baseline;
@property(nonatomic) BOOL brightness;
- (void)pan:(UIPanGestureRecognizer *)pan;
@end
@implementation RVSwipeController
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)recognizer {
    if (!RVEnabled(@"swipe_controls") || !RVBool(RVCurrentWatch,"isFullscreen") || !RVOrdinaryContent(RVCurrentPlayback)) return NO;
    CGPoint velocity=[(UIPanGestureRecognizer *)recognizer velocityInView:self.view];
    return fabs(velocity.y)>fabs(velocity.x)*1.5;
}
- (void)pan:(UIPanGestureRecognizer *)pan {
    if (!RVEnabled(@"swipe_controls")) return;
    if (pan.state==UIGestureRecognizerStateBegan) {
        self.brightness=[pan locationInView:self.view].x<self.view.bounds.size.width/2;
        self.baseline=self.brightness ? self.view.window.screen.brightness : AVAudioSession.sharedInstance.outputVolume;
    }
    if (pan.state!=UIGestureRecognizerStateChanged && pan.state!=UIGestureRecognizerStateEnded) return;
    float value=fmaxf(0,fminf(1,self.baseline-[pan translationInView:self.view].y/fmax(self.view.bounds.size.height,1)));
    if (self.brightness) self.view.window.screen.brightness=value;
    else {
        for (UIView *child in self.volume.subviews) if ([child isKindOfClass:[UISlider class]]) {
            UISlider *slider=(UISlider *)child;slider.value=value;[slider sendActionsForControlEvents:UIControlEventValueChanged];break;
        }
    }
}
@end
static void RVAttachSwipes(UIView *view) {
    if (!RVEnabled(@"swipe_controls") || objc_getAssociatedObject(view,RVSwipeKey)) return;
    RVSwipeController *adapter=[RVSwipeController new];adapter.view=view;
    adapter.volume=[[MPVolumeView alloc] initWithFrame:CGRectMake(-1000,-1000,1,1)];adapter.volume.alpha=.01;
    [view addSubview:adapter.volume];
    UIPanGestureRecognizer *pan=[[UIPanGestureRecognizer alloc] initWithTarget:adapter action:@selector(pan:)];pan.delegate=adapter;
    [view addGestureRecognizer:pan];objc_setAssociatedObject(view,RVSwipeKey,adapter,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static void RVInstallPlayerExtras(void) {
    RVHook(@"YTPlayabilityResolutionUserActionUIControllerImpl",@"resolvePlayabilityResolutionUserActionRequest:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id request) {
            id status=RVObject(request,"playabilityStatus");
            BOOL warning=RVEnabled(@"remove_discretion_dialog") && RVBool(status,"isContentCheckRequired");
            for (NSString *name in @[@"isAgeCheckRequired",@"isMatureContentAgeVerificationRequired",@"isKoreanAgeVerificationRequired",@"isAgeVerificationRequired",@"isLoginRequired",@"isRentalActivationVerificationRequired",@"isModernDialogAgeVerificationUseCase",@"isUnplayableWithIntersitialErrorRenderer"])
                if (!RVCan(status,name.UTF8String,@"B@:") || RVBool(status,name.UTF8String)) warning=NO;
            if (warning && RVCan(request,"completePlayabilityResolutionUserActionWithContentCheckOk:adultContentOk:offlineRentalActivationOk:ageVerificationType:",@"v@:BBBQ")) {
                ((void(*)(id,SEL,BOOL,BOOL,BOOL,unsigned long long))objc_msgSend)(request,sel_registerName("completePlayabilityResolutionUserActionWithContentCheckOk:adultContentOk:offlineRentalActivationOk:ageVerificationType:"),YES,NO,NO,0);return;
            }
            ((void(*)(id,SEL,id))original)(object,sel,request);
        };
    });
    RVHook(@"YTInnerTubeContextFactory",@"clientInfoWithSendDeviceIdentifier:serviceType:",@"@@:Bq",NO,^id(IMP original,SEL sel) {
        return ^id(id object,BOOL send,long long service) {
            id client=((id(*)(id,SEL,BOOL,long long))original)(object,sel,send,service);
            if ((!RVEnabled(@"spoof_app_version") && !RVEnabled(@"spoof_dimensions") && !RVEnabled(@"spoof_form_factor")) || ![client conformsToProtocol:@protocol(NSCopying)]) return client;
            id copy=[client copy];
            if (RVEnabled(@"spoof_app_version")) RVSetObject(copy,"setClientVersion:",RVSetting(@"client_version"));
            if (RVEnabled(@"spoof_dimensions")) for (NSString *field in @[@"setScreenWidthPoints:",@"setScreenHeightPoints:",@"setWindowWidthPoints:",@"setWindowHeightPoints:"]) {
                if (!RVCan(copy,field.UTF8String,@"v@:i")) continue;
                int value=[RVSetting([field containsString:@"Width"] ? @"screen_width_points" : @"screen_height_points") intValue];
                ((void(*)(id,SEL,int))objc_msgSend)(copy,NSSelectorFromString(field),value);
            }
            if (RVEnabled(@"spoof_form_factor") && RVCan(copy,"setClientFormFactor:",@"v@:i")) {
                int factor=[RVSetting(@"form_factor") isEqual:@"tablet"] ? 2 : 1;
                ((void(*)(id,SEL,int))objc_msgSend)(copy,sel_registerName("setClientFormFactor:"),factor);
            }
            return copy;
        };
    });
    RVBoolGate(@"YTBackgroundabilityPolicyImpl",@"isPlayableInPictureInPictureByUserSettings",@"picture_in_picture",YES);
    RVBoolGate(@"YTColdConfig",@"enablePipMenuItem",@"picture_in_picture",YES);
    RVBoolGate(@"YTColdConfig",@"addPipMenuItem",@"picture_in_picture",YES);
    RVBoolGate(@"YTColdConfig",@"enableResumeToShorts",@"disable_shorts_resume",NO);
    RVBoolGate(@"YTColdConfig",@"enableIosFloatingMiniplayer",@"classic_miniplayer",NO);
    RVBoolGate(@"YTColdConfigShortsConsumptionClientGlobalConfigImpl",@"iosEnableAutoAdvanceInThreeDotMenu",@"shorts_autoplay",YES);
    RVHook(@"YTReelPlayerViewController",@"playbackControllerDidFinishPlaybackAndWillTransitionInternallyToNextPlayback:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id object,BOOL internally) {
            ((void(*)(id,SEL,BOOL))original)(object,sel,internally);
            if (!RVEnabled(@"shorts_autoplay") || internally) return;
            NSString *video=RVObject(object,"videoId");__weak id weakObject=object;
            dispatch_async(dispatch_get_main_queue(),^{
                id live=weakObject;
                if (!live || !RVEnabled(@"shorts_autoplay") || ![video isEqual:RVObject(live,"videoId")] || !RVCan(live,"reelContentViewRequestsAdvanceToNextVideo:",@"v@:@")) return;
                id content=RVObject(live,"contentView");if (content) ((void(*)(id,SEL,id))objc_msgSend)(live,sel_registerName("reelContentViewRequestsAdvanceToNextVideo:"),content);
            });
        };
    });
    for (NSString *selector in @[@"onResponseReceivedEndpointsArray",@"onResponseReceivedActionsArray"]) RVHook(@"YTIWatchNextResponse",selector,@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id actions=((id(*)(id,SEL))original)(object,sel);
            if (!RVEnabled(@"disable_popup_panels") || ![actions isKindOfClass:[NSArray class]]) return actions;
            NSMutableArray *kept=[NSMutableArray array];
            for (id action in actions) if (!RVBool(action,"hasShowEngagementPanelEndpoint") && !RVBool(action,"hasShowEngagementPanelNavigationEndpoint")) [kept addObject:action];
            return kept;
        };
    });
    RVBoolGate(@"MDXSeamlessSignIn",@"presentSeamlessSignInIfNeeded",@"disable_tv_popup",NO);
    for (NSString *selector in @[@"shouldUpdateAutoCaptionsOnMute",@"shouldEnableAutoCaptionsOnMuteFeature",@"shouldShowAutoCaptionsOnMuteForMutedVideo"])
        RVBoolGate(@"YTMainAppVideoPlayerOverlayViewController",selector,@"disable_auto_captions",NO);
    for (NSString *selector in @[@"enableAutoCaptionsOnMuteClient",@"enablePauseForAutoCaptionOnMute"])
        RVBoolGate(@"YTColdConfig",selector,@"disable_auto_captions",NO);
    RVBoolGate(@"YTColdConfig",@"iosDisableCaptionsOnAppStartForVwc",@"disable_auto_captions",YES);
    for (NSString *selector in @[@"iosEnableTimelyShelfAmbientMode",@"enableFullbackgroundAmbientModeOnTablet",@"iosEnableFullbackgroundAmbientModeOnTablet"])
        RVBoolGate(@"YTColdConfig",selector,@"disable_ambient",NO);
    RVHook(@"YTNeonMetalView",@"setAmbientStrength:",@"v@:f",NO,^id(IMP original,SEL sel) {
        return ^(id object,float value) { ((void(*)(id,SEL,float))original)(object,sel,RVEnabled(@"disable_ambient") ? 0 : value); };
    });
    RVHook(@"YTSemanticHapticsController",@"playWithSemantic:",@"v@:q",NO,^id(IMP original,SEL sel) {
        return ^(id object,long long value) { if (!RVEnabled(@"disable_haptics")) ((void(*)(id,SEL,long long))original)(object,sel,value); };
    });
    RVHook(@"YTSemanticHapticsController",@"playWithSemantic:throttleTimeout:",@"v@:qd",NO,^id(IMP original,SEL sel) {
        return ^(id object,long long value,double timeout) { if (!RVEnabled(@"disable_haptics")) ((void(*)(id,SEL,long long,double))original)(object,sel,value,timeout); };
    });
    RVHook(@"YTDoubleTapToSeekController",@"enableDoubleTapToSeek:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id object,BOOL value) { ((void(*)(id,SEL,BOOL))original)(object,sel,RVEnabled(@"disable_double_tap") ? NO : value); };
    });
    RVHook(@"YTDoubleTapToSeekController",@"didDoubleTap:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id value) { if (!RVEnabled(@"disable_double_tap")) ((void(*)(id,SEL,id))original)(object,sel,value); };
    });
    RVHook(@"YTDoubleTapToSeekController",@"didTwoFingerDoubleTap:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id value) { if (!RVEnabled(@"disable_double_tap") && !RVEnabled(@"disable_chapter_skip")) ((void(*)(id,SEL,id))original)(object,sel,value); };
    });
    RVHook(@"YTVideoPlayerOverlayConfigTransformer",@"doubleTapSeekIntervalForVideoPlayerOverlayConfig:",@"d@:@",YES,^id(IMP original,SEL sel) {
        return ^double(id object,id config) { double value=[RVSetting(@"double_tap_seconds") doubleValue];return value>0 ? value : ((double(*)(id,SEL,id))original)(object,sel,config); };
    });
    RVHook(@"YTInlinePlayerBarContainerView",@"didPanFineScrubberEntryPoint:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id value) { if (!RVEnabled(@"disable_precise_seeking")) ((void(*)(id,SEL,id))original)(object,sel,value); };
    });
    RVHook(@"YTWatchViewController",@"playerViewController:didActivateVideo:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id watch,id player,id video) {
            ((void(*)(id,SEL,id,id))original)(watch,sel,player,video);
            __weak id weakWatch=watch;
            dispatch_async(dispatch_get_main_queue(),^{ if (weakWatch) { RVCurrentWatch=weakWatch;RVRefreshDislikeUI();if (RVEnabled(@"open_videos_fullscreen") && RVOrdinaryContent(RVCurrentPlayback)) RVSetBool(weakWatch,"setFullscreen:",YES); } });
        };
    });
    RVHook(@"YTLocalPlaybackController",@"finishPlayback",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id controller) {
            if ([NSThread isMainThread] && controller==RVCurrentPlayback && RVEnabled(@"loop_video") && RVOrdinaryContent(controller)) {
                Class times=NSClassFromString(@"YTSingleVideoTime");
                NSTimeInterval now=[NSDate timeIntervalSinceReferenceDate];
                NSNumber *last=objc_getAssociatedObject(controller,RVLoopKey);
                if ((!last || now-last.doubleValue>1) && RVCan(times,"timeWithTime:CPN:",@"@@:d@") && RVCan(controller,"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi") && RVCan(controller,"play",@"v@:")) {
                    NSString *cpn=RVObject(RVObject(controller,"contentVideoCurrentTime"),"CPN");
                    id time=((id(*)(id,SEL,double,id))objc_msgSend)(times,sel_registerName("timeWithTime:CPN:"),0,cpn);
                    if (time) {
                        objc_setAssociatedObject(controller,RVLoopKey,@(now),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                        ((void(*)(id,SEL,id,double,double,int))objc_msgSend)(controller,sel_registerName("seekToTime:toleranceBefore:toleranceAfter:seekSource:"),time,0,0,0);RVVoid(controller,"play");return;
                    }
                }
            }
            ((void(*)(id,SEL))original)(controller,sel);
            if (controller==RVCurrentPlayback && RVEnabled(@"exit_fullscreen_end")) dispatch_async(dispatch_get_main_queue(),^{ RVSetBool(RVCurrentWatch,"setFullscreen:",NO); });
        };
    });
    RVHook(@"YTAudioTrackSwitchControllerImpl",@"setUserSelectableFormats:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id formats) {
            ((void(*)(id,SEL,id))original)(object,sel,formats);
            if (!RVEnabled(@"force_original_audio") || ![formats isKindOfClass:[NSArray class]]) return;
            for (id format in formats) {
                NSString *identifier=RVObject(format,"audioTrackID");
                id track=RVObject(format,"audioTrack");
                if (![identifier isKindOfClass:[NSString class]]) identifier=RVObject(track,"id_p");
                // ReVanced's server-provided original-track discriminator. No match leaves native selection intact.
                if ([identifier isKindOfClass:[NSString class]] && [identifier hasSuffix:@".4"] && track && RVCan(object,"switchToAudioTrack:source:",@"v@:@q")) {
                    ((void(*)(id,SEL,id,long long))objc_msgSend)(object,sel_registerName("switchToAudioTrack:source:"),track,0);break;
                }
            }
        };
    });
    RVHook(@"YTVideoQualitySwitchOriginalController",@"showMenuFromView:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id view) {
            if (RVEnabled(@"advanced_quality_menu") && RVCan(object,"showAdvancedMenuFromView:",@"v@:@")) {
                ((void(*)(id,SEL,id))objc_msgSend)(object,sel_registerName("showAdvancedMenuFromView:"),view);return;
            }
            ((void(*)(id,SEL,id))original)(object,sel,view);
        };
    });
    RVHook(@"YTVideoQualitySwitchOriginalController",@"createActionSheetActions",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id actions=((id(*)(id,SEL))original)(object,sel);
            if (!RVEnabled(@"hide_premium_quality") || ![actions isKindOfClass:[NSArray class]]) return actions;
            NSMutableArray *kept=[NSMutableArray array];
            for (id action in actions) {
                NSString *title=RVObject(action,"title");
                if ([title isKindOfClass:[NSString class]] && [title rangeOfString:@"Premium" options:NSCaseInsensitiveSearch].location!=NSNotFound) continue;
                [kept addObject:action];
            }
            return kept.count ? kept : actions;
        };
    });
    RVHook(@"YTVideoQualitySwitchOriginalController",@"setSelectedQualityLabel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id label) {
            ((void(*)(id,SEL,id))original)(object,sel,label);
            if (!RVEnabled(@"remember_quality") || ![label isKindOfClass:[NSString class]]) return;
            int value=[label intValue];
            if ([@[@144,@240,@360,@480,@720,@1080,@1440,@2160] containsObject:@(value)]) [[NSUserDefaults standardUserDefaults] setInteger:value forKey:RVQualityPreference()];
            else if ([label caseInsensitiveCompare:@"Auto"]==NSOrderedSame) [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:RVQualityPreference()];
        };
    });
    [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionInterruptionNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
        if (RVEnabled(@"pause_on_interrupt") && [note.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue]==AVAudioSessionInterruptionTypeBegan) RVVoid(RVCurrentPlayer,"pause");
    }];
}

static void RVInstallLayout(void) {
    RVHook(@"YTModularPlayerBarView",@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) {
            ((void(*)(id,SEL))original)(object,sel);
            if ([object isKindOfClass:UIView.class]) RVLayoutSponsorMarkers(object);
        };
    });
    for (NSString *cls in @[@"YTCommonColorPalette",@"YTTokensColorPalette"]) for (NSString *selector in @[@"baseBackground",@"solidBackground",@"generalBackgroundA",@"background1"]) RVHook(cls,selector,@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id color=((id(*)(id,SEL))original)(object,sel);
            if (![color isKindOfClass:UIColor.class] || !RVCan(object,"pageStyle",@"q@:")) return color;
            long long style=((long long(*)(id,SEL))objc_msgSend)(object,sel_registerName("pageStyle"));
            if (style!=0 && style!=1) return color;
            NSString *hex=RVSetting(style==1 ? @"theme_dark_background" : @"theme_light_background");
            if (![hex isKindOfClass:NSString.class] || hex.length!=7) return color;
            unsigned int rgb=0;[[NSScanner scannerWithString:[hex substringFromIndex:1]] scanHexInt:&rgb];
            return [UIColor colorWithRed:((rgb>>16)&255)/255.0 green:((rgb>>8)&255)/255.0 blue:(rgb&255)/255.0 alpha:1];
        };
    });
    RVBoolGate(@"YTMainAppVideoPlayerOverlayView",@"forceHidePreviousAndNextButtons",@"hide_previous_next",YES);
    for (NSString *cls in @[@"YTMainAppVideoPlayerOverlayView",@"YTMainAppControlsOverlayView"]) for (NSString *selector in @[@"setPreviousButtonHidden:",@"setNextButtonHidden:"]) RVHook(cls,selector,@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id object,BOOL hidden) { ((void(*)(id,SEL,BOOL))original)(object,sel,hidden || RVEnabled(@"hide_previous_next")); };
    });
    RVHook(@"YTHeaderLogoImageProviderImpl",@"defaultHeaderLogoImageWithPageStyle:isPremium:",@"@@:qB",NO,^id(IMP original,SEL sel) {
        return ^id(id object,long long style,BOOL premium) {
            UIImage *image=RVEnabled(@"custom_header") ? [UIImage imageWithContentsOfFile:[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"RVHeader.png"]] : nil;
            return image ?: ((id(*)(id,SEL,long long,BOOL))original)(object,sel,style,premium);
        };
    });
    RVHook(@"YTHeaderLogoControllerImpl",@"updateLogoWithImage:needsRescaling:withYoodle:",@"v@:@BB",NO,^id(IMP original,SEL sel) {
        return ^(id object,id image,BOOL rescale,BOOL yoodle) {
            UIImage *custom=RVEnabled(@"custom_header") ? [UIImage imageWithContentsOfFile:[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"RVHeader.png"]] : nil;
            ((void(*)(id,SEL,id,BOOL,BOOL))original)(object,sel,custom ?: image,custom ? YES : rescale,custom ? NO : yoodle);
        };
    });
    // Native constant providers return dark=1 and light=0 in this image.
    RVHook(@"YTPageStyleControllerImpl",@"pageStyle",@"q@:",NO,^id(IMP original,SEL sel) {
        return ^long long(id object) {
            NSString *theme=RVSetting(@"theme");
            if ([theme isEqual:@"dark"]) return 1;if ([theme isEqual:@"light"]) return 0;
            return ((long long(*)(id,SEL))original)(object,sel);
        };
    });
    for (NSString *selector in @[@"setPageStyle:",@"setEffectivePageStyle:"]) RVHook(@"YTPageStyleControllerImpl",selector,@"v@:q",NO,^id(IMP original,SEL sel) {
        return ^(id object,long long value) {
            NSString *theme=RVSetting(@"theme");if ([theme isEqual:@"dark"]) value=1;else if ([theme isEqual:@"light"]) value=0;
            ((void(*)(id,SEL,long long))original)(object,sel,value);
        };
    });
    RVBoolGate(@"YTMainAppControlsOverlayView",@"forceAutonavSwitchHidden",@"hide_autoplay_button",YES);
    RVHook(@"YTMainAppControlsOverlayView",@"setForceAutonavSwitchHidden:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id object,BOOL value) { ((void(*)(id,SEL,BOOL))original)(object,sel,value || RVEnabled(@"hide_autoplay_button")); };
    });
    for (NSString *cls in @[@"YTPlayerBarSegmentedProgressView",@"YTInlinePlayerBarView"]) RVHook(cls,@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) { ((void(*)(id,SEL))original)(object,sel);RVHideView(object,RVEnabled(@"hide_seekbar"));if ([cls isEqual:@"YTInlinePlayerBarView"]) { if (RVEnabled(@"tap_to_seek")) RVAttachBarTap(object);RVLayoutSponsorMarkers(object); } };
    });
    RVHook(@"YTMainAppVideoPlayerOverlayView",@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) {
            ((void(*)(id,SEL))original)(object,sel);
            NSDictionary *views=@{@"hide_cast_button":@"playbackRouteButton",@"hide_watermark":@"featuredChannelWatermarkImageView",
                @"hide_end_cards":@"creatorEndscreenView",@"hide_autoplay_preview":@"autonavPreviewView",@"hide_info_cards":@"teaserContainerView",@"hide_related_overlay":@"fullscreenEngagementOverlayView"};
            for (NSString *feature in views) RVHideView(RVObject(object,[views[feature] UTF8String]),RVEnabled(feature));
            RVHideView(RVObject(object,"autonavEndscreenView"),RVEnabled(@"hide_autoplay_preview"));
            RVHideView(RVObject(object,"autonavBackgroundView"),RVEnabled(@"hide_autoplay_preview"));
            RVHideView(RVObject(object,"previousButton"),RVEnabled(@"hide_previous_next"));
            RVHideView(RVObject(object,"nextButton"),RVEnabled(@"hide_previous_next"));
            id controls=RVObject(object,"controlsOverlayView");
            RVHideView(RVObject(controls,"autonavSwitch"),RVEnabled(@"hide_autoplay_button"));
            RVHideView(RVObject(controls,"closedCaptionsOrSubtitlesButton"),RVEnabled(@"hide_captions_button"));
            UIView *background=RVObject(object,"backgroundView");
            if ([background isKindOfClass:[UIView class]] && [RVSetting(@"overlay_opacity") doubleValue]<1) background.alpha=[RVSetting(@"overlay_opacity") doubleValue];
            if ([object isKindOfClass:[UIView class]]) RVAttachSwipes(object);
        };
    });
    RVHook(@"YTMainAppVideoPlayerOverlayView",@"setClosedCaptionsOrSubtitlesButtonAvailable:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id object,BOOL value) { ((void(*)(id,SEL,BOOL))original)(object,sel,RVEnabled(@"hide_captions_button") ? NO : value); };
    });
    RVHook(@"YTInlinePlayerBarContainerView",@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) {
            ((void(*)(id,SEL))original)(object,sel);
            RVHideView(RVObject(object,"progressView"),RVEnabled(@"hide_seekbar"));
            RVHideView(RVObject(object,"currentTimeLabel"),RVEnabled(@"hide_timestamp"));
            RVHideView(RVObject(object,"currentTimeTitleView"),RVEnabled(@"hide_timestamp"));
            RVHideView(RVObject(object,"durationLabel"),RVEnabled(@"hide_timestamp"));
        };
    });
    RVBoolGate(@"YTInlinePlayerBarContainerView",@"shouldShowDurationLabel",@"hide_timestamp",NO);
    RVBoolGate(@"YTInlinePlayerBarContainerView",@"shouldShowCurrentTitleLabel",@"hide_timestamp",NO);
    RVHook(@"YTInlinePlayerBarContainerView",@"setPlayedProgressBarColor:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id color) {
            NSString *hex=RVSetting(@"seekbar_color");
            if ([hex isKindOfClass:[NSString class]] && hex.length==7) {
                unsigned int rgb=0;[[NSScanner scannerWithString:[hex substringFromIndex:1]] scanHexInt:&rgb];
                color=[UIColor colorWithRed:((rgb>>16)&255)/255.0 green:((rgb>>8)&255)/255.0 blue:(rgb&255)/255.0 alpha:1];
            }
            ((void(*)(id,SEL,id))original)(object,sel,color);
        };
    });
    RVHook(@"YTAppPivotBarController",@"defaultSelectedPivotIdentifier",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            return RVNavigationDefaultSelection(((id(*)(id,SEL))original)(object,sel));
        };
    });
    RVHook(@"YTRollingNumberView",@"setUpdatedCount:updatedCountNumber:font:fontAttributes:color:skipAnimation:",@"v@:@@@@@B",NO,^id(IMP original,SEL sel) {
        return ^(id object,id count,id number,id font,id attributes,id color,BOOL skip) {
            RVElementDislikeEvent(@"native_number_update");
            void (^update)(void)=^{ ((void(*)(id,SEL,id,id,id,id,id,BOOL))original)(object,sel,count,number,font,attributes,color,skip); };
            // Preserve native initialization/rendering flags and every text argument.
            if (NSThread.isMainThread && RVEnabled(@"disable_rolling_numbers")) {
                [CATransaction begin];[CATransaction setDisableActions:YES];[UIView performWithoutAnimation:update];[CATransaction commit];
            } else update();
        };
    });
    RVHook(@"YTRollingNumberView",@"setUpdatedCount:updatedCountNumber:rollFromCount:rollFromCountNumber:font:fontAttributes:color:skipAnimation:",@"v@:@@@@@@@B",NO,^id(IMP original,SEL sel) {
        return ^(id object,id count,id number,id from,id fromNumber,id font,id attributes,id color,BOOL skip) {
            RVElementDislikeEvent(@"native_number_transition");
            void (^update)(void)=^{ ((void(*)(id,SEL,id,id,id,id,id,id,id,BOOL))original)(object,sel,count,number,from,fromNumber,font,attributes,color,skip); };
            if (NSThread.isMainThread && RVEnabled(@"disable_rolling_numbers")) {
                [CATransaction begin];[CATransaction setDisableActions:YES];[UIView performWithoutAnimation:update];[CATransaction commit];
            } else update();
        };
    });
}
static void RVInstallExtras(void) {
    RVInstallSpeedBridge();RVInstallNativeDislikes();RVInstallElementDislikes();RVInstallVoteLifecycle();RVInstallDislikeUI();RVInstallLinks();RVInstallPlayerExtras();RVInstallLayout(); }
