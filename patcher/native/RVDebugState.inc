// SPDX-License-Identifier: MIT
// Explicit self-process debug state. Saved USB authentication is owned by RVUSB.
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <mach/mach_time.h>
#include <dlfcn.h>
#include <inttypes.h>
_Static_assert(sizeof(vm_address_t)==8 && sizeof(vm_size_t)==8,"Live debugging requires 64-bit VM addresses");
static NSString *const RVDBGPreference=@"rvport.usb-live-debug.enabled";
@interface RVDBGHandle : NSObject
@property(nonatomic,weak) id object,scope;
@property(nonatomic,strong) id retained;
@property(nonatomic) uint64_t generation,epoch,lastAccess;
@property(nonatomic,copy) NSString *identifier;
@end
@implementation RVDBGHandle @end
@interface RVDBGJob : NSObject
@property(nonatomic,copy) NSString *identifier,*operation,*owner,*state,*nonce;
@property(nonatomic,strong) NSDictionary *arguments,*result;
@property(nonatomic) NSTimeInterval queuedAt,completedAt;
@end
@implementation RVDBGJob @end
@interface RVDBGState : NSObject
@property(atomic,copy) NSString *nonce;
@property(nonatomic,strong) NSMutableDictionary<NSString *,RVDBGHandle *> *handles;
@property(nonatomic,strong) NSMapTable *reverse;
@property(nonatomic,strong) NSMutableDictionary<NSString *,RVDBGJob *> *jobs;
@property(nonatomic,strong) NSMutableArray<NSString *> *jobOrder;
@property(nonatomic,strong) NSMutableDictionary *transactions,*allocations;
@property(nonatomic,strong) NSMutableSet<NSString *> *mutations;
@property(nonatomic,strong) NSMutableArray *events;
@property(nonatomic) uint64_t sequence,dropped,handleSequence;
+ (instancetype)shared;
@end
@implementation RVDBGState
+ (instancetype)shared {
    static RVDBGState *state;static dispatch_once_t once;dispatch_once(&once,^{
        state=[RVDBGState new];state.nonce=NSUUID.UUID.UUIDString;
        state.handles=[NSMutableDictionary dictionary];state.reverse=[NSMapTable weakToStrongObjectsMapTable];
        state.jobs=[NSMutableDictionary dictionary];state.jobOrder=[NSMutableArray array];
        state.transactions=[NSMutableDictionary dictionary];state.allocations=[NSMutableDictionary dictionary];state.mutations=[NSMutableSet set];state.events=[NSMutableArray array];
    });return state;
}
@end
static NSString *RVDBGHex(uint64_t value) { return [NSString stringWithFormat:@"0x%" PRIx64,value]; }
static BOOL RVDBGAddress(id value,uint64_t *address) {
    if (![value isKindOfClass:NSString.class] || [value length]<3 || [value length]>18 || ![value hasPrefix:@"0x"]) return NO;
    NSString *hex=[value substringFromIndex:2];NSCharacterSet *allowed=[NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdefABCDEF"];
    if ([hex rangeOfCharacterFromSet:allowed.invertedSet].location!=NSNotFound) return NO;
    NSScanner *scanner=[NSScanner scannerWithString:hex];unsigned long long parsed=0;
    if (![scanner scanHexLongLong:&parsed] || !scanner.isAtEnd) return NO;*address=parsed;return YES;
}
static BOOL RVDBGBound(id value,NSUInteger low,NSUInteger high,NSUInteger *result) {
    if (![value isKindOfClass:NSNumber.class] || CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID()) return NO;
    double number=[value doubleValue];if (!isfinite(number) || number<low || number>high || floor(number)!=number) return NO;
    *result=(NSUInteger)number;return YES;
}
static NSDictionary *RVDBGFail(NSString *reason) { return @{@"ok":@NO,@"error":reason}; }
static BOOL RVDBGEnabled(void) { return atomic_load(&RVDBGActive); }
static void RVDBGSetEnabled(BOOL enabled) {
    if (!NSThread.isMainThread) return;
    [NSUserDefaults.standardUserDefaults setBool:enabled forKey:RVDBGPreference];atomic_store(&RVDBGActive,enabled);
    RVDBGState *state=RVDBGState.shared;@synchronized(state) {
        if (!enabled) {
            for (RVDBGJob *job in state.jobs.allValues) if ([job.state isEqual:@"queued"]) { job.state=@"cancelled";job.result=RVDBGFail(@"debug_mode_disabled"); }
            [state.handles removeAllObjects];[state.reverse removeAllObjects];[state.events removeAllObjects];[state.transactions removeAllObjects];
            for (NSData *allocation in state.allocations.allValues) { uint64_t address=0,size=0;[allocation getBytes:&address range:NSMakeRange(0,8)];[allocation getBytes:&size range:NSMakeRange(8,8)];vm_deallocate(mach_task_self(),address,size); }
            [state.allocations removeAllObjects];
            [state.jobs removeAllObjects];[state.jobOrder removeAllObjects];[state.mutations removeAllObjects];state.nonce=NSUUID.UUID.UUIDString;
        }
    }
}
static NSString *RVDBGHandleFor(id object,RVDAUIBinding *scope) {
    if (!object || !NSThread.isMainThread || !RVDBGEnabled()) return nil;
    RVDBGState *state=RVDBGState.shared;@synchronized(state) {
        RVDBGHandle *existing=[state.reverse objectForKey:object];
        if (existing && existing.scope==scope && existing.generation==(scope ? scope.generation : 0) && existing.epoch==(scope ? scope.consumerEpoch : 0)) { existing.lastAccess=++state.handleSequence;return existing.identifier; }
        for (NSString *key in state.handles.allKeys) { RVDBGHandle *entry=state.handles[key];RVDAUIBinding *binding=entry.scope;
            if (!entry.object || (entry.generation && (!binding || !RVDAUICurrent(binding) || binding.generation!=entry.generation || binding.consumerEpoch!=entry.epoch))) { if (entry.object && [state.reverse objectForKey:entry.object]==entry) [state.reverse removeObjectForKey:entry.object];[state.handles removeObjectForKey:key]; }
        }
        if (state.handles.count>=1024) {
            // Weak inspection references may expire; explicit retain leases may
            // not. Never recycle UUIDs or turn raw addresses into object handles.
            RVDBGHandle *oldest=nil;for (RVDBGHandle *candidate in state.handles.allValues)
                if (!candidate.retained && (!oldest || candidate.lastAccess<oldest.lastAccess)) oldest=candidate;
            if (!oldest) return nil;
            if (oldest.object && [state.reverse objectForKey:oldest.object]==oldest) [state.reverse removeObjectForKey:oldest.object];
            [state.handles removeObjectForKey:oldest.identifier];
        }
        RVDBGHandle *entry=[RVDBGHandle new];entry.object=object;entry.scope=scope;entry.generation=scope ? scope.generation : 0;entry.epoch=scope ? scope.consumerEpoch : 0;
        entry.lastAccess=++state.handleSequence;
        entry.identifier=NSUUID.UUID.UUIDString;state.handles[entry.identifier]=entry;[state.reverse setObject:entry forKey:object];return entry.identifier;
    }
}
static id RVDBGResolve(id identifier,RVDAUIBinding **scope) {
    if (![identifier isKindOfClass:NSString.class]) return nil;RVDBGState *state=RVDBGState.shared;
    @synchronized(state) {
        RVDBGHandle *entry=state.handles[identifier];RVDAUIBinding *binding=entry.scope;
        if (!entry || !entry.object || (entry.generation && (!binding || !RVDAUICurrent(binding) || binding.generation!=entry.generation || binding.consumerEpoch!=entry.epoch))) return nil;
        entry.lastAccess=++state.handleSequence;if (scope) *scope=binding;return entry.retained ?: entry.object;
    }
}
static NSDictionary *RVDBGReference(id object,RVDAUIBinding *scope) {
    if (!object) return @{@"kind":@"nil"};NSString *handle=RVDBGHandleFor(object,scope);
    return @{@"kind":@"object",@"handle":handle ?: @"limit",@"class":NSStringFromClass([object class]),@"address":RVDBGHex((uint64_t)(__bridge void *)object),@"generation":@(scope ? scope.generation : 0)};
}
static void RVDBGRecord(NSString *name,uint64_t generation,NSString *surface,NSDictionary *details) {
    if (!RVDBGEnabled()) return;RVDBGState *state=RVDBGState.shared;@synchronized(state) {
        NSMutableDictionary *event=[@{@"sequence":@(++state.sequence),@"event":name ?: @"native",@"generation":@(generation),@"surface":surface ?: @"unknown",@"uptime":@(NSProcessInfo.processInfo.systemUptime)} mutableCopy];
        for (NSString *key in @[@"nodes",@"owned_nodes",@"title_matches",@"image_matches",@"title_confirmed",@"thumbnail_confirmed",@"title_visible",@"image_visible",@"input_verified",@"callback_count",@"blocker"])
            if ([details[key] isKindOfClass:NSNumber.class] || [details[key] isKindOfClass:NSString.class]) event[key]=details[key];
        if (state.events.count>=256) { [state.events removeObjectAtIndex:0];state.dropped++; }[state.events addObject:event];
    }
}
