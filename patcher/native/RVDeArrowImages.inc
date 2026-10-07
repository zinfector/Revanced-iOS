// SPDX-License-Identifier: MIT
// Decoded fallback/effect requests are per-video, bounded and generation guarded.
static NSCache *RVDAUIAuxiliaryCache;
static void RVDAUIResolveFallback(RVDAUIBinding *state,uint64_t generation);
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
    return MAX(state.fallbackRetryAfter,MIN(300,30*pow(2,MIN(state.fallbackExhaustions ? state.fallbackExhaustions-1 : 0,4u))));
}
static void RVDAUIFallbackExhausted(RVDAUIBinding *state,NSString *reason) {
    state.fallbackExhaustions++;
    RVDAUIFallbackDefer(state,reason,RVDAUIFallbackCooldown(state));
}
static void RVDAUIFallbackWake(RVDAUIBinding *state) {
    if (!NSThread.isMainThread || !RVDAUIFallbackPending(state) || RVDAUIOriginal(state) || !RVDAUICurrent(state)) return;
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
    RVDADiagRecord(@"D08_fallback_resumed",state,nil,@{@"prior_cycle":@(state.fallbackCycles),@"row_visible":@YES});
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
    if (!(duration>0 && duration<=604800)) { duration=state.duration;state.durationSource=@"native_thumbnail_timestamp"; }
    if (!(duration>0 && duration<=604800) && RVOrdinaryContent(RVCurrentPlayback) &&
        [state.video isEqual:RVObject(RVCurrentPlayback,"contentVideoID")] && RVCan(RVCurrentPlayer,"currentVideoTotalMediaTime",@"d@:")) {
        duration=RVDouble(RVCurrentPlayer,"currentVideoTotalMediaTime");state.durationSource=@"matching_native_playback";
    }
    if (!(isfinite(duration) && duration>0 && duration<=604800)) { duration=0;state.durationSource=@"unavailable"; }
    RVDADiagRecord(@"D08_duration_resolved",state,duration ? nil : @"fallback_duration_unavailable",@{@"duration_present":@(duration>0),@"duration_source":state.durationSource,@"video_scoped":@YES});
    return duration;
}
static void RVDAUIApplyImage(RVDAUIBinding *state,UIImage *image,NSString *origin,uint64_t generation) {
    if (!image || RVDAUIOriginal(state)) return;
    uint64_t revision=++state.imageRevision;NSInteger saturation=[RVSetting(@"dearrow_thumbnail_saturation") integerValue];
    state.encodedImage=image;if (![origin isEqual:@"pending"]) state.fallbackPhase=@"encoding";__weak RVDAUIBinding *weak=state;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        @autoreleasepool {
            UIImage *source=[origin isEqual:@"upcoming_avatar"] ? RVDACoverImage(image) : image;
            UIImage *colored=[origin isEqual:@"pending"] ? source : RVDAColorImage(source,saturation);
            NSData *bytes=UIImageJPEGRepresentation(colored,.9);
            dispatch_async(dispatch_get_main_queue(),^{
                RVDAUIBinding *current=weak;NSString *blocker=RVDAUICurrentBlocker(current);
                if (!blocker && (current.generation!=generation || current.imageRevision!=revision || RVDAUIOriginal(current))) blocker=@"image_effect_generation_changed";
                if (blocker) { RVDAUIEvent(@"D08_image_encoding_rejected",current,blocker);return; }
                if (!bytes.length || bytes.length>1024*1024) { current.fallbackPhase=@"failed";current.imageBlocker=@"presentation_image_byte_limit";RVDAUIEvent(@"D08_image_received",current,current.imageBlocker);return; }
                current.presentationImage=colored;current.imageBytes=bytes;current.imageOrigin=origin;
                if (![origin isEqual:@"pending"]) current.fallbackPhase=@"ready";
                RVDADiagRecord(@"D08_image_encoded",current,nil,@{@"byte_length":@(bytes.length),@"image_origin":origin,@"saturation":@(saturation)});RVDAUIRefresh(current);
            });
        }
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
        dispatch_async(dispatch_get_main_queue(),^{
            RVDAUIBinding *current=weak;
            if (!current || current.generation!=generation || current.fallbackEpoch!=epoch || !current.fallbackTransfer || current.fallbackTransfer!=weakTransfer || !RVDAUICurrent(current)) return;
            current.fallbackTransfer=nil;current.fallbackNetworkErrorCode=code;
            RVDADiagRecord(@"D08_fallback_transport_finished",current,blocker ? RVDABlockerName(blocker) : nil,@{@"http_status":@(response.statusCode),@"network_error_code":@(code)});
            completion(data,response,blocker);
        });
    };
    [RVDeArrowBackend.sharedBackend enqueueAuxiliary:transfer URL:url timeout:limit==RVDAImageLimit ? 45 : 15];
}
static void RVDAUIFetchImage(RVDAUIBinding *state,NSURL *url,NSString *origin,NSNumber *timestamp,uint64_t generation) {
    if (!url || RVDAUIOriginal(state)) return;
    state.fallbackTimestampRequested=timestamp;state.fallbackTimestampReturned=nil;state.fallbackRequestKey=[RVDAHash([NSString stringWithFormat:@"%@|%@",RVDAUISalt,url.absoluteString]) substringToIndex:12];
    if (state.fallbackDeadline && RVDANow()>=state.fallbackDeadline) { RVDAUIFallbackExhausted(state,@"fallback_burst_deferred");return; }
    if (!RVDAUIAuxiliaryCache) { RVDAUIAuxiliaryCache=[NSCache new];RVDAUIAuxiliaryCache.countLimit=128;RVDAUIAuxiliaryCache.totalCostLimit=24*1024*1024; }
    NSDictionary *cached=[RVDAUIAuxiliaryCache objectForKey:url.absoluteString];
    if ([cached[@"expires"] doubleValue]>RVDANow()) {
        state.fallbackResponseReason=@"decoded_cache_hit";state.fallbackTimestampReturned=[cached[@"timestamp"] isKindOfClass:NSNumber.class] ? cached[@"timestamp"] : nil;state.imageBlocker=@"ready";
        RVDADiagRecord(@"D08_fallback_cache_hit",state,nil,@{@"request_key":state.fallbackRequestKey,@"timestamp_requested":timestamp ?: NSNull.null});
        RVDAUIApplyImage(state,cached[@"image"],origin,generation);return;
    }
    __weak RVDAUIBinding *weak=state;
    RVDAUIAuxiliary(state,url,RVDAImageLimit,generation,^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker) {
        RVDAUIBinding *current=weak;if (!current) return;
        current.fallbackHTTPStatus=response.statusCode;
        if (blocker!=RVDeArrowBlockerNone || response.statusCode!=200) {
            NSString *failure=RVDAHeader(response,@"X-Failure-Reason").lowercaseString;
            BOOL generationFailed=timestamp && response.statusCode==204 && [failure containsString:@"failed to generate"];
            BOOL pending=timestamp && response.statusCode==204 && ([failure containsString:@"not generated"] || [failure containsString:@"not cached"] || [failure containsString:@"timeout"] || [failure containsString:@"queue"]);
            NSString *reason=[failure containsString:@"not generated"] ? @"not_generated_yet" : [failure containsString:@"not cached"] ? @"not_cached" :
                [failure containsString:@"queue"] ? @"queue_busy" : [failure containsString:@"timeout"] ? @"generation_timeout" :
                [failure containsString:@"fail"] ? @"generation_failed" : response.statusCode==204 ? @"generation_unavailable" : @"other";
            NSString *retryHeader=RVDAHeader(response,@"Retry-After");NSScanner *retryScan=[NSScanner scannerWithString:retryHeader ?: @""];double retryAfter=0;
            current.fallbackRetryAfter=[retryScan scanDouble:&retryAfter] && retryScan.isAtEnd && isfinite(retryAfter) && retryAfter>0 ? MIN(300,retryAfter) : 0;
            if (generationFailed) current.fallbackRetryAfter=MAX(300,current.fallbackRetryAfter);
            current.fallbackResponseReason=reason;
            RVDADiagRecord(@"D08_fallback_image_response",current,nil,@{@"http_status":@(response.statusCode),@"generation_reason":pending ? @"generation_pending" : generationFailed ? @"generation_failed" : @"unavailable",
                @"failure_category":reason,@"retry_after_seconds":@(current.fallbackRetryAfter),@"remaining_seconds":@(MAX(0,current.fallbackDeadline-RVDANow())),@"timestamp_requested":@(timestamp!=nil)});
            RVDAUIFallbackFailure(current,pending ? @"fallback_generation_pending" : generationFailed ? @"fallback_generation_backoff" : RVDABlockerName(blocker==RVDeArrowBlockerNone ? RVDeArrowBlockerImageHTTP : blocker),pending || generationFailed || RVDAUIRetryableTransfer(blocker,response.statusCode),generation,^(RVDAUIBinding *next) { RVDAUIFetchImage(next,url,origin,timestamp,generation); });return;
        }
        current.fallbackResponseReason=@"image_received";current.fallbackRetryAfter=0;current.fallbackPhase=@"encoding";
        if (timestamp) {
            NSString *header=RVDAHeader(response,@"X-Timestamp");NSScanner *scan=[NSScanner scannerWithString:header ?: @""];double value=0;
            scan.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
            BOOL parsed=[scan scanDouble:&value] && scan.isAtEnd && isfinite(value);
            current.fallbackTimestampReturned=parsed ? @(value) : nil;
            RVDADiagRecord(@"D08_fallback_timestamp_response",current,nil,@{@"requested":timestamp,@"returned":current.fallbackTimestampReturned ?: NSNull.null,@"delta":parsed ? @(value-timestamp.doubleValue) : NSNull.null,@"http_status":@(response.statusCode)});
            if (!parsed || fabs(value-timestamp.doubleValue)>.001) {
                RVDAUIFallbackFailure(current,@"fallback_timestamp_mismatch",NO,generation,nil);return;
            }
            RVDAUIEvent(@"D08_fallback_timestamp_matched",current,nil);
        }
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
            NSUInteger cost=0;UIImage *image=[RVDeArrowBackend.sharedBackend decodeImage:data response:response cost:&cost];
            if ([origin isEqual:@"youtube_frame"] && (image.size.width<240 || image.size.height<135)) image=nil;
            dispatch_async(dispatch_get_main_queue(),^{
                RVDAUIBinding *latest=weak;if (!latest || latest.generation!=generation || !RVDAUICurrent(latest)) return;
                if (!image) { RVDAUIFallbackFailure(latest,@"fallback_decode_failed",NO,generation,nil);return; }
                RVDAUIEvent(@"D08_fallback_image_decoded",latest,nil);
                [RVDAUIAuxiliaryCache setObject:@{@"image":image,@"expires":@(RVDANow()+1800),@"timestamp":latest.fallbackTimestampReturned ?: NSNull.null} forKey:url.absoluteString cost:cost];
                latest.imageBlocker=@"ready";RVDAUIApplyImage(latest,image,origin,generation);
            });
        });
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
static void RVDAUIRandomImage(RVDAUIBinding *state,uint64_t generation) {
    double duration=RVDAUIOwnedDuration(state);
    if (!duration) { RVDAUIFallbackFailure(state,@"fallback_duration_unavailable",YES,generation,^(RVDAUIBinding *next) { RVDAUIRandomImage(next,generation); });return; }
    // Server randomTime already avoids promotional intervals. Do not resample,
    // re-adjust that value or round upward into a different cached millisecond.
    BOOL metadataTime=state.result.randomTime!=nil;
    double fraction=metadataTime ? state.result.randomTime.doubleValue : RVDAUIAleaFraction(state.video);
    NSNumber *timestamp=@(fraction*duration);
    state.fallbackTimestampSource=metadataTime ? @"dearrow_metadata" : @"upstream_alea";
    NSURL *endpoint=[NSURL URLWithString:RVSetting(@"dearrow_url")];
    NSURLComponents *image=[NSURLComponents componentsWithURL:RVDAURL(endpoint,state.video,timestamp,NO) resolvingAgainstBaseURL:NO];
    NSMutableArray *query=[image.queryItems mutableCopy];
    [query addObject:[NSURLQueryItem queryItemWithName:@"generateNow" value:@"true"]];
    BOOL official=[RVSetting(@"dearrow_branding_url") isEqual:@"https://sponsor.ajay.app/api/branding"];
    [query addObject:[NSURLQueryItem queryItemWithName:@"officialTime" value:official ? @"true" : @"false"]];image.queryItems=query;
    RVDADiagRecord(@"D08_random_time_selected",state,nil,@{@"duration_present":@YES,@"timestamp_source":state.fallbackTimestampSource,
        @"timestamp_requested":timestamp,@"cache_millisecond":@(floor(timestamp.doubleValue*1000)/1000),@"official_time":@(official),@"video_scoped":@YES});
    RVDAUIFetchImage(state,image.URL,@"random_time",timestamp,generation);
}
static void RVDAUIResolveFallback(RVDAUIBinding *state,uint64_t generation) {
    if (RVDAUIOriginal(state) || !(state.parts&RVDeArrowPartsThumbnail) || state.fallbackStarted) return;
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
