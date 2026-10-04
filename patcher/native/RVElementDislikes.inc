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
@end
@implementation RVElementBindingState @end
static RVElementBindingState *RVElementBinding(id component,NSString *video) {
    id element=RVObject(component,"element");
    RVElementBindingState *state=objc_getAssociatedObject(component,RVElementBindingKey);
    if (state && state.element==element && [state.video isEqual:video]) return state;
    state=[RVElementBindingState new];state.element=element;state.video=video;
    NSArray *roles=@[@"video_action_bar",@"segmented_like_dislike",@"segmentedLikeDislike",@"SegmentedLikeDislikeButton"];
    id key=RVObject(component,"key");
    if ([key isKindOfClass:NSString.class] && [key length]<=1024)
        for (NSString *role in roles) if ([key rangeOfString:role].location!=NSNotFound) { state.role=YES;break; }
    // ELMElement owns a C++ protobuf. Its native Objective-C diagnostic bridge
    // avoids fixed C++ offsets. Cache only boolean checks, never model text.
    Class cls=NSClassFromString(@"ELMElement");
    id text=cls && [element isKindOfClass:cls] ? RVObject(element,"protoText") : nil;
    if ([text isKindOfClass:NSString.class] && [text length]<=131072) {
        for (NSString *role in roles) if ([text rangeOfString:role].location!=NSNotFound) { state.role=YES;break; }
        static NSRegularExpression *ids;static dispatch_once_t once;
        dispatch_once(&once,^{ ids=[NSRegularExpression regularExpressionWithPattern:@"(?:video_id|videoId|videoID)\\s*:\\s*\"([A-Za-z0-9_-]{11})\"" options:0 error:nil]; });
        NSArray *matches=[ids matchesInString:text options:0 range:NSMakeRange(0,[text length])];
        state.explicitVideo=matches.count>0;
        state.videoMatches=matches.count>0 && matches.count<=32;
        for (NSTextCheckingResult *match in matches) if (![[text substringWithRange:[match rangeAtIndex:1]] isEqual:video]) { state.videoMatches=NO;break; }
    }
    objc_setAssociatedObject(component,RVElementBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);return state;
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
static void RVElementDislikeRemove(id parent,RVElementDislikeState *state) {
    if (!state.textNode || state.updating) return;
    state.updating=YES;
    RVSetObject(parent,"removeYogaChild:",state.textNode);
    if (RVCan(state.textNode,"removeFromSupernode",@"v@:")) ((void(*)(id,SEL))objc_msgSend)(state.textNode,sel_registerName("removeFromSupernode"));
    state.textNode=nil;state.text=nil;state.updating=NO;
}
static void RVElementDislikeApply(id parent,id pair) {
    RVElementDislikeState *state=objc_getAssociatedObject(parent,RVElementDislikeKey);
    if (state.updating) return;
    UIView *view=RVElementLoadedView(parent);
    if (!RVDislikeHasCount() || !RVOrdinaryContent(RVCurrentPlayback) || !RVDislikeWatchAction(view)) { RVElementDislikeRemove(parent,state);return; }
    id bound=nil,ancestor=pair;
    for (NSUInteger level=0;ancestor && !bound && level<12;level++,ancestor=RVElementParent(ancestor)) {
        id component=RVObject(RVObject(ancestor,"nodeController"),"owningComponent");
        for (NSUInteger i=0;component && i<6;i++,component=RVObject(component,"owningComponent")) {
            if (RVElementBinding(component,RVDislikeVideo).role) { bound=component;break; }
        }
    }
    if (!bound) { RVElementLastGate=@"semantic_role_missing";RVElementDislikeEvent(@"semantic_role_missing");RVElementDislikeRemove(parent,state);return; }
    RVElementDislikeEvent(@"semantic_role_matched");
    id element=RVObject(bound,"element");NSString *video=RVDislikeVideo;
    BOOL boundAlready=state && state.component==bound && state.element==element && [state.video isEqual:video];
    RVElementBindingState *binding=RVElementBinding(bound,video);
    id watchPlayer=RVObject(RVCurrentWatch,"playerViewController");
    BOOL watchMatches=watchPlayer==RVCurrentPlayer && [RVObject(watchPlayer,"contentVideoID") isEqual:video];
    BOOL videoMatches=binding.explicitVideo ? binding.videoMatches : watchMatches;
    if (!watchMatches || (!boundAlready && !videoMatches)) {
        RVElementLastGate=@"video_binding_missing";RVElementDislikeEvent(@"video_binding_missing");RVElementDislikeRemove(parent,state);return;
    }
    RVElementDislikeEvent(binding.explicitVideo ? @"element_video_bound" : @"watch_player_video_bound");
    if (!RVCan(parent,"appendYogaChild:",@"v@:@") || !RVCan(parent,"removeYogaChild:",@"v@:@") || !RVBool(parent,"yoga")) {
        RVElementLastGate=@"yoga_contract_missing";RVElementDislikeEvent(@"yoga_contract_missing");return;
    }
    if (!state) { state=[RVElementDislikeState new];objc_setAssociatedObject(parent,RVElementDislikeKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
    if (![state.video isEqual:video]) { state.rootInvalidations=0;state.rootLayoutPasses=0;state.textLoadCallbacks=0;state.textDisplayCallbacks=0; }
    state.component=bound;state.element=element;state.video=video;
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
        id style=RVObject(node,"style");
        if (RVCan(style,"setSpacingBefore:",@"v@:d")) ((void(*)(id,SEL,double))objc_msgSend)(style,sel_registerName("setSpacingBefore:"),8);
        if (RVCan(node,"setUserInteractionEnabled:",@"v@:B")) ((void(*)(id,SEL,BOOL))objc_msgSend)(node,sel_registerName("setUserInteractionEnabled:"),NO);
        objc_setAssociatedObject(node,RVElementTextTraceKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        state.textNode=node;RVSetObject(parent,"appendYogaChild:",node);
        RVVoid(node,"setNeedsDisplay");
        RVElementInvalidateRoot(parent,state);
        if (!RVElementDislikeParents) RVElementDislikeParents=[NSHashTable weakObjectsHashTable];[RVElementDislikeParents addObject:parent];
        RVElementDislikeEvent(@"owned_node_attached");
    } else {
        id existing=RVObject(state.textNode,"attributedText");
        if (![existing isKindOfClass:NSAttributedString.class] || ![existing isEqualToAttributedString:display]) RVSetObject(state.textNode,"setAttributedText:",display);
        if (![RVElementChildren(parent) containsObject:state.textNode]) {
            RVSetObject(parent,"appendYogaChild:",state.textNode);
            RVElementInvalidateRoot(parent,state);
        }
    }
    RVSetObject(state.textNode,"setAccessibilityLabel:",[NSString stringWithFormat:@"Return YouTube Dislike estimate: %@ dislikes",[NSNumberFormatter localizedStringFromNumber:RVDislikeCount numberStyle:NSNumberFormatterDecimalStyle]]);
    state.text=text;state.updating=NO;RVElementLastGate=@"owned_count_attached";
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
        if (!RVDislikeHasCount() || ![state.video isEqual:RVDislikeVideo] || !RVDislikeWatchAction(RVElementLoadedView(parent))) RVElementDislikeRemove(parent,state);
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
static NSDictionary *RVElementDislikeReport(void) {
    NSUInteger attached=0,laidOut=0,mounted=0,loaded=0,inWindow=0,unclipped=0,contents=0,displayFinished=0;
    NSMutableArray *geometry=[NSMutableArray array];
    for (id node in RVElementDislikeParents.allObjects) {
        RVElementDislikeState *state=objc_getAssociatedObject(node,RVElementDislikeKey);id text=state.textNode;
        if (!text) continue;attached++;
        CGRect frame=CGRectZero;
        if (RVCan(text,"frame",@"{CGRect={CGPoint=dd}{CGSize=dd}}@:")) frame=((CGRect(*)(id,SEL))objc_msgSend)(text,sel_registerName("frame"));
        BOOL positive=isfinite(frame.origin.x) && isfinite(frame.origin.y) && isfinite(frame.size.width) && isfinite(frame.size.height) && frame.size.width>0 && frame.size.height>0;
        laidOut+=positive;id renderParent=RVObject(text,"supernode");mounted+=renderParent!=nil;
        BOOL isLoaded=RVBool(text,"isNodeLoaded");loaded+=isLoaded;
        UIView *host=renderParent ? RVElementLoadedView(renderParent) : nil;
        CALayer *layer=isLoaded ? RVObject(text,"layer") : nil;
        BOOL onWindow=NO;
        if (host.window && [layer isKindOfClass:CALayer.class]) {
            CALayer *ancestor=layer;
            for (NSUInteger depth=0;ancestor && depth<64;depth++,ancestor=ancestor.superlayer)
                if (ancestor==host.window.layer) { onWindow=YES;break; }
        }
        inWindow+=onWindow;
        BOOL raster=[layer isKindOfClass:CALayer.class] && layer.contents!=nil;
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
            @"render_parent_present":@(renderParent!=nil),
            @"render_parent_class":renderParent ? NSStringFromClass([renderParent class]) : @"none",@"text_superlayer_present":@(layer.superlayer!=nil),@"node_loaded":@(isLoaded),@"render_layer_in_window":@(onWindow),
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
    return @{@"schema":@5,@"counts":counts,@"owned_text_nodes":@(attached),@"laid_out_text_nodes":@(laidOut),@"tagged_images":@(images),
        @"render_mounted_nodes":@(mounted),@"loaded_text_nodes":@(loaded),@"render_layers_in_window":@(inWindow),@"unclipped_text_nodes":@(unclipped),@"render_geometry":geometry,@"raster_content_nodes":@(contents),@"display_finished_nodes":@(displayFinished),
        @"watch_bound":@(watch!=nil),@"watch_loaded":@(watchLoaded),@"count_matches_playback":@(RVDislikeHasCount()),
        @"watch_player_matches":@(RVObject(watch,"playerViewController")==RVCurrentPlayer && RVCurrentPlayer!=nil),
        @"last_gate":RVElementLastGate ?: @"not_observed",@"native_like_arguments_preserved":@YES,
        @"strategy":@"flattening_aware_native_dislike_text_with_watch_binding",@"like_icon_required":@NO,@"device_verified":@NO};
}
static void RVInstallElementDislikes(void) {
    // Passive callbacks, restricted to owned text and its recorded Yoga roots.
    RVHook(@"ASDisplayNode",@"__layout",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id root) {
            ((void(*)(id,SEL))original)(root,sel);
            if (!NSThread.isMainThread || !RVEnabled(@"diagnostics")) return;
            NSHashTable *tracked=objc_getAssociatedObject(root,RVElementRootTraceKey);
            for (RVElementDislikeState *state in tracked.allObjects) if (state.textNode && state.layoutRoot==root && [state.video isEqual:RVDislikeVideo]) {
                state.rootLayoutPasses++;RVElementDislikeEvent(@"owned_root_layout_finished");
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
