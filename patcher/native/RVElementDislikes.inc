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
    if (!NSThread.isMainThread || !RVEnabled(@"diagnostics")) return;
    if (!RVElementDislikeCounts) RVElementDislikeCounts=[NSMutableDictionary dictionary];
    RVElementDislikeCounts[name]=@([RVElementDislikeCounts[name] unsignedLongLongValue]+1);
}
static int RVElementIconKind(int icon) {
    static NSDictionary *kinds;static dispatch_once_t once;
    dispatch_once(&once,^{
        Class providerClass=NSClassFromString(@"YTAppIconSupportProviderImpl");
        id provider=[providerClass new];id map=RVObject(provider,"delhiResourceMap");
        NSMutableDictionary *known=[NSMutableDictionary dictionary];
        if ([map isKindOfClass:NSDictionary.class] && [map count]<=4096) for (id key in map) {
            id name=map[key];if (![key isKindOfClass:NSNumber.class] || ![name isKindOfClass:NSString.class]) continue;
            BOOL down=[name hasPrefix:@"YticonOutlineThumbDownS"] || [name hasPrefix:@"YticonFillThumbDownS"];
            BOOL up=[name hasPrefix:@"YticonOutlineThumbUpS"] || [name hasPrefix:@"YticonFillThumbUpS"];
            if (down || up) known[key]=down ? @1 : @2;
        }
        // Toolbar resource calls independently prove these four native IDs.
        known[@0x250]=@1;known[@0x238]=@1;known[@0x251]=@2;known[@0x239]=@2;
        kinds=known.copy;
    });return [kinds[@(icon)] intValue];
}
static void RVElementRememberImage(id value,int kind) {
    if (!kind || ![value isKindOfClass:UIImage.class]) return;
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
    @synchronized(UIImage.class) {
        for (NSDictionary *entry in RVElementVoteImages.reverseObjectEnumerator) {
            UIImage *known=entry[@"image"];
            if (image==known || (known.CGImage && known.CGImage==[image CGImage])) return [entry[@"kind"] intValue];
        }
    }return 0;
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
    for (NSUInteger i=0;node && i<12;i++,node=RVObject(node,"supernode")) {
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
    id controller=RVObject(pair,"nodeController"),component=RVObject(controller,"owningComponent"),bound=nil;
    for (NSUInteger i=0;component && i<6;i++,component=RVObject(component,"owningComponent")) {
        if (RVElementBinding(component,RVDislikeVideo).role) { bound=component;break; }
    }
    if (!bound) { RVElementDislikeEvent(@"semantic_role_missing");RVElementDislikeRemove(parent,state);return; }
    RVElementDislikeEvent(@"semantic_role_matched");
    id element=RVObject(bound,"element");NSString *video=RVDislikeVideo;
    BOOL boundAlready=state && state.component==bound && state.element==element && [state.video isEqual:video];
    if (!boundAlready && !RVElementBinding(bound,video).videoMatches) {
        RVElementDislikeEvent(@"video_binding_missing");RVElementDislikeRemove(parent,state);return;
    }
    if (!RVCan(parent,"appendYogaChild:",@"v@:@") || !RVCan(parent,"removeYogaChild:",@"v@:@") || !RVBool(parent,"yoga")) return;
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
        RVSetObject(node,"setAttributedText:",display);
        RVSetObject(node,"setAccessibilityIdentifier:",@"rvport.dislike.count.element");
        id style=RVObject(node,"style");
        if (RVCan(style,"setSpacingBefore:",@"v@:d")) ((void(*)(id,SEL,double))objc_msgSend)(style,sel_registerName("setSpacingBefore:"),8);
        state.textNode=node;RVSetObject(parent,"appendYogaChild:",node);
        if (!RVElementDislikeParents) RVElementDislikeParents=[NSHashTable weakObjectsHashTable];[RVElementDislikeParents addObject:parent];
        RVElementDislikeEvent(@"owned_node_attached");
    } else {
        id existing=RVObject(state.textNode,"attributedText");
        if (![existing isKindOfClass:NSAttributedString.class] || ![existing isEqualToAttributedString:display]) RVSetObject(state.textNode,"setAttributedText:",display);
        if (![RVElementChildren(parent) containsObject:state.textNode]) RVSetObject(parent,"appendYogaChild:",state.textNode);
    }
    RVSetObject(state.textNode,"setAccessibilityLabel:",[NSString stringWithFormat:@"Return YouTube Dislike estimate: %@ dislikes",[NSNumberFormatter localizedStringFromNumber:RVDislikeCount numberStyle:NSNumberFormatterDecimalStyle]]);
    state.text=text;state.updating=NO;
}
static void RVElementDislikeObserve(id root) {
    if (!NSThread.isMainThread || !RVDislikeHasCount()) return;
    UIView *view=RVElementLoadedView(root);if (!RVDislikeWatchAction(view)) return;
    for (id image in RVElementSubtree(root)) {
        if (RVElementImageKind(image)!=1) continue;
        RVElementDislikeEvent(@"dislike_icon_seen");
        id branch=image;
        for (NSUInteger level=0;branch && level<8;level++,branch=RVObject(branch,"supernode")) {
            id pair=RVObject(branch,"supernode");if (!pair) break;
            NSUInteger up=0,down=0;
            for (id child in RVElementSubtree(pair)) { int kind=RVElementImageKind(child);up+=kind==2;down+=kind==1; }
            if (!up) continue;
            if (up==1 && down==1 && branch!=image) { RVElementDislikeEvent(@"native_pair_seen");RVElementDislikeApply(branch,pair); }
            break;
        }
    }
}
static void RVRefreshElementDislikes(void) {
    if (!NSThread.isMainThread) return;
    for (id parent in RVElementDislikeParents.allObjects) {
        RVElementDislikeState *state=objc_getAssociatedObject(parent,RVElementDislikeKey);
        if (!RVDislikeHasCount() || ![state.video isEqual:RVDislikeVideo]) RVElementDislikeRemove(parent,state);
    }
    UIViewController *watch=RVCurrentWatch;if (![watch isKindOfClass:UIViewController.class] || !watch.isViewLoaded) return;
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
static NSDictionary *RVElementDislikeReport(void) {
    NSUInteger attached=0;for (id node in RVElementDislikeParents.allObjects) if (((RVElementDislikeState *)objc_getAssociatedObject(node,RVElementDislikeKey)).textNode) attached++;
    return @{@"schema":@1,@"counts":RVElementDislikeCounts.copy ?: @{},@"owned_text_nodes":@(attached),
        @"strategy":@"video_bound_native_element_pair_yoga_child",@"device_verified":@NO};
}
static void RVInstallElementDislikes(void) {
    RVHook(@"YTIconResolverImpl",@"imageForIcon:color:",@"@@:i@",NO,^id(IMP original,SEL sel) {
        return ^id(id resolver,int icon,id color) {
            id image=((id(*)(id,SEL,int,id))original)(resolver,sel,icon,color);
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
            id image=((id(*)(id,SEL,const void *))original)(controller,sel,icon);
            int kind=RVElementResolvedIcon;RVElementResolvedIcon=previous;RVElementRememberImage(image,kind);return image;
        };
    });
    RVHook(@"ELMContainerNode",@"layoutDidFinish",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id node) { ((void(*)(id,SEL))original)(node,sel);RVElementDislikeObserve(node); };
    });
    RVHook(@"ASImageNode",@"setImage:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id node,id image) {
            ((void(*)(id,SEL,id))original)(node,sel,image);
            if (!NSThread.isMainThread || !RVElementImageKind(node) || RVElementRefreshQueued) return;
            RVElementRefreshQueued=YES;dispatch_async(dispatch_get_main_queue(),^{ RVElementRefreshQueued=NO;RVRefreshElementDislikes(); });
        };
    });
}
