// SPDX-License-Identifier: MIT
// Bounded asynchronous thumbnail probes. Original URLs remain active until a probe succeeds.
@interface RVImageProbe : NSObject <NSURLSessionDataDelegate>
@property(nonatomic,strong) NSURLSession *session;
@property(nonatomic,strong) NSMutableData *prefix;
@property(nonatomic,copy) void (^result)(BOOL);
@property(nonatomic) BOOL completed;
- (void)finish:(BOOL)available;
@end
@implementation RVImageProbe
- (void)finish:(BOOL)available {
    if (self.completed) return;self.completed=YES;
    if (self.result) self.result(available);
    self.result=nil;[self.session invalidateAndCancel];self.session=nil;
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition))completion {
    NSInteger status=[response isKindOfClass:NSHTTPURLResponse.class] ? [(NSHTTPURLResponse *)response statusCode] : 0;
    BOOL valid=RVImageResponseValid((int)status,response.MIMEType.lowercaseString.UTF8String,response.expectedContentLength);
    completion(valid ? NSURLSessionResponseAllow : NSURLSessionResponseCancel);
    if (!valid) [self finish:NO];
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveData:(NSData *)data {
    if (self.completed) return;
    NSUInteger take=MIN(data.length,32-self.prefix.length);[self.prefix appendBytes:data.bytes length:take];
    const uint8_t *p=self.prefix.bytes;NSUInteger n=self.prefix.length;
    BOOL image=RVImagePrefixValid(p,n);
    if (image) [self finish:YES];else if (n>=32) [self finish:NO];
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error { if (!self.completed) [self finish:NO]; }
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completion {
    NSURL *url=request.URL;BOOL valid=[url.scheme isEqual:@"https"] && url.host.length && !url.user && !url.password;
    completion(valid ? request : nil);if (!valid) [self finish:NO];
}
@end
static NSMutableDictionary *RVThumbnailProbeCache;
static BOOL RVImageCachedAvailable(NSString *url) {
    NSMutableDictionary *cache=RVThumbnailProbeCache;if (!cache) return NO;
    @synchronized(cache) { NSDictionary *entry=cache[url];return [entry[@"available"] boolValue] && ![entry[@"pending"] boolValue] && NSDate.timeIntervalSinceReferenceDate<[entry[@"expires"] doubleValue]; }
}
static BOOL RVImageAvailable(NSString *url) {
    static NSMutableDictionary *cache;static NSUInteger pending;static dispatch_once_t once;
    dispatch_once(&once,^{ cache=[NSMutableDictionary dictionary];RVThumbnailProbeCache=cache; });
    if (!url.length || url.length>4096) return NO;
    NSURL *target=[NSURL URLWithString:url];if (![target.scheme isEqual:@"https"] || !target.host.length || target.user || target.password) return NO;
    @synchronized(cache) {
        NSTimeInterval now=NSDate.timeIntervalSinceReferenceDate;
        NSDictionary *entry=cache[url];
        if (entry && ([entry[@"pending"] boolValue] || now<[entry[@"expires"] doubleValue])) return [entry[@"available"] boolValue];
        if (pending>=8) return NO;
        if (cache.count>=256) {
            for (NSString *key in cache.allKeys) if (![cache[key][@"pending"] boolValue]) [cache removeObjectForKey:key];
        }
        cache[url]=@{@"pending":@YES,@"available":@NO};pending++;
    }
    RVImageProbe *probe=[RVImageProbe new];probe.prefix=[NSMutableData data];
    probe.result=^(BOOL available) {
        @synchronized(cache) { if (pending) pending--;cache[url]=@{@"available":@(available),@"expires":@(NSDate.timeIntervalSinceReferenceDate+(available ? 1800 : 300))}; }
    };
    NSURLSessionConfiguration *configuration=NSURLSessionConfiguration.ephemeralSessionConfiguration;
    configuration.timeoutIntervalForRequest=8;configuration.timeoutIntervalForResource=10;configuration.HTTPMaximumConnectionsPerHost=4;
    configuration.URLCredentialStorage=nil;
    probe.session=[NSURLSession sessionWithConfiguration:configuration delegate:probe delegateQueue:nil];
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:target];[request setValue:@"bytes=0-31" forHTTPHeaderField:@"Range"];
    [[probe.session dataTaskWithRequest:request] resume];return NO;
}
