// The on-device resolver consumes only the exact, host-verified target profile.
#include "RVABI.h"
#include "RVAdaptiveCatalog.h"
#include <CommonCrypto/CommonDigest.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
__attribute__((used)) static const char RVAdaptivePayloadTag[]=RV_ADAPTIVE_PAYLOAD_TAG;
static NSDictionary *RVAdaptiveProfile;
static NSMutableDictionary<NSString *,NSNumber *> *RVAdaptiveAllowed;
static NSMutableArray<NSDictionary *> *RVAdaptiveLedger;
static NSMutableDictionary *RVAdaptiveDiagnostics;
static NSMutableSet<NSString *> *RVAdaptiveBlockedFeatures;
static NSString *RVSignature(Method method) {
    char normalized[4097];
    return method && RVABINormalize(method_getTypeEncoding(method),normalized,sizeof(normalized)) ? @(normalized) : nil;
}
static NSString *RVAdaptiveID(NSString *cls,NSString *selector,NSString *abi,BOOL classMethod) {
    return [NSString stringWithFormat:@"%@|%@|%@|%@",cls,classMethod ? @"+" : @"-",selector,abi];
}
static BOOL RVAdaptivePermit(NSString *cls,NSString *selector,NSString *abi,BOOL classMethod) {
    return RVCompatible && [RVAdaptiveAllowed[RVAdaptiveID(cls,selector,abi,classMethod)] boolValue];
}
static BOOL RVAdaptiveFeature(NSString *key) {
    return ![RVAdaptiveProfile[@"disabled_features"] containsObject:key] && ![RVAdaptiveBlockedFeatures containsObject:key];
}
static NSString *RVAdaptiveSHA(NSData *data) {
    if (data.length>UINT32_MAX) return nil;
    unsigned char bytes[CC_SHA256_DIGEST_LENGTH];CC_SHA256(data.bytes,(CC_LONG)data.length,bytes);
    NSMutableString *value=[NSMutableString string];
    for (unsigned i=0;i<sizeof(bytes);i++) [value appendFormat:@"%02x",bytes[i]];
    return value;
}
static BOOL RVAdaptiveIdentity(void) {
    NSString *root=NSBundle.mainBundle.bundlePath;
    NSData *data=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"RVPortProfile.json"]];
    NSData *receiptData=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"RVPortPatchManifest.json"]];
    if (!data || data.length>8*1024*1024 || !receiptData) return NO;
    id p=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    id receipt=[NSJSONSerialization JSONObjectWithData:receiptData options:0 error:nil];
    if (![p isKindOfClass:NSDictionary.class] || ![receipt isKindOfClass:NSDictionary.class]) return NO;
    if (![RVAdaptiveSHA(data) isEqual:receipt[@"adaptive_profile_sha256"]] ||
        [p[@"schema"] intValue]!=1 || ![p[@"catalog_sha256"] isEqual:RV_ADAPTIVE_CATALOG_SHA256] ||
        ![p[@"executable_sha256"] isEqual:receipt[@"original_executable_sha256"]] ||
        ![p[@"id"] isEqual:receipt[@"profile"]]) return NO;
    NSDictionary *info=NSBundle.mainBundle.infoDictionary;
    if (![p[@"version"] isEqual:info[@"CFBundleShortVersionString"]] || ![p[@"executable"] isEqual:info[@"CFBundleExecutable"]]) return NO;
    const struct mach_header_64 *header=(const struct mach_header_64 *)_dyld_get_image_header(0);
    if (!header || header->magic!=MH_MAGIC_64) return NO;
    const uint8_t *cursor=(const uint8_t *)(header+1),*end=cursor+header->sizeofcmds;
    BOOL found=NO;
    for (uint32_t i=0;i<header->ncmds && cursor+sizeof(struct load_command)<=end;i++) {
        const struct load_command *command=(const struct load_command *)cursor;
        if (command->cmdsize<8 || cursor+command->cmdsize>end) return NO;
        if (command->cmd==LC_UUID && command->cmdsize>=sizeof(struct uuid_command)) {
            NSMutableString *uuid=[NSMutableString string];const uint8_t *bytes=((const struct uuid_command *)command)->uuid;
            for (unsigned j=0;j<16;j++) [uuid appendFormat:@"%02x",bytes[j]];
            found=[uuid isEqual:p[@"uuid"]];
        }
        cursor+=command->cmdsize;
    }
    if (!found || cursor!=end) return NO;
    RVAdaptiveProfile=p;return YES;
}
static Method RVAdaptiveMethod(NSString *name,NSString *selectorName,BOOL classMethod) {
    Class cls=NSClassFromString(name);if (!cls) return NULL;
    SEL descriptor=NSSelectorFromString(@"descriptor");
    Method dm=class_getClassMethod(cls,descriptor);
    if ([RVSignature(dm) isEqual:@"@@:"]) ((id(*)(id,SEL))objc_msgSend)(cls,descriptor);
    SEL selector=NSSelectorFromString(selectorName);Class target=classMethod ? object_getClass(cls) : cls;
    Method method=class_getInstanceMethod(target,selector);
    if (!method && !classMethod && [cls respondsToSelector:@selector(resolveInstanceMethod:)]) {
        [cls resolveInstanceMethod:selector];method=class_getInstanceMethod(target,selector);
    }
    return method;
}
static BOOL RVAdaptiveField(NSString *name,NSDictionary *expected) {
    Class cls=NSClassFromString(name);SEL descriptor=NSSelectorFromString(@"descriptor");
    if (![RVSignature(class_getClassMethod(cls,descriptor)) isEqual:@"@@:"]) return NO;
    id d=((id(*)(id,SEL))objc_msgSend)(cls,descriptor);SEL byName=NSSelectorFromString(@"fieldWithName:");
    if (![RVSignature(class_getInstanceMethod(object_getClass(d),byName)) isEqual:@"@@:@"]) return NO;
    id field=((id(*)(id,SEL,id))objc_msgSend)(d,byName,expected[@"name"]);
    SEL number=NSSelectorFromString(@"number"),type=NSSelectorFromString(@"dataType");
    if (![RVSignature(class_getInstanceMethod(object_getClass(field),number)) isEqual:@"I@:"] ||
        ![RVSignature(class_getInstanceMethod(object_getClass(field),type)) isEqual:@"C@:"]) return NO;
    return ((uint32_t(*)(id,SEL))objc_msgSend)(field,number)==[expected[@"number"] unsignedIntValue] &&
        ((uint8_t(*)(id,SEL))objc_msgSend)(field,type)==[expected[@"data_type"] unsignedCharValue];
}
static BOOL RVAdaptivePreflight(void) {
    RVAdaptiveAllowed=[NSMutableDictionary dictionary];RVAdaptiveLedger=[NSMutableArray array];
    RVAdaptiveDiagnostics=[NSMutableDictionary dictionary];
    RVAdaptiveBlockedFeatures=[NSMutableSet set];
    NSArray *contracts=RVAdaptiveProfile[@"contracts"];
    size_t expectedCount=sizeof(RVAdaptiveContractIDs)/sizeof(RVAdaptiveContractIDs[0]);
    if (![contracts isKindOfClass:NSArray.class] || contracts.count!=expectedCount) return NO;
    NSSet *accepted=[NSSet setWithArray:@[@"static_match",@"runtime_inherited",@"runtime_framework",@"runtime_descriptor"]];
    for (NSUInteger i=0;i<contracts.count;i++) {
        NSDictionary *r=contracts[i];NSString *key=r[@"id"],*name=r[@"class_name"],*selector=r[@"selector"],*abi=r[@"abi"];
        BOOL classMethod=[r[@"kind"] isEqual:@"+"];
        if (![key isEqual:RVAdaptiveContractIDs[i]] || ![key isEqual:RVAdaptiveID(name,selector,abi,classMethod)]) return NO;
        NSString *status=r[@"resolution"][@"status"];BOOL valid=NO;
        if ([accepted containsObject:status]) {
            @try {
                valid=[RVSignature(RVAdaptiveMethod(name,selector,classMethod)) isEqual:abi];
                if (valid && [status isEqual:@"runtime_descriptor"]) valid=RVAdaptiveField(name,r[@"resolution"][@"field"]);
            } @catch (NSException *exception) { valid=NO; }
        }
        RVAdaptiveAllowed[key]=@(valid);
        RVAdaptiveDiagnostics[key]=valid ? @"ready" : [accepted containsObject:status] ? @"runtime_contract_failed" : status ?: @"invalid";
    }
    NSDictionary *capabilities=RVAdaptiveCapabilities();
    for (NSString *feature in capabilities) for (NSString *key in capabilities[feature]) {
        if (![RVAdaptiveAllowed[key] boolValue]) {
            [RVAdaptiveBlockedFeatures addObject:feature];
            RVLog([NSString stringWithFormat:@"Disabled %@: required contract %@ unavailable",feature,key]);
        }
    }
    if ([RVAdaptiveBlockedFeatures containsObject:@"sponsorblock"]) [RVAdaptiveBlockedFeatures addObjectsFromArray:@[@"sponsorblock_manual",@"sponsorblock_markers",@"sponsorblock_contribute"]];
    RVLog([NSString stringWithFormat:@"Adaptive preflight: %lu/%lu contracts ready",(unsigned long)[RVAdaptiveAllowed.allValues filteredArrayUsingPredicate:[NSPredicate predicateWithFormat:@"boolValue == YES"]].count,(unsigned long)contracts.count]);
    return YES;
}
static void RVAdaptiveRollback(void) {
    for (NSDictionary *entry in RVAdaptiveLedger.reverseObjectEnumerator) {
        Class target=NSClassFromString(entry[@"class"]);if ([entry[@"class_method"] boolValue]) target=object_getClass(target);
        SEL sel=NSSelectorFromString(entry[@"selector"]);
        class_replaceMethod(target,sel,[entry[@"original"] pointerValue],[entry[@"encoding"] UTF8String]);
        // A later block may have captured this IMP; retain it until process exit.
    }
    [RVAdaptiveLedger removeAllObjects];RVCompatible=NO;
    RVLog(@"Adaptive hook transaction rolled back; all features disabled");
}
