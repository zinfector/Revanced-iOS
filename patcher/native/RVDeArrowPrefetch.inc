// SPDX-License-Identifier: MIT
// Feed-owned cache warming; no native row/view leases and no response mutation.
@interface RVDAPrefetch : NSObject
@property(nonatomic,copy) NSString *video,*surface,*phase;
@property(nonatomic) double duration;
@property(nonatomic) uint64_t revision;
@property(nonatomic) NSTimeInterval expires;
@property(nonatomic) NSTimeInterval started;
@property(nonatomic) NSUInteger attempts;
@property(nonatomic) RVDeArrowParts parts;
@property(nonatomic,strong) RVDeArrowRequest *request;
@property(nonatomic,strong) RVDAHTTP *transfer;
@end
@implementation RVDAPrefetch @end
static NSMutableDictionary<NSString *,RVDAPrefetch *> *RVDAPrefetchEntries;
static NSUInteger RVDAPrefetchPendingPages;
static BOOL RVDAPrefetchCurrent(RVDAPrefetch *state) {
    return state && RVDAPrefetchEntries[state.video]==state && state.revision==RVDAUISettingsRevision &&
        state.expires>RVDANow() && RVCompatible && UIApplication.sharedApplication.applicationState==UIApplicationStateActive;
}
static void RVDAPrefetchCancelLeases(void) {
    for (RVDAPrefetch *state in RVDAPrefetchEntries.allValues) { [state.request cancel];[state.transfer cancel]; }
    [RVDAPrefetchEntries removeAllObjects];
}
static void RVDAPrefetchReset(void) {
    RVDAPrefetchCancelLeases();[RVDAUIAuxiliaryCache removeAllObjects];[RVDAUIPresentationCache removeAllObjects];
}
static void RVDAPrefetchFrame(RVDAPrefetch *state,NSURL *url,NSString *origin,NSNumber *timestamp) {
    if (!RVDAPrefetchCurrent(state) || state.transfer) return;
    if (!RVDAUIAuxiliaryCache) RVDAUIInitializeImageCaches();
    if ([[RVDAUIAuxiliaryCache objectForKey:url.absoluteString][@"expires"] doubleValue]>RVDANow()) { state.phase=@"cache_ready";return; }
    state.phase=@"frame_queued";state.attempts++;
    RVDAHTTP *ticket=[RVDAHTTP new];ticket.limit=RVDAImageLimit;ticket.background=YES;state.transfer=ticket;
    __weak RVDAPrefetch *weak=state;
    ticket.completion=^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker,NSInteger code) {
        dispatch_async(dispatch_get_main_queue(),^{
            RVDAPrefetch *current=weak;if (!RVDAPrefetchCurrent(current)) return;current.transfer=nil;
            NSString *failure=RVDAHeader(response,@"X-Failure-Reason").lowercaseString;
            if (blocker || response.statusCode!=200) {
                BOOL pending=timestamp && response.statusCode==204 && ([failure containsString:@"not generated"] || [failure containsString:@"not cached"] || [failure containsString:@"queue"] || [failure containsString:@"timeout"]);
                current.phase=pending ? @"server_generation_pending" : @"frame_unavailable";
                RVDADiagCount(pending ? @"prefetch.frame_generation_pending" : @"prefetch.frame_unavailable");
                RVDADiagRecord(@"F04_prefetch_frame_response",nil,nil,@{@"video_hash":RVDADiagVideo(current.video),@"http_status":@(response.statusCode),@"network_error_code":@(code),@"phase":current.phase,@"attempt":@(current.attempts)});
                // Short speculative burst; the visible-row recovery scheduler owns
                // longer server backoff. Never retry a speculative failure forever.
                if ((pending || RVDAUIRetryableTransfer(blocker,response.statusCode)) && current.attempts<4) {
                    double retryAfter=RVDAHeader(response,@"Retry-After").doubleValue;
                    NSTimeInterval delay=MAX(4*pow(2,current.attempts-1),isfinite(retryAfter) ? MIN(300,MAX(0,retryAfter)) : 0);
                    if ([failure containsString:@"failed to generate"]) delay=300;
                    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(delay*NSEC_PER_SEC)),dispatch_get_main_queue(),^{ RVDAPrefetchFrame(weak,url,origin,timestamp); });
                }return;
            }
            NSNumber *returned=nil;
            if (timestamp) {
                NSScanner *scan=[NSScanner scannerWithString:RVDAHeader(response,@"X-Timestamp") ?: @""];scan.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];double value=0;
                if (![scan scanDouble:&value] || !scan.isAtEnd || !isfinite(value) || fabs(value-timestamp.doubleValue)>.001) { current.phase=@"timestamp_rejected";RVDADiagCount(@"prefetch.timestamp_rejected");return; }
                returned=@(value);
            }
            current.phase=@"decoding";
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
                @autoreleasepool {
                    NSUInteger cost=0;UIImage *image=[RVDeArrowBackend.sharedBackend decodeImage:data response:response cost:&cost];
                    if ([origin isEqual:@"youtube_frame"] && (image.size.width<240 || image.size.height<135)) image=nil;
                    dispatch_async(dispatch_get_main_queue(),^{
                        RVDAPrefetch *latest=weak;if (!RVDAPrefetchCurrent(latest)) return;
                        if (!image) { latest.phase=@"decode_rejected";RVDADiagCount(@"prefetch.decode_rejected");return; }
                        [RVDAUIAuxiliaryCache setObject:@{@"image":image,@"expires":@(RVDANow()+1800),@"timestamp":returned ?: NSNull.null} forKey:url.absoluteString cost:cost];
                        RVDAUIPrepareImage(image,origin,^{ if (RVDAPrefetchCurrent(weak)) { weak.phase=@"frame_ready";RVDADiagCount(@"prefetch.frame_ready"); } });
                    });
                }
            });
        });
    };
    [RVDeArrowBackend.sharedBackend enqueueAuxiliary:ticket URL:url timeout:45];
}
static void RVDAPrefetchVideo(NSString *video,double duration,NSString *surface,BOOL titleRole,BOOL imageRole) {
    if (!RVVideoIDValid(video) || [RVDAUIOriginalVideos containsObject:video] ||
        (RVEnabled(@"dearrow_default_original") && ![RVDAUICustomVideos containsObject:video])) return;
    RVDeArrowParts parts=(titleRole && RVDAUIPolicy(surface,YES) ? RVDeArrowPartsTitle : 0)|(imageRole && RVDAUIPolicy(surface,NO) ? RVDeArrowPartsThumbnail : 0);
    if (!RVEnabled(@"dearrow_use_crowdsourced_titles") && ![RVSetting(@"dearrow_mode") isEqual:@"casual"]) parts&=~RVDeArrowPartsTitle;
    if (!parts) return;
    if (!RVDAPrefetchEntries) RVDAPrefetchEntries=[NSMutableDictionary dictionary];
    RVDAPrefetch *prior=RVDAPrefetchEntries[video];
    if (prior && RVDAPrefetchCurrent(prior) && (parts&~prior.parts)==0 &&
        !([prior.phase isEqual:@"duration_missing"] && duration>0 && duration<=604800)) { RVDADiagCount(@"prefetch.video_coalesced");return; }
    if (prior) { [prior.request cancel];[prior.transfer cancel];[RVDAPrefetchEntries removeObjectForKey:video]; }
    // Bound speculative leases independently of the decoded/backend cache limits.
    while (RVDAPrefetchEntries.count>=48) {
        RVDAPrefetch *old=nil;for (RVDAPrefetch *item in RVDAPrefetchEntries.allValues) if (!old || item.expires<old.expires) old=item;
        [old.request cancel];[old.transfer cancel];[RVDAPrefetchEntries removeObjectForKey:old.video];RVDADiagCount(@"prefetch.lease_evicted");
    }
    RVDAPrefetch *state=[RVDAPrefetch new];state.video=video;state.surface=surface;state.parts=parts;state.duration=duration;state.revision=RVDAUISettingsRevision;state.started=NSProcessInfo.processInfo.systemUptime;state.expires=RVDANow()+600;state.phase=@"metadata_queued";RVDAPrefetchEntries[video]=state;
    RVDADiagCount(@"prefetch.video_admitted");__weak RVDAPrefetch *weak=state;
    state.request=[RVDeArrowBackend.sharedBackend prefetchVideoID:video parts:parts clientGeneration:state.revision completion:^(RVDeArrowResult *result) {
        RVDAPrefetch *current=weak;if (!RVDAPrefetchCurrent(current)) return;
        if (result.configurationRevision!=RVDeArrowBackend.sharedBackend.configurationRevision || ![result.videoID isEqual:current.video]) return;
        current.phase=result.isFinal ? @"metadata_ready" : @"community_frame_pending";
        RVDADiagCount(@"prefetch.metadata_received");
        RVDADiagRecord(@"F03_prefetch_metadata_received",nil,nil,@{@"video_hash":RVDADiagVideo(current.video),@"surface":current.surface,@"elapsed_ms":@(1000*(NSProcessInfo.processInfo.systemUptime-current.started)),@"metadata_http":@(result.metadataHTTPStatus),@"metadata_cache_hit":@(result.metadataCacheHit),@"title_decision":RVDADecisionName(result.titleDecision),@"image_decision":RVDADecisionName(result.thumbnailDecision),@"image_http":@(result.imageHTTPStatus),@"image_ready":@(result.thumbnailImage!=nil),@"final":@(result.isFinal)});
        if (result.isFinal) current.request=nil;
        if (result.thumbnailImage && result.imageState==RVDeArrowImageStateReady) {
            RVDAUIPrepareImage(result.thumbnailImage,@"community",^{ if (RVDAPrefetchCurrent(weak)) { weak.phase=@"community_ready";RVDADiagCount(@"prefetch.community_ready"); } });return;
        }
        if (!result.isFinal || !(parts&RVDeArrowPartsThumbnail) || result.thumbnailDecision==RVDeArrowDecisionOriginal || result.thumbnailDecision==RVDeArrowDecisionDisabled) return;
        NSString *mode=RVSetting(@"dearrow_fallback");
        if ([mode isEqual:@"random_time"]) {
            double length=result.videoDuration.doubleValue;if (!(length>0 && length<=604800)) length=current.duration;
            NSNumber *timestamp=nil;NSURL *url=RVDAUIRandomURL(current.video,result,length,&timestamp);
            if (url) RVDAPrefetchFrame(current,url,@"random_time",timestamp);
            else { current.phase=@"duration_missing";RVDADiagCount(@"prefetch.duration_missing"); }
        } else if ([@[@"youtube_frame",@"stills"] containsObject:mode]) {
            NSURL *url=[NSURL URLWithString:[NSString stringWithFormat:@"https://i.ytimg.com/vi/%@/hq%d.jpg",current.video,[RVSetting(@"thumbnail_frame") intValue]]];RVDAPrefetchFrame(current,url,@"youtube_frame",nil);
        }
    }];
}
static void RVDAPrefetchCollect(id root,NSUInteger depth,NSUInteger *budget,NSUInteger *bytes,NSMutableArray *snapshots,NSHashTable *seen) {
    if (!root || depth>8 || !*budget || snapshots.count>=32 || [seen containsObject:root]) return;
    (*budget)--;[seen addObject:root];
    if ([root isKindOfClass:NSArray.class]) { if ([root count]<=4096) for (id child in root) RVDAPrefetchCollect(child,depth+1,budget,bytes,snapshots,seen);return; }
    id element=RVAdRootElement(root);
    if (element) {
        id options=RVAdResolvedObject(element,"compatibilityOptions");
        if (!RVAdResolve(options,"hasAdLoggingData",@"B@:") || RVBool(options,"hasAdLoggingData")) return;
        NSData *data=RVAdResolvedObject(element,"elementData");
        if ([data isKindOfClass:NSData.class] && data.length && data.length<=2*1024*1024 && data.length<=8*1024*1024-*bytes) { *bytes+=data.length;[snapshots addObject:@{@"data":data.copy}]; }return;
    }
    id renderer=RVDAUILegacyRenderer(root,NULL);
    if (renderer && RVAdResolve(renderer,"isAd",@"B@:") && !RVBool(renderer,"isAd")) {
        NSString *video=RVAdResolvedObject(renderer,"videoId");
        if (RVVideoIDValid(video)) [snapshots addObject:@{@"video":video,@"duration":@(RVDADurationText(RVPlainText(RVAdResolvedObject(renderer,"lengthText"))))}];return;
    }
    NSString *name=NSStringFromClass([root class]);NSArray *fields=RVAdFeedChildren()[name];
    if ([root isKindOfClass:NSClassFromString(@"YTArrayViewModel")]) fields=@[@"entries"];
    for (NSString *field in fields) {
        BOOL repeated=[field hasSuffix:@"Array"] || [field isEqual:@"entries"];
        if (!repeated && (!RVAdResolve(root,RVAdPresence(field).UTF8String,@"B@:") || !RVBool(root,RVAdPresence(field).UTF8String))) continue;
        if (RVAdResolve(root,field.UTF8String,@"@@:")) RVDAPrefetchCollect(RVAdResolvedObject(root,field.UTF8String),depth+1,budget,bytes,snapshots,seen);
    }
}
static void RVDAPrefetchIncoming(id owner,id root,NSString *route) {
    if (!NSThread.isMainThread || !RVCompatible || UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return;
    NSString *surface=RVDAUIOwnerSurface(owner,NO,NULL);
    if (!surface || (!RVDAUIPolicy(surface,YES) && !RVDAUIPolicy(surface,NO))) return;
    RVDADiagCount(@"prefetch.page_received");
    if (RVDAPrefetchPendingPages>=4) { RVDADiagCount(@"prefetch.page_budget_full");return; }
    NSMutableArray *snapshots=[NSMutableArray array];NSUInteger budget=256,bytes=0;
    RVDAPrefetchCollect(root,0,&budget,&bytes,snapshots,[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality]);
    RVDADiagRecord(@"F01_feed_page_received",nil,nil,@{@"route":route,@"surface":surface,@"snapshots":@(snapshots.count),@"serialized_bytes":@(bytes),@"walk_budget_exhausted":@(!budget)});
    if (!snapshots.count) { RVDADiagCount(@"prefetch.page_no_candidates");return; }
    RVDAPrefetchPendingPages++;uint64_t revision=RVDAUISettingsRevision;NSArray *owned=snapshots.copy;
    static dispatch_queue_t queue;static dispatch_once_t once;dispatch_once(&once,^{ queue=dispatch_queue_create("RVPort.DeArrow.FeedPrefetch",dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,QOS_CLASS_UTILITY,0)); });
    dispatch_async(queue,^{
        @autoreleasepool {
            NSMutableArray *candidates=[NSMutableArray array];
            for (NSDictionary *snapshot in owned) {
                if (snapshot[@"video"]) { [candidates addObject:@{@"video":snapshot[@"video"],@"duration":snapshot[@"duration"],@"title":@YES,@"image":@YES}];continue; }
                for (RVDAProjection *profile in RVDAProfiles(snapshot[@"data"],nil)) if (!profile.watchTitle && RVVideoIDValid(profile.video) && candidates.count<48)
                    [candidates addObject:@{@"video":profile.video,@"duration":@(profile.duration),@"title":@(profile.titleRole),@"image":@(profile.imageRole)}];
            }
            dispatch_async(dispatch_get_main_queue(),^{
                if (RVDAPrefetchPendingPages) RVDAPrefetchPendingPages--;
                if (revision!=RVDAUISettingsRevision || UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return;
                RVDADiagRecord(@"F02_feed_identities_resolved",nil,nil,@{@"route":route,@"surface":surface,@"candidates":@(candidates.count)});
                for (NSDictionary *candidate in candidates) RVDAPrefetchVideo(candidate[@"video"],[candidate[@"duration"] doubleValue],surface,[candidate[@"title"] boolValue],[candidate[@"image"] boolValue]);
            });
        }
    });
}
static NSDictionary *RVDAPrefetchReport(void) {
    NSMutableDictionary *phases=[NSMutableDictionary dictionary];NSMutableArray *samples=[NSMutableArray array];
    for (RVDAPrefetch *state in RVDAPrefetchEntries.allValues) {
        phases[state.phase ?: @"unknown"]=@([phases[state.phase ?: @"unknown"] unsignedIntegerValue]+1);
        if (samples.count<24) [samples addObject:@{@"video_hash":RVDADiagVideo(state.video),@"surface":state.surface,@"phase":state.phase ?: @"unknown",@"parts":@(state.parts),@"frame_attempts":@(state.attempts),@"native_duration_available":@(state.duration>0),@"age_seconds":@(MAX(0,NSProcessInfo.processInfo.systemUptime-state.started)),@"lease_current":@(RVDAPrefetchCurrent(state))}];
    }
    return @{@"leases":@(RVDAPrefetchEntries.count),@"pending_pages":@(RVDAPrefetchPendingPages),@"phases":phases,@"samples":samples,@"page_snapshot_limit":@32,@"lease_limit":@48,@"started_at_feed_delivery":@YES};
}
static void RVDAPrefetchInstall(void) {
    [NSNotificationCenter.defaultCenter addObserverForName:UIApplicationDidEnterBackgroundNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) { RVDAPrefetchCancelLeases(); }];
    [NSNotificationCenter.defaultCenter addObserverForName:UIApplicationDidReceiveMemoryWarningNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) { RVDAPrefetchReset(); }];
    // Same verified 21.39.4 ABI contracts as the ad feed adapter. Chained
    // originals forward all model/return/error objects unchanged.
    for (NSString *cls in @[@"YTSectionListViewController",@"YTInnerTubeCollectionViewController",@"YTSectionController"]) RVDAUIHook(cls,@"loadWithModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id owner,id model) { RVDAPrefetchIncoming(owner,model,@"initial_or_section_load");((void(*)(id,SEL,id))original)(owner,sel,model); };
    });
    RVDAUIHook(@"YTInnerTubeCollectionViewController",@"addSectionsFromArray:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id owner,id entries) { RVDAPrefetchIncoming(owner,entries,@"append_sections");((void(*)(id,SEL,id))original)(owner,sel,entries); };
    });
    RVDAUIHook(@"YTInnerTubeCollectionViewController",@"insertSections:byPosition:error:",@"B@:@i^@",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id owner,id entries,int position,id __autoreleasing *error) { RVDAPrefetchIncoming(owner,entries,@"insert_sections");return ((BOOL(*)(id,SEL,id,int,id __autoreleasing *))original)(owner,sel,entries,position,error); };
    });
    RVDAUIHook(@"YTInnerTubeCollectionViewController",@"insertContents:byPosition:inSection:error:",@"@@:@i@^@",NO,^id(IMP original,SEL sel) {
        return ^id(id owner,id entries,int position,id section,id __autoreleasing *error) { RVDAPrefetchIncoming(owner,entries,@"continuation_contents");return ((id(*)(id,SEL,id,int,id,id __autoreleasing *))original)(owner,sel,entries,position,section,error); };
    });
}
