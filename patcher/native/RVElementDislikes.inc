// SPDX-License-Identifier: MIT
// Element-based watch controls use ASDisplayNode/Yoga, not the slim UIKit row.
// Keep the native icons, commands, hit targets and backgrounds. Add one owned
// text node only to a video-bound, semantically identified native action pair.
static const void *RVElementDislikeKey=&RVElementDislikeKey;
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
@property(nonatomic) BOOL updating;
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
    state.component=bound;state.element=element;state.video=video;
    NSDictionary *attributes=nil;
    for (id node in RVElementSubtree(pair)) {
        if (node==state.textNode) continue;
        id native=RVObject(node,"attributedText");
        if ([native isKindOfClass:NSAttributedString.class] && [native length]) { attributes=[native attributesAtIndex:0 effectiveRange:NULL];break; }
    }
    UIFont *font=[attributes[NSFontAttributeName] isKindOfClass:UIFont.class] ? attributes[NSFontAttributeName] :
        [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody] scaledFontForFont:[UIFont systemFontOfSize:14 weight:UIFontWeightMedium]];
    UIColor *color=[attributes[NSForegroundColorAttributeName] isKindOfClass:UIColor.class] ? attributes[NSForegroundColorAttributeName] : UIColor.labelColor;
    NSString *text=RVDislikeCompactCount(RVDislikeCount);
    NSAttributedString *display=[[NSAttributedString alloc] initWithString:text attributes:@{NSFontAttributeName:font,NSForegroundColorAttributeName:color}];
    state.updating=YES;
    if (!state.textNode) {
        Class textClass=NSClassFromString(@"ASTextNode");id node=[textClass new];
        if (!RVCan(node,"setAttributedText:",@"v@:@")) { state.updating=NO;return; }
        RVVoid(node,"enableYoga");
        RVSetObject(node,"setAttributedText:",display);
        RVSetObject(node,"setAccessibilityIdentifier:",@"rvport.dislike.count.element");
        id style=RVObject(node,"style");
        if (RVCan(style,"setSpacingBefore:",@"v@:d")) ((void(*)(id,SEL,double))objc_msgSend)(style,sel_registerName("setSpacingBefore:"),8);
        state.textNode=node;RVSetObject(parent,"appendYogaChild:",node);
        RVVoid(parent,"setNeedsLayout");RVVoid(RVObject(parent,"yogaRoot"),"setNeedsLayout");
        if (!RVElementDislikeParents) RVElementDislikeParents=[NSHashTable weakObjectsHashTable];[RVElementDislikeParents addObject:parent];
        RVElementDislikeEvent(@"owned_node_attached");
    } else {
        id existing=RVObject(state.textNode,"attributedText");
        if (![existing isKindOfClass:NSAttributedString.class] || ![existing isEqualToAttributedString:display]) RVSetObject(state.textNode,"setAttributedText:",display);
        if (![RVElementChildren(parent) containsObject:state.textNode]) RVSetObject(parent,"appendYogaChild:",state.textNode);
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
    NSUInteger attached=0,laidOut=0;
    for (id node in RVElementDislikeParents.allObjects) {
        id text=((RVElementDislikeState *)objc_getAssociatedObject(node,RVElementDislikeKey)).textNode;
        if (!text) continue;attached++;
        if (RVCan(text,"frame",@"{CGRect={CGPoint=dd}{CGSize=dd}}@:")) {
            CGRect frame=((CGRect(*)(id,SEL))objc_msgSend)(text,sel_registerName("frame"));
            if (isfinite(frame.size.width) && isfinite(frame.size.height) && frame.size.width>0 && frame.size.height>0) laidOut++;
        }
    }
    NSDictionary *counts;NSUInteger images;
    @synchronized(UIImage.class) { counts=RVElementDislikeCounts.copy ?: @{};images=RVElementVoteImages.count; }
    UIViewController *watch=RVCurrentWatch;
    BOOL watchLoaded=[watch isKindOfClass:UIViewController.class] && watch.isViewLoaded;
    return @{@"schema":@3,@"counts":counts,@"owned_text_nodes":@(attached),@"laid_out_text_nodes":@(laidOut),@"tagged_images":@(images),
        @"watch_bound":@(watch!=nil),@"watch_loaded":@(watchLoaded),@"count_matches_playback":@(RVDislikeHasCount()),
        @"watch_player_matches":@(RVObject(watch,"playerViewController")==RVCurrentPlayer && RVCurrentPlayer!=nil),
        @"last_gate":RVElementLastGate ?: @"not_observed",@"native_like_arguments_preserved":@YES,
        @"strategy":@"independent_dislike_yoga_button_with_active_watch_player",@"like_icon_required":@NO,@"device_verified":@NO};
}
static void RVInstallElementDislikes(void) {
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
            if (NSThread.isMainThread) RVElementDislikeObserve(node);else RVElementQueueRefresh(); };
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
