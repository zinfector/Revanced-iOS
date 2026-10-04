// SPDX-License-Identifier: MIT
// Element-based watch controls use ASDisplayNode/Yoga, not the slim UIKit row.
// Keep the native icons, commands, hit targets and backgrounds. Add one owned
// text node only to a video-bound, semantically identified native action pair.
static const void *RVElementDislikeKey=&RVElementDislikeKey;
static const void *RVElementRootTraceKey=&RVElementRootTraceKey;
static const void *RVElementTextTraceKey=&RVElementTextTraceKey;
static NSMutableDictionary *RVElementDislikeCounts;
static NSHashTable *RVElementDislikeParents;
static NSMutableArray<NSDictionary *> *RVElementVoteImages;
static _Thread_local int RVElementResolvedIcon;
static BOOL RVElementRefreshQueued;
static const void *RVElementImageKindKey=&RVElementImageKindKey;
static NSArray *RVElementBindingProbe;
static NSString *RVElementLastGate=@"not_observed";

@interface RVElementDislikeState : NSObject
@property(nonatomic,strong) id textNode;
@property(nonatomic,weak) id element;
@property(nonatomic,weak) id component;
@property(nonatomic,copy) NSString *video;
@property(nonatomic,copy) NSString *text;
@property(nonatomic,copy) NSString *colorSource;
@property(nonatomic) BOOL updating;
@property(nonatomic,weak) id layoutRoot;
@property(nonatomic,weak) id button;
@property(nonatomic,strong) UILabel *renderLabel;
@property(nonatomic,copy) NSString *mountMode;
@property(nonatomic,copy) NSString *mountGate;
@property(nonatomic) NSUInteger mountAttempts;
@property(nonatomic) CGFloat spacingPoints;
@property(nonatomic) CGFloat endMargin;
@property(nonatomic) CGFloat leadingInset;
@property(nonatomic) CGFloat trailingInset;
@property(nonatomic,copy) NSString *edgeGate;
@property(nonatomic,weak) UIViewController *watch;
@property(nonatomic,copy) NSString *spacingGate;
@property(nonatomic) NSUInteger suspensionCount;
@property(nonatomic) NSUInteger resumeCount;
@property(nonatomic) BOOL suspended;
@property(nonatomic) NSUInteger rootInvalidations;
@property(nonatomic) NSUInteger rootLayoutPasses;
@property(nonatomic) NSUInteger textLoadCallbacks;
@property(nonatomic) NSUInteger textDisplayCallbacks;
@end
@implementation RVElementDislikeState @end

static void RVElementDislikeEvent(NSString *name) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(UIImage.class) {
        if (!RVElementDislikeCounts) RVElementDislikeCounts=[NSMutableDictionary dictionary];
        RVElementDislikeCounts[name]=@([RVElementDislikeCounts[name] unsignedLongLongValue]+1);
    }
}
static int RVElementNamedIconKind(id value) {
    if (![value isKindOfClass:NSString.class] || [value length]>128) return 0;
    NSString *name=value;
    if ([name hasPrefix:@"YticonOutlineThumbDownS"] || [name hasPrefix:@"YticonFillThumbDownS"]) return 1;
    if ([name hasPrefix:@"YticonOutlineThumbUpS"] || [name hasPrefix:@"YticonFillThumbUpS"]) return 2;
    // Names are native assets, not localized accessibility labels. No Shorts assets.
    static NSDictionary *names;static dispatch_once_t once;
    dispatch_once(&once,^{ names=@{
        @"Dislike":@1,@"DislikeSelected":@1,@"Like":@2,@"LikeSelected":@2,
        @"FillThumbUp24":@2,@"OutlineThumbUp":@2,
        @"ic_thumb_down":@1,@"ic_thumb_down_outline":@1,@"ic_thumb_up":@2,@"ic_thumb_up_outline":@2,
        @"google_symbols/thumb_down_symbol":@1,@"google_symbols/thumb_up_symbol":@2,
        @"youtube_fill/thumb_down_24pt":@1,@"youtube_outline/thumb_down_24pt":@1,
        @"youtube_fill/thumb_up_24pt":@2,@"youtube_outline/thumb_up_24pt":@2}; });
    int exact=[names[name] intValue];if (exact) return exact;
    static NSRegularExpression *assets;static dispatch_once_t assetOnce;
    dispatch_once(&assetOnce,^{ assets=[NSRegularExpression regularExpressionWithPattern:
        @"^(?:yt_(?:fill|outline)(?:_experimental)?_thumb_(down|up)_(?:(?:12|16|18|24|36)pt|(?:black|vd_theme)_(?:12|16|18|24|36))|youtube_(?:fill|outline)_experimental/thumb_(down|up)_(?:12|18|24|36)pt)$" options:0 error:nil]; });
    if (![assets firstMatchInString:name options:0 range:NSMakeRange(0,name.length)]) return 0;
    return [name containsString:@"thumb_down"] ? 1 : 2;
}
static int RVElementIconKind(int icon) {
    static NSDictionary *kinds;static dispatch_once_t once;
    dispatch_once(&once,^{
        Class providerClass=NSClassFromString(@"YTAppIconSupportProviderImpl");
        id provider=[providerClass new];
        NSMutableDictionary *known=[NSMutableDictionary dictionary];
        for (NSString *selector in @[@"delhiResourceMap",@"resourceMap"]) {
        id map=RVObject(provider,selector.UTF8String);
        if ([map isKindOfClass:NSDictionary.class] && [map count]<=4096) for (id key in map) {
            id name=map[key];if (![key isKindOfClass:NSNumber.class] || ![name isKindOfClass:NSString.class]) continue;
            int kind=RVElementNamedIconKind(name);if (kind) known[key]=@(kind);
        }
        }
        // Toolbar resource calls independently prove these four native IDs.
        known[@0x250]=@1;known[@0x238]=@1;known[@0x251]=@2;known[@0x239]=@2;
        kinds=known.copy;
    });return [kinds[@(icon)] intValue];
}
static void RVElementRememberImage(id value,int kind) {
    if (!kind || ![value isKindOfClass:UIImage.class]) return;
    objc_setAssociatedObject(value,RVElementImageKindKey,@(kind),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    RVElementDislikeEvent(kind==1 ? @"down_image_tagged" : @"up_image_tagged");
    @synchronized(UIImage.class) {
        if (!RVElementVoteImages) RVElementVoteImages=[NSMutableArray array];
        for (NSDictionary *entry in RVElementVoteImages) if (entry[@"image"]==value) return;
        if (RVElementVoteImages.count>=64) [RVElementVoteImages removeObjectAtIndex:0];
        [RVElementVoteImages addObject:@{@"image":value,@"kind":@(kind)}];
    }
}
static int RVElementImageKind(id node) {
    Class cls=NSClassFromString(@"ASImageNode");
    if (!cls || ![node isKindOfClass:cls]) return 0;
    id image=RVObject(node,"image");if (![image isKindOfClass:UIImage.class]) return 0;
    NSNumber *tag=objc_getAssociatedObject(image,RVElementImageKindKey);if (tag) return tag.intValue;
    @synchronized(UIImage.class) {
        for (NSDictionary *entry in RVElementVoteImages.reverseObjectEnumerator) {
            UIImage *known=entry[@"image"];
            if (image==known || (known.CGImage && known.CGImage==[image CGImage])) return [entry[@"kind"] intValue];
        }
    }return 0;
}
static id RVElementParent(id node) {
    // Flattened render subnodes share a supernode; Yoga retains the button branches.
    return RVObject(node,"yogaParent") ?: RVObject(node,"supernode");
}
static NSArray *RVElementChildren(id node) {
    id children=RVObject(node,"yogaChildren");
    if (![children isKindOfClass:NSArray.class] || ![children count]) children=RVObject(node,"subnodes");
    return [children isKindOfClass:NSArray.class] ? children : @[];
}
static NSArray *RVElementSubtree(id root) {
    NSMutableArray *nodes=[NSMutableArray arrayWithObject:root];
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];[seen addObject:root];
    for (NSUInteger i=0;i<nodes.count && i<128;i++) {
        for (id child in RVElementChildren(nodes[i])) {
        if (nodes.count>=128) break;
        if (![seen containsObject:child]) { [seen addObject:child];[nodes addObject:child]; }
        }
    }
    return nodes;
}
static UIView *RVElementLoadedView(id node) {
    for (NSUInteger i=0;node && i<12;i++,node=RVElementParent(node)) {
        if (RVBool(node,"isNodeLoaded") && !RVBool(node,"isLayerBacked")) {
            id view=RVObject(node,"view");if ([view isKindOfClass:UIView.class]) return view;
        }
    }return nil;
}
static const void *RVElementBindingKey=&RVElementBindingKey;
@interface RVElementBindingState : NSObject
@property(nonatomic,weak) id element;
@property(nonatomic,copy) NSString *video;
@property(nonatomic) BOOL role;
@property(nonatomic) BOOL videoMatches;
@property(nonatomic) BOOL explicitVideo;
@property(nonatomic) NSTimeInterval checkedAt;
@property(nonatomic) BOOL protoAvailable;
@property(nonatomic) NSUInteger protoLength;
@property(nonatomic,copy) NSString *roleSource;
@end
@implementation RVElementBindingState @end
static RVElementBindingState *RVElementBinding(id component,NSString *video) {
    id element=RVObject(component,"element");
    RVElementBindingState *state=objc_getAssociatedObject(component,RVElementBindingKey);
    if (state && state.element==element && [state.video isEqual:video] && (state.role || [NSDate timeIntervalSinceReferenceDate]-state.checkedAt<.25)) return state;
    state=[RVElementBindingState new];state.element=element;state.video=video;state.checkedAt=[NSDate timeIntervalSinceReferenceDate];
    NSArray *roles=@[@"video_action_bar",@"segmented_like_dislike",@"segmentedLikeDislike",@"SegmentedLikeDislikeButton"];
    for (NSString *source in @[@"key",@"templateURI"]) {
        id key=RVObject(component,source.UTF8String);
        if ([key isKindOfClass:NSString.class] && [key length]<=2048)
            for (NSString *role in roles) if ([key rangeOfString:role options:NSCaseInsensitiveSearch].location!=NSNotFound) { state.role=YES;state.roleSource=source;break; }
    }
    // ELMElement owns a C++ protobuf. Its native Objective-C diagnostic bridge
    // avoids fixed C++ offsets. Cache only boolean checks, never model text.
    Class cls=NSClassFromString(@"ELMElement");
    id text=cls && [element isKindOfClass:cls] ? RVObject(element,"protoText") : nil;
    state.protoAvailable=[text isKindOfClass:NSString.class];state.protoLength=state.protoAvailable ? [text length] : 0;
    if (state.protoAvailable && [text length]<=131072) {
        for (NSString *role in roles) if ([text rangeOfString:role options:NSCaseInsensitiveSearch].location!=NSNotFound) { state.role=YES;state.roleSource=@"element_proto";break; }
        static NSRegularExpression *ids;static dispatch_once_t once;
        dispatch_once(&once,^{ ids=[NSRegularExpression regularExpressionWithPattern:@"(?:video_id|videoId|videoID)\\s*:\\s*\"([A-Za-z0-9_-]{11})\"" options:0 error:nil]; });
        NSArray *matches=[ids matchesInString:text options:0 range:NSMakeRange(0,[text length])];
        state.explicitVideo=matches.count>0;
        state.videoMatches=matches.count>0 && matches.count<=32;
        for (NSTextCheckingResult *match in matches) if (![[text substringWithRange:[match rangeAtIndex:1]] isEqual:video]) { state.videoMatches=NO;break; }
    }
    objc_setAssociatedObject(component,RVElementBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);return state;
}
static NSArray *RVElementOwnerChain(id controller) {
    if (!controller) return @[];
    NSMutableArray *owners=[NSMutableArray arrayWithObject:controller];
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];[seen addObject:controller];
    // Materialized descendants can have a parent without an owningComponent.
    // Follow both native Objective-C links; never cache a negative role forever.
    for (NSUInteger i=0;i<owners.count && i<24;i++) for (NSString *link in @[@"owningComponent",@"parent"]) {
        id next=RVObject(owners[i],link.UTF8String);
        if (next && owners.count<24 && ![seen containsObject:next]) { [seen addObject:next];[owners addObject:next]; }
    }
    return owners;
}
static id RVElementFindBinding(id node,NSString *video) {
    NSMutableArray *probe=[NSMutableArray array];
    for (NSUInteger level=0;node && level<12;level++,node=RVElementParent(node)) {
        id controller=RVObject(node,"nodeController");
        for (id component in RVElementOwnerChain(controller)) {
            RVElementBindingState *binding=RVElementBinding(component,video);
            if (RVEnabled(@"diagnostics") && probe.count<24) [probe addObject:@{
                @"owner_class":NSStringFromClass([component class]),@"role_found":@(binding.role),
                @"role_source":binding.roleSource ?: @"absent",@"proto_available":@(binding.protoAvailable),
                @"proto_length":@(binding.protoLength),@"explicit_video":@(binding.explicitVideo),
                @"video_matches":@(binding.videoMatches),@"parent_present":@(RVObject(component,"parent")!=nil),
                @"owning_component_present":@(RVObject(component,"owningComponent")!=nil)}];
            if (binding.role) { RVElementBindingProbe=probe.copy;return component; }
        }
    }
    RVElementBindingProbe=probe.copy;return nil;
}
static void RVElementInvalidateRoot(id parent,RVElementDislikeState *state) {
    id root=RVObject(parent,"yogaRoot");state.layoutRoot=root;
    NSHashTable *tracked=objc_getAssociatedObject(root,RVElementRootTraceKey);
    if (root && !tracked) { tracked=[NSHashTable weakObjectsHashTable];objc_setAssociatedObject(root,RVElementRootTraceKey,tracked,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
    [tracked addObject:state];state.rootInvalidations++;
    RVVoid(parent,"setNeedsLayout");RVVoid(root,"setNeedsLayout");
}
static void RVElementTextTrace(id node,BOOL drawing) {
    if (!RVEnabled(@"diagnostics")) return;
    void (^record)(void)=^{
        for (id parent in RVElementDislikeParents.allObjects) {
            RVElementDislikeState *state=objc_getAssociatedObject(parent,RVElementDislikeKey);
            if (state.textNode!=node || ![state.video isEqual:RVDislikeVideo]) continue;
            if (drawing) state.textDisplayCallbacks++;else state.textLoadCallbacks++;
            RVElementDislikeEvent(drawing ? @"owned_text_display_finished" : @"owned_text_did_load");
        }
    };
    if (NSThread.isMainThread) record();else dispatch_async(dispatch_get_main_queue(),record);
}
static CGRect RVElementTextFrame(id text) {
    if (!RVCan(text,"frame",@"{CGRect={CGPoint=dd}{CGSize=dd}}@:")) return CGRectZero;
    CGRect frame=((CGRect(*)(id,SEL))objc_msgSend)(text,sel_registerName("frame"));
    return isfinite(frame.origin.x) && isfinite(frame.origin.y) && isfinite(frame.size.width) && isfinite(frame.size.height) ? frame : CGRectZero;
}
// Native ASEdgeInsets is nine {NSInteger unit; double value;} dimensions:
// top,left,bottom,right,start,end,horizontal,vertical,all. Points are unit 1.
// ASLayoutElementStyleYoga.setSpacingBefore: is a no-op; use its margin API.
typedef struct { NSInteger unit; double value; } RVElementDimension;
typedef struct { RVElementDimension top,left,bottom,right,start,end,horizontal,vertical,all; } RVElementEdges;
static BOOL RVElementSetTextGap(id text,CGFloat gap,CGFloat endMargin) {
    id style=RVObject(text,"style");
    if (!RVCan(style,"setMargin:",@"v@:{?={?=qd}{?=qd}{?=qd}{?=qd}{?=qd}{?=qd}{?=qd}{?=qd}{?=qd}}")) return NO;
    RVElementEdges margin={0};margin.start=(RVElementDimension){1,gap};margin.end=(RVElementDimension){1,endMargin};
    ((void(*)(id,SEL,RVElementEdges))objc_msgSend)(style,sel_registerName("setMargin:"),margin);return YES;
}
static BOOL RVElementOwnedWatch(id parent,RVElementDislikeState *state) {
    if (!state.textNode || ![state.video isEqual:RVDislikeVideo] || !RVDislikeHasCount() || !RVOrdinaryContent(RVCurrentPlayback)) return NO;
    UIViewController *watch=state.watch;UIView *view=RVElementLoadedView(parent);
    if (!watch.isViewLoaded || !view.window || ![view isDescendantOfView:watch.view] ||
        (RVCurrentWatch && RVCurrentWatch!=watch) || state.element!=RVObject(state.component,"element")) return NO;
    id ancestor=parent;
    for (NSUInteger i=0;ancestor && i<12;i++,ancestor=RVElementParent(ancestor)) {
        for (id component in RVElementOwnerChain(RVObject(ancestor,"nodeController"))) if (component==state.component) return YES;
    }return NO;
}
static BOOL RVElementRectInAncestor(id node,id ancestor,CGRect *result) {
    CGRect rect=RVElementTextFrame(node);if (rect.size.width<=0 || rect.size.height<=0) return NO;
    id parent=RVElementParent(node);
    for (NSUInteger i=0;parent && parent!=ancestor && i<12;i++,parent=RVElementParent(parent)) {
        CGRect frame=RVElementTextFrame(parent);rect.origin.x+=frame.origin.x;rect.origin.y+=frame.origin.y;
    }
    if (parent!=ancestor) return NO;*result=rect;return YES;
}
static void RVElementBalanceEdges(id parent,RVElementDislikeState *state) {
    if (!state.textNode || state.updating || !RVElementOwnedWatch(parent,state)) return;
    id pair=RVElementParent(parent);
    BOOL rtl=RVElementLoadedView(parent).effectiveUserInterfaceLayoutDirection==UIUserInterfaceLayoutDirectionRightToLeft;
    for (NSUInteger level=0;pair && level<6;level++,pair=RVElementParent(pair)) {
        NSArray *nodes=RVElementSubtree(pair);if (nodes.count>=128) break;
        NSMutableArray *images=[NSMutableArray array];BOOL nativeText=NO;
        for (id node in nodes) {
            if ([node isKindOfClass:NSClassFromString(@"ASImageNode")] && [RVObject(node,"image") isKindOfClass:UIImage.class]) [images addObject:node];
            id text=RVObject(node,"attributedText");if (node!=state.textNode && [text isKindOfClass:NSAttributedString.class] && [text length]) nativeText=YES;
        }
        if (images.count>2) break;
        if (images.count!=2 || !nativeText) continue;
        id other=nil;NSUInteger downs=0;
        for (id image in images) if (RVElementImageKind(image)==1) downs++;else other=image;
        if (downs!=1 || !other) continue;
        CGRect iconRect,textRect,pairRect=RVElementTextFrame(pair);
        if (!RVElementRectInAncestor(other,pair,&iconRect) || !RVElementRectInAncestor(state.textNode,pair,&textRect) || pairRect.size.width<=0) continue;
        CGFloat leading=rtl ? pairRect.size.width-CGRectGetMaxX(iconRect) : CGRectGetMinX(iconRect);
        CGFloat trailing=rtl ? CGRectGetMinX(textRect) : pairRect.size.width-CGRectGetMaxX(textRect);
        if (!isfinite(leading) || !isfinite(trailing) || leading<1 || leading>32 || trailing<0 || trailing>32) continue;
        state.leadingInset=leading;state.trailingInset=trailing;
        CGFloat target=MAX(0,MIN(24,state.endMargin+leading-trailing));
        state.edgeGate=fabs(leading-trailing)<=.5 ? @"native_outer_edges_balanced" : @"native_outer_edge_adjustment";
        if (fabs(target-state.endMargin)>.5 && RVElementSetTextGap(state.textNode,state.spacingPoints,target)) {
            state.endMargin=target;RVElementInvalidateRoot(parent,state);RVElementDislikeEvent(@"outer_edge_margin_updated");
        }return;
    }
    state.edgeGate=@"native_pair_geometry_unavailable";
}
static void RVElementSuspend(RVElementDislikeState *state,NSString *reason) {
    if (!state.textNode) return;
    state.mountGate=reason;state.renderLabel.hidden=YES;
    if (!state.suspended) { state.suspended=YES;state.suspensionCount++;RVElementDislikeEvent(@"watch_count_suspended"); }
}
static void RVElementMountOwnedText(id parent,RVElementDislikeState *state) {
    if (!NSThread.isMainThread || !state.textNode || ![state.video isEqual:RVDislikeVideo] || !RVDislikeHasCount()) return;
    if ((!RVDislikeWatchAction(RVElementLoadedView(parent)) || !RVDislikeWatchPlayerMatches(RVCurrentWatch)) && !RVElementOwnedWatch(parent,state)) {
        RVElementSuspend(state,@"watch_transition_suspended");return;
    }
    CGRect frame=RVElementTextFrame(state.textNode);
    if (frame.size.width<=0 || frame.size.height<=0) { state.mountGate=@"waiting_for_measurement";return; }
    // Mount only inside a real, existing native button, with button-local frame.
    // Never create/load a native host or append to an unrelated cell/window.
    if (!RVBool(parent,"isNodeLoaded")) { state.mountGate=@"button_not_loaded";return; }
    if (RVBool(parent,"isFlattenable")) { state.mountGate=@"flattened_button_requires_native_renderer";return; }
    if (RVBool(parent,"isLayerBacked")) { state.mountGate=@"layer_backed_button";return; }
    UIView *view=RVObject(parent,"view");
    if (![view isKindOfClass:UIView.class] || !view.window) { state.mountGate=@"button_view_not_in_window";return; }
    id renderParent=RVObject(state.textNode,"supernode");
    if (renderParent) {
        [state.renderLabel removeFromSuperview];state.renderLabel=nil;
        state.mountMode=@"native_text_node";state.mountGate=@"native_render_parent_present";return;
    }
    // Native _insertSubnode refuses nodes with different ASNodeContext identity.
    // Do not rewrite that context: it also owns the node's locking configuration.
    BOOL contextMatches=RVCan(parent,"nodeContext",@"@@:") && RVCan(state.textNode,"nodeContext",@"@@:") &&
        RVObject(parent,"nodeContext")==RVObject(state.textNode,"nodeContext");
    if (contextMatches && RVCan(parent,"addSubnode:",@"v@:@")) {
        state.mountAttempts++;RVElementDislikeEvent(@"native_mount_attempt");
        RVSetObject(parent,"addSubnode:",state.textNode);
        if (RVObject(state.textNode,"supernode")) {
            [state.renderLabel removeFromSuperview];state.renderLabel=nil;
            state.mountMode=@"native_text_node";state.mountGate=@"native_button_mounted";
            RVVoid(state.textNode,"setNeedsDisplay");return;
        }
    }
    // Keep the ASTextNode as the Yoga sizing child. Render the exact same text
    // using one owned UILabel inside this native button when native mounting is
    // rejected. UIKit owns its context/layer lifecycle. Native commands and all
    // existing subnodes/subviews stay intact; the label cannot intercept taps.
    id display=RVObject(state.textNode,"attributedText");
    if (![display isKindOfClass:NSAttributedString.class]) { state.mountGate=@"text_attributes_unavailable";return; }
    if (!state.renderLabel) {
        state.renderLabel=[UILabel new];state.renderLabel.userInteractionEnabled=NO;
        state.renderLabel.accessibilityIdentifier=@"rvport.dislike.count.button";
        state.renderLabel.backgroundColor=UIColor.clearColor;state.renderLabel.numberOfLines=1;
    }
    if (![state.renderLabel.attributedText isEqualToAttributedString:display]) state.renderLabel.attributedText=display;
    state.renderLabel.accessibilityLabel=RVObject(state.textNode,"accessibilityLabel");
    CGSize natural=[state.renderLabel sizeThatFits:CGSizeMake(frame.size.width,CGFLOAT_MAX)];
    if (isfinite(natural.height) && natural.height>frame.size.height) {
        frame.origin.y-=(natural.height-frame.size.height)/2;frame.size.height=natural.height;
    }
    state.renderLabel.frame=frame;state.renderLabel.hidden=NO;
    if (state.suspended) { state.suspended=NO;state.resumeCount++;RVElementDislikeEvent(@"watch_count_resumed"); }
    if (state.renderLabel.superview!=view) {
        [view addSubview:state.renderLabel];state.mountAttempts++;RVElementDislikeEvent(@"owned_label_mounted");
    }
    state.mountMode=@"owned_label_in_native_button";
    state.mountGate=contextMatches ? @"native_mount_rejected_using_button_label" : @"context_mismatch_using_button_label";
}
static void RVElementDislikeRemove(id parent,RVElementDislikeState *state) {
    if (!state.textNode || state.updating) return;
    state.updating=YES;
    RVSetObject(parent,"removeYogaChild:",state.textNode);
    if (RVCan(state.textNode,"removeFromSupernode",@"v@:")) ((void(*)(id,SEL))objc_msgSend)(state.textNode,sel_registerName("removeFromSupernode"));
    [state.renderLabel removeFromSuperview];state.renderLabel=nil;
    state.textNode=nil;state.text=nil;state.suspended=NO;state.mountMode=@"removed";state.mountGate=@"binding_invalidated";state.updating=NO;
}
static void RVElementDislikeApply(id parent,id pair) {
    RVElementDislikeState *state=objc_getAssociatedObject(parent,RVElementDislikeKey);
    if (state.updating) return;
    UIView *view=RVElementLoadedView(parent);
    if (!RVDislikeHasCount() || (state.textNode && ![state.video isEqual:RVDislikeVideo])) { RVElementDislikeRemove(parent,state);return; }
    if ((!RVOrdinaryContent(RVCurrentPlayback) || !RVDislikeWatchAction(view) || !RVDislikeWatchPlayerMatches(RVCurrentWatch)) && !RVElementOwnedWatch(parent,state)) {
        RVElementSuspend(state,@"watch_transition_suspended");return;
    }
    id bound=RVElementFindBinding(pair,RVDislikeVideo);
    if (!bound) { RVElementLastGate=@"semantic_role_missing";RVElementDislikeEvent(@"semantic_role_missing");RVElementDislikeRemove(parent,state);return; }
    RVElementDislikeEvent(@"semantic_role_matched");
    id element=RVObject(bound,"element");NSString *video=RVDislikeVideo;
    BOOL boundAlready=state && state.component==bound && state.element==element && [state.video isEqual:video];
    RVElementBindingState *binding=RVElementBinding(bound,video);
    BOOL watchMatches=RVDislikeWatchPlayerMatches(RVCurrentWatch) || RVElementOwnedWatch(parent,state);
    BOOL videoMatches=binding.explicitVideo ? binding.videoMatches : watchMatches;
    if (!watchMatches || (!boundAlready && !videoMatches)) {
        RVElementLastGate=@"video_binding_missing";RVElementDislikeEvent(@"video_binding_missing");RVElementDislikeRemove(parent,state);return;
    }
    RVElementDislikeEvent(binding.explicitVideo ? @"element_video_bound" : @"watch_player_video_bound");
    if (!RVCan(parent,"appendYogaChild:",@"v@:@") || !RVCan(parent,"removeYogaChild:",@"v@:@") || !RVBool(parent,"yoga")) {
        RVElementLastGate=@"yoga_contract_missing";RVElementDislikeEvent(@"yoga_contract_missing");return;
    }
    if (!state) { state=[RVElementDislikeState new];objc_setAssociatedObject(parent,RVElementDislikeKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
    if (![state.video isEqual:video]) { state.rootInvalidations=0;state.rootLayoutPasses=0;state.textLoadCallbacks=0;state.textDisplayCallbacks=0;state.mountAttempts=0; }
    state.button=parent;if (RVCurrentWatch) state.watch=RVCurrentWatch;state.component=bound;state.element=element;state.video=video;
    // The independently discovered dislike branch has no native text. Search
    // its nearest ancestors for the neighboring native like typography.
    NSDictionary *attributes=nil;id typography=pair;
    for (NSUInteger level=0;typography && !attributes && level<4;level++,typography=RVElementParent(typography)) {
    for (id node in RVElementSubtree(typography)) {
        if (node==state.textNode) continue;
        id native=RVObject(node,"attributedText");
        if ([native isKindOfClass:NSAttributedString.class] && [native length]) { attributes=[native attributesAtIndex:0 effectiveRange:NULL];break; }
    }
    }
    UIFont *font=[attributes[NSFontAttributeName] isKindOfClass:UIFont.class] ? attributes[NSFontAttributeName] :
        [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody] scaledFontForFont:[UIFont systemFontOfSize:14 weight:UIFontWeightMedium]];
    UIColor *color=[attributes[NSForegroundColorAttributeName] isKindOfClass:UIColor.class] ? attributes[NSForegroundColorAttributeName] : nil;
    state.colorSource=color ? @"native_neighbor" : @"native_palette";
    if (!color) {
        NSInteger pageStyle=view.traitCollection.userInterfaceStyle==UIUserInterfaceStyleDark ? 1 : 0;
        if (RVCan(RVCurrentWatch,"pageStyle",@"q@:")) pageStyle=((NSInteger(*)(id,SEL))objc_msgSend)(RVCurrentWatch,sel_registerName("pageStyle"));
        id palette=RVObject(NSClassFromString(@"YTCommonColorPalette"),pageStyle==1 ? "darkPalette" : "lightPalette");
        id native=RVObject(palette,"textPrimary");
        color=[native isKindOfClass:UIColor.class] ? native : (pageStyle==1 ? UIColor.whiteColor : UIColor.blackColor);
    }
    NSString *text=RVDislikeCompactCount(RVDislikeCount);
    NSAttributedString *display=[[NSAttributedString alloc] initWithString:text attributes:@{NSFontAttributeName:font,NSForegroundColorAttributeName:color}];
    state.updating=YES;
    if (!state.textNode) {
        Class textClass=NSClassFromString(@"ASTextNode");id node=[textClass new];
        if (!RVCan(node,"setAttributedText:",@"v@:@")) { state.updating=NO;return; }
        if (!RVCan(node,"enableYoga",@"v@:") || !RVCan(node,"enableViewFlattening",@"v@:")) { state.updating=NO;return; }
        RVVoid(node,"enableYoga");
        // Native +yogaNode enables both. The renderer sums skipped ancestor
        // offsets only for flattening-enabled nodes; otherwise a text frame
        // relative to the button is applied relative to the flattened host.
        // Yoga's renderer manages render subnodes itself: do not addSubnode.
        RVVoid(node,"enableViewFlattening");
        RVSetObject(node,"setAttributedText:",display);
        RVSetObject(node,"setAccessibilityIdentifier:",@"rvport.dislike.count.element");
        state.spacingPoints=8;
        state.spacingGate=RVElementSetTextGap(node,state.spacingPoints,state.endMargin) ? @"logical_start_margin_reserved" : @"margin_contract_unavailable";
        if (RVCan(node,"setUserInteractionEnabled:",@"v@:B")) ((void(*)(id,SEL,BOOL))objc_msgSend)(node,sel_registerName("setUserInteractionEnabled:"),NO);
        objc_setAssociatedObject(node,RVElementTextTraceKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        state.textNode=node;RVSetObject(parent,"appendYogaChild:",node);
        RVVoid(node,"setNeedsDisplay");
        RVElementInvalidateRoot(parent,state);
        if (!RVElementDislikeParents) RVElementDislikeParents=[NSHashTable weakObjectsHashTable];[RVElementDislikeParents addObject:parent];
        RVElementDislikeEvent(@"owned_node_attached");
    } else {
        id existing=RVObject(state.textNode,"attributedText");
        if (![existing isKindOfClass:NSAttributedString.class] || ![existing isEqualToAttributedString:display]) {
            RVSetObject(state.textNode,"setAttributedText:",display);RVElementInvalidateRoot(parent,state);
        }
        if (![RVElementChildren(parent) containsObject:state.textNode]) {
            RVSetObject(parent,"appendYogaChild:",state.textNode);
            RVElementInvalidateRoot(parent,state);
        }
    }
    RVSetObject(state.textNode,"setAccessibilityLabel:",[NSString stringWithFormat:@"Return YouTube Dislike estimate: %@ dislikes",[NSNumberFormatter localizedStringFromNumber:RVDislikeCount numberStyle:NSNumberFormatterDecimalStyle]]);
    state.text=text;RVElementMountOwnedText(parent,state);state.updating=NO;RVElementBalanceEdges(parent,state);RVElementLastGate=@"owned_count_attached";
}
static BOOL RVElementDownOnlyButton(id node) {
    id style=RVObject(node,"style");
    if (!RVBool(node,"yoga") || !RVCan(style,"flexDirection",@"C@:")) return NO;
    // Native ASDisplayNode yogaHorizontalStack sets direction 1. This is the
    // app's Objective-C layout enum, not the underlying Yoga C enum.
    unsigned char direction=((unsigned char(*)(id,SEL))objc_msgSend)(style,sel_registerName("flexDirection"));
    if (direction!=1) return NO;
    NSArray *children=RVElementSubtree(node);if (children.count>=128) return NO;
    Class imageClass=NSClassFromString(@"ASImageNode");NSUInteger images=0,down=0;
    for (id child in children) {
        if (imageClass && [child isKindOfClass:imageClass] && [RVObject(child,"image") isKindOfClass:UIImage.class]) images++;
        down+=RVElementImageKind(child)==1;
    }
    // Exactly one native icon prevents appending to the combined pill or a row
    // of unrelated controls even when their other icons have not been tagged.
    return images==1 && down==1;
}
static void RVElementDislikeObserve(id root) {
    RVElementDislikeEvent(@"observe_entered");
    if (!NSThread.isMainThread) { RVElementDislikeEvent(@"observe_background");return; }
    if (!RVDislikeHasCount()) { RVElementLastGate=@"count_or_video_unavailable";return; }
    UIView *view=RVElementLoadedView(root);
    if (!RVDislikeWatchAction(view)) { RVElementLastGate=@"outside_active_watch";return; }
    RVElementLastGate=@"scanning_watch";RVElementDislikeEvent(@"watch_node_scanned");
    for (id image in RVElementSubtree(root)) {
        if ([image isKindOfClass:NSClassFromString(@"ASImageNode")]) RVElementDislikeEvent(@"image_node_scanned");
        if (RVElementImageKind(image)!=1) continue;
        RVElementDislikeEvent(@"dislike_icon_seen");
        id branch=image;
        // The 0.3.11 device report sees down icons but no up icons. Discover the
        // native dislike branch on its own; do not require a second image path.
        for (NSUInteger level=0;branch && level<12;level++,branch=RVElementParent(branch)) {
            if (branch==image || !RVElementDownOnlyButton(branch)) continue;
            RVElementDislikeEvent(@"dislike_button_candidate");RVElementDislikeApply(branch,branch);
            RVElementDislikeState *state=objc_getAssociatedObject(branch,RVElementDislikeKey);
            if (state.textNode) { RVElementDislikeEvent(@"independent_button_bound");break; }
        }
        if (branch && ((RVElementDislikeState *)objc_getAssociatedObject(branch,RVElementDislikeKey)).textNode) continue;
        branch=image;
        for (NSUInteger level=0;branch && level<12;level++,branch=RVElementParent(branch)) {
            id pair=RVElementParent(branch);if (!pair) break;
            NSUInteger up=0,down=0;
            for (id child in RVElementSubtree(pair)) { int kind=RVElementImageKind(child);up+=kind==2;down+=kind==1; }
            if (!up) continue;
            NSUInteger branchUp=0,branchDown=0;
            for (id child in RVElementSubtree(branch)) { int kind=RVElementImageKind(child);branchUp+=kind==2;branchDown+=kind==1; }
            if (up==1 && down==1 && branch!=image && !branchUp && branchDown==1) { RVElementDislikeEvent(@"native_pair_seen");RVElementDislikeApply(branch,pair); }
            break;
        }
    }
}
static void RVRefreshElementDislikes(void) {
    if (!NSThread.isMainThread) return;
    RVElementDislikeEvent(@"refresh_entered");
    for (id parent in RVElementDislikeParents.allObjects) {
        RVElementDislikeState *state=objc_getAssociatedObject(parent,RVElementDislikeKey);
        if (!RVDislikeHasCount() || ![state.video isEqual:RVDislikeVideo]) RVElementDislikeRemove(parent,state);
        else if ((!RVDislikeWatchAction(RVElementLoadedView(parent)) || !RVDislikeWatchPlayerMatches(RVCurrentWatch)) && !RVElementOwnedWatch(parent,state)) RVElementSuspend(state,@"watch_transition_suspended");
    }
    UIViewController *watch=RVCurrentWatch;
    if (![watch isKindOfClass:UIViewController.class] || !watch.isViewLoaded) { RVElementLastGate=@"watch_unavailable";return; }
    RVElementDislikeEvent(@"watch_refresh");
    NSMutableArray *views=[NSMutableArray arrayWithObject:watch.view];NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    for (NSUInteger i=0;i<views.count && i<2048;i++) {
        UIView *view=views[i];id node=RVObject(view,"asyncdisplaykit_node");
        if (node && ![seen containsObject:node]) {
            for (id child in RVElementSubtree(node)) [seen addObject:child];
            RVElementDislikeObserve(node);
        }
        for (UIView *child in view.subviews) if (views.count<2048) [views addObject:child];
    }
    RVDislikeCaptureCheckpoint(@"watch_scan");
}
static void RVElementQueueRefresh(void) {
    @synchronized(UIImage.class) {
        if (RVElementRefreshQueued) return;RVElementRefreshQueued=YES;
    }
    // Coalesce background element/image work to at most four whole-watch scans/s.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,250*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
        RVRefreshElementDislikes();
        @synchronized(UIImage.class) { RVElementRefreshQueued=NO; }
    });
}
static const void *RVElementWatchGenerationKey=&RVElementWatchGenerationKey;
static const void *RVElementWatchOwnerKey=&RVElementWatchOwnerKey;
static const void *RVElementWatchWindowKey=&RVElementWatchWindowKey;
static void RVElementWatchTransition(id watch,NSString *reason) {
    if (!NSThread.isMainThread || ![watch isKindOfClass:UIViewController.class]) return;
    NSUInteger generation=[objc_getAssociatedObject(watch,RVElementWatchGenerationKey) unsignedIntegerValue]+1;
    objc_setAssociatedObject(watch,RVElementWatchGenerationKey,@(generation),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    RVElementDislikeEvent(reason);__weak id weakWatch=watch;NSString *video=RVDislikeVideo;
    UIViewController *current=watch;
    if (current.isViewLoaded && current.view.window && RVDislikeWatchPlayerMatches(current)) RVCurrentWatch=current;
    RVRefreshElementDislikes();RVElementDislikeEvent(@"synchronous_transition_refresh");
    // Bounded transition retries catch the native row's deferred reconstruction.
    // They are independent of playback state and do not request count data.
    for (NSNumber *delay in @[@16,@64,@250,@750]) dispatch_after(dispatch_time(DISPATCH_TIME_NOW,delay.longLongValue*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
        UIViewController *current=weakWatch;
        if (!current || [objc_getAssociatedObject(current,RVElementWatchGenerationKey) unsignedIntegerValue]!=generation || ![video isEqual:RVDislikeVideo]) return;
        if (current.isViewLoaded && current.view.window && RVDislikeWatchPlayerMatches(current)) RVCurrentWatch=current;
        RVElementDislikeEvent(@"transition_refresh");RVRefreshElementDislikes();RVDislikeCaptureCheckpoint(@"watch_transition_refresh");
    });
}
static NSDictionary *RVElementDislikeReport(void) {
    NSUInteger attached=0,laidOut=0,mounted=0,loaded=0,inWindow=0,unclipped=0,contents=0,displayFinished=0;
    NSMutableArray *geometry=[NSMutableArray array];
    for (id node in RVElementDislikeParents.allObjects) {
        RVElementDislikeState *state=objc_getAssociatedObject(node,RVElementDislikeKey);id text=state.textNode;
        if (!text) continue;attached++;
        CGRect iconFrame=CGRectZero;BOOL gapMeasured=NO;CGFloat actualGap=0;
        if (state.renderLabel.superview) for (id image in RVElementSubtree(node)) {
            if (RVElementImageKind(image)!=1 || !RVBool(image,"isNodeLoaded") || RVBool(image,"isLayerBacked")) continue;
            id iconView=RVObject(image,"view");UIView *buttonView=state.renderLabel.superview;
            if (![iconView isKindOfClass:UIView.class] || ![iconView isDescendantOfView:buttonView]) continue;
            iconFrame=[iconView convertRect:[iconView bounds] toView:buttonView];
            CGRect labelFrame=state.renderLabel.frame;
            BOOL rtl=buttonView.effectiveUserInterfaceLayoutDirection==UIUserInterfaceLayoutDirectionRightToLeft;
            actualGap=rtl ? CGRectGetMinX(iconFrame)-CGRectGetMaxX(labelFrame) : CGRectGetMinX(labelFrame)-CGRectGetMaxX(iconFrame);
            gapMeasured=isfinite(actualGap);break;
        }
        CGRect frame=CGRectZero;
        if (RVCan(text,"frame",@"{CGRect={CGPoint=dd}{CGSize=dd}}@:")) frame=((CGRect(*)(id,SEL))objc_msgSend)(text,sel_registerName("frame"));
        BOOL labelMounted=state.renderLabel.superview!=nil;
        if (labelMounted) frame=state.renderLabel.frame;
        BOOL positive=isfinite(frame.origin.x) && isfinite(frame.origin.y) && isfinite(frame.size.width) && isfinite(frame.size.height) && frame.size.width>0 && frame.size.height>0;
        laidOut+=positive;id renderParent=RVObject(text,"supernode");mounted+=renderParent!=nil || labelMounted;
        BOOL nativeLoaded=RVBool(text,"isNodeLoaded");BOOL isLoaded=nativeLoaded || labelMounted;loaded+=isLoaded;
        UIView *host=labelMounted ? state.renderLabel.superview : renderParent ? RVElementLoadedView(renderParent) : nil;
        CALayer *layer=labelMounted ? state.renderLabel.layer : nativeLoaded ? RVObject(text,"layer") : nil;
        BOOL onWindow=NO;
        if (host.window && [layer isKindOfClass:CALayer.class]) {
            CALayer *ancestor=layer;
            for (NSUInteger depth=0;ancestor && depth<64;depth++,ancestor=ancestor.superlayer)
                if (ancestor==host.window.layer) { onWindow=YES;break; }
        }
        inWindow+=onWindow;
        BOOL raster=[layer isKindOfClass:CALayer.class] && layer.contents!=nil;
        if (labelMounted && !raster) {
            NSMutableArray<CALayer *> *layers=[NSMutableArray arrayWithObject:layer];
            for (NSUInteger i=0;i<layers.count && i<16;i++) {
                if (layers[i].contents) { raster=YES;break; }
                for (CALayer *child in layers[i].sublayers) if (layers.count<16) [layers addObject:child];
            }
        }
        contents+=raster;displayFinished+=state.textDisplayCallbacks>0;
        BOOL visible=NO;NSUInteger clipping=0;
        if (onWindow && positive) {
            CGRect rect=layer.bounds;CALayer *ancestor=layer;
            visible=!layer.hidden && layer.opacity>0;
            for (NSUInteger i=0;ancestor && i<32;i++) {
                if (ancestor.hidden || ancestor.opacity<=0) visible=NO;
                if (ancestor.masksToBounds) {
                    CGRect overlap=CGRectIntersection(rect,ancestor.bounds);
                    if (CGRectIsNull(overlap) || CGRectIsEmpty(overlap)) visible=NO;
                    if (!CGRectContainsRect(ancestor.bounds,rect)) clipping++;
                }
                CALayer *next=ancestor.superlayer;
                if (!next) break;
                rect=[ancestor convertRect:rect toLayer:next];ancestor=next;
            }
            CGRect windowRect=[layer convertRect:layer.bounds toLayer:host.window.layer];
            if (!CGRectIntersectsRect(windowRect,host.window.bounds)) visible=NO;
        }
        unclipped+=visible && clipping==0;
        if (geometry.count<8) [geometry addObject:@{
            @"frame_x":@(positive ? frame.origin.x : 0),@"frame_y":@(positive ? frame.origin.y : 0),
            @"frame_width":@(positive ? frame.size.width : 0),@"frame_height":@(positive ? frame.size.height : 0),
            @"render_parent_present":@(renderParent!=nil || labelMounted),
            @"native_text_supernode_present":@(renderParent!=nil),@"native_text_node_loaded":@(nativeLoaded),
            @"button_node_loaded":@(RVBool(node,"isNodeLoaded")),@"button_layer_backed":@(RVBool(node,"isLayerBacked")),
            @"node_context_matches_button":@(RVObject(node,"nodeContext")==RVObject(text,"nodeContext")),
            @"icon_gap_measured":@(gapMeasured),@"icon_text_gap_points":@(actualGap),@"icon_frame_x":@(iconFrame.origin.x),@"icon_frame_width":@(iconFrame.size.width),
            @"outer_leading_inset":@(state.leadingInset),@"outer_trailing_inset":@(state.trailingInset),@"owned_end_margin":@(state.endMargin),@"outer_edge_gate":state.edgeGate ?: @"not_measured",
            @"spacing_points":@(state.spacingPoints),@"spacing_gate":state.spacingGate ?: @"unknown",
            @"watch_suspended":@(state.suspended),@"suspension_count":@(state.suspensionCount),@"resume_count":@(state.resumeCount),
            @"button_width":@(labelMounted ? state.renderLabel.superview.bounds.size.width : 0),
            @"mount_mode":state.mountMode ?: @"not_mounted",@"mount_gate":state.mountGate ?: @"not_attempted",@"mount_attempts":@(state.mountAttempts),
            @"label_superview_is_button":@(labelMounted && RVBool(node,"isNodeLoaded") && !RVBool(node,"isLayerBacked") && state.renderLabel.superview==RVObject(node,"view")),
            @"label_fits_button_bounds":@(labelMounted && CGRectContainsRect(state.renderLabel.superview.bounds,state.renderLabel.frame)),
            @"render_parent_class":labelMounted ? NSStringFromClass([state.renderLabel.superview class]) : renderParent ? NSStringFromClass([renderParent class]) : @"none",
            @"render_surface_class":labelMounted ? @"UILabel" : NSStringFromClass([text class]),@"text_superlayer_present":@(layer.superlayer!=nil),@"node_loaded":@(isLoaded),@"render_layer_in_window":@(onWindow),
            @"button_class":NSStringFromClass([node class]),@"text_class":NSStringFromClass([text class]),
            @"button_host_in_window":@(RVElementLoadedView(node).window!=nil),
            @"video_matches_current":@([state.video isEqual:RVDislikeVideo]),
            @"owned_node_is_yoga_child":@([RVObject(node,"yogaChildren") containsObject:text]),
            @"owned_node_is_render_child":@([RVObject(node,"subnodes") containsObject:text]),
            @"yoga_parent_present":@(RVObject(text,"yogaParent")!=nil),
            @"yoga_parent_is_button":@(RVObject(text,"yogaParent")==node),
            @"button_yoga_root_class":RVObject(node,"yogaRoot") ? NSStringFromClass([RVObject(node,"yogaRoot") class]) : @"none",
            @"root_loaded":@(RVBool(state.layoutRoot,"isNodeLoaded")),
            @"root_host_in_window":@(RVElementLoadedView(state.layoutRoot).window!=nil),
            @"root_invalidation_requests":@(state.rootInvalidations),@"root_layout_passes":@(state.rootLayoutPasses),
            @"text_load_callbacks":@(state.textLoadCallbacks),@"text_display_callbacks":@(state.textDisplayCallbacks),
            @"raster_contents_present":@(raster),@"text_layer_hidden":@(layer.hidden),@"text_layer_opacity":@(layer ? layer.opacity : 0),
            @"button_flattenable":@(RVBool(node,"isFlattenable")),@"text_flattenable":@(RVBool(text,"isFlattenable")),
            @"clipping_ancestors":@(clipping),@"geometrically_visible":@(visible),@"color_source":state.colorSource ?: @"unknown"}];
    }
    NSDictionary *counts;NSUInteger images;
    @synchronized(UIImage.class) { counts=RVElementDislikeCounts.copy ?: @{};images=RVElementVoteImages.count; }
    UIViewController *watch=RVCurrentWatch;
    BOOL watchLoaded=[watch isKindOfClass:UIViewController.class] && watch.isViewLoaded;
    return @{@"schema":@9,@"counts":counts,@"owned_text_nodes":@(attached),@"laid_out_text_nodes":@(laidOut),@"tagged_images":@(images),
        @"render_mounted_nodes":@(mounted),@"loaded_text_nodes":@(loaded),@"render_layers_in_window":@(inWindow),@"unclipped_text_nodes":@(unclipped),@"render_geometry":geometry,@"raster_content_nodes":@(contents),@"display_finished_nodes":@(displayFinished),
        @"watch_bound":@(watch!=nil),@"watch_loaded":@(watchLoaded),@"count_matches_playback":@(RVDislikeHasCount()),
        @"watch_player_matches":@(RVDislikeWatchPlayerMatches(watch)),@"watch_player_pointer_matches":@(RVObject(watch,"playerViewController")==RVCurrentPlayer && RVCurrentPlayer!=nil),
        @"last_gate":RVElementLastGate ?: @"not_observed",@"native_like_arguments_preserved":@YES,
        @"strategy":@"flattening_aware_native_dislike_text_with_watch_binding",@"semantic_binding_probe":RVElementBindingProbe ?: @[],@"like_icon_required":@NO,@"device_verified":@NO};
}
static void RVInstallElementDislikes(void) {
    RVHook(@"YTWatchViewController",@"viewWillAppear:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id watch,BOOL animated) {
            if (NSThread.isMainThread && RVDislikeWatchPlayerMatches(watch)) RVCurrentWatch=watch;
            ((void(*)(id,SEL,BOOL))original)(watch,sel,animated);
            RVElementWatchTransition(watch,@"watch_will_appear");
        };
    });
    for (NSString *name in @[@"playerViewModifierDidBeginGesture:",@"playerViewModifierDidEndGesture:"]) RVHook(@"YTWatchViewController",name,@"v@:@",NO,^id(IMP original,SEL sel) {
        BOOL beginning=[name isEqual:@"playerViewModifierDidBeginGesture:"];
        return ^(id watch,id modifier) {
            ((void(*)(id,SEL,id))original)(watch,sel,modifier);
            RVElementWatchTransition(watch,beginning ? @"watch_gesture_began" : @"watch_gesture_ended");
        };
    });
    RVHook(@"YTWatchViewController",@"setPlayerViewController:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id watch,id player) {
            ((void(*)(id,SEL,id))original)(watch,sel,player);
            __weak id weakWatch=watch;
            dispatch_async(dispatch_get_main_queue(),^{ RVElementWatchTransition(weakWatch,@"watch_player_changed"); });
        };
    });
    RVHook(@"YTWatchViewController",@"watchViewDidLayoutSubviews:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id watch,id view) {
            ((void(*)(id,SEL,id))original)(watch,sel,view);
            if (!NSThread.isMainThread || ![watch isKindOfClass:UIViewController.class]) return;
            UIViewController *controller=watch;
            for (id parent in RVElementDislikeParents.allObjects) {
                RVElementDislikeState *state=objc_getAssociatedObject(parent,RVElementDislikeKey);
                if (state.watch==controller) RVElementMountOwnedText(parent,state);
            }
            id player=RVObject(watch,"playerViewController");
            NSValue *owner=[NSValue valueWithNonretainedObject:player];
            BOOL inWindow=controller.isViewLoaded && controller.view.window;
            if (![objc_getAssociatedObject(watch,RVElementWatchOwnerKey) isEqual:owner] ||
                [objc_getAssociatedObject(watch,RVElementWatchWindowKey) boolValue]!=inWindow) {
                objc_setAssociatedObject(watch,RVElementWatchOwnerKey,owner,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                objc_setAssociatedObject(watch,RVElementWatchWindowKey,@(inWindow),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                RVElementWatchTransition(watch,@"watch_layout_ownership_changed");
            }
        };
    });
    // Passive callbacks, restricted to owned text and its recorded Yoga roots.
    RVHook(@"ASDisplayNode",@"__layout",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id root) {
            ((void(*)(id,SEL))original)(root,sel);
            if (!NSThread.isMainThread) return;
            NSHashTable *tracked=objc_getAssociatedObject(root,RVElementRootTraceKey);
            for (RVElementDislikeState *state in tracked.allObjects) if (state.textNode && state.layoutRoot==root && [state.video isEqual:RVDislikeVideo]) {
                state.rootLayoutPasses++;RVElementDislikeEvent(@"owned_root_layout_finished");RVElementMountOwnedText(state.button,state);RVElementBalanceEdges(state.button,state);
            }
        };
    });
    for (NSArray *contract in @[@[@"ASTextNode",@"didLoad"],@[@"ASDisplayNode",@"displayDidFinish"]]) RVHook(contract[0],contract[1],@"v@:",NO,^id(IMP original,SEL sel) {
        BOOL drawing=[contract[1] isEqual:@"displayDidFinish"];
        return ^(id node) {
            ((void(*)(id,SEL))original)(node,sel);
            if (objc_getAssociatedObject(node,RVElementTextTraceKey)) RVElementTextTrace(node,drawing);
        };
    });
    RVHook(@"YTELMResourceLoaderImpl",@"imageWithName:color:bundleId:",@"@@:@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id loader,id name,id color,id bundle) {
            id image=((id(*)(id,SEL,id,id,id))original)(loader,sel,name,color,bundle);
            RVElementDislikeEvent(@"named_resource_requested");
            int kind=RVElementNamedIconKind(name);if (kind) { RVElementRememberImage(image,kind);RVElementQueueRefresh(); }
            return image;
        };
    });
    RVHook(@"YTIconResolverImpl",@"imageForIcon:color:",@"@@:i@",NO,^id(IMP original,SEL sel) {
        return ^id(id resolver,int icon,id color) {
            id image=((id(*)(id,SEL,int,id))original)(resolver,sel,icon,color);
            RVElementDislikeEvent(@"resolver_requested");
            // Native toolbar resources prove Delhi outline/fill vote icon IDs.
            int kind=RVElementIconKind(icon);
            if (kind) { RVElementResolvedIcon=kind;RVElementRememberImage(image,kind); }return image;
        };
    });
    RVHook(@"CompositeIconController",@"imageForIcon:",@"@@:r^v",NO,^id(IMP original,SEL sel) {
        return ^id(id controller,const void *icon) {
            // The compositor may tint into a new UIImage. Carry only the proven
            // resolver identity through this synchronous call, not C++ memory.
            int previous=RVElementResolvedIcon;RVElementResolvedIcon=0;
            RVElementDislikeEvent(@"compositor_requested");
            id image=((id(*)(id,SEL,const void *))original)(controller,sel,icon);
            int kind=RVElementResolvedIcon;RVElementResolvedIcon=previous;RVElementRememberImage(image,kind);return image;
        };
    });
    RVHook(@"ELMContainerNode",@"layoutDidFinish",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id node) { ((void(*)(id,SEL))original)(node,sel);RVElementDislikeEvent(@"container_layout");
            if (NSThread.isMainThread) { RVElementDislikeObserve(node);RVDislikeCaptureCheckpoint(@"container_layout_finished"); }
            else RVElementQueueRefresh(); };
    });
    // ASNetworkImageNode has its own setter path. Hook the concrete ELM override too.
    for (NSString *cls in @[@"ASImageNode",@"ELMImageNode"]) RVHook(cls,@"setImage:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id node,id image) {
            ((void(*)(id,SEL,id))original)(node,sel,image);
            RVElementDislikeEvent(@"image_assigned");
            if (image && objc_getAssociatedObject(image,RVElementImageKindKey)) RVElementQueueRefresh();
        };
    });
}
