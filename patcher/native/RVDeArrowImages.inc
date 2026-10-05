// SPDX-License-Identifier: MIT
// Decoded fallback/effect requests are per-video, bounded and generation guarded.
static NSCache *RVDAUIAuxiliaryCache;
static void RVDAUIResolveFallback(RVDAUIBinding *state,uint64_t generation);
static void RVDAUIFallbackFailure(RVDAUIBinding *state,NSString *blocker,BOOL retryable,uint64_t generation,void (^retry)(RVDAUIBinding *)) {
    if (!state || state.generation!=generation || !RVDAUICurrent(state)) return;
    state.imageBlocker=blocker;
    NSTimeInterval delay=MIN(16,2*pow(2,state.fallbackRetries));
    if (retryable && !state.fallbackRetryScheduled && state.fallbackRetries<4 && RVDANow()+delay<state.fallbackDeadline) {
        state.fallbackRetries++;state.fallbackRetryScheduled=YES;state.fallbackPhase=@"retry_wait";
        RVDADiagRecord(@"D08_fallback_retry_scheduled",state,blocker,@{@"retry_attempt":@(state.fallbackRetries),@"delay_seconds":@(delay)});
        __weak RVDAUIBinding *weak=state;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(delay*NSEC_PER_SEC)),dispatch_get_main_queue(),^{
            RVDAUIBinding *current=weak;if (!current || current.generation!=generation) return;
            current.fallbackRetryScheduled=NO;NSString *stale=RVDAUICurrentBlocker(current);
            if (stale || RVDAUIOriginal(current)) { RVDAUIEvent(@"D11_fallback_retry_discarded",current,stale ?: @"original_selected");return; }
            retry(current);
        });
    } else {
        state.fallbackStarted=NO;state.fallbackPhase=@"failed";
        RVDAUIEvent(@"D08_fallback_rejected",state,blocker);RVDAUIRefresh(state);
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
    state.encodedImage=image;__weak RVDAUIBinding *weak=state;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        @autoreleasepool {
            UIImage *source=[origin isEqual:@"upcoming_avatar"] ? RVDACoverImage(image) : image;
            UIImage *colored=[origin isEqual:@"pending"] ? source : RVDAColorImage(source,saturation);
            NSData *bytes=UIImageJPEGRepresentation(colored,.9);
            dispatch_async(dispatch_get_main_queue(),^{
                RVDAUIBinding *current=weak;NSString *blocker=RVDAUICurrentBlocker(current);
                if (!blocker && (current.generation!=generation || current.imageRevision!=revision || RVDAUIOriginal(current))) blocker=@"image_effect_generation_changed";
                if (blocker) { RVDAUIEvent(@"D08_image_encoding_rejected",current,blocker);return; }
                if (!bytes.length || bytes.length>1024*1024) { current.imageBlocker=@"presentation_image_byte_limit";RVDAUIEvent(@"D08_image_received",current,current.imageBlocker);return; }
                current.presentationImage=colored;current.imageBytes=bytes;current.imageOrigin=origin;
                current.fallbackPhase=@"ready";
                RVDADiagRecord(@"D08_image_encoded",current,nil,@{@"byte_length":@(bytes.length),@"image_origin":origin,@"saturation":@(saturation)});RVDAUIRefresh(current);
            });
        }
    });
}
static void RVDAUIAuxiliary(RVDAUIBinding *state,NSURL *url,NSUInteger limit,uint64_t generation,void (^completion)(NSData *,NSHTTPURLResponse *,RVDeArrowBlocker)) {
    if (!url || !RVDAUICurrent(state) || state.generation!=generation) return;
    RVDAHTTP *transfer=[RVDAHTTP new];transfer.limit=limit;state.fallbackTransfer=transfer;
    state.fallbackPhase=@"queued";RVDAUIEvent(@"D08_fallback_queued",state,nil);
    __weak RVDAUIBinding *weak=state;__weak RVDAHTTP *weakTransfer=transfer;
    transfer.onStart=^{ dispatch_async(dispatch_get_main_queue(),^{
        RVDAUIBinding *current=weak;if (!current || current.generation!=generation || !RVDAUICurrent(current)) return;
        current.fallbackPhase=@"fetching";RVDAUIEvent(@"D08_fallback_transport_started",current,nil);
    }); };
    transfer.completion=^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker,NSInteger code) {
        dispatch_async(dispatch_get_main_queue(),^{
            RVDAUIBinding *current=weak;
            if (!current || current.generation!=generation || current.fallbackTransfer!=weakTransfer || !RVDAUICurrent(current)) return;
            current.fallbackTransfer=nil;
            RVDADiagRecord(@"D08_fallback_transport_finished",current,blocker ? RVDABlockerName(blocker) : nil,@{@"http_status":@(response.statusCode),@"network_error_code":@(code)});
            completion(data,response,blocker);
        });
    };
    [RVDeArrowBackend.sharedBackend enqueueAuxiliary:transfer URL:url timeout:limit==RVDAImageLimit ? 45 : 15];
}
static void RVDAUIFetchImage(RVDAUIBinding *state,NSURL *url,NSString *origin,NSNumber *timestamp,uint64_t generation) {
    if (!url || RVDAUIOriginal(state)) return;
    if (!RVDAUIAuxiliaryCache) { RVDAUIAuxiliaryCache=[NSCache new];RVDAUIAuxiliaryCache.countLimit=128;RVDAUIAuxiliaryCache.totalCostLimit=24*1024*1024; }
    NSDictionary *cached=[RVDAUIAuxiliaryCache objectForKey:url.absoluteString];
    if ([cached[@"expires"] doubleValue]>RVDANow()) { RVDAUIApplyImage(state,cached[@"image"],origin,generation);return; }
    __weak RVDAUIBinding *weak=state;
    RVDAUIAuxiliary(state,url,RVDAImageLimit,generation,^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker) {
        RVDAUIBinding *current=weak;if (!current) return;
        if (blocker!=RVDeArrowBlockerNone || response.statusCode!=200) {
            NSString *failure=RVDAHeader(response,@"X-Failure-Reason").lowercaseString;
            BOOL pending=timestamp && response.statusCode==204 && ([failure containsString:@"not generated"] || [failure containsString:@"not cached"] || [failure containsString:@"timeout"] || [failure containsString:@"queue"]);
            NSString *reason=pending ? @"generation_pending" : response.statusCode==204 ? @"generation_unavailable" : @"other";
            RVDADiagRecord(@"D08_fallback_image_response",current,nil,@{@"http_status":@(response.statusCode),@"generation_reason":reason});
            RVDAUIFallbackFailure(current,pending ? @"fallback_generation_pending" : RVDABlockerName(blocker==RVDeArrowBlockerNone ? RVDeArrowBlockerImageHTTP : blocker),pending || RVDAUIRetryableTransfer(blocker,response.statusCode),generation,^(RVDAUIBinding *next) { RVDAUIFetchImage(next,url,origin,timestamp,generation); });return;
        }
        if (timestamp) {
            NSString *header=RVDAHeader(response,@"X-Timestamp");NSScanner *scan=[NSScanner scannerWithString:header ?: @""];double value=0;
            scan.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
            if (![scan scanDouble:&value] || !scan.isAtEnd || !isfinite(value) || fabs(value-timestamp.doubleValue)>.001) {
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
                [RVDAUIAuxiliaryCache setObject:@{@"image":image,@"expires":@(RVDANow()+1800)} forKey:url.absoluteString cost:cost];
                latest.imageBlocker=@"ready";RVDAUIApplyImage(latest,image,origin,generation);
            });
        });
    });
}
static void RVDAUIRandomImage(RVDAUIBinding *state,uint64_t generation) {
    double duration=RVDAUIOwnedDuration(state);
    if (!duration) { RVDAUIFallbackFailure(state,@"fallback_duration_unavailable",YES,generation,^(RVDAUIBinding *next) { RVDAUIRandomImage(next,generation); });return; }
    double fraction=state.result.randomTime ? state.result.randomTime.doubleValue : strtoul([[RVDAHash(state.video) substringToIndex:8] UTF8String],NULL,16)/(double)UINT32_MAX;
    if (fraction>.9) fraction-=.9;
    double chosen=fraction*duration;
    NSURLComponents *segments=[NSURLComponents componentsWithString:@"https://sponsor.ajay.app/api/skipSegments"];
    segments.queryItems=@[[NSURLQueryItem queryItemWithName:@"videoID" value:state.video],[NSURLQueryItem queryItemWithName:@"categories" value:@"[\"sponsor\",\"selfpromo\",\"interaction\"]"]];
    __weak RVDAUIBinding *weak=state;
    RVDAUIAuxiliary(state,segments.URL,RVDAMetadataLimit,generation,^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker) {
        RVDAUIBinding *current=weak;if (!current) return;
        id records=response.statusCode==404 ? @[] : data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        RVDADiagRecord(@"D08_fallback_segments_received",current,nil,@{@"http_status":@(response.statusCode),@"shape_valid":@([records isKindOfClass:NSArray.class]),@"empty_404":@(response.statusCode==404)});
        if (blocker || ![@[@200,@404] containsObject:@(response.statusCode)] || ![records isKindOfClass:NSArray.class] || [records count]>512) {
            RVDAUIFallbackFailure(current,@"fallback_segment_data_unavailable",RVDAUIRetryableTransfer(blocker,response.statusCode),generation,^(RVDAUIBinding *next) { RVDAUIRandomImage(next,generation); });return;
        }
        NSMutableArray *intervals=[NSMutableArray array];BOOL valid=YES;
        for (id record in records) {
            id span=[record isKindOfClass:NSDictionary.class] ? record[@"segment"] : nil;
            if (![span isKindOfClass:NSArray.class] || [span count]!=2 || !RVDANumber(span[0]) || !RVDANumber(span[1]) || [span[0] doubleValue]<0 || [span[1] doubleValue]<[span[0] doubleValue] || [span[1] doubleValue]>duration+1) { valid=NO;break; }
            [intervals addObject:span];
        }
        double time=chosen;BOOL safe=NO;
        for (NSUInteger pass=0;valid && pass<=intervals.count;pass++) {
            safe=YES;for (NSArray *span in intervals) if (time>=[span[0] doubleValue] && time<=[span[1] doubleValue]) { safe=NO;time=[span[1] doubleValue]+.1;if (time>=duration*.9) time=fmod(time+.37*duration,duration*.9);break; }
            if (safe) break;
        }
        if (!valid || !safe) { RVDAUIFallbackFailure(current,@"fallback_safe_time_unavailable",NO,generation,nil);return; }
        NSNumber *timestamp=@(round(time*1000)/1000);
        NSURL *endpoint=[NSURL URLWithString:RVSetting(@"dearrow_url")];NSURLComponents *image=[NSURLComponents componentsWithURL:RVDAURL(endpoint,current.video,timestamp,NO) resolvingAgainstBaseURL:NO];
        NSMutableArray *query=[image.queryItems mutableCopy];[query addObject:[NSURLQueryItem queryItemWithName:@"generateNow" value:@"true"]];image.queryItems=query;
        RVDADiagRecord(@"D08_random_time_selected",current,nil,@{@"duration_present":@YES,@"segment_count":@(intervals.count),@"video_scoped":@YES});
        RVDAUIFetchImage(current,image.URL,@"random_time",timestamp,generation);
    });
}
static void RVDAUIResolveFallback(RVDAUIBinding *state,uint64_t generation) {
    if (RVDAUIOriginal(state) || !(state.parts&RVDeArrowPartsThumbnail) || state.fallbackStarted) return;
    state.fallbackStarted=YES;
    if (!state.fallbackDeadline) {
        state.fallbackDeadline=RVDANow()+150;__weak RVDAUIBinding *weak=state;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,150*NSEC_PER_SEC),dispatch_get_main_queue(),^{
            RVDAUIBinding *current=weak;if (!current || current.generation!=generation || !RVDAUICurrent(current) ||
                [@[@"ready",@"failed",@"retained_original"] containsObject:current.fallbackPhase]) return;
            current.fallbackDeadline=RVDANow();[current.fallbackTransfer cancel];current.fallbackTransfer=nil;
            RVDAUIFallbackFailure(current,@"fallback_deadline_expired",NO,generation,nil);
        });
    }
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
