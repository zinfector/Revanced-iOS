// Native context bindings for the generated, original-source Java kernels.
#include "RVSourceKernels.h"
#include <limits.h>
static BOOL RVSourceFeatureEnabled(NSString *feature) {
    return rvj_native_feature_enabled(feature.UTF8String,RVEnabled(feature));
}
static BOOL RVSourceDataMatches(NSData *data,NSArray<NSString *> *patterns) {
    if (![data isKindOfClass:NSData.class] || ![patterns isKindOfClass:NSArray.class]) return NO;
    static NSCache<NSData *,NSData *> *cache;static dispatch_once_t once;
    dispatch_once(&once,^{ cache=[NSCache new];cache.countLimit=128;cache.totalCostLimit=4*1024*1024; });
    for (id value in patterns) {
        if (![value isKindOfClass:NSString.class]) continue;
        NSData *needle=[value dataUsingEncoding:NSUTF8StringEncoding];if (!needle) continue;
        // Retain Foundation behavior outside the original Java algorithm's contract.
        if (!needle.length || needle.length>1024*1024 || data.length>INT32_MAX) {
            if ([data rangeOfData:needle options:0 range:NSMakeRange(0,data.length)].location!=NSNotFound) return YES;
            continue;
        }
        RVJContext ctx={0};RVJBytes pattern={(const int8_t *)needle.bytes,(int32_t)needle.length};
        NSData *failureData=[cache objectForKey:needle];
        if (!failureData) {
            RVJInts failure=rvj_kmp_failure(&ctx,pattern);
            if (!ctx.error && failure.data) failureData=[NSData dataWithBytes:failure.data length:(NSUInteger)failure.length*sizeof(int32_t)];
            rvj_ints_free(failure);
            if (failureData) [cache setObject:failureData forKey:needle cost:failureData.length+needle.length];
        }
        if (!failureData || ctx.error) {
            if ([data rangeOfData:needle options:0 range:NSMakeRange(0,data.length)].location!=NSNotFound) return YES;
            continue;
        }
        RVJBytes bytes={(const int8_t *)data.bytes,(int32_t)data.length};
        RVJInts failure={(int32_t *)(void *)failureData.bytes,(int32_t)needle.length};
        int32_t index=rvj_kmp_index(&ctx,bytes,pattern,failure);
        if (ctx.error) {
            if ([data rangeOfData:needle options:0 range:NSMakeRange(0,data.length)].location!=NSNotFound) return YES;
        } else if (index>=0) return YES;
    }
    return NO;
}
