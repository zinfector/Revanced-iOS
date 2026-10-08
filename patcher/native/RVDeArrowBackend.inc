// SPDX-License-Identifier: MIT
// UI-independent, on-demand DeArrow coordinator. Included once by RVPort.m.
#import "RVDeArrowBackend.h"
#import <ImageIO/ImageIO.h>
#include <CommonCrypto/CommonDigest.h>
#include <float.h>
#include <math.h>

static const NSUInteger RVDAMetadataLimit=1024*1024, RVDAImageLimit=8*1024*1024;
static const NSUInteger RVDAMaxEntries=256, RVDAMaxJobs=128, RVDAMaxTransfers=6, RVDAMaxPrefetchTransfers=4;
static const NSUInteger RVDAMaxImageCost=24*1024*1024;
static const void *RVDAQueueKey=&RVDAQueueKey;
static NSTimeInterval RVDANow(void) { return NSDate.date.timeIntervalSince1970; }
static BOOL RVDAVideoID(id value) {
    if (![value isKindOfClass:NSString.class] || [value length]!=11) return NO;
    for (NSUInteger i=0;i<11;i++) {
        unichar c=[value characterAtIndex:i];
        if (!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-')) return NO;
    }return YES;
}
static NSString *RVDAHash(NSString *value) {
    NSData *data=[value dataUsingEncoding:NSUTF8StringEncoding];unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes,(CC_LONG)data.length,digest);NSMutableString *result=[NSMutableString stringWithCapacity:64];
    for (NSUInteger i=0;i<sizeof(digest);i++) [result appendFormat:@"%02x",digest[i]];return result;
}
static BOOL RVDANumber(id value) {
    return [value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value)!=CFBooleanGetTypeID() && isfinite([value doubleValue]);
}
static BOOL RVDABoolean(id value) {
    return [value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID();
}
static NSArray<NSDictionary *> *RVDACasualVotes(id input) {
    if (![input isKindOfClass:NSArray.class] || [input count]>128) return @[];
    NSMutableArray *result=[NSMutableArray array];
    for (id vote in input) {
        if (![vote isKindOfClass:NSDictionary.class] || ![@[@"funny",@"creative",@"clever",@"descriptive",@"other"] containsObject:vote[@"id"]] || !RVDANumber(vote[@"count"]) || fabs([vote[@"count"] doubleValue])>1e9) continue;
        id title=vote[@"title"];if (title && (![title isKindOfClass:NSString.class] || [title length]>1024)) continue;
        NSMutableDictionary *item=[@{@"id":vote[@"id"],@"count":vote[@"count"]} mutableCopy];if (title) item[@"title"]=title;
        [result addObject:item.copy];
    }return result.copy;
}
static NSString *RVDATitleText(id value) {
    if (![value isKindOfClass:NSString.class] || [value length]>1024) return nil;
    NSString *title=value;
    for (NSUInteger i=0;i<title.length;i++) {
        unichar c=[title characterAtIndex:i];if ((c<32 && c!='\n' && c!='\t') || c==127) return nil;
    }
    title=[title stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];return title.length ? title : nil;
}
static BOOL RVDAEndpoint(NSURL *value) {
    if (![value isKindOfClass:NSURL.class] || value.absoluteString.length>2048) return NO;
    NSURLComponents *parts=[NSURLComponents componentsWithURL:value resolvingAgainstBaseURL:NO];
    return [parts.scheme.lowercaseString isEqual:@"https"] && parts.host.length && !parts.user && !parts.password && !parts.fragment && parts.path.length;
}
static NSString *RVDABlockerName(RVDeArrowBlocker value) {
    NSArray *names=@[@"ready",@"invalid_video_id",@"invalid_options",@"queue_full",@"metadata_http_error",
        @"metadata_shape_invalid",@"metadata_video_missing",@"top_candidate_untrusted",@"candidate_invalid",
        @"community_selected_original",@"thumbnail_http_error",@"thumbnail_no_content",@"thumbnail_timestamp_mismatch",
        @"thumbnail_decode_failed",@"response_body_limit",@"redirect_rejected",@"network_error",@"service_backoff",
        @"cancelled",@"configuration_changed",@"owner_unknown",@"video_binding_ambiguous",@"ad_root",
        @"title_role_unknown",@"image_role_unknown",@"stale_generation",@"native_refresh_contract_missing",@"native_text_overwritten",@"no_submission"];
    return value<names.count ? names[value] : @"unknown";
}
static NSString *RVDACheckpointName(RVDeArrowCheckpoint value) {
    NSArray *names=@[@"invalid",@"D01_backend_ready",@"D02_owner_bound",@"D03_video_bound",@"D04_role_bound",
        @"D05_metadata_requested",@"D06_metadata_received",@"D07_decision_selected",@"D08_image_received",
        @"D09_replacement_applied",@"D10_native_layout",@"D11_lifecycle_rebound",@"D12_original_restored"];
    return value<names.count ? names[value] : @"invalid";
}
static NSString *RVDADecisionName(RVDeArrowDecision value) {
    NSArray *names=@[@"disabled",@"original",@"custom",@"missing",@"untrusted",@"invalid",@"unavailable"];
    return value<names.count ? names[value] : @"invalid";
}
static RVDeArrowBlocker RVDASelectionBlocker(RVDeArrowDecision decision) {
    switch (decision) {
        case RVDeArrowDecisionOriginal:return RVDeArrowBlockerCommunityOriginal;
        case RVDeArrowDecisionMissing:return RVDeArrowBlockerCandidateMissing;
        case RVDeArrowDecisionUntrusted:return RVDeArrowBlockerCandidateUntrusted;
        case RVDeArrowDecisionInvalid:return RVDeArrowBlockerCandidateInvalid;
        default:return RVDeArrowBlockerNone;
    }
}
static NSURL *RVDAURL(NSURL *endpoint,NSString *videoID,NSNumber *timestamp,BOOL metadata) {
    NSURLComponents *url=[NSURLComponents componentsWithURL:endpoint resolvingAgainstBaseURL:NO];
    NSSet *reserved=[NSSet setWithArray:@[@"videoID",@"time",@"redirectUrl",@"title",@"officialTime",@"generateNow",@"isLivestream",@"fetchAll",@"returnUserID"]];
    NSMutableArray *query=[NSMutableArray array];
    for (NSURLQueryItem *item in url.queryItems) if (![reserved containsObject:item.name]) [query addObject:item];
    if (metadata) {
        NSString *path=url.path;
        while ([path hasSuffix:@"/"]) path=[path substringToIndex:path.length-1];
        url.path=[path stringByAppendingFormat:@"/%@",[RVDAHash(videoID) substringToIndex:4]];
    } else {
        [query addObject:[NSURLQueryItem queryItemWithName:@"videoID" value:videoID]];
        [query addObject:[NSURLQueryItem queryItemWithName:@"time" value:timestamp ? [NSString stringWithFormat:@"%.17g",timestamp.doubleValue] : nil]];
    }
    url.queryItems=query.count ? query : nil;return url.URL;
}
static NSString *RVDAHeader(NSHTTPURLResponse *response,NSString *name) {
    for (id key in response.allHeaderFields) {
        if ([key isKindOfClass:NSString.class] && [key caseInsensitiveCompare:name]==NSOrderedSame) {
            id value=response.allHeaderFields[key];return [value isKindOfClass:NSString.class] ? value : nil;
        }
    }
    return nil;
}

@implementation RVDeArrowOptions
+ (instancetype)defaultOptions {
    RVDeArrowOptions *options=[self new];options.brandingEndpoint=[NSURL URLWithString:@"https://sponsor.ajay.app/api/branding"];
    options.thumbnailEndpoint=[NSURL URLWithString:@"https://dearrow-thumb.ajay.app/api/v1/getThumbnail"];
    options.metadataTTL=1800;options.negativeTTL=300;options.requestTimeout=15;
    options.diagnosticsEnabled=YES;options.persistMetadata=YES;return options;
}
- (id)copyWithZone:(NSZone *)zone {
    RVDeArrowOptions *copy=[RVDeArrowOptions allocWithZone:zone];copy=[copy init];
    copy.brandingEndpoint=self.brandingEndpoint;copy.thumbnailEndpoint=self.thumbnailEndpoint;
    copy.metadataTTL=self.metadataTTL;copy.negativeTTL=self.negativeTTL;copy.requestTimeout=self.requestTimeout;
    copy.diagnosticsEnabled=self.diagnosticsEnabled;copy.persistMetadata=self.persistMetadata;return copy;
}
@end

@interface RVDeArrowResult ()
@property(nonatomic,readwrite,copy) NSString *videoID;
@property(nonatomic,readwrite) uint64_t configurationRevision,clientGeneration;
@property(nonatomic,readwrite) RVDeArrowDecision titleDecision,thumbnailDecision;
@property(nonatomic,readwrite,copy) NSString *title;
@property(nonatomic,readwrite,copy) NSArray<NSDictionary *> *casualVotes;
@property(nonatomic,readwrite,strong) NSNumber *videoDuration,*randomTime;
@property(nonatomic,readwrite,strong) NSNumber *thumbnailTimestamp;
@property(nonatomic,readwrite,strong) UIImage *thumbnailImage;
@property(nonatomic,readwrite) RVDeArrowImageState imageState;
@property(nonatomic,readwrite) RVDeArrowBlocker metadataBlocker,imageBlocker;
@property(nonatomic,readwrite) RVDeArrowBlocker titleBlocker,thumbnailSelectionBlocker;
@property(nonatomic,readwrite) NSInteger metadataHTTPStatus,imageHTTPStatus;
@property(nonatomic,readwrite) BOOL metadataCacheHit,imageCacheHit,final;
@end
@implementation RVDeArrowResult @end

@class RVDAJob;
@interface RVDeArrowRequest ()
@property(nonatomic,weak) RVDeArrowBackend *backend;
@property(nonatomic,copy) NSString *identifier;
@property(nonatomic,copy) NSString *videoID;
@property(nonatomic) RVDeArrowParts parts;
@property(nonatomic,copy) void (^completion)(RVDeArrowResult *);
@property(atomic,readwrite,getter=isCancelled) BOOL cancelled;
@property(nonatomic) BOOL finished,pendingDelivered,background;
@property(atomic,readwrite) uint64_t configurationRevision,clientGeneration;
@end

// One ephemeral, streaming session per transfer. Delegate serialization and a
// locked finish prevent cancellation/redirect/response races from completing twice.
@interface RVDAHTTP : NSObject <NSURLSessionDataDelegate>
@property(nonatomic,strong) NSURLSession *session;
@property(nonatomic,strong) NSURLSessionDataTask *task;
@property(nonatomic,strong) NSHTTPURLResponse *response;
@property(nonatomic,strong) NSMutableData *body;
@property(nonatomic) NSUInteger limit;
@property(nonatomic) BOOL completed,background;
@property(nonatomic,copy) void (^completion)(NSData *,NSHTTPURLResponse *,RVDeArrowBlocker,NSInteger);
@property(nonatomic,copy) void (^onStart)(void);
- (void)start:(NSURL *)url timeout:(NSTimeInterval)timeout;
- (void)finish:(RVDeArrowBlocker)blocker errorCode:(NSInteger)code;
- (void)finishData:(NSData *)data response:(NSHTTPURLResponse *)response blocker:(RVDeArrowBlocker)blocker errorCode:(NSInteger)code;
- (void)cancel;
@end
@implementation RVDAHTTP
- (void)start:(NSURL *)url timeout:(NSTimeInterval)timeout {
    @synchronized(self) {
    if (self.completed) return; // queued cancellation must never start a session
    self.body=[NSMutableData data];NSURLSessionConfiguration *config=NSURLSessionConfiguration.ephemeralSessionConfiguration;
    config.URLCache=nil;config.HTTPCookieStorage=nil;config.HTTPShouldSetCookies=NO;config.URLCredentialStorage=nil;
    config.requestCachePolicy=NSURLRequestReloadIgnoringLocalCacheData;config.timeoutIntervalForRequest=timeout;
    config.timeoutIntervalForResource=MIN(90,timeout*2);config.HTTPMaximumConnectionsPerHost=4;
    NSOperationQueue *delegate=[NSOperationQueue new];delegate.maxConcurrentOperationCount=1;
    self.session=[NSURLSession sessionWithConfiguration:config delegate:self delegateQueue:delegate];
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:url];
    [request setValue:@"Revanced-iOS/DeArrow-backend-1" forHTTPHeaderField:@"User-Agent"];
    [request setValue:self.limit==RVDAMetadataLimit ? @"application/json" : @"image/webp,image/jpeg,image/png" forHTTPHeaderField:@"Accept"];
    self.task=[self.session dataTaskWithRequest:request];[self.task resume];
    void (^started)(void)=self.onStart;self.onStart=nil;if (started) started();
    }
}
- (void)finish:(RVDeArrowBlocker)blocker errorCode:(NSInteger)code {
    [self finishData:nil response:nil blocker:blocker errorCode:code];
}
- (void)finishData:(NSData *)data response:(NSHTTPURLResponse *)incomingResponse blocker:(RVDeArrowBlocker)blocker errorCode:(NSInteger)code {
    void (^completion)(NSData *,NSHTTPURLResponse *,RVDeArrowBlocker,NSInteger);NSURLSession *session;NSData *body;NSHTTPURLResponse *response;
    @synchronized(self) {
        if (self.completed) return;self.completed=YES;completion=self.completion;self.completion=nil;self.onStart=nil;
        session=self.session;self.session=nil;self.task=nil;body=data ?: self.body.copy;self.body=nil;response=incomingResponse ?: self.response;
    }
    [session invalidateAndCancel];if (completion) completion(body,response,blocker,code);
}
- (void)cancel { [self finish:RVDeArrowBlockerCancelled errorCode:NSURLErrorCancelled]; }
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition))completion {
    RVDeArrowBlocker blocker=RVDeArrowBlockerNone;BOOL allow=NO;
    @synchronized(self) {
        if (!self.completed && [response isKindOfClass:NSHTTPURLResponse.class]) {
            self.response=(NSHTTPURLResponse *)response;
            if (response.expectedContentLength>(int64_t)self.limit) blocker=RVDeArrowBlockerBodyLimit;
            else allow=self.response.statusCode==200;
        } else blocker=RVDeArrowBlockerNetwork;
    }
    completion(allow ? NSURLSessionResponseAllow : NSURLSessionResponseCancel);
    if (!allow) [self finish:blocker errorCode:0];
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveData:(NSData *)data {
    BOOL overflow=NO;
    @synchronized(self) {
        if (self.completed) return;
        overflow=data.length>self.limit-self.body.length;if (!overflow) [self.body appendData:data];
    }
    if (overflow) [self finish:RVDeArrowBlockerBodyLimit errorCode:0];
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completion {
    // Do not follow an original-image fallback, forward credentials, or count it
    // as a custom image. Custom endpoints should be their final HTTPS location.
    @synchronized(self) { self.response=response; }
    completion(nil);[self finish:RVDeArrowBlockerRedirect errorCode:0];
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
    [self finish:error ? RVDeArrowBlockerNetwork : RVDeArrowBlockerNone errorCode:error.code];
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,NSURLCredential *))completion {
    completion([challenge.protectionSpace.authenticationMethod isEqual:NSURLAuthenticationMethodServerTrust] ? NSURLSessionAuthChallengePerformDefaultHandling : NSURLSessionAuthChallengeCancelAuthenticationChallenge,nil);
}
@end

// A network flight owns independent cancellation tickets. One recycled row
// cannot cancel another row's request for the same image/configuration.
@interface RVDAAuxiliaryFlight : NSObject
@property(nonatomic,strong) RVDAHTTP *transfer;
@property(nonatomic,strong) NSMutableArray<RVDAHTTP *> *subscribers;
@property(nonatomic) BOOL started;
@end
@implementation RVDAAuxiliaryFlight @end

@interface RVDAMetadata : NSObject
@property(nonatomic) RVDeArrowDecision titleDecision,thumbnailDecision;
@property(nonatomic,copy) NSString *title;
@property(nonatomic,copy) NSArray<NSDictionary *> *casualVotes;
@property(nonatomic,strong) NSNumber *videoDuration,*randomTime;
@property(nonatomic,strong) NSNumber *timestamp;
@property(nonatomic) NSUInteger titleCandidates,thumbnailCandidates;
@property(nonatomic) NSInteger httpStatus;
@property(nonatomic) RVDeArrowBlocker blocker;
@property(nonatomic) NSTimeInterval expires,lastUsed;
@end
@implementation RVDAMetadata @end
@interface RVDAImage : NSObject
@property(nonatomic,strong) UIImage *image;
@property(nonatomic) NSUInteger cost;
@property(nonatomic) NSInteger httpStatus;
@property(nonatomic) RVDeArrowBlocker blocker;
@property(nonatomic) NSTimeInterval expires,lastUsed;
@end
@implementation RVDAImage @end
@interface RVDAJob : NSObject
@property(nonatomic,copy) NSString *videoID;
@property(nonatomic) uint64_t revision;
@property(nonatomic,strong) RVDeArrowOptions *options;
@property(nonatomic,strong) NSMutableArray<RVDeArrowRequest *> *consumers;
@property(nonatomic,strong) RVDAMetadata *metadata;
@property(nonatomic,strong) RVDAImage *image;
@property(nonatomic,strong) RVDAHTTP *transfer;
@property(nonatomic) BOOL metadataCacheHit,imageCacheHit,imageQueued;
@property(nonatomic) NSUInteger imageRetries;
@end
@implementation RVDAJob @end
@interface RVDAOperation : NSObject
@property(nonatomic,strong) RVDAJob *job;
@property(nonatomic,strong) NSURL *URL;
@property(nonatomic) BOOL image;
@end
@implementation RVDAOperation @end
static BOOL RVDAJobBackground(RVDAJob *job) {
    for (RVDeArrowRequest *consumer in job.consumers) if (!consumer.cancelled && !consumer.background) return NO;
    return YES;
}

@interface RVDeArrowBackend ()
@property(atomic,readwrite) uint64_t configurationRevision;
@property(nonatomic,strong) dispatch_queue_t queue;
@property(nonatomic,strong) RVDeArrowOptions *options;
@property(nonatomic,strong) NSMutableDictionary<NSString *,RVDAJob *> *jobs;
@property(nonatomic,strong) NSMutableArray<RVDAOperation *> *operations;
@property(nonatomic,strong) NSMutableArray<NSDictionary *> *auxiliaryOperations;
@property(nonatomic,strong) NSMutableDictionary<NSString *,RVDAAuxiliaryFlight *> *auxiliaryFlights;
@property(nonatomic) BOOL auxiliaryTurn;
@property(nonatomic,strong) NSMutableDictionary<NSString *,RVDAMetadata *> *metadataCache;
@property(nonatomic,strong) NSMutableDictionary<NSString *,RVDAImage *> *imageCache;
@property(nonatomic,strong) NSMutableDictionary<NSString *,NSNumber *> *cooldowns;
@property(nonatomic) NSUInteger activeTransfers,activePrefetchTransfers,imageCost;
@property(nonatomic) BOOL diskLoaded,flushScheduled,pumping;
@property(nonatomic,copy) NSString *salt;
@property(nonatomic,strong) NSMutableDictionary *counts,*state;
@property(nonatomic,strong) NSMutableArray *events;
@property(nonatomic) NSTimeInterval started;
- (void)cancelRequest:(RVDeArrowRequest *)request;
- (RVDeArrowRequest *)requestVideoID:(NSString *)videoID parts:(RVDeArrowParts)parts clientGeneration:(uint64_t)generation background:(BOOL)background completion:(void (^)(RVDeArrowResult *))completion;
- (void)record:(RVDeArrowCheckpoint)checkpoint blocker:(RVDeArrowBlocker)blocker video:(NSString *)video details:(NSDictionary *)details;
- (void)recordRequest:(RVDeArrowRequest *)request checkpoint:(RVDeArrowCheckpoint)checkpoint blocker:(RVDeArrowBlocker)blocker;
- (void)pump;
- (void)enqueueAuxiliary:(RVDAHTTP *)transfer URL:(NSURL *)url timeout:(NSTimeInterval)timeout;
- (void)processMetadata:(RVDAJob *)job;
- (void)deliver:(RVDAJob *)job;
- (void)scheduleFlush;
@end

@implementation RVDeArrowRequest
- (void)cancel {
    @synchronized(self) { if (self.cancelled) return;self.cancelled=YES;self.completion=nil; }
    [self.backend cancelRequest:self];
}
- (void)recordCheckpoint:(RVDeArrowCheckpoint)checkpoint blocker:(RVDeArrowBlocker)blocker {
    [self.backend recordRequest:self checkpoint:checkpoint blocker:blocker];
}
@end

@implementation RVDeArrowBackend
+ (instancetype)sharedBackend {
    static RVDeArrowBackend *backend;static dispatch_once_t once;dispatch_once(&once,^{ backend=[self new]; });return backend;
}
- (instancetype)init {
    if ((self=[super init])) {
        _configurationRevision=1;_options=RVDeArrowOptions.defaultOptions;
        _queue=dispatch_queue_create("RVPort.DeArrow.Backend",DISPATCH_QUEUE_SERIAL);
        dispatch_queue_set_specific(_queue,RVDAQueueKey,(__bridge void *)self,NULL);
        _jobs=[NSMutableDictionary dictionary];_operations=[NSMutableArray array];_auxiliaryOperations=[NSMutableArray array];_auxiliaryFlights=[NSMutableDictionary dictionary];
        _metadataCache=[NSMutableDictionary dictionary];_imageCache=[NSMutableDictionary dictionary];_cooldowns=[NSMutableDictionary dictionary];
        _counts=[NSMutableDictionary dictionary];_state=[NSMutableDictionary dictionary];_events=[NSMutableArray array];
        _salt=NSUUID.UUID.UUIDString;_started=NSProcessInfo.processInfo.systemUptime;
        [self record:RVDeArrowCheckpointBackendReady blocker:RVDeArrowBlockerNone video:nil details:@{@"backend_schema":@1}];
    }return self;
}
- (void)sync:(dispatch_block_t)block {
    if (dispatch_get_specific(RVDAQueueKey)==(__bridge void *)self) block();else dispatch_sync(self.queue,block);
}
+ (NSString *)cleanSubmittedTitle:(id)value {
    NSString *title=RVDATitleText(value);if (!title) return nil;
    static NSRegularExpression *markers;static dispatch_once_t once;
    dispatch_once(&once,^{ markers=[NSRegularExpression regularExpressionWithPattern:@"(^|\\s)>(\\S)" options:0 error:nil]; });
    title=[markers stringByReplacingMatchesInString:title options:0 range:NSMakeRange(0,title.length) withTemplate:@"$1$2"];
    title=[title stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];return title.length ? title : nil;
}
- (NSDictionary *)optionIdentity:(RVDeArrowOptions *)options {
    return @{@"branding":options.brandingEndpoint.absoluteString,@"thumbnail":options.thumbnailEndpoint.absoluteString,
        @"metadata_ttl":@(options.metadataTTL),@"negative_ttl":@(options.negativeTTL),@"timeout":@(options.requestTimeout),
        @"persist":@(options.persistMetadata)};
}
- (void)invalidateRequests {
    self.configurationRevision++;
    for (RVDAJob *job in self.jobs.allValues) {
        for (RVDeArrowRequest *request in job.consumers) @synchronized(request) { request.cancelled=YES;request.completion=nil; }
        [job.transfer cancel];job.transfer=nil;
    }
    [self.jobs removeAllObjects];[self.operations removeAllObjects];
}
- (BOOL)configure:(RVDeArrowOptions *)options {
    if (![options isKindOfClass:RVDeArrowOptions.class]) return NO;
    RVDeArrowOptions *copy=[options copy];
    if (!RVDAEndpoint(copy.brandingEndpoint) || !RVDAEndpoint(copy.thumbnailEndpoint) ||
        !isfinite(copy.metadataTTL) || copy.metadataTTL<60 || copy.metadataTTL>86400 ||
        !isfinite(copy.negativeTTL) || copy.negativeTTL<10 || copy.negativeTTL>3600 ||
        !isfinite(copy.requestTimeout) || copy.requestTimeout<3 || copy.requestTimeout>60) return NO;
    [self sync:^{
        if ([[self optionIdentity:self.options] isEqual:[self optionIdentity:copy]]) { self.options.diagnosticsEnabled=copy.diagnosticsEnabled;return; }
        [self record:RVDeArrowCheckpointBackendReady blocker:RVDeArrowBlockerConfigurationChanged video:nil details:nil];
        [self invalidateRequests];self.options=copy;self.diskLoaded=NO;
        [self.metadataCache removeAllObjects];[self.imageCache removeAllObjects];self.imageCost=0;[self.cooldowns removeAllObjects];
    }];return YES;
}
- (NSURL *)diskURL {
    NSURL *root=[NSFileManager.defaultManager URLsForDirectory:NSCachesDirectory inDomains:NSUserDomainMask].firstObject;
    return [[root URLByAppendingPathComponent:@"RVPort.DeArrow" isDirectory:YES] URLByAppendingPathComponent:@"branding-v1.json"];
}
- (void)setDiagnosticsEnabled:(BOOL)enabled {
    [self sync:^{ self.options.diagnosticsEnabled=enabled; }];
}
- (void)loadDisk {
    if (self.diskLoaded) return;self.diskLoaded=YES;if (!self.options.persistMetadata) return;
    NSURL *url=[self diskURL];if (!url) return;
    NSDictionary *attributes=[NSFileManager.defaultManager attributesOfItemAtPath:url.path error:nil];
    if (!attributes || [attributes[NSFileSize] unsignedLongLongValue]>RVDAMetadataLimit) return;
    NSData *data=[NSData dataWithContentsOfURL:url options:0 error:nil];
    id root=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    if (![root isKindOfClass:NSDictionary.class] || ![root[@"schema"] isEqual:@1] ||
        ![root[@"namespace"] isEqual:RVDAHash(self.options.brandingEndpoint.absoluteString)] || ![root[@"records"] isKindOfClass:NSArray.class]) return;
    NSTimeInterval now=RVDANow();NSUInteger loaded=0;
    for (id record in root[@"records"]) {
        if (loaded>=RVDAMaxEntries) break;
        if (![record isKindOfClass:NSDictionary.class] || !RVDAVideoID(record[@"video"]) || !RVDANumber(record[@"expires"])) continue;
        NSTimeInterval expiry=[record[@"expires"] doubleValue];if (expiry<=now || expiry>now+MAX(self.options.metadataTTL,self.options.negativeTTL)+60) continue;
        id td=record[@"title_decision"],hd=record[@"thumbnail_decision"];
        if (!RVDANumber(td) || !RVDANumber(hd) || [td unsignedIntegerValue]>RVDeArrowDecisionInvalid ||
            [td doubleValue]!=floor([td doubleValue]) || [hd doubleValue]!=floor([hd doubleValue]) ||
            [hd unsignedIntegerValue]>RVDeArrowDecisionInvalid || [td unsignedIntegerValue]<RVDeArrowDecisionOriginal ||
            [hd unsignedIntegerValue]<RVDeArrowDecisionOriginal) continue;
        RVDAMetadata *entry=[RVDAMetadata new];entry.titleDecision=[td unsignedIntegerValue];entry.thumbnailDecision=[hd unsignedIntegerValue];
        // Stored titles are already normalized. Reapplying escape-marker cleanup
        // on every launch could progressively remove literal '>' characters.
        entry.title=RVDATitleText(record[@"title"]);id timestamp=record[@"timestamp"];
        entry.casualVotes=RVDACasualVotes(record[@"casual_votes"]);
        if (RVDANumber(record[@"duration"]) && [record[@"duration"] doubleValue]>0 && [record[@"duration"] doubleValue]<=604800) entry.videoDuration=record[@"duration"];
        if (RVDANumber(record[@"random_time"]) && [record[@"random_time"] doubleValue]>=0 && [record[@"random_time"] doubleValue]<1) entry.randomTime=record[@"random_time"];
        if (RVDANumber(timestamp) && [timestamp doubleValue]>=0 && [timestamp doubleValue]<=604800) entry.timestamp=timestamp;
        if ((entry.titleDecision==RVDeArrowDecisionCustom && !entry.title) || (entry.thumbnailDecision==RVDeArrowDecisionCustom && !entry.timestamp)) continue;
        entry.expires=expiry;entry.lastUsed=now;
        entry.httpStatus=[record[@"http"] isEqual:@404] ? 404 : 200;
        entry.blocker=entry.httpStatus==404 ? RVDeArrowBlockerVideoMissing : RVDeArrowBlockerNone;
        self.metadataCache[record[@"video"]]=entry;loaded++;
    }
    [self record:RVDeArrowCheckpointBackendReady blocker:RVDeArrowBlockerNone video:nil details:@{@"disk_records_loaded":@(loaded)}];
}
- (void)scheduleFlush {
    if (!self.options.persistMetadata || self.flushScheduled) return;self.flushScheduled=YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),self.queue,^{
        self.flushScheduled=NO;if (!self.options.persistMetadata) return;NSMutableArray *records=[NSMutableArray array];NSTimeInterval now=RVDANow();
        for (NSString *video in self.metadataCache) {
            RVDAMetadata *entry=self.metadataCache[video];
            if (entry.expires<=now || entry.titleDecision==RVDeArrowDecisionUnavailable) continue;
            NSMutableDictionary *record=[@{@"video":video,@"expires":@(entry.expires),@"http":@(entry.httpStatus),
                @"title_decision":@(entry.titleDecision),@"thumbnail_decision":@(entry.thumbnailDecision)} mutableCopy];
            if (entry.title) record[@"title"]=entry.title;if (entry.timestamp) record[@"timestamp"]=entry.timestamp;
            if (entry.casualVotes) record[@"casual_votes"]=entry.casualVotes;
            if (entry.videoDuration) record[@"duration"]=entry.videoDuration;if (entry.randomTime) record[@"random_time"]=entry.randomTime;
            [records addObject:record];if (records.count>=RVDAMaxEntries) break;
        }
        NSDictionary *root=@{@"schema":@1,@"namespace":RVDAHash(self.options.brandingEndpoint.absoluteString),@"records":records};
        NSData *data=[NSJSONSerialization dataWithJSONObject:root options:0 error:nil];if (!data || data.length>RVDAMetadataLimit) return;
        NSURL *url=[self diskURL];if (!url) return;
        [NSFileManager.defaultManager createDirectoryAtURL:url.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil];
        [data writeToURL:url options:NSDataWritingAtomic error:nil];
    });
}
- (void)clearCaches {
    [self sync:^{
        [self invalidateRequests];[self.metadataCache removeAllObjects];[self.imageCache removeAllObjects];self.imageCost=0;
        [self.cooldowns removeAllObjects];self.diskLoaded=YES;NSURL *url=[self diskURL];
        if (url) [NSFileManager.defaultManager removeItemAtURL:url error:nil];
        [self record:RVDeArrowCheckpointBackendReady blocker:RVDeArrowBlockerConfigurationChanged video:nil details:@{@"cache_cleared":@YES}];
    }];
}
- (void)record:(RVDeArrowCheckpoint)checkpoint blocker:(RVDeArrowBlocker)blocker video:(NSString *)video details:(NSDictionary *)details {
    if (!self.options.diagnosticsEnabled || checkpoint<1 || checkpoint>12) return;
    NSString *name=RVDACheckpointName(checkpoint);self.counts[name]=@([self.counts[name] unsignedLongLongValue]+1);
    NSMutableDictionary *event=[@{@"checkpoint":name,@"blocker":RVDABlockerName(blocker),
        @"elapsed_ms":@(MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-self.started))),@"configuration_revision":@(self.configurationRevision)} mutableCopy];
    if (video.length) event[@"video_hash"]=[RVDAHash([self.salt stringByAppendingString:video]) substringToIndex:12];
    if (details) [event addEntriesFromDictionary:details];
    if (self.events.count>=48) [self.events removeObjectAtIndex:0];
    NSDictionary *snapshot=event.copy;[self.events addObject:snapshot];self.state[@"last_checkpoint"]=snapshot;
}
- (void)recordRequest:(RVDeArrowRequest *)request checkpoint:(RVDeArrowCheckpoint)checkpoint blocker:(RVDeArrowBlocker)blocker {
    if (![@[@2,@3,@4,@9,@10,@11,@12] containsObject:@(checkpoint)] || blocker>RVDeArrowBlockerCandidateMissing) return;
    dispatch_async(self.queue,^{
        if (request.cancelled || request.configurationRevision!=self.configurationRevision) return;
        self.state[@"presentation_observed"]=@YES;
        [self record:checkpoint blocker:blocker video:request.videoID details:@{@"client_generation":@(request.clientGeneration),@"presentation_observation":@YES}];
    });
}
- (NSDictionary *)diagnostics {
    __block NSDictionary *report;
    [self sync:^{
        report=@{@"schema":@1,@"backend_implemented":@YES,@"presentation_observed":@([self.state[@"presentation_observed"] boolValue]),@"device_verified":@NO,
            @"presentation_observed_meaning":@"adapter_checkpoint_only_not_visible_replacement",@"diagnostics_enabled":@(self.options.diagnosticsEnabled),@"configuration_revision":@(self.configurationRevision),
            @"in_flight_videos":@(self.jobs.count),@"active_transfers":@(self.activeTransfers),@"queued_transfers":@(self.operations.count),@"queued_auxiliary_transfers":@(self.auxiliaryOperations.count),@"auxiliary_shared_flights":@(self.auxiliaryFlights.count),
            @"metadata_cache_entries":@(self.metadataCache.count),@"image_cache_entries":@(self.imageCache.count),@"decoded_image_bytes":@(self.imageCost),
            @"active_prefetch_transfers":@(self.activePrefetchTransfers),@"parallel_transfer_limit":@(RVDAMaxTransfers),@"prefetch_transfer_limit":@(RVDAMaxPrefetchTransfers),@"foreground_reserved_slots":@(RVDAMaxTransfers-RVDAMaxPrefetchTransfers),
            @"counts":self.counts.copy,@"state":self.state.copy,@"recent_events":self.events.copy};
    }];return report;
}

- (RVDeArrowDecision)select:(id)candidates title:(BOOL)isTitle metadata:(RVDAMetadata *)entry {
    if (![candidates isKindOfClass:NSArray.class] || [candidates count]>512) return RVDeArrowDecisionInvalid;
    if (![candidates count]) return RVDeArrowDecisionMissing;
    id top=[candidates firstObject];if (![top isKindOfClass:NSDictionary.class]) return RVDeArrowDecisionInvalid;
    if (!RVDABoolean(top[@"locked"]) || !RVDABoolean(top[@"original"]) || !RVDANumber(top[@"votes"]) || fabs([top[@"votes"] doubleValue])>1e9) return RVDeArrowDecisionInvalid;
    if (![top[@"locked"] boolValue] && [top[@"votes"] doubleValue]<0) return RVDeArrowDecisionUntrusted;
    if ([top[@"original"] boolValue]) return RVDeArrowDecisionOriginal;
    if (isTitle) {
        NSString *title=[RVDeArrowBackend cleanSubmittedTitle:top[@"title"]];if (!title) return RVDeArrowDecisionInvalid;
        entry.title=title;
    } else {
        id time=top[@"timestamp"];if (!RVDANumber(time) || [time doubleValue]<0 || [time doubleValue]>604800) return RVDeArrowDecisionInvalid;
        entry.timestamp=time;
    }return RVDeArrowDecisionCustom;
}
- (RVDAMetadata *)unavailable:(RVDeArrowBlocker)blocker status:(NSInteger)status {
    RVDAMetadata *entry=[RVDAMetadata new];entry.titleDecision=RVDeArrowDecisionUnavailable;
    entry.thumbnailDecision=RVDeArrowDecisionUnavailable;entry.httpStatus=status;entry.blocker=blocker;
    entry.lastUsed=RVDANow();entry.expires=entry.lastUsed+15;return entry;
}
- (void)storeMetadata:(RVDAMetadata *)entry video:(NSString *)video {
    NSTimeInterval now=RVDANow();
    for (NSString *key in self.metadataCache.allKeys) if (self.metadataCache[key].expires<=now) [self.metadataCache removeObjectForKey:key];
    while (self.metadataCache.count>=RVDAMaxEntries && !self.metadataCache[video]) {
        NSString *oldest=nil;NSTimeInterval age=DBL_MAX;
        for (NSString *key in self.metadataCache) if (self.metadataCache[key].lastUsed<age) { oldest=key;age=self.metadataCache[key].lastUsed; }
        if (!oldest) break;[self.metadataCache removeObjectForKey:oldest];
    }
    self.metadataCache[video]=entry;[self scheduleFlush];
}
- (NSString *)imageKey:(RVDAJob *)job {
    return [NSString stringWithFormat:@"%@/%@",job.videoID,job.metadata.timestamp.stringValue];
}
- (void)removeImage:(NSString *)key {
    RVDAImage *entry=self.imageCache[key];if (!entry) return;
    self.imageCost-=MIN(self.imageCost,entry.cost);[self.imageCache removeObjectForKey:key];
}
- (void)storeImage:(RVDAImage *)entry job:(RVDAJob *)job {
    NSString *key=[self imageKey:job];[self removeImage:key];NSTimeInterval now=RVDANow();
    for (NSString *other in self.imageCache.allKeys) if (self.imageCache[other].expires<=now) [self removeImage:other];
    while (self.imageCache.count>=RVDAMaxEntries || self.imageCost+entry.cost>RVDAMaxImageCost) {
        NSString *oldest=nil;NSTimeInterval age=DBL_MAX;
        for (NSString *other in self.imageCache) if (self.imageCache[other].lastUsed<age) { oldest=other;age=self.imageCache[other].lastUsed; }
        if (!oldest) break;[self removeImage:oldest];
    }
    self.imageCache[key]=entry;self.imageCost+=entry.cost;
}
- (void)deliver:(RVDAJob *)job {
    if (!job.metadata) return;
    for (RVDeArrowRequest *request in job.consumers.copy) {
        if (request.cancelled) { [job.consumers removeObjectIdenticalTo:request];continue; }
        RVDeArrowResult *result=[RVDeArrowResult new];result.videoID=job.videoID;result.configurationRevision=job.revision;
        result.clientGeneration=request.clientGeneration;BOOL title=(request.parts&RVDeArrowPartsTitle)!=0,image=(request.parts&RVDeArrowPartsThumbnail)!=0;
        result.titleDecision=title ? job.metadata.titleDecision : RVDeArrowDecisionDisabled;
        result.thumbnailDecision=image ? job.metadata.thumbnailDecision : RVDeArrowDecisionDisabled;
        result.title=title && result.titleDecision==RVDeArrowDecisionCustom ? job.metadata.title : nil;
        result.casualVotes=job.metadata.casualVotes ?: @[];result.videoDuration=job.metadata.videoDuration;result.randomTime=job.metadata.randomTime;
        result.thumbnailTimestamp=image && result.thumbnailDecision==RVDeArrowDecisionCustom ? job.metadata.timestamp : nil;
        result.metadataBlocker=job.metadata.blocker;result.metadataHTTPStatus=job.metadata.httpStatus;result.metadataCacheHit=job.metadataCacheHit;
        result.titleBlocker=title ? [self blockerForMetadata:job.metadata decision:result.titleDecision] : RVDeArrowBlockerNone;
        result.thumbnailSelectionBlocker=image ? [self blockerForMetadata:job.metadata decision:result.thumbnailDecision] : RVDeArrowBlockerNone;
        if (image && result.thumbnailDecision==RVDeArrowDecisionCustom) {
            result.imageState=job.image ? (job.image.image ? RVDeArrowImageStateReady : RVDeArrowImageStateUnavailable) : RVDeArrowImageStatePending;
            result.thumbnailImage=job.image.image;result.imageBlocker=job.image.blocker;result.imageHTTPStatus=job.image.httpStatus;result.imageCacheHit=job.imageCacheHit;
        } else result.imageState=RVDeArrowImageStateNotRequested;
        result.final=result.imageState!=RVDeArrowImageStatePending;
        if (!result.final && request.pendingDelivered) continue;
        if (!result.final) request.pendingDelivered=YES;
        dispatch_async(dispatch_get_main_queue(),^{
            void (^completion)(RVDeArrowResult *)=nil;
            @synchronized(request) {
                if (request.configurationRevision!=self.configurationRevision) { request.cancelled=YES;request.completion=nil; }
                if (!request.cancelled && !request.finished) completion=request.completion;
            }
            if (completion) completion(result);
            if (result.final) @synchronized(request) { request.finished=YES;request.completion=nil; }
        });
        if (result.final) [job.consumers removeObjectIdenticalTo:request];
    }
    if (!job.consumers.count && self.jobs[job.videoID]==job) {
        [job.transfer cancel];job.transfer=nil;[self.jobs removeObjectForKey:job.videoID];
        NSIndexSet *indexes=[self.operations indexesOfObjectsPassingTest:^BOOL(RVDAOperation *operation,NSUInteger index,BOOL *stop) { return operation.job==job; }];
        [self.operations removeObjectsAtIndexes:indexes];
    }
}
- (RVDeArrowRequest *)requestVideoID:(NSString *)videoID parts:(RVDeArrowParts)parts clientGeneration:(uint64_t)generation completion:(void (^)(RVDeArrowResult *))completion {
    return [self requestVideoID:videoID parts:parts clientGeneration:generation background:NO completion:completion];
}
- (RVDeArrowRequest *)prefetchVideoID:(NSString *)videoID parts:(RVDeArrowParts)parts clientGeneration:(uint64_t)generation completion:(void (^)(RVDeArrowResult *))completion {
    return [self requestVideoID:videoID parts:parts clientGeneration:generation background:YES completion:completion];
}
- (RVDeArrowRequest *)requestVideoID:(NSString *)videoID parts:(RVDeArrowParts)parts clientGeneration:(uint64_t)generation background:(BOOL)background completion:(void (^)(RVDeArrowResult *))completion {
    RVDeArrowRequest *request=[RVDeArrowRequest new];request.backend=self;request.identifier=NSUUID.UUID.UUIDString;
    request.background=background;
    request.videoID=[videoID isKindOfClass:NSString.class] && videoID.length<=128 ? [videoID copy] : @"";
    request.clientGeneration=generation;request.parts=parts&RVDeArrowPartsAll;request.completion=completion;
    [self sync:^{ request.configurationRevision=self.configurationRevision; }];
    dispatch_async(self.queue,^{
        if (request.cancelled || request.configurationRevision!=self.configurationRevision) { [request cancel];return; }
        RVDAJob *job;
        if (!request.parts) {
            job=[RVDAJob new];job.videoID=request.videoID;job.revision=request.configurationRevision;
            job.consumers=[NSMutableArray arrayWithObject:request];job.metadata=[self unavailable:RVDeArrowBlockerNone status:0];[self deliver:job];return;
        }
        job=self.jobs[request.videoID];
        if (job) {
            [job.consumers addObject:request];
            [self record:RVDeArrowCheckpointMetadataRequested blocker:RVDeArrowBlockerNone video:job.videoID details:@{@"request_coalesced":@YES}];
            if (job.metadata) [self processMetadata:job];else [self pump];return;
        }
        job=[RVDAJob new];job.videoID=request.videoID;job.revision=request.configurationRevision;job.options=self.options;
        job.consumers=[NSMutableArray arrayWithObject:request];
        if (!RVDAVideoID(request.videoID) || !request.parts || self.jobs.count>=RVDAMaxJobs) {
            RVDeArrowBlocker blocker=!request.parts ? RVDeArrowBlockerNone : !RVDAVideoID(request.videoID) ? RVDeArrowBlockerInvalidVideoID : RVDeArrowBlockerQueueFull;
            job.metadata=[self unavailable:blocker status:0];[self deliver:job];return;
        }
        [self loadDisk];self.jobs[job.videoID]=job;NSTimeInterval now=RVDANow();
        RVDAMetadata *entry=self.metadataCache[job.videoID];
        if (entry && entry.expires>now) {
            job.metadata=entry;job.metadataCacheHit=YES;entry.lastUsed=now;
            [self record:RVDeArrowCheckpointMetadataRequested blocker:RVDeArrowBlockerNone video:job.videoID details:@{@"cache_hit":@YES}];
            [self record:RVDeArrowCheckpointMetadataReceived blocker:entry.blocker video:job.videoID details:@{@"cache_hit":@YES,@"http_status":@(entry.httpStatus)}];
            [self processMetadata:job];return;
        }
        if (entry) [self.metadataCache removeObjectForKey:job.videoID];
        [self record:RVDeArrowCheckpointMetadataRequested blocker:RVDeArrowBlockerNone video:job.videoID details:@{@"cache_hit":@NO}];
        RVDAOperation *operation=[RVDAOperation new];operation.job=job;operation.URL=RVDAURL(job.options.brandingEndpoint,job.videoID,nil,YES);
        [self.operations addObject:operation];[self pump];
    });return request;
}
- (void)cancelRequest:(RVDeArrowRequest *)request {
    dispatch_async(self.queue,^{
        RVDAJob *job=self.jobs[request.videoID];if (!job) return;
        if (![job.consumers containsObject:request]) return;
        [job.consumers removeObjectIdenticalTo:request];
        [self record:RVDeArrowCheckpointLifecycleRebound blocker:RVDeArrowBlockerCancelled video:request.videoID details:@{@"remaining_consumers":@(job.consumers.count)}];
        if (job.consumers.count) return;
        [job.transfer cancel];job.transfer=nil;[self.jobs removeObjectForKey:job.videoID];
        NSIndexSet *indexes=[self.operations indexesOfObjectsPassingTest:^BOOL(RVDAOperation *operation,NSUInteger index,BOOL *stop) { return operation.job==job; }];
        [self.operations removeObjectsAtIndexes:indexes];[self pump];
    });
}
- (void)processMetadata:(RVDAJob *)job {
    [self record:RVDeArrowCheckpointDecisionSelected blocker:job.metadata.blocker video:job.videoID details:@{
        @"title_decision":RVDADecisionName(job.metadata.titleDecision),@"thumbnail_decision":RVDADecisionName(job.metadata.thumbnailDecision),
        @"title_blocker":RVDABlockerName([self blockerForMetadata:job.metadata decision:job.metadata.titleDecision]),
        @"thumbnail_selection_blocker":RVDABlockerName([self blockerForMetadata:job.metadata decision:job.metadata.thumbnailDecision]),
        @"title_candidates":@(job.metadata.titleCandidates),@"thumbnail_candidates":@(job.metadata.thumbnailCandidates),@"metadata_cache_hit":@(job.metadataCacheHit)}];
    BOOL wantsImage=NO;
    for (RVDeArrowRequest *request in job.consumers) if (!request.cancelled && (request.parts&RVDeArrowPartsThumbnail)) { wantsImage=YES;break; }
    if (wantsImage && job.metadata.thumbnailDecision==RVDeArrowDecisionCustom && !job.image && !job.imageQueued) {
        RVDAImage *image=self.imageCache[[self imageKey:job]];NSTimeInterval now=RVDANow();
        if (image && image.expires>now) {
            job.image=image;job.imageCacheHit=YES;image.lastUsed=now;
            [self record:RVDeArrowCheckpointImageReceived blocker:image.blocker video:job.videoID details:@{@"cache_hit":@YES,@"decoded":@(image.image!=nil),@"http_status":@(image.httpStatus)}];
        }
        else {
            if (image) [self removeImage:[self imageKey:job]];
            job.imageQueued=YES;RVDAOperation *operation=[RVDAOperation new];operation.job=job;operation.image=YES;
            operation.URL=RVDAURL(job.options.thumbnailEndpoint,job.videoID,job.metadata.timestamp,NO);[self.operations addObject:operation];
        }
    }
    [self deliver:job];[self pump];
}
- (RVDeArrowBlocker)blockerForMetadata:(RVDAMetadata *)metadata decision:(RVDeArrowDecision)decision {
    if ((decision==RVDeArrowDecisionUnavailable || decision==RVDeArrowDecisionMissing) && metadata.blocker!=RVDeArrowBlockerNone) return metadata.blocker;
    return RVDASelectionBlocker(decision);
}
- (void)cooldown:(RVDAOperation *)operation response:(NSHTTPURLResponse *)response blocker:(RVDeArrowBlocker)blocker {
    NSInteger status=response.statusCode;NSTimeInterval delay=0;
    if (status==429) {
        delay=60;NSString *retry=RVDAHeader(response,@"Retry-After");NSScanner *scan=[NSScanner scannerWithString:retry ?: @""];
        scan.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];double seconds=0;
        if ([scan scanDouble:&seconds] && scan.isAtEnd && isfinite(seconds)) delay=seconds;
        else if (retry.length) {
            NSDateFormatter *format=[NSDateFormatter new];format.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
            format.timeZone=[NSTimeZone timeZoneForSecondsFromGMT:0];format.dateFormat=@"EEE, dd MMM yyyy HH:mm:ss z";
            NSDate *date=[format dateFromString:retry];if (date) delay=[date timeIntervalSinceNow];
        }
        delay=MAX(1,MIN(300,delay));
    } else if (status>=500 || blocker==RVDeArrowBlockerNetwork) delay=15;
    if (delay) self.cooldowns[operation.URL.host.lowercaseString]=@(RVDANow()+delay);
}
- (void)metadataResponse:(NSData *)data response:(NSHTTPURLResponse *)response blocker:(RVDeArrowBlocker)blocker operation:(RVDAOperation *)operation {
    RVDAJob *job=operation.job;NSInteger status=response.statusCode;RVDAMetadata *entry=nil;BOOL exactKey=NO,parsed=NO;
    if (blocker==RVDeArrowBlockerNone && status==404) {
        entry=[RVDAMetadata new];entry.titleDecision=RVDeArrowDecisionMissing;entry.thumbnailDecision=RVDeArrowDecisionMissing;
        entry.blocker=RVDeArrowBlockerVideoMissing;entry.httpStatus=404;
    } else if (blocker==RVDeArrowBlockerNone && status==200) {
        id bucket=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        if ([bucket isKindOfClass:NSDictionary.class]) {
            parsed=YES;id record=bucket[job.videoID];exactKey=record!=nil;
            entry=[RVDAMetadata new];entry.httpStatus=200;
            if (!record) {
                entry.titleDecision=RVDeArrowDecisionMissing;entry.thumbnailDecision=RVDeArrowDecisionMissing;entry.blocker=RVDeArrowBlockerVideoMissing;
            } else if ([record isKindOfClass:NSDictionary.class]) {
                entry.casualVotes=RVDACasualVotes(record[@"casualVotes"]);
                if (RVDANumber(record[@"videoDuration"]) && [record[@"videoDuration"] doubleValue]>0 && [record[@"videoDuration"] doubleValue]<=604800) entry.videoDuration=record[@"videoDuration"];
                if (RVDANumber(record[@"randomTime"]) && [record[@"randomTime"] doubleValue]>=0 && [record[@"randomTime"] doubleValue]<1) entry.randomTime=record[@"randomTime"];
                entry.titleDecision=[self select:record[@"titles"] title:YES metadata:entry];
                entry.thumbnailDecision=[self select:record[@"thumbnails"] title:NO metadata:entry];
                entry.titleCandidates=[record[@"titles"] isKindOfClass:NSArray.class] ? MIN(512,[record[@"titles"] count]) : 0;
                entry.thumbnailCandidates=[record[@"thumbnails"] isKindOfClass:NSArray.class] ? MIN(512,[record[@"thumbnails"] count]) : 0;
                // Candidate-specific decisions remain independent of each other.
                if (entry.titleDecision==RVDeArrowDecisionInvalid && entry.thumbnailDecision==RVDeArrowDecisionInvalid) entry.blocker=RVDeArrowBlockerCandidateInvalid;
            } else entry=[self unavailable:RVDeArrowBlockerMetadataShape status:status];
        } else entry=[self unavailable:RVDeArrowBlockerMetadataShape status:status];
    } else entry=[self unavailable:blocker==RVDeArrowBlockerNone ? RVDeArrowBlockerMetadataHTTP : blocker status:status];
    NSTimeInterval now=RVDANow();entry.lastUsed=now;
    if (entry.titleDecision!=RVDeArrowDecisionUnavailable) {
        BOOL useful=entry.titleDecision==RVDeArrowDecisionCustom || entry.titleDecision==RVDeArrowDecisionOriginal ||
            entry.thumbnailDecision==RVDeArrowDecisionCustom || entry.thumbnailDecision==RVDeArrowDecisionOriginal;
        entry.expires=now+(useful ? job.options.metadataTTL : job.options.negativeTTL);
    }
    [self storeMetadata:entry video:job.videoID];job.metadata=entry;
    [self record:RVDeArrowCheckpointMetadataReceived blocker:entry.blocker video:job.videoID details:@{@"http_status":@(status),@"json_parsed":@(parsed),@"exact_video_key":@(exactKey)}];
    [self processMetadata:job];
}
- (UIImage *)decodeImage:(NSData *)data response:(NSHTTPURLResponse *)response cost:(NSUInteger *)cost {
    NSString *mime=response.MIMEType.lowercaseString;
    if (![@[@"image/webp",@"image/jpeg",@"image/png"] containsObject:mime] || !data.length || data.length>RVDAImageLimit) return nil;
    CGImageSourceRef source=CGImageSourceCreateWithData((__bridge CFDataRef)data,(__bridge CFDictionaryRef)@{(id)kCGImageSourceShouldCache:@NO});
    if (!source) return nil;
    NSDictionary *properties=CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(source,0,NULL));
    NSUInteger width=[properties[(id)kCGImagePropertyPixelWidth] unsignedIntegerValue],height=[properties[(id)kCGImagePropertyPixelHeight] unsignedIntegerValue];
    if (!width || !height || width>4096 || height>4096 || width*height>16*1024*1024 || CGImageSourceGetCount(source)!=1) { CFRelease(source);return nil; }
    CGImageRef decoded=CGImageSourceCreateThumbnailAtIndex(source,0,(__bridge CFDictionaryRef)@{
        (id)kCGImageSourceCreateThumbnailFromImageAlways:@YES,(id)kCGImageSourceCreateThumbnailWithTransform:@YES,
        (id)kCGImageSourceShouldCacheImmediately:@YES,(id)kCGImageSourceThumbnailMaxPixelSize:@1280});
    CFRelease(source);if (!decoded) return nil;
    NSUInteger bytes=CGImageGetBytesPerRow(decoded)*CGImageGetHeight(decoded);
    UIImage *image=bytes<=RVDAMaxImageCost ? [UIImage imageWithCGImage:decoded] : nil;
    if (image && cost) *cost=bytes;CGImageRelease(decoded);return image;
}
- (void)imageResponse:(NSData *)data response:(NSHTTPURLResponse *)response blocker:(RVDeArrowBlocker)blocker operation:(RVDAOperation *)operation {
    RVDAJob *job=operation.job;RVDAImage *entry=[RVDAImage new];entry.httpStatus=response.statusCode;
    NSString *failure=RVDAHeader(response,@"X-Failure-Reason").lowercaseString;
    if (blocker==RVDeArrowBlockerNone && entry.httpStatus==204 && job.imageRetries<2 &&
        ([failure containsString:@"not generated"] || [failure containsString:@"not cached"] || [failure containsString:@"queue"])) {
        NSUInteger attempt=++job.imageRetries;
        [self record:RVDeArrowCheckpointImageReceived blocker:RVDeArrowBlockerImageNoContent video:job.videoID details:@{@"http_status":@204,@"generation_retry":@(attempt),@"image_pending":@YES}];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(attempt*2*NSEC_PER_SEC)),self.queue,^{
            if (self.jobs[job.videoID]!=job || job.revision!=self.configurationRevision || !job.consumers.count) return;
            RVDAOperation *retry=[RVDAOperation new];retry.job=job;retry.image=YES;retry.URL=operation.URL;[self.operations addObject:retry];[self pump];
        });return;
    }
    BOOL timestampMatches=NO;
    if (blocker==RVDeArrowBlockerNone && entry.httpStatus==200) {
        NSString *raw=RVDAHeader(response,@"X-Timestamp");double timestamp=0;
        NSScanner *scan=[NSScanner scannerWithString:raw ?: @""];scan.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
        timestampMatches=[scan scanDouble:&timestamp] && scan.isAtEnd && isfinite(timestamp) && timestamp>=0 && timestamp<=604800 &&
            [[NSString stringWithFormat:@"%.3f",timestamp] isEqual:[NSString stringWithFormat:@"%.3f",job.metadata.timestamp.doubleValue]];
        if (!timestampMatches) blocker=RVDeArrowBlockerImageTimestamp;
        else { NSUInteger cost=0;entry.image=[self decodeImage:data response:response cost:&cost];entry.cost=cost;if (!entry.image) blocker=RVDeArrowBlockerImageDecode; }
    } else if (blocker==RVDeArrowBlockerNone) blocker=entry.httpStatus==204 ? RVDeArrowBlockerImageNoContent : RVDeArrowBlockerImageHTTP;
    entry.blocker=blocker;entry.lastUsed=RVDANow();entry.expires=entry.lastUsed+(entry.image ? job.options.metadataTTL : entry.httpStatus==204 ? MIN(60,job.options.negativeTTL) : 15);
    [self storeImage:entry job:job];job.image=entry;
    NSString *reason=RVDAHeader(response,@"X-Failure-Reason").lowercaseString;
    NSString *reasonClass=[reason containsString:@"queue"] ? @"queue" : [reason containsString:@"not cached"] ? @"not_cached" :
        [reason containsString:@"not generated"] ? @"not_generated" : [reason containsString:@"timeout"] ? @"timeout" : reason.length ? @"other" : @"absent";
    [self record:RVDeArrowCheckpointImageReceived blocker:blocker video:job.videoID details:@{@"http_status":@(entry.httpStatus),
        @"timestamp_matches":@(timestampMatches),@"decoded":@(entry.image!=nil),@"decoded_bytes":@(entry.cost),@"failure_reason_class":reasonClass}];
    [self deliver:job];
}
- (void)blockedOperation:(RVDAOperation *)operation {
    RVDAJob *job=operation.job;
    if (operation.image) {
        RVDAImage *entry=[RVDAImage new];entry.blocker=RVDeArrowBlockerBackoff;entry.expires=RVDANow()+15;entry.lastUsed=RVDANow();
        job.image=entry;[self storeImage:entry job:job];[self record:RVDeArrowCheckpointImageReceived blocker:entry.blocker video:job.videoID details:nil];[self deliver:job];
    } else {
        job.metadata=[self unavailable:RVDeArrowBlockerBackoff status:0];[self storeMetadata:job.metadata video:job.videoID];
        [self record:RVDeArrowCheckpointMetadataReceived blocker:RVDeArrowBlockerBackoff video:job.videoID details:nil];[self processMetadata:job];
    }
}
- (void)enqueueAuxiliary:(RVDAHTTP *)subscriber URL:(NSURL *)url timeout:(NSTimeInterval)timeout {
    // All dictionary/queue/subscriber changes use the existing serialized queue.
    dispatch_async(self.queue,^{
        BOOL done;@synchronized(subscriber) { done=subscriber.completed; }if (done) return;
        if (!RVDAEndpoint(url)) { [subscriber finish:RVDeArrowBlockerInvalidOptions errorCode:0];return; }
        NSString *key=[NSString stringWithFormat:@"%llu|%lu|%.3f|%@",(unsigned long long)self.configurationRevision,(unsigned long)subscriber.limit,timeout,url.absoluteString];
        RVDAAuxiliaryFlight *flight=self.auxiliaryFlights[key];
        if (flight) { @synchronized(flight.transfer) { done=flight.transfer.completed; }if (done) flight=nil; }
        if (flight && flight.subscribers.count>=32) { [subscriber finish:RVDeArrowBlockerQueueFull errorCode:0];return; }
        if (!flight) {
            NSIndexSet *cancelled=[self.auxiliaryOperations indexesOfObjectsPassingTest:^BOOL(NSDictionary *op,NSUInteger index,BOOL *stop) {
                RVDAHTTP *pending=op[@"transfer"];@synchronized(pending) { return pending.completed; }
            }];
            [self.auxiliaryOperations removeObjectsAtIndexes:cancelled];
            if (self.auxiliaryFlights.count>=64) { [subscriber finish:RVDeArrowBlockerQueueFull errorCode:0];return; }
            flight=[RVDAAuxiliaryFlight new];flight.subscribers=[NSMutableArray array];flight.transfer=[RVDAHTTP new];flight.transfer.limit=subscriber.limit;
            self.auxiliaryFlights[key]=flight;__weak RVDAAuxiliaryFlight *weakFlight=flight;__weak RVDeArrowBackend *weakSelf=self;__block BOOL admitted=NO,backgroundAdmission=NO;
            flight.transfer.onStart=^{
                RVDAAuxiliaryFlight *current=weakFlight;if (!current) return;current.started=YES;
                for (RVDAHTTP *ticket in current.subscribers.copy) {
                    void (^start)(void);@synchronized(ticket) { start=ticket.onStart;ticket.onStart=nil; }if (start) start();
                }
            };
            RVDAAuxiliaryFlight *ownedFlight=flight;
            flight.transfer.completion=^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker,NSInteger code) {
                RVDeArrowBackend *backend=weakSelf;if (!backend) return;
                dispatch_async(backend.queue,^{
                    RVDAAuxiliaryFlight *current=ownedFlight;
                    if (admitted && backend.activeTransfers) backend.activeTransfers--;
                    if (admitted && backgroundAdmission && backend.activePrefetchTransfers) backend.activePrefetchTransfers--;
                    if (current && backend.auxiliaryFlights[key]==current) [backend.auxiliaryFlights removeObjectForKey:key];
                    NSArray *tickets=current.subscribers.copy;[current.subscribers removeAllObjects];
                    for (RVDAHTTP *ticket in tickets) {
                        [ticket finishData:data response:response blocker:blocker errorCode:code];
                    }
                    [backend pump];
                });
            };
            BOOL (^background)(void)=^{
                for (RVDAHTTP *ticket in weakFlight.subscribers) if (!ticket.background && !ticket.completed) return NO;
                return YES;
            };
            [self.auxiliaryOperations addObject:@{@"transfer":flight.transfer,@"url":url,@"timeout":@(timeout),@"queued_at":@(RVDANow()),@"background":[background copy],@"admit":[^(BOOL speculative) { admitted=YES;backgroundAdmission=speculative; } copy]}];
        }
        __weak RVDAHTTP *weakSubscriber=subscriber;__weak RVDAAuxiliaryFlight *weakFlight=flight;__weak RVDeArrowBackend *weakSelf=self;
        @synchronized(subscriber) {
        if (subscriber.completed) { if (!flight.subscribers.count) [flight.transfer cancel];return; }
        void (^completion)(NSData *,NSHTTPURLResponse *,RVDeArrowBlocker,NSInteger)=subscriber.completion;
        subscriber.completion=^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker,NSInteger code) {
            RVDeArrowBackend *backend=weakSelf;if (!backend) return;
            dispatch_async(backend.queue,^{
                RVDAAuxiliaryFlight *current=weakFlight;RVDAHTTP *ticket=weakSubscriber;
                if (current && ticket) [current.subscribers removeObjectIdenticalTo:ticket];
                if (current && !current.subscribers.count) [current.transfer cancel];
                if (completion) completion(data,response,blocker,code);
            });
        };
        [flight.subscribers addObject:subscriber];
        }
        if (flight.started) { void (^start)(void);@synchronized(subscriber) { start=subscriber.onStart;subscriber.onStart=nil; }if (start) start(); }
        [self pump];
    });
}
- (void)pump {
    if (self.pumping) return;self.pumping=YES;
    while (self.activeTransfers<RVDAMaxTransfers && (self.operations.count || self.auxiliaryOperations.count)) {
        NSIndexSet *dead=[self.operations indexesOfObjectsPassingTest:^BOOL(RVDAOperation *op,NSUInteger index,BOOL *stop) { return self.jobs[op.job.videoID]!=op.job || op.job.revision!=self.configurationRevision || !op.job.consumers.count; }];[self.operations removeObjectsAtIndexes:dead];
        dead=[self.auxiliaryOperations indexesOfObjectsPassingTest:^BOOL(NSDictionary *op,NSUInteger index,BOOL *stop) { RVDAHTTP *ticket=op[@"transfer"];@synchronized(ticket) { return ticket.completed; } }];[self.auxiliaryOperations removeObjectsAtIndexes:dead];
        NSUInteger nativeIndex=NSNotFound,auxiliaryIndex=NSNotFound;BOOL nativeBackground=YES,auxiliaryBackground=YES;
        for (NSUInteger index=0;index<self.operations.count;index++) {
            BOOL background=RVDAJobBackground(self.operations[index].job);
            if (background && self.activePrefetchTransfers>=RVDAMaxPrefetchTransfers) continue;
            if (nativeIndex==NSNotFound || !background || (nativeBackground && self.operations[index].image && !self.operations[nativeIndex].image)) { nativeIndex=index;nativeBackground=background;if (!background) break; }
        }
        for (NSUInteger index=0;index<self.auxiliaryOperations.count;index++) {
            BOOL (^query)(void)=self.auxiliaryOperations[index][@"background"];BOOL background=query();
            if (background && self.activePrefetchTransfers>=RVDAMaxPrefetchTransfers) continue;
            if (auxiliaryIndex==NSNotFound || !background) { auxiliaryIndex=index;auxiliaryBackground=background;if (!background) break; }
        }
        if (nativeIndex==NSNotFound && auxiliaryIndex==NSNotFound) break;
        BOOL useAuxiliary=auxiliaryIndex!=NSNotFound && (nativeIndex==NSNotFound || (nativeBackground && !auxiliaryBackground) || (nativeBackground==auxiliaryBackground && self.auxiliaryTurn));
        if (useAuxiliary) {
            NSDictionary *operation=self.auxiliaryOperations[auxiliaryIndex];[self.auxiliaryOperations removeObjectAtIndex:auxiliaryIndex];
            RVDAHTTP *transfer=operation[@"transfer"];BOOL completed;@synchronized(transfer) { completed=transfer.completed; }
            if (completed) continue;
            if (RVDANow()-[operation[@"queued_at"] doubleValue]>60) { [transfer finish:RVDeArrowBlockerBackoff errorCode:NSURLErrorTimedOut];continue; }
            self.auxiliaryTurn=NO;self.activeTransfers++;if (auxiliaryBackground) self.activePrefetchTransfers++;
            void (^admit)(BOOL)=operation[@"admit"];admit(auxiliaryBackground);
            [transfer start:operation[@"url"] timeout:[operation[@"timeout"] doubleValue]];continue;
        }
        self.auxiliaryTurn=YES;
        RVDAOperation *operation=self.operations[nativeIndex];[self.operations removeObjectAtIndex:nativeIndex];RVDAJob *job=operation.job;
        if (self.jobs[job.videoID]!=job || job.revision!=self.configurationRevision || !job.consumers.count) continue;
        if ([self.cooldowns[operation.URL.host.lowercaseString] doubleValue]>RVDANow()) { [self blockedOperation:operation];continue; }
        RVDAHTTP *transfer=[RVDAHTTP new];transfer.limit=operation.image ? RVDAImageLimit : RVDAMetadataLimit;
        job.transfer=transfer;self.activeTransfers++;if (nativeBackground) self.activePrefetchTransfers++;__weak RVDeArrowBackend *weakSelf=self;
        transfer.completion=^(NSData *data,NSHTTPURLResponse *response,RVDeArrowBlocker blocker,NSInteger code) {
            RVDeArrowBackend *backend=weakSelf;if (!backend) return;
            dispatch_async(backend.queue,^{
                if (backend.activeTransfers) backend.activeTransfers--;
                if (nativeBackground && backend.activePrefetchTransfers) backend.activePrefetchTransfers--;
                if (backend.jobs[job.videoID]==job && job.revision==backend.configurationRevision) {
                    job.transfer=nil;[backend cooldown:operation response:response blocker:blocker];
                    [backend record:operation.image ? RVDeArrowCheckpointImageReceived : RVDeArrowCheckpointMetadataReceived blocker:blocker video:job.videoID details:@{@"network_error_code":@(code),@"transport_finished":@YES}];
                    if (operation.image) [backend imageResponse:data response:response blocker:blocker operation:operation];
                    else [backend metadataResponse:data response:response blocker:blocker operation:operation];
                }
                [backend pump];
            });
        };
        [transfer start:operation.URL timeout:job.options.requestTimeout];
    }
    self.pumping=NO;
}
@end
