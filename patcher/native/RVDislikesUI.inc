// SPDX-License-Identifier: MIT
// An inline accessory owned by the native watch action, never the video overlay.
static void RVRefreshElementDislikes(void);
static const void *RVDislikeInlineKey=&RVDislikeInlineKey;
static NSHashTable<UIView *> *RVDislikeActions;
static NSString *RVNativeDislikeVideoForAction(id action);
@interface RVDislikeInlineState : NSObject
@property(nonatomic,strong) UILabel *label;
@property(nonatomic,copy) NSString *video;
@property(nonatomic) BOOL requestedLayout;
@end
@implementation RVDislikeInlineState @end

static BOOL RVDislikeWatchAction(UIView *view) {
    if (!NSThread.isMainThread || ![RVCurrentWatch isKindOfClass:UIViewController.class]) return NO;
    UIViewController *watch=RVCurrentWatch;
    if (!watch.isViewLoaded || !view.window || ![view isDescendantOfView:watch.view]) return NO;
    UIView *overlay=RVPlayerOverlay();
    return !overlay || ![view isDescendantOfView:overlay];
}
static BOOL RVDislikeFindAction(UIView *view,NSDictionary **attributes) {
    // Exact stable native ID excludes comments, Shorts, ads, and localized text.
    NSMutableArray *views=[NSMutableArray arrayWithObject:view],*nodes=[NSMutableArray array];
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    BOOL found=NO;
    for (NSUInteger i=0;i<views.count && i<256;i++) {
        UIView *candidate=views[i];
        if ([candidate.accessibilityIdentifier isEqual:@"id.video.dislike.button"]) found=YES;
        id node=RVObject(candidate,"asyncdisplaykit_node");if (node) [nodes addObject:node];
        for (UIView *child in candidate.subviews) if (views.count<256 && ![child.accessibilityIdentifier isEqual:@"rvport.dislike.count"]) [views addObject:child];
    }
    for (NSUInteger i=0;i<nodes.count && i<256;i++) {
        id node=nodes[i];if ([seen containsObject:node]) continue;[seen addObject:node];
        if ([RVObject(node,"accessibilityIdentifier") isEqual:@"id.video.dislike.button"]) found=YES;
        id text=RVObject(node,"attributedText");
        if (attributes && !*attributes && [text isKindOfClass:NSAttributedString.class] && [text length]) {
            NSDictionary *native=[text attributesAtIndex:0 effectiveRange:NULL];NSMutableDictionary *style=[NSMutableDictionary dictionary];
            if ([native[NSFontAttributeName] isKindOfClass:UIFont.class]) style[NSFontAttributeName]=native[NSFontAttributeName];
            if ([native[NSForegroundColorAttributeName] isKindOfClass:UIColor.class]) style[NSForegroundColorAttributeName]=native[NSForegroundColorAttributeName];
            if (style.count) *attributes=style;
        }
        id children=RVObject(node,"subnodes");
        if ([children isKindOfClass:NSArray.class]) for (id child in children) if (nodes.count<256) [nodes addObject:child];
    }return found;
}
static NSString *RVDislikeCompactCount(NSNumber *count) {
    double value=count.doubleValue;NSString *suffix=@"";
    if (value>=1e9) { value/=1e9;suffix=@"B"; }
    else if (value>=1e6) { value/=1e6;suffix=@"M"; }
    else if (value>=1e3) { value/=1e3;suffix=@"K"; }
    NSNumberFormatter *format=[NSNumberFormatter new];format.numberStyle=NSNumberFormatterDecimalStyle;
    format.maximumFractionDigits=suffix.length ? 1 : 0;
    return [[format stringFromNumber:@(value)] stringByAppendingString:suffix];
}
static BOOL RVDislikeHasCount(void) {
    return RVEnabled(@"return_dislikes") && RVDislikeCount &&
        [RVDislikeVideo isEqual:RVObject(RVCurrentPlayback,"contentVideoID")];
}
static RVDislikeInlineState *RVDislikeUpdateAction(UIView *view) {
    if (!NSThread.isMainThread) return nil;
    RVDislikeInlineState *state=objc_getAssociatedObject(view,RVDislikeInlineKey);
    // The separate verified native-text adapter owns legacy formatted labels.
    BOOL visible=[RVObject(view,"elementView") isKindOfClass:UIView.class] && RVDislikeWatchAction(view) && RVDislikeHasCount() &&
        [RVNativeDislikeVideoForAction(view) isEqual:RVDislikeVideo];NSDictionary *style=nil;
    if (visible) visible=RVDislikeFindAction(view,&style);
    if (visible && !state) {
        state=[RVDislikeInlineState new];state.label=[UILabel new];state.label.userInteractionEnabled=NO;
        state.label.backgroundColor=UIColor.clearColor;state.label.accessibilityIdentifier=@"rvport.dislike.count";
        state.label.isAccessibilityElement=YES;state.label.adjustsFontForContentSizeCategory=YES;
        [view addSubview:state.label];objc_setAssociatedObject(view,RVDislikeInlineKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        if (!RVDislikeActions) RVDislikeActions=[NSHashTable weakObjectsHashTable];[RVDislikeActions addObject:view];
    }
    state.label.hidden=!visible;
    if (visible) {
        NSString *text=RVDislikeCompactCount(RVDislikeCount);
        if (![state.video isEqual:RVDislikeVideo] || ![state.label.text isEqual:text]) state.requestedLayout=NO;
        state.video=RVDislikeVideo;
        id native=RVObject(view,"label");UILabel *label=[native isKindOfClass:UILabel.class] ? native : nil;
        state.label.font=style[NSFontAttributeName] ?: label.font ?: [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody] scaledFontForFont:[UIFont systemFontOfSize:14 weight:UIFontWeightMedium]];
        BOOL dark=view.traitCollection.userInterfaceStyle==UIUserInterfaceStyleDark;
        id palette=RVObject(NSClassFromString(@"YTCommonColorPalette"),dark ? "darkPalette" : "lightPalette");id color=RVObject(palette,"textPrimary");
        state.label.textColor=style[NSForegroundColorAttributeName] ?: label.textColor ?: ([color isKindOfClass:UIColor.class] ? color : UIColor.labelColor);
        state.label.text=text;
        state.label.accessibilityLabel=[NSString stringWithFormat:@"Return YouTube Dislike estimate: %@ dislikes",[NSNumberFormatter localizedStringFromNumber:RVDislikeCount numberStyle:NSNumberFormatterDecimalStyle]];
    }
    return state;
}
static CGFloat RVDislikeAccessoryWidth(RVDislikeInlineState *state) {
    return state && !state.label.hidden ? ceil([state.label sizeThatFits:CGSizeMake(CGFLOAT_MAX,CGFLOAT_MAX)].width)+8 : 0;
}
static void RVRefreshDislikeUI(void) {
    if (!NSThread.isMainThread) { dispatch_async(dispatch_get_main_queue(),^{ RVRefreshDislikeUI(); });return; }
    static NSString *previous;static __weak id previousWatch;
    NSString *revision=[NSString stringWithFormat:@"%@:%@:%d",RVObject(RVCurrentPlayback,"contentVideoID"),RVDislikeHasCount() ? RVDislikeCount : nil,RVEnabled(@"return_dislikes")];
    if ([revision isEqual:previous] && previousWatch==RVCurrentWatch) return;
    previous=revision;previousWatch=RVCurrentWatch;
    RVRefreshElementDislikes();
    // Layout also discovers newly created/reused actions. Existing accessories
    // are refreshed immediately on video changes, toggle changes, and responses.
    for (UIView *view in RVDislikeActions.allObjects) {
        RVDislikeUpdateAction(view);[view setNeedsLayout];[view.superview setNeedsLayout];
    }
    UIViewController *watch=RVCurrentWatch;
    if ([watch isKindOfClass:UIViewController.class] && watch.isViewLoaded) {
        // A count can arrive after the initial action layout. Find already
        // visible native actions without creating offscreen views or nodes.
        NSMutableArray<UIView *> *pending=[NSMutableArray arrayWithObject:watch.view];
        Class actionClass=NSClassFromString(@"YTSlimVideoDetailsActionView");
        for (NSUInteger i=0;i<pending.count && i<2048;i++) {
            UIView *candidate=pending[i];
            if (actionClass && [candidate isKindOfClass:actionClass]) { [candidate setNeedsLayout];[candidate.superview setNeedsLayout]; }
            for (UIView *child in candidate.subviews) if (pending.count<2048) [pending addObject:child];
        }
    }
}
static void RVInstallDislikeUI(void) {
    RVHook(@"YTSlimVideoDetailsActionView",@"sizeThatFits:",@"{CGSize=dd}@:{CGSize=dd}",NO,^id(IMP original,SEL selector) {
        return ^CGSize(UIView *view,CGSize proposed) {
            CGSize size=((CGSize(*)(id,SEL,CGSize))original)(view,selector,proposed);
            if (size.width<=0 || size.height<=0) return size;
            RVDislikeInlineState *state=RVDislikeUpdateAction(view);CGFloat extra=RVDislikeAccessoryWidth(state);
            if (extra>0) {
                id element=RVObject(view,"elementView");
                if ([element isKindOfClass:UIView.class]) size.width=MAX(size.width,CGRectGetWidth([element bounds]));
                size.width+=extra;size.height=MAX(size.height,ceil(state.label.font.lineHeight));
            }return size;
        };
    });
    RVHook(@"YTSlimVideoDetailsActionView",@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^(UIView *view) {
            ((void(*)(id,SEL))original)(view,selector);
            RVDislikeInlineState *state=RVDislikeUpdateAction(view);CGFloat extra=RVDislikeAccessoryWidth(state);
            if (!extra) return;
            id native=RVObject(view,"elementView");
            if (![native isKindOfClass:UIView.class]) { state.label.hidden=YES;return; }
            UIView *control=native;CGRect frame=control.frame;CGFloat width=frame.size.width+extra;
            // The parent action row reserves the accessory width via sizeThatFits:.
            // Keep the original native control and its tap/vote behavior intact.
            if (CGRectGetWidth(view.bounds)+.5<width) {
                state.label.hidden=YES;
                if (!state.requestedLayout) { state.requestedLayout=YES;[view.superview setNeedsLayout]; }
                return;
            }
            CGFloat start=(CGRectGetWidth(view.bounds)-width)/2;
            BOOL rtl=view.effectiveUserInterfaceLayoutDirection==UIUserInterfaceLayoutDirectionRightToLeft;
            frame.origin.x=start+(rtl ? extra : 0);
            control.frame=frame;
            CGFloat height=ceil(state.label.font.lineHeight);
            state.label.frame=CGRectMake(rtl ? start : CGRectGetMaxX(frame)+8,(CGRectGetHeight(view.bounds)-height)/2,extra-8,height);
        };
    });
}
