// SPDX-License-Identifier: MIT
// Navigation policy shares the verified native pivot model used by Shorts.
static NSHashTable<UIView *> *RVNavigationItemViews;
static BOOL RVNavigationFilteringEnabled(void) {
    return RVEnabled(@"hide_shorts_navigation") || RVEnabled(@"hide_home_navigation") ||
        RVEnabled(@"hide_subscriptions_navigation") || RVEnabled(@"hide_library_navigation");
}
static BOOL RVNavigationIdentifierHidden(id identifier) {
    if (![identifier isKindOfClass:NSString.class]) return NO;
    if (RVShortsIdentifier(identifier)) return RVEnabled(@"hide_shorts_navigation");
    if ([identifier isEqual:@"FEwhat_to_watch"]) return RVEnabled(@"hide_home_navigation");
    if ([identifier isEqual:@"FEsubscriptions"]) return RVEnabled(@"hide_subscriptions_navigation");
    if ([identifier isEqual:@"FElibrary"]) return RVEnabled(@"hide_library_navigation");
    return NO;
}
static NSString *RVNavigationFallback(NSArray<NSString *> *identifiers) {
    id preferred=RVSetting(@"start_page");
    if ([preferred isKindOfClass:NSString.class] && [identifiers containsObject:preferred]) return preferred;
    if (RVShortsIdentifier(preferred)) for (NSString *identifier in identifiers)
        if (RVShortsIdentifier(identifier)) return identifier;
    for (NSString *identifier in @[@"FEwhat_to_watch",@"FEsubscriptions",@"FElibrary",@"FEshorts",@"FEshorts_tab"])
        if ([identifiers containsObject:identifier]) return identifier;
    return identifiers.firstObject;
}
static id RVNavigationDefaultSelection(id native) {
    id configured=RVSetting(@"start_page");
    id desired=[configured isKindOfClass:NSString.class] && [configured length] ? configured : native;
    if (!RVNavigationIdentifierHidden(desired)) return desired;
    NSMutableArray *survivors=[NSMutableArray array];
    for (NSString *identifier in @[@"FEwhat_to_watch",@"FEsubscriptions",@"FElibrary",@"FEshorts"])
        if (!RVNavigationIdentifierHidden(identifier)) [survivors addObject:identifier];
    // The renderer stage retains a native content tab when every option is hidden.
    NSString *fallback=RVNavigationFallback(survivors) ?: @"FEwhat_to_watch";
    RVShortsEvent(@"default_selection_recovered");RVShortsValue(@"default_selection",fallback);
    return fallback;
}
static NSArray<NSString *> *RVNavigationIdentifiers(NSArray *items) {
    NSMutableArray *identifiers=[NSMutableArray array];
    for (id item in items) { NSString *identifier=RVShortsPivotID(item);if (identifier.length) [identifiers addObject:identifier]; }
    return identifiers.copy;
}
static NSArray<NSString *> *RVNavigationContentIdentifiers(NSArray *items) {
    NSMutableArray *content=[NSMutableArray array];
    for (NSString *identifier in RVNavigationIdentifiers(items))
        if ([@[@"FEwhat_to_watch",@"FEsubscriptions",@"FElibrary",@"FEshorts",@"FEshorts_tab"] containsObject:identifier]) [content addObject:identifier];
    return content.copy;
}

static const void *RVNavigationLabelLeaseKey=&RVNavigationLabelLeaseKey;
@interface RVNavigationLabelLease : NSObject
@property(nonatomic,weak) UIButton *button;
@property(nonatomic,copy) NSString *nativeAccessibility;
@property(nonatomic,copy) NSString *installedAccessibility;
@end
@implementation RVNavigationLabelLease @end
static void RVNavigationRestoreAccessibility(id item) {
    RVNavigationLabelLease *lease=objc_getAssociatedObject(item,RVNavigationLabelLeaseKey);
    if (!lease) return;
    if ([lease.button.accessibilityLabel isEqual:lease.installedAccessibility])
        lease.button.accessibilityLabel=lease.nativeAccessibility;
    objc_setAssociatedObject(item,RVNavigationLabelLeaseKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
static void RVNavigationApplyLabels(id item) {
    if (!NSThread.isMainThread || ![item isKindOfClass:UIView.class]) return;
    [RVNavigationItemViews addObject:item];
    if (!RVEnabled(@"hide_navigation_labels")) return;
    id button=RVObject(item,"navigationButton");
    if (![button isKindOfClass:UIButton.class] || !RVCan(button,"setButtonLayoutStyle:",@"v@:q") ||
        !RVCan(button,"setButtonImageTitlePadding:",@"v@:d")) {
        RVShortsValue(@"label_checkpoint",@"native_button_contract_missing");return;
    }
    NSString *title=[button titleForState:UIControlStateNormal];
    if (!title.length) { RVShortsValue(@"label_checkpoint",@"native_already_icon_only");return; }
    // Reproduce the native isIconsOnly branch, retaining the accessible tab name.
    RVNavigationLabelLease *lease=[RVNavigationLabelLease new];lease.button=button;
    lease.nativeAccessibility=[button accessibilityLabel];
    lease.installedAccessibility=lease.nativeAccessibility.length ? lease.nativeAccessibility : title;
    [button setTitle:nil forState:UIControlStateNormal];
    [button titleLabel].hidden=YES;
    ((void(*)(id,SEL,NSInteger))objc_msgSend)(button,sel_registerName("setButtonLayoutStyle:"),0);
    ((void(*)(id,SEL,double))objc_msgSend)(button,sel_registerName("setButtonImageTitlePadding:"),0.0);
    [button setAccessibilityLabel:lease.installedAccessibility];
    objc_setAssociatedObject(item,RVNavigationLabelLeaseKey,lease,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    RVShortsEvent(@"navigation_label_hidden");RVShortsValue(@"label_checkpoint",@"native_icon_only_layout_applied");
}
static void RVNavigationRefreshLabels(void) {
    for (UIView *view in RVNavigationItemViews.allObjects) {
        if (RVCan(view,"updateTitleAndLayoutForRenderer",@"v@:")) {
            ((void(*)(id,SEL))objc_msgSend)(view,sel_registerName("updateTitleAndLayoutForRenderer"));
            [view setNeedsLayout];
        }
    }
}
static void RVNavigationInstallLabels(void) {
    RVNavigationItemViews=[NSHashTable weakObjectsHashTable];
    RVShortsHooks[@"navigation_labels"]=@(RVHook(@"YTPivotBarItemView",@"updateTitleAndLayoutForRenderer",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) {
            if (NSThread.isMainThread) RVNavigationRestoreAccessibility(object);
            ((void(*)(id,SEL))original)(object,sel);
            RVNavigationApplyLabels(object);
        };
    }));
}
