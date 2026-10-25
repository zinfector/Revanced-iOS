// SPDX-License-Identifier: MIT
// Queue-only replacement of paired Premium and real queue menu endpoints.
// Pairing corroborated by itzzace/ytkace QueueHooks.mm (MIT), commit
// 1afd955cd695c1d07d2722579d6b0abda8dc9ecd. Uses typed GPB extensions and
// copy-on-write instead of parsing message descriptions or changing entitlement.
static atomic_uint RVQueueMenus,RVQueueItems,RVQueueOffers,RVQueueRewrites;
static atomic_uint RVQueueMissing,RVQueueAmbiguous,RVQueueFailures,RVQueueHandlers;
static atomic_uint RVQueueOptionalOffers;
static _Thread_local NSUInteger RVQueueRewriteDepth;
static _Thread_local NSUInteger RVQueueActionDepth;
static NSDictionary *RVQueueMDXOperation(id command);

static BOOL RVQueueClass(id value,NSString *name) {
    Class cls=NSClassFromString(name);return cls && [value isKindOfClass:cls];
}
static BOOL RVQueueSet(id object,NSString *name,id value) {
    if (!RVCan(object,name.UTF8String,@"v@:@")) return NO;
    ((void(*)(id,SEL,id))objc_msgSend)(object,NSSelectorFromString(name),value);return YES;
}
static BOOL RVQueuePresent(id object,NSString *field) {
    NSString *has=[NSString stringWithFormat:@"has%@%@",[[field substringToIndex:1] uppercaseString],[field substringFromIndex:1]];
    return RVCan(object,has.UTF8String,@"B@:") && RVBool(object,has.UTF8String);
}
// Bits: concrete queue-add command, and Premium acquisition command.
static NSUInteger RVQueueCommandFlags(id object,NSUInteger depth,NSUInteger *budget,NSHashTable *seen) {
    if (!object || [seen containsObject:object]) return 0;
    if (!depth || !*budget) return 4; // Incomplete classification cannot be a replacement candidate.
    --*budget;[seen addObject:object];NSUInteger flags=0;
    if (RVQueueClass(object,@"YTIGetWatchNextQueueAddCommand")) {
        id params=RVObject(object,"params");
        if ([params isKindOfClass:NSString.class] && [params length]>0) flags|=1;
    }
    if (RVQueueClass(object,@"YTIYPCOffersEndpoint") || RVQueueClass(object,@"YTIYPCGetOfflineUpsellEndpoint")) flags|=2;
    id browse=RVObject(object,"browseId");
    if ([browse isKindOfClass:NSString.class] && [browse isEqual:@"SPunlimited"]) flags|=2;
    id extensions=RVObject(object,"extensionsCurrentlySet");
    if ([extensions isKindOfClass:NSArray.class] && [extensions count]<=32 && RVCan(object,"getExtension:",@"@@:@")) {
        for (id ext in extensions) {
            id child=((id(*)(id,SEL,id))objc_msgSend)(object,sel_registerName("getExtension:"),ext);
            flags|=RVQueueCommandFlags(child,depth-1,budget,seen);
        }
    }
    if ([extensions isKindOfClass:NSArray.class] && [extensions count]>32) flags|=4;
    for (NSString *field in @[@"activePlaybackCommand",@"noActivePlaybackCommand",@"onTapAction",@"trueCommand",@"falseCommand",@"command",@"navigationEndpoint",@"serviceEndpoint",@"browseEndpoint"]) {
        if (RVQueuePresent(object,field)) flags|=RVQueueCommandFlags(RVObject(object,field.UTF8String),depth-1,budget,seen);
    }
    id commands=RVObject(object,"commandsArray");
    if ([commands isKindOfClass:NSArray.class] && [commands count]<=32)
        for (id child in commands) flags|=RVQueueCommandFlags(child,depth-1,budget,seen);
    if ([commands isKindOfClass:NSArray.class] && [commands count]>32) flags|=4;
    return flags;
}
static NSUInteger RVQueueFlags(id command) {
    NSUInteger budget=128;
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    return RVQueueCommandFlags(command,8,&budget,seen);
}
static id RVQueueTapAction(id message,id action) {
    id params=RVObject(message,"params");
    if (![params isKindOfClass:NSString.class] || ![params length] || !action || RVQueueActionDepth) return action;
    id result=action;RVQueueActionDepth++;
    @try {
        // The native handler refreshes with params before separately dispatching
        // onTapAction. Remove only a complete acquisition-only optional action.
        if (RVQueueFlags(action)==2) { atomic_fetch_add(&RVQueueOptionalOffers,1);result=nil; }
    } @catch (__unused NSException *exception) { result=action; }
    @finally { RVQueueActionDepth--; }
    return result;
}
static id RVQueueItem(id wrapper,NSString **setter) {
    if (!RVQueueClass(wrapper,@"YTIMenuItemSupportedRenderers")) return nil;
    if (RVQueuePresent(wrapper,@"menuConditionalServiceItemRenderer")) {
        *setter=@"setMenuConditionalServiceItemRenderer:";return RVObject(wrapper,"menuConditionalServiceItemRenderer");
    }
    if (RVQueuePresent(wrapper,@"menuServiceItemRenderer")) {
        *setter=@"setMenuServiceItemRenderer:";return RVObject(wrapper,"menuServiceItemRenderer");
    }
    if (RVQueuePresent(wrapper,@"menuConditionalNavigationItemRenderer")) {
        *setter=@"setMenuConditionalNavigationItemRenderer:";return RVObject(wrapper,"menuConditionalNavigationItemRenderer");
    }
    if (RVQueuePresent(wrapper,@"menuNavigationItemRenderer")) {
        *setter=@"setMenuNavigationItemRenderer:";return RVObject(wrapper,"menuNavigationItemRenderer");
    }
    return nil;
}
static NSNumber *RVQueueRole(id item) {
    id icon=RVObject(item,"icon");
    if (!RVCan(icon,"iconType",@"i@:")) return nil;
    int kind=RVInt(icon,"iconType");
    if (kind==251) return @1; // QueuePlayNext in the matching IPA enum
    if (kind==63 || kind==895) return @2; // AddToRemoteQueue / QueuePlayLast
    return nil;
}
static NSArray *RVQueueMenu(NSArray *renderers) {
    if (!RVCompatible || RVQueueRewriteDepth || ![renderers isKindOfClass:NSArray.class] || renderers.count>128) return renderers;
    RVQueueRewriteDepth++;atomic_fetch_add(&RVQueueMenus,1);NSArray *result=renderers;
    @try {
        NSMutableDictionary *endpoints=[NSMutableDictionary dictionary],*identities=[NSMutableDictionary dictionary];
        NSMutableSet *ambiguous=[NSMutableSet set];NSMutableArray *offers=[NSMutableArray array];
        for (NSUInteger index=0;index<renderers.count;index++) {
            NSString *setter=nil;id item=RVQueueItem(renderers[index],&setter);NSNumber *role=RVQueueRole(item);
            if (!role) continue;atomic_fetch_add(&RVQueueItems,1);
            id endpoint=RVObject(item,"serviceEndpoint");
            if (!endpoint) endpoint=RVObject(item,"navigationEndpoint");
            if (!RVQueueClass(endpoint,@"YTICommand")) continue;
            NSUInteger flags=RVQueueFlags(endpoint);
            NSDictionary *mdx=RVQueueMDXOperation(endpoint);
            BOOL matchingMDX=mdx && [mdx[@"play_next"] boolValue]==[role isEqual:@1];
            if (flags==1 || matchingMDX) {
                id bytes=RVObject(endpoint,"data");
                if (![bytes isKindOfClass:NSData.class] || [bytes length]>65536) continue;
                if (identities[role] && ![identities[role] isEqual:bytes]) [ambiguous addObject:role];
                else { endpoints[role]=endpoint;identities[role]=bytes; }
            } else if (flags&2) {
                [offers addObject:@{@"index":@(index),@"role":role,@"item":item,@"setter":setter}];
                atomic_fetch_add(&RVQueueOffers,1);
            }
        }
        NSMutableArray *rewritten=nil;
        for (NSDictionary *offer in offers) {
            NSNumber *role=offer[@"role"];
            if ([ambiguous containsObject:role]) { atomic_fetch_add(&RVQueueAmbiguous,1);continue; }
            id endpoint=endpoints[role];
            if (!endpoint) { atomic_fetch_add(&RVQueueMissing,1);continue; }
            id wrapper=renderers[[offer[@"index"] unsignedIntegerValue]],item=offer[@"item"];
            if (![wrapper conformsToProtocol:@protocol(NSCopying)] || ![item conformsToProtocol:@protocol(NSCopying)]) continue;
            id copy=[item copy],owner=[wrapper copy];
            NSString *endpointSetter=[offer[@"setter"] containsString:@"Navigation"] ? @"setNavigationEndpoint:" : @"setServiceEndpoint:";
            if (!RVQueueSet(copy,endpointSetter,endpoint) || !RVQueueSet(owner,offer[@"setter"],copy)) continue;
            RVQueueSet(copy,@"setSecondaryIcon:",nil);
            if (!rewritten) rewritten=[renderers mutableCopy];
            rewritten[[offer[@"index"] unsignedIntegerValue]]=owner;atomic_fetch_add(&RVQueueRewrites,1);
        }
        if (rewritten) result=rewritten;
    } @catch (__unused NSException *exception) { atomic_fetch_add(&RVQueueFailures,1);result=renderers; }
    @finally { RVQueueRewriteDepth--; }
    return result;
}
#include "RVQueueLocal.inc"
static NSDictionary *RVQueueReport(void) {
    NSMutableDictionary *report=[@{@"revision":@"native-source-queue-5",@"menus_examined":@(atomic_load(&RVQueueMenus)),
        @"queue_items_examined":@(atomic_load(&RVQueueItems)),@"upsell_items_found":@(atomic_load(&RVQueueOffers)),
        @"upsell_items_replaced":@(atomic_load(&RVQueueRewrites)),@"missing_queue_sibling":@(atomic_load(&RVQueueMissing)),
        @"ambiguous_queue_sibling":@(atomic_load(&RVQueueAmbiguous)),@"rewrite_failures":@(atomic_load(&RVQueueFailures)),
        @"native_queue_handlers_reached":@(atomic_load(&RVQueueHandlers)),
        @"optional_upsell_actions_filtered":@(atomic_load(&RVQueueOptionalOffers)),@"device_verified":@NO} mutableCopy];
    [report addEntriesFromDictionary:RVQueueLocalReport()];return report;
}
static void RVInstallQueue(void) {
    RVInstallQueueLocal();
    RVHook(@"YTColdConfig",@"iosEnableQueuePhase4",@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id object) { return YES; };
    });
    RVHook(@"YTMenuController",@"actionsForRenderers:fromView:entry:shouldLogItems:firstResponder:",@"@@:@@@B@",NO,^id(IMP original,SEL sel) {
        return ^id(id controller,id renderers,id view,id entry,BOOL log,id responder) {
            if (!NSThread.isMainThread) return ((id(*)(id,SEL,id,id,id,BOOL,id))original)(controller,sel,renderers,view,entry,log,responder);
            id previous=RVQueueMenuSourceEntry;RVQueueMenuSourceEntry=entry;
            @try { return ((id(*)(id,SEL,id,id,id,BOOL,id))original)(controller,sel,RVQueueMenu(renderers),view,entry,log,responder); }
            @finally { RVQueueMenuSourceEntry=previous; }
        };
    });
    RVHook(@"YTIMenuRenderer",@"itemsArray",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id menu) { return RVQueueMenu(((id(*)(id,SEL))original)(menu,sel)); };
    });
    RVHook(@"YTIGetWatchNextQueueAddCommand",@"onTapAction",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id message) { return RVQueueTapAction(message,((id(*)(id,SEL))original)(message,sel)); };
    });
    for (NSString *name in @[@"YTQueueAddMenuItemCommandHandlerImpl",@"YTGetWatchNextQueueAddCommandHandlerImpl"])
        RVHook(name,@"executeWithCommand:entry:fromView:sender:",@"v@:@@@@",NO,^id(IMP original,SEL sel) {
            return ^(id handler,id command,id entry,id view,id sender) {
                atomic_fetch_add(&RVQueueHandlers,1);
                ((void(*)(id,SEL,id,id,id,id))original)(handler,sel,command,entry,view,sender);
            };
        });
}
