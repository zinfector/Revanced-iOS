// Diagnostic only. Does not call patch policies or admit application roles.
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach_time.h>
static NSString *RVGProbeConfiguration(void) { return @"e2cd9ab9056982466213c99943d29e178d8195f3f0d751b2254e4746109bec1e"; }
static NSMutableArray *events;
static NSString *nonce;
static uint64_t eventCounter;
static _Thread_local bool tracing;
static id objectIdentity(id value) { if (!value) return NSNull.null; return @{ @"abi": @"@", @"handle": @((uintptr_t)(__bridge void *)value), @"class": @(class_getName(object_getClass(value))) }; }
static id floatingBits(const void *value, size_t count, NSString *abi) { return @{ @"abi": abi, @"bits_base64": [[NSData dataWithBytes:value length:count] base64EncodedStringWithOptions:0] }; }
static uint64_t trace(const char *method, id receiver, SEL selector, NSString *phase, NSArray *arguments, id result, NSString *exception, uint64_t invocation) {
 if (tracing) return 0;
 tracing = true;
 uint64_t event = 0;
 @try {
  @synchronized(NSProcessInfo.processInfo) {
   if (!events) { events = [NSMutableArray array]; nonce = NSUUID.UUID.UUIDString; }
   if (events.count == 256) [events removeObjectAtIndex:0];
   event = ++eventCounter;
   [events addObject:@{ @"process": nonce, @"event": @(event), @"invocation": @(invocation ?: event), @"monotonic_ticks": @(mach_absolute_time()), @"method": @(method), @"selector": @(sel_getName(selector)), @"receiver": objectIdentity(receiver), @"phase": phase, @"arguments": arguments ?: @[], @"result": result ?: NSNull.null, @"exception": exception ?: NSNull.null, @"main": @(NSThread.isMainThread), @"configuration": RVGProbeConfiguration() }];
  }
 } @catch (NSException *error) { } @finally { tracing = false; }
 return event;
}
NSArray *RVGObservationSnapshot(void) { @synchronized(NSProcessInfo.processInfo) { return [events copy] ?: @[]; } }
static bool install(const char *name, const char *selectorName, bool classMethod, const char *encoding, IMP replacement, IMP *original, uint64_t rva) {
 Class cls = objc_getClass(name); if (!cls) return false;
 if (classMethod) cls = object_getClass(cls);
 SEL selector = sel_registerName(selectorName); Method method = class_getInstanceMethod(cls, selector);
 if (!method || strcmp(method_getTypeEncoding(method), encoding) != 0) return false;
 IMP previous = method_getImplementation(method);
 if (*original) return previous == replacement;
 if ((uintptr_t)previous != (uintptr_t)_dyld_get_image_header(0) + rva) return false;
 *original = previous;
 if (!class_addMethod(cls, selector, replacement, encoding)) class_replaceMethod(cls, selector, replacement, encoding);
 return true;
}

static bool RVGObservationImageMatches(void) {
 const struct mach_header_64 *header = (const struct mach_header_64 *)_dyld_get_image_header(0);
 if (!header || header->magic != MH_MAGIC_64) return false;
 const uint8_t expected[16] = { 0x1f, 0x61, 0x66, 0x5a, 0x5d, 0x4a, 0x35, 0x66, 0x8b, 0x77, 0xda, 0x82, 0x1e, 0x1d, 0x6c, 0x17 };
 const uint8_t *cursor = (const uint8_t *)(header + 1), *end = cursor + header->sizeofcmds;
 for (uint32_t i = 0; i < header->ncmds; ++i) {
  if (cursor + sizeof(struct load_command) > end) return false;
  const struct load_command *command = (const struct load_command *)cursor;
  if (command->cmdsize < sizeof(*command) || cursor + command->cmdsize > end) return false;
  if (command->cmd == LC_UUID && command->cmdsize >= sizeof(struct uuid_command)) return memcmp(((const struct uuid_command *)command)->uuid, expected, 16) == 0;
  cursor += command->cmdsize;
 }
 return false;
}

typedef bool (*Original_0)(id self, SEL cmd, id a0);
static Original_0 original_0;
static bool probe_0(id self, SEL cmd, id a0) {
  NSArray *arguments = nil;
  @try { arguments = @[objectIdentity(a0)]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:67206cc73b0240043f2f13f8", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    bool result = original_0(self, cmd, a0);
    @try { trace("native-method:67206cc73b0240043f2f13f8", self, cmd, @"normal_after", arguments, @{ @"abi": @"B", @"value": @(result) }, nil, invocation); } @catch(NSException *captureError) { }
    return result;
  } @catch(NSException *error) {
    trace("native-method:67206cc73b0240043f2f13f8", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}

typedef void (*Original_1)(id self, SEL cmd, bool a0, id a1);
static Original_1 original_1;
static void probe_1(id self, SEL cmd, bool a0, id a1) {
  NSArray *arguments = nil;
  @try { arguments = @[@{ @"abi": @"B", @"value": @(a0) }, objectIdentity(a1)]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:fd972edfa499aee28a12dae9", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    original_1(self, cmd, a0, a1);
    @try { trace("native-method:fd972edfa499aee28a12dae9", self, cmd, @"normal_after", arguments, NSNull.null, nil, invocation); } @catch(NSException *captureError) { }
    return;
  } @catch(NSException *error) {
    trace("native-method:fd972edfa499aee28a12dae9", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}

typedef void (*Original_2)(id self, SEL cmd, bool a0);
static Original_2 original_2;
static void probe_2(id self, SEL cmd, bool a0) {
  NSArray *arguments = nil;
  @try { arguments = @[@{ @"abi": @"B", @"value": @(a0) }]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:8a1197ab4ed4d59f582b5f37", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    original_2(self, cmd, a0);
    @try { trace("native-method:8a1197ab4ed4d59f582b5f37", self, cmd, @"normal_after", arguments, NSNull.null, nil, invocation); } @catch(NSException *captureError) { }
    return;
  } @catch(NSException *error) {
    trace("native-method:8a1197ab4ed4d59f582b5f37", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}

typedef void (*Original_3)(id self, SEL cmd, double a0);
static Original_3 original_3;
static void probe_3(id self, SEL cmd, double a0) {
  NSArray *arguments = nil;
  @try { arguments = @[floatingBits(&a0, sizeof(a0), @"d")]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:6274ba0d6015d0d2ecac1a5d", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    original_3(self, cmd, a0);
    @try { trace("native-method:6274ba0d6015d0d2ecac1a5d", self, cmd, @"normal_after", arguments, NSNull.null, nil, invocation); } @catch(NSException *captureError) { }
    return;
  } @catch(NSException *error) {
    trace("native-method:6274ba0d6015d0d2ecac1a5d", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}

typedef void (*Original_4)(id self, SEL cmd, bool a0);
static Original_4 original_4;
static void probe_4(id self, SEL cmd, bool a0) {
  NSArray *arguments = nil;
  @try { arguments = @[@{ @"abi": @"B", @"value": @(a0) }]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:4f04ddb70b00ddc7512b4604", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    original_4(self, cmd, a0);
    @try { trace("native-method:4f04ddb70b00ddc7512b4604", self, cmd, @"normal_after", arguments, NSNull.null, nil, invocation); } @catch(NSException *captureError) { }
    return;
  } @catch(NSException *error) {
    trace("native-method:4f04ddb70b00ddc7512b4604", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}

typedef void (*Original_5)(id self, SEL cmd, bool a0);
static Original_5 original_5;
static void probe_5(id self, SEL cmd, bool a0) {
  NSArray *arguments = nil;
  @try { arguments = @[@{ @"abi": @"B", @"value": @(a0) }]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:b1073cc8c6bd3b5441097dde", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    original_5(self, cmd, a0);
    @try { trace("native-method:b1073cc8c6bd3b5441097dde", self, cmd, @"normal_after", arguments, NSNull.null, nil, invocation); } @catch(NSException *captureError) { }
    return;
  } @catch(NSException *error) {
    trace("native-method:b1073cc8c6bd3b5441097dde", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}

typedef void (*Original_6)(id self, SEL cmd, bool a0);
static Original_6 original_6;
static void probe_6(id self, SEL cmd, bool a0) {
  NSArray *arguments = nil;
  @try { arguments = @[@{ @"abi": @"B", @"value": @(a0) }]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:f8d4835dc1335ebe6d140782", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    original_6(self, cmd, a0);
    @try { trace("native-method:f8d4835dc1335ebe6d140782", self, cmd, @"normal_after", arguments, NSNull.null, nil, invocation); } @catch(NSException *captureError) { }
    return;
  } @catch(NSException *error) {
    trace("native-method:f8d4835dc1335ebe6d140782", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}

typedef void (*Original_7)(id self, SEL cmd, bool a0);
static Original_7 original_7;
static void probe_7(id self, SEL cmd, bool a0) {
  NSArray *arguments = nil;
  @try { arguments = @[@{ @"abi": @"B", @"value": @(a0) }]; } @catch(NSException *captureError) { }
  uint64_t invocation = trace("native-method:100727d12555746613810a2b", self, cmd, @"before", arguments, nil, nil, 0);
  @try {
    original_7(self, cmd, a0);
    @try { trace("native-method:100727d12555746613810a2b", self, cmd, @"normal_after", arguments, NSNull.null, nil, invocation); } @catch(NSException *captureError) { }
    return;
  } @catch(NSException *error) {
    trace("native-method:100727d12555746613810a2b", self, cmd, @"exceptional_after", arguments, nil, error.name, invocation);
    @throw;
  }
}
unsigned RVGInstallObservations(void) { @synchronized(NSProcessInfo.processInfo) { if (!RVGObservationImageMatches()) return 0; unsigned count = 0;
  count += install("YTCRUDBackedUserDefaults", "boolForKey:", false, "B24@0:8@16", (IMP)probe_0, (IMP *)&original_0, 14657136ULL);
  count += install("YTCRUDBackedUserDefaults", "setBool:forKey:", false, "v28@0:8B16@20", (IMP)probe_1, (IMP *)&original_1, 14657192ULL);
  count += install("ASMRefreshControl", "setHidden:", false, "v20@0:8B16", (IMP)probe_2, (IMP *)&original_2, 19780820ULL);
  count += install("YTWatchPlaybackController", "seekToTime:", false, "v24@0:8d16", (IMP)probe_3, (IMP *)&original_3, 20375328ULL);
  count += install("YTInlineMutedPlaybackAudioIconViewController", "setHidden:", false, "v20@0:8B16", (IMP)probe_4, (IMP *)&original_4, 22390960ULL);
  count += install("YTGLAnaglyph3DVideoNode", "setHidden:", false, "v20@0:8B16", (IMP)probe_5, (IMP *)&original_5, 24970564ULL);
  count += install("YTGLBaseNode", "setHidden:", false, "v20@0:8B16", (IMP)probe_6, (IMP *)&original_6, 24977336ULL);
  count += install("YTGLGroupNode", "setHidden:", false, "v20@0:8B16", (IMP)probe_7, (IMP *)&original_7, 24986504ULL);
  return count; } }
