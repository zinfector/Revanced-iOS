// SPDX-License-Identifier: MIT
// Cached random frames have a server-selected time. Explicit time requests
// retain their exact-match contract; neither path trusts unvalidated pixels.
static NSURL *RVDAUICachedRandomURL(NSString *video) {
    if (!RVVideoIDValid(video)) return nil;
    NSURLComponents *parts=[NSURLComponents componentsWithURL:RVDAURL([NSURL URLWithString:RVSetting(@"dearrow_url")],video,nil,NO) resolvingAgainstBaseURL:NO];
    NSMutableArray *items=[NSMutableArray array];
    for (NSURLQueryItem *item in parts.queryItems) if (![@[@"time",@"generateNow",@"officialTime"] containsObject:item.name]) [items addObject:item];
    parts.queryItems=items;return parts.URL;
}
static NSURL *RVDAUICachedFrameURL(NSString *video,BOOL livestream) {
    NSURL *url=RVDAUICachedRandomURL(video);if (!livestream || !url) return url;
    NSURLComponents *parts=[NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
    parts.queryItems=[parts.queryItems arrayByAddingObject:[NSURLQueryItem queryItemWithName:@"isLivestream" value:@"true"]];return parts.URL;
}
static BOOL RVDAFrameIsLive(NSURL *url) {
    for (NSURLQueryItem *item in [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO].queryItems)
        if ([item.name isEqual:@"isLivestream"] && [item.value isEqual:@"true"]) return YES;
    return NO;
}
static NSNumber *RVDAFrameTimestamp(NSHTTPURLResponse *response,NSNumber *expected,double duration) {
    NSScanner *scan=[NSScanner scannerWithString:RVDAHeader(response,@"X-Timestamp") ?: @""];
    scan.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];double value=0;
    if (![scan scanDouble:&value] || !scan.isAtEnd || !isfinite(value) || value<0 || value>604800) return nil;
    if (isfinite(duration) && duration>0 && value>duration) return nil;
    if (expected && fabs(value-expected.doubleValue)>.001) return nil;
    return @(value);
}
static BOOL RVDAFrameCachedTimestamp(NSDictionary *entry,NSNumber *expected,double duration) {
    NSNumber *timestamp=entry[@"timestamp"];
    if (![timestamp isKindOfClass:NSNumber.class]) return NO;
    double value=timestamp.doubleValue;
    return isfinite(value) && value>=0 && value<=604800 &&
        (!(isfinite(duration) && duration>0) || value<=duration) && (!expected || fabs(value-expected.doubleValue)<=.001);
}
static NSTimeInterval RVDAFrameExpiry(NSHTTPURLResponse *response) {
    NSString *header=RVDAHeader(response,@"X-RVPort-Expires");
    double expiry=header.doubleValue;
    NSTimeInterval lifetime=RVDAFrameIsLive(response.URL) ? 60 : 1800;
    return [RVDAHeader(response,@"X-RVPort-Cache") isEqual:@"disk"] && isfinite(expiry) && expiry>RVDANow() ? MIN(expiry,RVDANow()+lifetime) : RVDANow()+lifetime;
}
static dispatch_queue_t RVDAFrameDiskQueue(void) {
    static dispatch_queue_t queue;static dispatch_once_t once;
    dispatch_once(&once,^{ queue=dispatch_queue_create("app.revanced.dearrow.frame-disk",DISPATCH_QUEUE_SERIAL); });return queue;
}
static NSURL *RVDAFrameDiskRoot(void) {
    NSURL *root=[NSFileManager.defaultManager URLsForDirectory:NSCachesDirectory inDomains:NSUserDomainMask].firstObject;
    return [root URLByAppendingPathComponent:@"RVDeArrowFrames-v1" isDirectory:YES];
}
static NSURL *RVDAFrameDiskFile(NSURL *url) {
    return [RVDAFrameDiskRoot() URLByAppendingPathComponent:[RVDAHash(url.absoluteString) stringByAppendingString:@".plist"]];
}
// Main-thread epoch invalidates queued reads and late stores on explicit clear.
static uint64_t RVDAFrameDiskEpoch;
static void RVDAFrameClearDisk(void) {
    RVDAFrameDiskEpoch++;
    dispatch_async(RVDAFrameDiskQueue(),^{ [NSFileManager.defaultManager removeItemAtURL:RVDAFrameDiskRoot() error:nil]; });
}
static void RVDAFrameStore(NSURL *url,NSData *data,NSHTTPURLResponse *response) {
    if ([RVDAHeader(response,@"X-RVPort-Cache") isEqual:@"disk"]) return; // do not extend freshness on read
    if (!url || !data.length || data.length>RVDAImageLimit || !RVDAFrameTimestamp(response,nil,0)) return;
    NSString *mime=response.MIMEType.lowercaseString;
    if (![@[@"image/webp",@"image/jpeg",@"image/png"] containsObject:mime]) return;
    NSDictionary *entry=@{@"request":url.absoluteString,@"expires":@(RVDAFrameExpiry(response)),@"data":data,
        @"mime":mime,@"timestamp":RVDAHeader(response,@"X-Timestamp")};
    dispatch_async(RVDAFrameDiskQueue(),^{
        NSFileManager *manager=NSFileManager.defaultManager;NSURL *root=RVDAFrameDiskRoot();
        [manager createDirectoryAtURL:root withIntermediateDirectories:YES attributes:nil error:nil];
        NSData *encoded=[NSPropertyListSerialization dataWithPropertyList:entry format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
        if (!encoded) return;
        [encoded writeToURL:RVDAFrameDiskFile(url) options:NSDataWritingAtomic error:nil];
        NSArray *files=[manager contentsOfDirectoryAtURL:root includingPropertiesForKeys:@[NSURLFileSizeKey,NSURLContentModificationDateKey] options:NSDirectoryEnumerationSkipsHiddenFiles error:nil];
        files=[files sortedArrayUsingComparator:^NSComparisonResult(NSURL *a,NSURL *b) {
            NSDate *left=nil,*right=nil;[a getResourceValue:&left forKey:NSURLContentModificationDateKey error:nil];[b getResourceValue:&right forKey:NSURLContentModificationDateKey error:nil];
            return [right ?: NSDate.distantPast compare:left ?: NSDate.distantPast];
        }];
        NSUInteger count=0,bytes=0;
        for (NSURL *file in files) {
            NSNumber *size=nil;NSDate *date=nil;[file getResourceValue:&size forKey:NSURLFileSizeKey error:nil];[file getResourceValue:&date forKey:NSURLContentModificationDateKey error:nil];
            if (!date || -date.timeIntervalSinceNow>1800 || count>=128 || size.unsignedIntegerValue>32*1024*1024-bytes) [manager removeItemAtURL:file error:nil];
            else { count++;bytes+=size.unsignedIntegerValue; }
        }
    });
}
static void RVDAFrameEnqueue(RVDAHTTP *ticket,NSURL *url,NSTimeInterval timeout) {
    // Only frame-service URLs participate. Original effects/avatars/stills stay
    // on the ordinary shared pool. Cache bytes must pass the normal decoder.
    NSURLComponents *parts=[NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
    BOOL frame=NO;for (NSURLQueryItem *item in parts.queryItems) if ([item.name isEqual:@"videoID"] && RVVideoIDValid(item.value)) frame=YES;
    if (!frame) { [RVDeArrowBackend.sharedBackend enqueueAuxiliary:ticket URL:url timeout:timeout];return; }
    uint64_t epoch=RVDAFrameDiskEpoch,revision=RVDAUISettingsRevision;
    dispatch_async(RVDAFrameDiskQueue(),^{
        NSURL *file=RVDAFrameDiskFile(url);NSNumber *size=nil;[file getResourceValue:&size forKey:NSURLFileSizeKey error:nil];
        NSData *raw=size.unsignedIntegerValue>0 && size.unsignedIntegerValue<=RVDAImageLimit+8192 ? [NSData dataWithContentsOfURL:file] : nil;
        NSDictionary *entry=raw ? [NSPropertyListSerialization propertyListWithData:raw options:NSPropertyListImmutable format:NULL error:nil] : nil;
        if (![entry isKindOfClass:NSDictionary.class] || ![entry[@"request"] isEqual:url.absoluteString] || ![entry[@"expires"] isKindOfClass:NSNumber.class] || [entry[@"expires"] doubleValue]<=RVDANow() || ![entry[@"data"] isKindOfClass:NSData.class] || ![entry[@"mime"] isKindOfClass:NSString.class] || ![entry[@"timestamp"] isKindOfClass:NSString.class]) entry=nil;
        NSHTTPURLResponse *response=entry ? [[NSHTTPURLResponse alloc] initWithURL:url statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{@"Content-Type":entry[@"mime"],@"X-Timestamp":entry[@"timestamp"],@"X-RVPort-Cache":@"disk",@"X-RVPort-Expires":[entry[@"expires"] stringValue]}] : nil;
        if (response && (!RVDAFrameTimestamp(response,nil,0) || ![entry[@"data"] length] || [entry[@"data"] length]>RVDAImageLimit)) { entry=nil;response=nil; }
        dispatch_async(dispatch_get_main_queue(),^{
            @synchronized(ticket) { if (ticket.completed) return; }
            if (epoch!=RVDAFrameDiskEpoch || revision!=RVDAUISettingsRevision) { [ticket cancel];return; }
            if (entry) { RVDADiagCount(@"frame.disk_hit");[ticket finishData:entry[@"data"] response:response blocker:RVDeArrowBlockerNone errorCode:0]; }
            else { RVDADiagCount(@"frame.disk_miss");[RVDeArrowBackend.sharedBackend enqueueAuxiliary:ticket URL:url timeout:timeout]; }
        });
    });
}
