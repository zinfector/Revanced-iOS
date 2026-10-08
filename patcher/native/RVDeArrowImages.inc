// SPDX-License-Identifier: MIT
// Decoded fallback/effect requests are per-video, bounded and generation guarded.
// The service's timeout/queue responses also contain "failed to generate".
// Classify the specific transient cause first, never both pending and failed.
typedef NS_ENUM(NSUInteger,RVDAFrameFailure) {
    RVDAFrameOther, RVDAFramePending, RVDAFrameNotCached, RVDAFrameTimeout,
    RVDAFrameQueueBusy, RVDAFrameWorkerFailed, RVDAFrameUnavailable
};
static RVDAFrameFailure RVDAFrameClassify(NSHTTPURLResponse *response,NSNumber *timestamp) {
    if (!timestamp || response.statusCode!=204) return RVDAFrameOther;
    NSString *reason=RVDAHeader(response,@"X-Failure-Reason").lowercaseString;
    if ([reason containsString:@"timeout"] || [reason containsString:@"timed out"]) return RVDAFrameTimeout;
    if ([reason containsString:@"queue"]) return RVDAFrameQueueBusy;
    if ([reason containsString:@"not generated"]) return RVDAFramePending;
    if ([reason containsString:@"not cached"]) return RVDAFrameNotCached;
    if ([reason containsString:@"failed to generate"]) return RVDAFrameWorkerFailed;
    return RVDAFrameUnavailable;
}
static NSString *RVDAFrameFailureName(RVDAFrameFailure failure) {
    switch (failure) {
        case RVDAFramePending:return @"not_generated_yet";
        case RVDAFrameNotCached:return @"not_cached";
        case RVDAFrameTimeout:return @"generation_timeout";
        case RVDAFrameQueueBusy:return @"queue_busy";
        case RVDAFrameWorkerFailed:return @"generation_failed";
        case RVDAFrameUnavailable:return @"generation_unavailable";
        default:return @"other";
    }
}
static BOOL RVDAFrameTransient(RVDAFrameFailure failure) {
    return failure!=RVDAFrameOther && failure!=RVDAFrameWorkerFailed;
}
static NSTimeInterval RVDAFrameRetryAfter(NSHTTPURLResponse *response) {
    NSString *header=RVDAHeader(response,@"Retry-After");
    NSScanner *scan=[NSScanner scannerWithString:header ?: @""];scan.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];double value=0;
    if ([scan scanDouble:&value] && scan.isAtEnd && isfinite(value) && value>0) return MIN(300,value);
    // Retry-After may be an HTTP date rather than delta seconds.
    static NSDateFormatter *formatter;static dispatch_once_t once;dispatch_once(&once,^{
        formatter=[NSDateFormatter new];formatter.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
        formatter.timeZone=[NSTimeZone timeZoneForSecondsFromGMT:0];formatter.dateFormat=@"EEE, dd MMM yyyy HH:mm:ss zzz";
    });
    NSDate *date=[formatter dateFromString:header ?: @""];return date ? MIN(300,MAX(0,date.timeIntervalSinceNow)) : 0;
}
static NSCache *RVDAUIAuxiliaryCache;
static NSCache *RVDAUIPresentationCache;
static void RVDAUIInitializeImageCaches(void) {
    if (!RVDAUIAuxiliaryCache) { RVDAUIAuxiliaryCache=[NSCache new];RVDAUIAuxiliaryCache.countLimit=128;RVDAUIAuxiliaryCache.totalCostLimit=24*1024*1024; }
    if (!RVDAUIPresentationCache) { RVDAUIPresentationCache=[NSCache new];RVDAUIPresentationCache.countLimit=64;RVDAUIPresentationCache.totalCostLimit=24*1024*1024; }
}
static NSString *RVDAUIPresentationKey(UIImage *image,NSString *origin,NSInteger saturation) {
    return [NSString stringWithFormat:@"%p|%ld|%d",image,(long)saturation,[origin isEqual:@"upcoming_avatar"]];
}
static NSDictionary *RVDAUIPreparedImage(UIImage *image,NSString *origin) {
    RVDAUIInitializeImageCaches();NSInteger saturation=[origin isEqual:@"pending"] ? 100 : [RVSetting(@"dearrow_thumbnail_saturation") integerValue];
    NSDictionary *cached=[RVDAUIPresentationCache objectForKey:RVDAUIPresentationKey(image,origin,saturation)];
    return cached[@"source"]==image ? cached : nil;
}
static NSMutableDictionary *RVDAUIPreparingImages;
static void RVDAUIPrepareImage(UIImage *image,NSString *origin,void (^completion)(void)) {
    if (!image || !NSThread.isMainThread) return;
    if (RVDAUIPreparedImage(image,origin)) { if (completion) completion();return; }
    NSInteger saturation=[origin isEqual:@"pending"] ? 100 : [RVSetting(@"dearrow_thumbnail_saturation") integerValue];uint64_t settingsRevision=RVDAUISettingsRevision;
    NSString *key=[NSString stringWithFormat:@"%llu|%@",(unsigned long long)settingsRevision,RVDAUIPresentationKey(image,origin,saturation)];
    if (!RVDAUIPreparingImages) RVDAUIPreparingImages=[NSMutableDictionary dictionary];
    NSMutableArray *callbacks=RVDAUIPreparingImages[key];
    if (callbacks) { if (completion) [callbacks addObject:[completion copy]];RVDADiagCount(@"performance.image_preparation_coalesced");return; }
    callbacks=[NSMutableArray array];if (completion) [callbacks addObject:[completion copy]];RVDAUIPreparingImages[key]=callbacks;
    [RVDAImageWorkQueue() addOperationWithBlock:^{
        @autoreleasepool {
            NSTimeInterval started=NSProcessInfo.processInfo.systemUptime;
            UIImage *source=[origin isEqual:@"upcoming_avatar"] ? RVDACoverImage(image) : image;
            UIImage *colored=RVDAColorImage(source,saturation);NSData *bytes=UIImageJPEGRepresentation(colored,.9);
            RVDADiagTiming(@"image_preparation",started);
            dispatch_async(dispatch_get_main_queue(),^{
                if (RVDAUIPreparingImages[key]!=callbacks) return;
                [RVDAUIPreparingImages removeObjectForKey:key];
                if (settingsRevision!=RVDAUISettingsRevision) return;
                if (bytes.length && bytes.length<=1024*1024) {
                    NSUInteger cost=bytes.length+CGImageGetBytesPerRow(image.CGImage)*CGImageGetHeight(image.CGImage)+CGImageGetBytesPerRow(colored.CGImage)*CGImageGetHeight(colored.CGImage);
                    [RVDAUIPresentationCache setObject:@{@"source":image,@"image":colored,@"bytes":bytes} forKey:RVDAUIPresentationKey(image,origin,saturation) cost:cost];
                }
                for (void (^callback)(void) in callbacks.copy) callback();
            });
        }
    }];
}
#include "RVDeArrowFrameCache.inc"
static void RVDAUIRandomImage(RVDAUIBinding *state,uint64_t generation);
static void RVDAUIRandomGenerate(RVDAUIBinding *state,uint64_t generation);
static void RVDAUIResolveFallback(RVDAUIBinding *state,uint64_t generation);
static BOOL RVDAUIUsePrefetchedFallback(RVDAUIBinding *state,uint64_t generation);
static NSTimer *RVDAUIFallbackTimer;
static BOOL RVDAUIFallbackVisible(RVDAUIBinding *state) {
    if (UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return NO;
    if (RVDAUIConsumerVisible(state.nativeThumbnailNode) || RVDAUIConsumerVisible(state.nativeFactoryRoot)) return YES;
    UIView *view=state.imageView;
    if (![view isKindOfClass:UIView.class] || !view.window || CGRectIsEmpty(view.bounds)) return NO;
    for (UIView *parent=view;parent;parent=parent.superview) if (parent.hidden || parent.alpha<.05) return NO;
    return !CGRectIsEmpty(CGRectIntersection([view convertRect:view.bounds toView:view.window],view.window.bounds));
}
// A native rematerialization can briefly detach an otherwise visible row.
// Keep one existing burst alive through that gap; do not treat it as recycling.
static BOOL RVDAUIFallbackMayRun(RVDAUIBinding *state) {
    if (UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return NO;
    NSTimeInterval now=NSProcessInfo.processInfo.systemUptime;
    if (RVDAUIFallbackVisible(state)) { state.fallbackLastVisible=now;return YES; }
    return state.fallbackLastVisible>0 && now-state.fallbackLastVisible<3;
}
static BOOL RVDAUIFallbackPending(RVDAUIBinding *state) {
    return state.result.isFinal && (state.parts&RVDeArrowPartsThumbnail) &&
        [@[@"waiting_visibility",@"deferred",@"selecting",@"queued",@"fetching",@"retry_wait"] containsObject:state.fallbackPhase];
}
static void RVDAUIFallbackEnsureTimer(void) {
    if (RVDAUIFallbackTimer) return;
    RVDAUIFallbackTimer=[NSTimer timerWithTimeInterval:2 repeats:YES block:^(NSTimer *timer) {
        BOOL pending=NO;
        for (RVDAUIBinding *state in RVDAUIBindings.allObjects) {
            if (!RVDAUIFallbackPending(state) || RVDAUIOriginal(state) || !RVDAUICurrent(state)) continue;
            pending=YES;RVDAUIFallbackWake(state);
        }
        if (!pending) { [timer invalidate];RVDAUIFallbackTimer=nil; }
    }];
    [NSRunLoop.mainRunLoop addTimer:RVDAUIFallbackTimer forMode:NSRunLoopCommonModes];
}
static void RVDAUIFallbackDefer(RVDAUIBinding *state,NSString *reason,NSTimeInterval cooldown) {
    // Retire this burst, not the row binding or its title/consumer permissions.
    state.fallbackEpoch++;state.fallbackStarted=NO;state.fallbackRetryScheduled=NO;state.fallbackDeadline=0;
    state.fallbackNextAttempt=MAX(state.fallbackNextAttempt,RVDANow()+MAX(2,cooldown));
    state.fallbackPhase=cooldown>0 ? @"deferred" : @"waiting_visibility";state.imageBlocker=reason;
    RVDAHTTP *transfer=state.fallbackTransfer;state.fallbackTransfer=nil;[transfer cancel];
    BOOL restorePending=state.presentationImage && [state.imageOrigin isEqual:@"pending"];
    if ([state.imageOrigin isEqual:@"pending"] || state.encodedImage==RVDABlankImage()) {
        state.imageRevision++;state.presentationImage=nil;state.imageBytes=nil;state.encodedImage=nil;state.imageOrigin=nil;
    }
    state.pendingExpired=YES;
    RVDADiagRecord(@"D08_fallback_deferred",state,reason,@{@"recoverable":@YES,@"cooldown_seconds":@(cooldown),@"cycle":@(state.fallbackCycles),@"row_visible":@(RVDAUIFallbackVisible(state))});
    RVDAUIFallbackEnsureTimer();
    // Most deferrals change scheduler state only. Reloading an unchanged native
    // row here creates a load -> invisible -> pause -> reload feedback loop.
    if (restorePending) RVDAUIRefresh(state);
}
static NSTimeInterval RVDAUIFallbackCooldown(RVDAUIBinding *state) {
    BOOL transient=[@[@"not_generated_yet",@"not_cached",@"generation_timeout",@"queue_busy",@"generation_unavailable"] containsObject:state.fallbackResponseReason];
    // A finite burst still yields to the shared service; temporary generation
    // delays never accumulate the five-minute worker-failure penalty.
    return MAX(state.fallbackRetryAfter,MIN(transient ? 60 : 300,30*pow(2,MIN(state.fallbackExhaustions ? state.fallbackExhaustions-1 : 0,4u))));
}
static void RVDAUIFallbackExhausted(RVDAUIBinding *state,NSString *reason) {
    state.fallbackExhaustions++;
    RVDAUIFallbackDefer(state,reason,RVDAUIFallbackCooldown(state));
}
static void RVDAUIFallbackWake(RVDAUIBinding *state) {
    if (!NSThread.isMainThread || !RVDAUIFallbackPending(state) || RVDAUIOriginal(state) || !RVDAUICurrent(state)) return;
    if (!state.fallbackStarted && RVDAUIUsePrefetchedFallback(state,state.generation)) return;
    if (!RVDAUIFallbackMayRun(state)) {
        if (state.fallbackStarted) RVDAUIFallbackDefer(state,@"fallback_waiting_visibility",0);
        return;
    }
    if (state.fallbackStarted) {
        if (state.fallbackDeadline && RVDANow()>=state.fallbackDeadline)
            RVDAUIFallbackExhausted(state,@"fallback_burst_deferred");
        return;
    }
    if (RVDANow()<state.fallbackNextAttempt) return;
    RVDADiagRecord(@"D08_fallback_resumed",state,nil,@{@"prior_cycle":@(state.fallbackCycles),@"row_visible":@(RVDAUIFallbackVisible(state)),@"visibility_grace":@(!RVDAUIFallbackVisible(state))});
    RVDAUIResolveFallback(state,state.generation);
}
static void RVDAUIFallbackFailure(RVDAUIBinding *state,NSString *blocker,BOOL retryable,uint64_t generation,void (^retry)(RVDAUIBinding *)) {
    if (!state || state.generation!=generation || !RVDAUICurrent(state)) return;
    state.imageBlocker=blocker;
    NSTimeInterval remaining=state.fallbackDeadline-RVDANow();
    NSTimeInterval delay=MAX(MIN(16,2*pow(2,MIN(state.fallbackRetries,4u))),state.fallbackRetryAfter);
    if (retryable && !RVDAUIFallbackMayRun(state)) { RVDAUIFallbackDefer(state,@"fallback_waiting_visibility",delay);return; }
    if (retryable && retry && !state.fallbackRetryScheduled && state.fallbackRetries<12 && remaining>delay) {
        state.fallbackRetries++;state.fallbackRetryScheduled=YES;state.fallbackPhase=@"retry_wait";
        RVDADiagRecord(@"D08_fallback_retry_scheduled",state,blocker,@{@"retry_attempt":@(state.fallbackRetries),@"delay_seconds":@(delay),@"remaining_seconds":@(MAX(0,remaining)),@"cycle":@(state.fallbackCycles),@"retry_limit":@12});
        __weak RVDAUIBinding *weak=state;uint64_t epoch=state.fallbackEpoch;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(delay*NSEC_PER_SEC)),dispatch_get_main_queue(),^{
            RVDAUIBinding *current=weak;if (!current || current.generation!=generation || current.fallbackEpoch!=epoch) return;
            current.fallbackRetryScheduled=NO;
            NSString *stale=RVDAUICurrentBlocker(current);
            if (stale || RVDAUIOriginal(current)) { RVDAUIEvent(@"D11_fallback_retry_discarded",current,stale ?: @"original_selected");return; }
            if (!RVDAUIFallbackMayRun(current)) { RVDAUIFallbackDefer(current,@"fallback_waiting_visibility",0);return; }
            if (RVDANow()>=current.fallbackDeadline) { RVDAUIFallbackExhausted(current,@"fallback_burst_deferred");return; }
            retry(current);
        });
    } else if (retryable) {
        RVDAUIFallbackExhausted(state,@"fallback_generation_deferred");
    } else {
        state.fallbackEpoch++;state.fallbackStarted=NO;state.fallbackRetryScheduled=NO;state.fallbackPhase=@"failed";
        RVDADiagRecord(@"D08_fallback_rejected",state,blocker,@{@"last_response_blocker":blocker ?: @"unknown",@"recoverable":@NO,@"retries":@(state.fallbackRetries)});RVDAUIRefresh(state);
    }
}
static BOOL RVDAUIRetryableTransfer(RVDeArrowBlocker blocker,NSInteger status) {
    return blocker==RVDeArrowBlockerNetwork || blocker==RVDeArrowBlockerBackoff || blocker==RVDeArrowBlockerQueueFull || status==429 || status>=500;
}
static double RVDAUIOwnedDuration(RVDAUIBinding *state) {
    double duration=state.result.videoDuration.doubleValue;
    state.durationSource=@"dearrow_metadata";
    if (!(duration>0 && duration<=604800) && RVOrdinaryContent(RVCurrentPlayback) &&
        [state.video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")] && RVCan(RVCurrentPlayer,"currentVideoTotalMediaTime",@"d@:")) {
        duration=RVDouble(RVCurrentPlayer,"currentVideoTotalMediaTime");state.durationSource=@"matching_native_playback";
    }
    if (!(duration>0 && duration<=604800)) { duration=state.duration;state.durationSource=@"native_thumbnail_timestamp_approximate"; }
    if (!(isfinite(duration) && duration>0 && duration<=604800)) { duration=0;state.durationSource=@"unavailable"; }
    RVDADiagRecord(@"D08_duration_resolved",state,duration ? nil : @"fallback_duration_unavailable",@{@"duration_present":@(duration>0),@"duration_source":state.durationSource,@"video_scoped":@YES});
    return duration;
}
static void RVDAUIApplyImage(RVDAUIBinding *state,UIImage *image,NSString *origin,uint64_t generation) {
    if (!image || RVDAUIOriginal(state) || state.generation!=generation || !RVDAUICurrent(state)) return;
    if (state.encodedImage==image && [state.imageOrigin isEqual:origin] && state.imageBytes) { RVDADiagCount(@"performance.image_already_prepared");return; }
    uint64_t revision=++state.imageRevision;state.encodedImage=image;
    if (![origin isEqual:@"pending"]) state.fallbackPhase=@"encoding";
    __weak RVDAUIBinding *weak=state;
    RVDAUIPrepareImage(image,origin,^{
        RVDAUIBinding *current=weak;NSString *blocker=RVDAUICurrentBlocker(current);
        if (!blocker && (current.generation!=generation || current.imageRevision!=revision || RVDAUIOriginal(current))) blocker=@"image_effect_generation_changed";
        if (blocker) { RVDAUIEvent(@"D08_image_encoding_rejected",current,blocker);return; }
        NSDictionary *prepared=RVDAUIPreparedImage(image,origin);
        if (!prepared) { current.fallbackPhase=@"failed";current.imageBlocker=@"presentation_image_byte_limit";RVDAUIEvent(@"D08_image_received",current,current.imageBlocker);return; }
        current.presentationImage=prepared[@"image"];current.imageBytes=prepared[@"bytes"];current.imageOrigin=origin;
        if (![origin isEqual:@"pending"]) current.fallbackPhase=@"ready";
        RVDADiagRecord(@"D08_image_encoded",current,nil,@{@"byte_length":@(current.imageBytes.length),@"image_origin":origin,@"preparation_shared":@YES});RVDAUIRefresh(current);
    });
}
static void RVDAUIAuxiliary(RVDAUIBinding *state,NSURL *url,NSUInteger limit,uint64_t generation,void (^completion)(NSData *,NSHTTPURLResponse *,RVDeArrowBlocker)) {
    if (!url || !RVDAUICurrent(state) || state.generation!=generation) return;
    RVDAHTTP *transfer=[RVDAHTTP new];transfer.limit=limit;state.fallbackTransfer=transfer;
    state.fallbackPhase=@"queued";RVDAUIEvent(@"D08_fallback_queued",state,nil);
    __weak RVDAUIBinding *weak=state;__weak RVDAHTTP *weakTransfer=transfer;uint64_t epoch=state.fallbackEpoch;
    transfer.onStart=^{ dispatch_async(dispatch_get_main_queue(),^{
        RVDAUIBinding *current=weak;if (!current || current.generation!=generation || current.fallbackEpoch!=epoch || !RVDAUICurrent(current)) return;
        current.fallbackPhase=@"fetching";RVDAUIEvent(@"D08_fallback_transport_started",current,nil);
    }); };
    transfer.completion=^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker,NSInteger code) {
        NSDictionary *network=weakTransfer.networkDetails ?: @{};
        dispatch_async(dispatch_get_main_queue(),^{
            RVDAUIBinding *current=weak;
            if (!current || current.generation!=generation || current.fallbackEpoch!=epoch || !current.fallbackTransfer || current.fallbackTransfer!=weakTransfer || !RVDAUICurrent(current)) return;
            current.fallbackTransfer=nil;current.fallbackNetworkErrorCode=code;
            RVDADiagRecord(@"D08_fallback_transport_finished",current,blocker ? RVDABlockerName(blocker) : nil,@{@"http_status":@(response.statusCode),@"network_error_code":@(code),@"network_details":network,@"endpoint_host_hash":[RVDAHash(url.host) substringToIndex:12],@"livestream":@(current.livestream),@"timestamp_requested":current.fallbackTimestampRequested ?: NSNull.null});
            completion(data,response,blocker);
        });
    };
    RVDAFrameEnqueue(transfer,url,limit==RVDAImageLimit ? 45 : 15);
}
static void RVDAUIFetchImage(RVDAUIBinding *state,NSURL *url,NSString *origin,NSNumber *timestamp,uint64_t generation) {
    if (!url || RVDAUIOriginal(state) || state.generation!=generation || !RVDAUICurrent(state)) return;
    BOOL cachedRandom=[origin isEqual:@"cached_random_time"];
    state.fallbackTimestampRequested=timestamp;state.fallbackTimestampReturned=nil;state.fallbackRequestKey=[RVDAHash([NSString stringWithFormat:@"%@|%@",RVDAUISalt,url.absoluteString]) substringToIndex:12];
    if (state.fallbackDeadline && RVDANow()>=state.fallbackDeadline) { RVDAUIFallbackExhausted(state,@"fallback_burst_deferred");return; }
    RVDAUIInitializeImageCaches();
    NSDictionary *cached=[RVDAUIAuxiliaryCache objectForKey:url.absoluteString];
    if ([cached[@"expires"] doubleValue]>RVDANow() && (!(timestamp || cachedRandom) || RVDAFrameCachedTimestamp(cached,timestamp,state.result.videoDuration.doubleValue))) {
        state.fallbackResponseReason=@"decoded_cache_hit";state.fallbackTimestampReturned=[cached[@"timestamp"] isKindOfClass:NSNumber.class] ? cached[@"timestamp"] : nil;state.imageBlocker=@"ready";
        RVDADiagRecord(@"D08_fallback_cache_hit",state,nil,@{@"request_key":state.fallbackRequestKey,@"timestamp_requested":timestamp ?: NSNull.null});
        RVDAUIApplyImage(state,cached[@"image"],origin,generation);return;
    }
    __weak RVDAUIBinding *weak=state;
    RVDAUIAuxiliary(state,url,RVDAImageLimit,generation,^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker) {
        RVDAUIBinding *current=weak;if (!current) return;
        current.fallbackHTTPStatus=response.statusCode;
        if (cachedRandom) {
            current.fallbackCachedHTTPStatus=response.statusCode;
            RVDADiagRecord(@"D08_cached_random_response",current,nil,@{@"http_status":@(response.statusCode),@"generation_requested":@NO});
            if (!blocker && response.statusCode==204 && RVDAFrameRetryAfter(response)==0 && RVDAFrameClassify(response,@0)==RVDAFrameNotCached) {
                RVDAUIRandomGenerate(current,generation);return;
            }
        }
        if (blocker!=RVDeArrowBlockerNone || response.statusCode!=200) {
            RVDAFrameFailure failure=RVDAFrameClassify(response,cachedRandom ? @0 : timestamp);
            BOOL pending=RVDAFrameTransient(failure),generationFailed=failure==RVDAFrameWorkerFailed;
            NSString *reason=RVDAFrameFailureName(failure);
            NSTimeInterval headerDelay=RVDAFrameRetryAfter(response);
            current.fallbackRetryAfter=generationFailed ? MAX(300,headerDelay) : headerDelay;
            current.fallbackResponseReason=reason;
            RVDADiagRecord(@"D08_fallback_image_response",current,nil,@{@"http_status":@(response.statusCode),@"generation_reason":pending ? @"generation_pending" : generationFailed ? @"generation_failed" : @"unavailable",
                @"failure_category":reason,@"transient":@(pending),@"header_retry_after_seconds":@(headerDelay),@"retry_after_seconds":@(current.fallbackRetryAfter),@"remaining_seconds":@(MAX(0,current.fallbackDeadline-RVDANow())),@"timestamp_requested":@(timestamp!=nil)});
            RVDAUIFallbackFailure(current,pending ? @"fallback_generation_pending" : generationFailed ? @"fallback_generation_backoff" : RVDABlockerName(blocker==RVDeArrowBlockerNone ? RVDeArrowBlockerImageHTTP : blocker),pending || generationFailed || RVDAUIRetryableTransfer(blocker,response.statusCode),generation,^(RVDAUIBinding *next) { if (cachedRandom || [origin isEqual:@"random_time"]) RVDAUIRandomImage(next,generation);else RVDAUIFetchImage(next,url,origin,timestamp,generation); });return;
        }
        current.fallbackResponseReason=@"image_received";current.fallbackRetryAfter=0;current.fallbackPhase=@"encoding";
        if (timestamp || cachedRandom) {
            NSNumber *returned=RVDAFrameTimestamp(response,timestamp,current.result.videoDuration.doubleValue);
            current.fallbackTimestampReturned=returned;
            RVDADiagRecord(@"D08_fallback_timestamp_response",current,nil,@{@"requested":timestamp ?: NSNull.null,@"returned":returned ?: NSNull.null,@"exact_required":@(!cachedRandom),@"http_status":@(response.statusCode)});
            if (!returned) { RVDAUIFallbackFailure(current,@"fallback_timestamp_mismatch",NO,generation,nil);return; }
            RVDAUIEvent(cachedRandom ? @"D08_cached_random_timestamp_validated" : @"D08_fallback_timestamp_matched",current,nil);
        }
        [RVDAImageWorkQueue() addOperationWithBlock:^{
            NSTimeInterval decodeStarted=NSProcessInfo.processInfo.systemUptime;
            NSUInteger cost=0;UIImage *image=[RVDeArrowBackend.sharedBackend decodeImage:data response:response cost:&cost];
            RVDADiagTiming(@"fallback_decode",decodeStarted);
            if ([origin isEqual:@"youtube_frame"] && (image.size.width<240 || image.size.height<135)) image=nil;
            dispatch_async(dispatch_get_main_queue(),^{
                RVDAUIBinding *latest=weak;if (!latest || latest.generation!=generation || !RVDAUICurrent(latest)) return;
                if (!image) { RVDAUIFallbackFailure(latest,@"fallback_decode_failed",NO,generation,nil);return; }
                RVDAUIEvent(@"D08_fallback_image_decoded",latest,nil);
                if (timestamp || cachedRandom) RVDAFrameStore(url,data,response);
                NSDictionary *entry=@{@"image":image,@"expires":@(RVDAFrameExpiry(response)),@"timestamp":latest.fallbackTimestampReturned ?: NSNull.null};
                [RVDAUIAuxiliaryCache setObject:entry forKey:url.absoluteString cost:cost];
                if ([origin isEqual:@"random_time"]) {
                    NSURL *canonical=RVDAUICachedFrameURL(latest.video,latest.livestream && !latest.upcoming);RVDAFrameStore(canonical,data,response);
                    [RVDAUIAuxiliaryCache setObject:entry forKey:canonical.absoluteString cost:cost];
                }
                latest.imageBlocker=@"ready";RVDAUIApplyImage(latest,image,origin,generation);
            });
        }];
    });
}
/* Alea/Mash attribution: Copyright (C) 2010 Johannes Baagoe.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
// Alea first output, matching seedrandom/alea used by DeArrow when metadata has
// no randomTime. Mash is Johannes Baagoe's MIT-licensed algorithm (2010).
static double RVDAUIMash(NSString *text,double *n) {
    for (NSUInteger index=0;index<text.length;index++) {
        *n+=[text characterAtIndex:index];double h=.02519603282416938*(*n);
        *n=(uint32_t)(uint64_t)h;h-=*n;h*=*n;*n=(uint32_t)(uint64_t)h;h-=*n;
        *n+=h*4294967296.0;
    }
    return (uint32_t)(uint64_t)(*n)*2.3283064365386963e-10;
}
static double RVDAUIAleaFraction(NSString *video) {
    double n=0xefc8249d,s0=RVDAUIMash(@" ",&n);
    (void)RVDAUIMash(@" ",&n);(void)RVDAUIMash(@" ",&n);
    s0-=RVDAUIMash(video,&n);if (s0<0) s0+=1;
    volatile double product=2091639*s0;double t=product+2.3283064365386963e-10;
    double fraction=t-floor(t);return fraction>.9 ? fraction-.9 : fraction;
}
static NSURL *RVDAUIRandomURL(NSString *video,RVDeArrowResult *result,double duration,NSNumber **selectedTimestamp) {
    if (!RVVideoIDValid(video) || !isfinite(duration) || duration<=0 || duration>604800) return nil;
    // Server randomTime already avoids promotional intervals. Do not resample,
    // re-adjust that value or round upward into a different cached millisecond.
    BOOL metadataTime=result.randomTime!=nil;
    double fraction=metadataTime ? result.randomTime.doubleValue : RVDAUIAleaFraction(video);
    NSNumber *timestamp=@(fraction*duration);
    if (selectedTimestamp) *selectedTimestamp=timestamp;
    NSURL *endpoint=[NSURL URLWithString:RVSetting(@"dearrow_url")];
    NSURLComponents *image=[NSURLComponents componentsWithURL:RVDAURL(endpoint,video,timestamp,NO) resolvingAgainstBaseURL:NO];
    NSMutableArray *query=[image.queryItems mutableCopy];
    [query addObject:[NSURLQueryItem queryItemWithName:@"generateNow" value:@"true"]];
    BOOL official=metadataTime && isfinite(result.videoDuration.doubleValue) && result.videoDuration.doubleValue>0 && fabs(duration-result.videoDuration.doubleValue)<.001 && [RVSetting(@"dearrow_branding_url") isEqual:@"https://sponsor.ajay.app/api/branding"];
    [query addObject:[NSURLQueryItem queryItemWithName:@"officialTime" value:official ? @"true" : @"false"]];image.queryItems=query;
    return image.URL;
}
static NSURL *RVDAUILiveURL(NSString *video,NSNumber **timestamp) {
    if (!RVVideoIDValid(video)) return nil;
    if (timestamp) *timestamp=@0;
    NSURLComponents *parts=[NSURLComponents componentsWithURL:RVDAURL([NSURL URLWithString:RVSetting(@"dearrow_url")],video,@0,NO) resolvingAgainstBaseURL:NO];
    NSMutableArray *items=[parts.queryItems mutableCopy];
    [items addObject:[NSURLQueryItem queryItemWithName:@"isLivestream" value:@"true"]];
    [items addObject:[NSURLQueryItem queryItemWithName:@"generateNow" value:@"true"]];
    [items addObject:[NSURLQueryItem queryItemWithName:@"officialTime" value:@"false"]];parts.queryItems=items;return parts.URL;
}
static void RVDAUIRandomImage(RVDAUIBinding *state,uint64_t generation) {
    state.fallbackTimestampSource=@"server_cached_frame";
    RVDAUIFetchImage(state,RVDAUICachedFrameURL(state.video,state.livestream && !state.upcoming),@"cached_random_time",nil,generation);
}
static void RVDAUIRandomGenerate(RVDAUIBinding *state,uint64_t generation) {
    if (state.livestream && !state.upcoming) {
        NSNumber *timestamp=nil;NSURL *url=RVDAUILiveURL(state.video,&timestamp);
        state.fallbackTimestampSource=@"native_live_timestamp_style";
        RVDADiagRecord(@"D08_live_frame_requested",state,nil,@{@"livestream_confirmed":@YES,@"timestamp_requested":@0,@"generate_now":@YES,@"video_scoped":@YES});
        RVDAUIFetchImage(state,url,@"random_time",timestamp,generation);return;
    }
    double duration=RVDAUIOwnedDuration(state);
    if (!duration) { RVDAUIFallbackFailure(state,@"fallback_duration_unavailable",YES,generation,^(RVDAUIBinding *next) { RVDAUIRandomImage(next,generation); });return; }
    NSNumber *timestamp=nil;NSURL *url=RVDAUIRandomURL(state.video,state.result,duration,&timestamp);
    state.fallbackTimestampSource=state.result.randomTime ? @"dearrow_metadata" : @"upstream_alea";
    BOOL official=state.result.randomTime && [state.durationSource isEqual:@"dearrow_metadata"] && [RVSetting(@"dearrow_branding_url") isEqual:@"https://sponsor.ajay.app/api/branding"];
    RVDADiagRecord(@"D08_random_time_selected",state,nil,@{@"duration_present":@YES,@"timestamp_source":state.fallbackTimestampSource,
        @"timestamp_requested":timestamp,@"cache_millisecond":@(floor(timestamp.doubleValue*1000)/1000),@"official_time":@(official),@"video_scoped":@YES});
    RVDAUIFetchImage(state,url,@"random_time",timestamp,generation);
}
static BOOL RVDAUIUsePrefetchedFallback(RVDAUIBinding *state,uint64_t generation) {
    if (state.result.thumbnailDecision==RVDeArrowDecisionOriginal || state.result.thumbnailDecision==RVDeArrowDecisionDisabled ||
        (RVEnabled(@"dearrow_upcoming_avatar") && state.upcoming && state.avatarURL.length)) return NO;
    NSString *mode=RVSetting(@"dearrow_fallback"),*origin=nil;NSURL *url=nil;NSNumber *timestamp=nil;
    if ([mode isEqual:@"random_time"]) {
        url=RVDAUICachedFrameURL(state.video,state.livestream && !state.upcoming);origin=@"cached_random_time";
        if ([[RVDAUIAuxiliaryCache objectForKey:url.absoluteString][@"expires"] doubleValue]<=RVDANow()) {
            double duration=state.result.videoDuration.doubleValue;if (!(duration>0 && duration<=604800)) duration=state.duration;
            url=state.livestream && !state.upcoming ? RVDAUILiveURL(state.video,&timestamp) : RVDAUIRandomURL(state.video,state.result,duration,&timestamp);origin=@"random_time";
        }
    } else if ([@[@"youtube_frame",@"stills"] containsObject:mode]) {
        url=[NSURL URLWithString:[NSString stringWithFormat:@"https://i.ytimg.com/vi/%@/hq%d.jpg",state.video,[RVSetting(@"thumbnail_frame") intValue]]];origin=@"youtube_frame";
    }
    if (!url || [[RVDAUIAuxiliaryCache objectForKey:url.absoluteString][@"expires"] doubleValue]<=RVDANow()) return NO;
    state.fallbackStarted=YES;state.fallbackEpoch++;state.fallbackDeadline=0;state.fallbackNextAttempt=0;
    state.fallbackTimestampSource=timestamp ? (state.result.randomTime ? @"dearrow_metadata" : @"upstream_alea") : [origin isEqual:@"cached_random_time"] ? @"server_cached_frame" : @"not_applicable";
    RVDADiagCount(@"prefetch.row_cache_hit");RVDAUIFetchImage(state,url,origin,timestamp,generation);return YES;
}
static void RVDAUIResolveFallback(RVDAUIBinding *state,uint64_t generation) {
    if (RVDAUIOriginal(state) || !(state.parts&RVDeArrowPartsThumbnail) || state.fallbackStarted) return;
    if (RVDAUIUsePrefetchedFallback(state,generation)) return;
    if (!RVDAUIFallbackMayRun(state)) {
        if (![state.fallbackPhase isEqual:@"waiting_visibility"]) RVDAUIFallbackDefer(state,@"fallback_waiting_visibility",0);
        return;
    }
    if (RVDANow()<state.fallbackNextAttempt) { RVDAUIFallbackEnsureTimer();return; }
    state.fallbackStarted=YES;state.fallbackEpoch++;state.fallbackCycles++;
    state.fallbackRetries=0;state.fallbackRetryScheduled=NO;state.fallbackNextAttempt=0;state.fallbackDeadline=RVDANow()+150;
    RVDAUIFallbackEnsureTimer();
    state.fallbackPhase=@"selecting";
    state.pendingExpired=YES;
    if ([state.imageOrigin isEqual:@"pending"] || state.encodedImage==RVDABlankImage()) { state.imageRevision++;state.presentationImage=nil;state.imageBytes=nil;state.imageOrigin=nil; }
    if (RVEnabled(@"dearrow_upcoming_avatar") && state.upcoming && state.avatarURL.length) {
        RVDAUIFetchImage(state,[NSURL URLWithString:state.avatarURL],@"upcoming_avatar",nil,generation);return;
    }
    NSString *mode=RVSetting(@"dearrow_fallback");
    BOOL fallback=state.result.thumbnailDecision!=RVDeArrowDecisionOriginal && state.result.thumbnailDecision!=RVDeArrowDecisionDisabled;
    if (fallback && [mode isEqual:@"blank"]) { RVDAUIApplyImage(state,RVDABlankImage(),@"blank",generation);return; }
    if (fallback && [mode isEqual:@"random_time"]) { RVDAUIRandomImage(state,generation);return; }
    if (fallback && [@[@"youtube_frame",@"stills"] containsObject:mode]) {
        NSString *url=[NSString stringWithFormat:@"https://i.ytimg.com/vi/%@/hq%d.jpg",state.video,[RVSetting(@"thumbnail_frame") intValue]];
        RVDAUIFetchImage(state,[NSURL URLWithString:url],@"youtube_frame",nil,generation);return;
    }
    NSString *url=state.originalImageURL;
    if (!url.length) { state.fallbackPhase=@"retained_original";state.fallbackStarted=NO;RVDAUIEvent(@"D08_fallback_original_retained",state,@"original_image_url_unavailable");return; }
    NSString *stable=url;
    if (RVEnabled(@"dearrow_ignore_ab_thumbnails")) {
        NSURLComponents *parts=[NSURLComponents componentsWithString:url];NSRegularExpression *custom=[NSRegularExpression regularExpressionWithPattern:@"_custom_[0-9]+(?=\\.)" options:0 error:nil];
        parts.path=[custom stringByReplacingMatchesInString:parts.path options:0 range:NSMakeRange(0,parts.path.length) withTemplate:@""];stable=parts.string;
    }
    if (![stable isEqual:url] || [RVSetting(@"dearrow_thumbnail_saturation") integerValue]<100) RVDAUIFetchImage(state,[NSURL URLWithString:stable],@"original_effect",nil,generation);
    else { state.fallbackPhase=@"retained_original";RVDAUIEvent(@"D08_fallback_original_retained",state,nil); }
}
