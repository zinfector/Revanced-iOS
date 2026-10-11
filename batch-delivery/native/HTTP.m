#import "HTTP.h"
#include "IOSObjectArray.h"
#include "IOSPrimitiveArray.h"
#include "rv/batch/Network.h"
#include "java/io/IOException.h"
#include "java/net/SocketTimeoutException.h"
#include "java/lang/IllegalStateException.h"
#include "java/util/Map.h"
#include "java/util/Set.h"
#include "java/util/List.h"
#include "java/util/Iterator.h"
#include "java/util/HashMap.h"
#include "java/util/ArrayList.h"
#include <float.h>

@interface RVBHTTPDelegate : NSObject <NSURLSessionTaskDelegate>
@property BOOL redirects;
@end
@implementation RVBHTTPDelegate
- (void)URLSession:(NSURLSession *)s task:(NSURLSessionTask *)t willPerformHTTPRedirection:(NSHTTPURLResponse *)r newRequest:(NSURLRequest *)q completionHandler:(void (^)(NSURLRequest *))done {done(self.redirects?q:nil);}
@end
id RVBHTTP(NSString *op,IOSObjectArray *args,BOOL *handled){
 *handled=YES;if([op isEqual:@"http.disconnect"])return nil;
 if(![op isEqual:@"http.request"]){*handled=NO;return nil;}
 if(NSThread.isMainThread)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"HTTP operation requires a background thread"];
 RVBNetwork_Request *input=IOSObjectArray_Get(args,0);
 NSURL *url=[NSURL URLWithString:input->url_];if(!url)@throw [[JavaIoIOException alloc] initWithNSString:@"Invalid URL"];
 NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:url];request.HTTPMethod=input->method_;
 request.cachePolicy=input->useCaches_?NSURLRequestUseProtocolCachePolicy:NSURLRequestReloadIgnoringLocalCacheData;
 if(input->body_->size_)request.HTTPBody=[NSData dataWithBytes:input->body_->buffer_ length:input->body_->size_];
 id<JavaUtilIterator> iterator=[[input->headers_ keySet] iterator];
 while([iterator hasNext]){NSString *key=[iterator next];id<JavaUtilList> values=[input->headers_ getWithId:key];for(int i=0;i<[values size];i++)[request addValue:[values getWithInt:i] forHTTPHeaderField:key];}
 // Foundation has one inactivity timeout, rather than separate TCP/read timers.
 NSInteger connect=input->connectTimeout_,read=input->readTimeout_;NSTimeInterval inactivity=read>0?read/1000.0:60;
 request.timeoutInterval=inactivity;
 NSURLSessionConfiguration *config=NSURLSessionConfiguration.ephemeralSessionConfiguration;
 config.timeoutIntervalForRequest=inactivity;config.timeoutIntervalForResource=(connect>0||read>0)?MAX(0.001,(connect+read)/1000.0):DBL_MAX;
 RVBHTTPDelegate *delegate=[RVBHTTPDelegate new];delegate.redirects=input->followRedirects_;
 NSURLSession *session=[NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:nil];
 dispatch_semaphore_t gate=dispatch_semaphore_create(0);__block NSData *body;__block NSHTTPURLResponse *response;__block NSError *error;
 NSURLSessionDataTask *task=[session dataTaskWithRequest:request completionHandler:^(NSData *d,NSURLResponse *r,NSError *e){body=d;response=(id)r;error=e;dispatch_semaphore_signal(gate);}];
 [task resume];dispatch_semaphore_wait(gate,DISPATCH_TIME_FOREVER);[session finishTasksAndInvalidate];
 if(error){if(error.code==NSURLErrorTimedOut)@throw [[JavaNetSocketTimeoutException alloc] initWithNSString:error.localizedDescription];@throw [[JavaIoIOException alloc] initWithNSString:error.localizedDescription];}
 if(!response)@throw [[JavaIoIOException alloc] initWithNSString:@"No HTTP response"];
 JavaUtilHashMap *headers=[[JavaUtilHashMap alloc] init];
 for(NSString *key in response.allHeaderFields){JavaUtilArrayList *values=[[JavaUtilArrayList alloc] init];[values addWithId:[response.allHeaderFields[key] description]];[headers putWithId:key withId:values];}
 IOSByteArray *bytes=[IOSByteArray arrayWithBytes:(const int8_t *)body.bytes count:(int32_t)body.length];
 return [[RVBNetwork_Response alloc] initWithInt:(int32_t)response.statusCode withByteArray:bytes withJavaUtilMap:headers];
}
