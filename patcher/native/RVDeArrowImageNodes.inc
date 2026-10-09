// SPDX-License-Identifier: MIT
// Template-independent, presentation-only leases on native network image nodes.
// Never intercept downloader completion blocks, cancellation or native caches.
static const void *RVDAImageNodeLeaseKey=&RVDAImageNodeLeaseKey;
static const void *RVDAImageNodeContextKey=&RVDAImageNodeContextKey;
static _Thread_local BOOL RVDAImageNodeWriting;
@interface RVDAImageNodeContext : NSObject
@property(nonatomic,weak) id context;
@end
@implementation RVDAImageNodeContext @end
@interface RVDAImageNodeLease : NSObject
@property(nonatomic,copy) NSString *video;
@property(nonatomic,strong) UIImage *incomingImage;
@property(nonatomic) BOOL hasIncomingImage,queued,rejectionReported;
@end
@implementation RVDAImageNodeLease @end
static NSString *RVDAImageNodeVideo(id value) {
    if (![value isKindOfClass:NSURL.class]) return nil;
    NSURL *url=value;
    if (![@[@"https",@"http"] containsObject:url.scheme.lowercaseString] ||
        ![@[@"i.ytimg.com",@"i1.ytimg.com",@"i2.ytimg.com",@"i3.ytimg.com",@"i4.ytimg.com"] containsObject:url.host.lowercaseString]) return nil;
    NSArray *path=url.path.pathComponents;
    if (path.count!=4 || ![@[@"vi",@"vi_webp"] containsObject:path[1]] || !RVVideoIDValid(path[2])) return nil;
    static NSRegularExpression *file;static dispatch_once_t once;
    dispatch_once(&once,^{ file=[NSRegularExpression regularExpressionWithPattern:@"^(default|mqdefault|hqdefault|sddefault|maxresdefault|hq720|[0-3])(_live)?\\.(jpg|webp)$" options:0 error:nil]; });
    return [file numberOfMatchesInString:path[3] options:0 range:NSMakeRange(0,[path[3] length])]==1 ? path[2] : nil;
}
static id RVDAImageNodeParent(id context) {
    Class proxy=NSClassFromString(@"ELMCollectionCellContextProxy");
    for (NSUInteger depth=0;proxy && [context isKindOfClass:proxy] && depth<2;depth++) {
        if (!RVCan(context,"forwardingTargetForSelector:",@"@@::")) return nil;
        context=((id(*)(id,SEL,SEL))objc_msgSend)(context,sel_registerName("forwardingTargetForSelector:"),sel_registerName("parentResponder"));
    }
    return RVObject(context,"parentResponder");
}
static id RVDAImageNodeOwner(id context) {
    // Admit only an explicitly non-ad native renderer in a recognized surface.
    // This reads existing owners; it never walks descendant nodes or loads views.
    id owner=RVDAImageNodeParent(context);
    for (NSUInteger depth=0;owner && depth<16;depth++,owner=RVObject(owner,"parentResponder")) {
        id entry=RVObject(owner,"entry"),renderer=RVAdRootElement(entry);
        id options=RVAdResolvedObject(renderer,"compatibilityOptions");
        if (RVAdResolve(options,"hasAdLoggingData",@"B@:")) {
            if (RVBool(options,"hasAdLoggingData")) return nil;
            return RVDAUIOwnerSurface(owner,NO,NULL) ? owner : nil;
        }
    }return nil;
}
static NSString *RVDAImageNodeBlocker(RVDAUIBinding *state) {
    id node=state.controller;
    if (objc_getAssociatedObject(node,RVDAImageNodeLeaseKey)!=state.imageNodeLease) return @"image_url_generation_changed";
    if (!state.imageContext || !state.imageOwner) return @"image_owner_released";
    if (RVDAImageNodeOwner(state.imageContext)!=state.imageOwner) return @"image_owner_changed_or_ad";
    if (![RVDAUIOwnerSurface(state.imageOwner,NO,NULL) isEqual:state.surface]) return @"image_surface_changed";
    if (![RVDAImageNodeVideo(RVObject(node,"URL")) isEqual:state.video]) return @"image_url_identity_changed";
    return nil;
}
static void RVDAImageNodeWrite(id node,UIImage *image) {
    if (!RVCan(node,"_setImage:",@"v@:@")) return;
    BOOL prior=RVDAImageNodeWriting;RVDAImageNodeWriting=YES;
    @try { ((void(*)(id,SEL,id))objc_msgSend)(node,sel_registerName("_setImage:"),image); }
    @finally { RVDAImageNodeWriting=prior; }
}
static void RVDAImageNodeRestore(RVDAUIBinding *state) {
    if (!state.imageOnly || !state.imageNodeApplied || RVDAUICurrentBlocker(state)) return;
    // A native cache/download delivery may already have replaced our image.
    // Preserve it even when its coalesced notification has not drained yet.
    if (RVObject(state.controller,"image")!=state.imageNodeApplied) { state.imageNodeApplied=nil;return; }
    // Do not restore onto a new URL or ownership generation.
    RVDAImageNodeWrite(state.controller,state.imageNodeOriginal);state.imageNodeApplied=nil;
    RVDADiagCount(@"image_node.original_restored");
}
static void RVDAImageNodePresent(RVDAUIBinding *state) {
    if (!state.imageOnly || !RVDAUICurrent(state)) return;
    UIImage *replacement=RVDAUIImage(state);
    if (!replacement) { RVDAImageNodeRestore(state);return; }
    if (state.imageNodeApplied==replacement && RVObject(state.controller,"image")==replacement) return;
    RVDAImageNodeWrite(state.controller,replacement);state.imageNodeApplied=replacement;
    state.nativeImageProjected=RVObject(state.controller,"image")==replacement;
    RVDADiagRecord(@"I03_image_node_presented",state,state.nativeImageProjected ? nil : @"native_image_not_confirmed",
        @{@"visible":@(RVDAUIConsumerVisible(state.controller)),@"confirmed":@(state.nativeImageProjected)});
}
static void RVDAImageNodeQueue(id node,RVDAImageNodeLease *lease) {
    if (!node || !lease) return;
    @synchronized(lease) { if (lease.queued) return;lease.queued=YES; }
    __weak id weak=node;
    dispatch_async(dispatch_get_main_queue(),^{
        id target=weak;UIImage *incoming=nil;BOOL hasIncoming;
        @synchronized(lease) { lease.queued=NO;incoming=lease.incomingImage;hasIncoming=lease.hasIncomingImage;lease.incomingImage=nil;lease.hasIncomingImage=NO; }
        if (!target || objc_getAssociatedObject(target,RVDAImageNodeLeaseKey)!=lease) return;
        RVDAImageNodeContext *capture=objc_getAssociatedObject(target,RVDAImageNodeContextKey);
        id context=capture.context,owner=RVDAImageNodeOwner(context);
        RVDAUIBinding *state=objc_getAssociatedObject(target,RVDAUIBindingKey);
        if (!owner || ![RVDAImageNodeVideo(RVObject(target,"URL")) isEqual:lease.video]) {
            if (!lease.rejectionReported) {
                lease.rejectionReported=YES;
                RVDAUIBinding *sample=[RVDAUIBinding new];sample.video=lease.video;sample.route=@"native_video_thumbnail_url";
                NSMutableArray *parents=[NSMutableArray array];id parent=RVDAImageNodeParent(context);
                for (NSUInteger depth=0;parent && depth<8;depth++,parent=RVObject(parent,"parentResponder")) [parents addObject:NSStringFromClass([parent class])];
                RVDADiagRecord(@"I00_image_node_rejected",sample,!context ? @"image_context_missing" : !owner ? @"non_ad_owner_unproven" : @"url_identity_changed",
                    @{@"context_class":context ? NSStringFromClass([context class]) : @"none",@"parent_classes":parents});
            }
            return;
        }
        if (!state || !state.imageOnly || state.imageNodeLease!=lease || state.imageOwner!=owner) {
            [state.request cancel];[state.fallbackTransfer cancel];
            state=[RVDAUIBinding new];state.imageOnly=YES;state.modern=YES;state.controller=target;
            state.imageContext=context;state.imageOwner=owner;state.imageNodeLease=lease;
            state.video=lease.video;state.surface=RVDAUIOwnerSurface(owner,NO,NULL);state.route=@"native_video_thumbnail_url";
            state.originalImageURL=[RVObject(target,"URL") absoluteString];state.index=-1;
            state.imageNodeOriginal=hasIncoming ? incoming : RVObject(target,"image");
            objc_setAssociatedObject(target,RVDAUIBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            [RVDAUIBindings addObject:state];
            RVDADiagRecord(@"I01_image_node_bound",state,nil,@{@"owner_class":NSStringFromClass([owner class]),@"template_independent":@YES});
            RVDAUIRequest(state);
        } else if (hasIncoming && incoming!=state.imageNodeApplied) {
            state.imageNodeOriginal=incoming;
            RVDADiagCount(@"image_node.native_delivery");
        }
        RVDAImageNodePresent(state);RVDAUIFallbackWake(state);RVDAUITextObserveImage(state);
    });
}
static void RVDAImageNodeAssigned(id node,id url) {
    NSString *video=RVDAImageNodeVideo(url);
    RVDAImageNodeLease *lease=nil;
    if (video) { lease=[RVDAImageNodeLease new];lease.video=video; }
    // Atomic association publishes identity before native setters may deliver images.
    objc_setAssociatedObject(node,RVDAImageNodeLeaseKey,lease,OBJC_ASSOCIATION_RETAIN);
    if (!lease) {
        __weak id weak=node;dispatch_async(dispatch_get_main_queue(),^{
            id target=weak;if (!target || objc_getAssociatedObject(target,RVDAImageNodeLeaseKey)) return;
            RVDAUIBinding *state=objc_getAssociatedObject(target,RVDAUIBindingKey);
            if (state.imageOnly) { [state.request cancel];[state.fallbackTransfer cancel];
                objc_setAssociatedObject(target,RVDAUIBindingKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
        });
    }
}
static void RVDAImageNodesInstall(void) {
    RVDAUIHook(@"ELMImageNode",@"initWithElement:context:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id node,id element,id context) {
            id result=((id(*)(id,SEL,id,id))original)(node,sel,element,context);
            if (result) { RVDAImageNodeContext *capture=[RVDAImageNodeContext new];capture.context=context;
                objc_setAssociatedObject(result,RVDAImageNodeContextKey,capture,OBJC_ASSOCIATION_RETAIN);
                RVDAImageNodeQueue(result,objc_getAssociatedObject(result,RVDAImageNodeLeaseKey)); }
            return result;
        };
    });
    RVDAUIHook(@"ASNetworkImageNode",@"setURL:resetToDefault:",@"v@:@B",NO,^id(IMP original,SEL sel) {
        return ^(id node,id url,BOOL reset) {
            if ([node isKindOfClass:NSClassFromString(@"ELMImageNode")]) RVDAImageNodeAssigned(node,url);
            ((void(*)(id,SEL,id,BOOL))original)(node,sel,url,reset);
            RVDAImageNodeQueue(node,objc_getAssociatedObject(node,RVDAImageNodeLeaseKey));
        };
    });
    RVDAUIHook(@"ASNetworkImageNode",@"setURLs:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id node,id urls) {
            if ([node isKindOfClass:NSClassFromString(@"ELMImageNode")])
                RVDAImageNodeAssigned(node,[urls isKindOfClass:NSArray.class] ? [urls firstObject] : nil);
            ((void(*)(id,SEL,id))original)(node,sel,urls);
            RVDAImageNodeQueue(node,objc_getAssociatedObject(node,RVDAImageNodeLeaseKey));
        };
    });
    RVDAUIHook(@"ASNetworkImageNode",@"_locked__setImage:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id node,id image) {
            RVDAImageNodeLease *lease=objc_getAssociatedObject(node,RVDAImageNodeLeaseKey);
            ((void(*)(id,SEL,id))original)(node,sel,image);
            if (RVDAImageNodeWriting || !lease) return;
            @synchronized(lease) { lease.incomingImage=image;lease.hasIncomingImage=YES; }
            RVDAImageNodeQueue(node,lease);
        };
    });
    RVDAUIHook(@"ASNetworkImageNode",@"didEnterPreloadState",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id node) { ((void(*)(id,SEL))original)(node,sel);
            RVDAImageNodeQueue(node,objc_getAssociatedObject(node,RVDAImageNodeLeaseKey)); };
    });
}
