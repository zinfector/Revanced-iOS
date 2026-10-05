// SPDX-License-Identifier: MIT
// Lease the existing interactive pop recognizer only while our pages are on top.
// Native Settings assigns its own gesture delegate; the styled base host does not.
@interface RVSettingsContentController : UIViewController
@property(nonatomic,weak) UINavigationController *backGestureNavigation;
@end

@interface RVSettingsBackGestureDelegate : NSObject <UIGestureRecognizerDelegate>
@property(nonatomic,weak) UINavigationController *navigation;
@property(nonatomic,weak) UIGestureRecognizer *gesture;
@property(nonatomic,weak) id<UIGestureRecognizerDelegate> previousDelegate;
@property(nonatomic) BOOL previousEnabled;
@end
static const void *RVSettingsBackGestureKey=&RVSettingsBackGestureKey;
static BOOL RVSettingsOwnsNavigationTop(UINavigationController *navigation) {
    UIViewController *top=navigation.topViewController;
    return top && (objc_getAssociatedObject(top,RVSettingsHostKey) ||
        [top isKindOfClass:RVSettingsContentController.class]);
}
@implementation RVSettingsBackGestureDelegate
- (BOOL)respondsToSelector:(SEL)selector {
    return [super respondsToSelector:selector] || [self.previousDelegate respondsToSelector:selector];
}
- (id)forwardingTargetForSelector:(SEL)selector {
    id previous=self.previousDelegate;
    return [previous respondsToSelector:selector] ? previous : [super forwardingTargetForSelector:selector];
}
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gesture {
    UINavigationController *navigation=self.navigation;
    id<UIGestureRecognizerDelegate> previous=self.previousDelegate;
    if (gesture!=self.gesture || !RVSettingsOwnsNavigationTop(navigation))
        return [previous respondsToSelector:_cmd] ? [previous gestureRecognizerShouldBegin:gesture] : YES;
    UIViewController *top=navigation.topViewController;
    if (navigation.viewControllers.count<2 || navigation.visibleViewController!=top ||
        navigation.transitionCoordinator || !top.viewIfLoaded.window || top.presentedViewController ||
        navigation.isBeingDismissed || top.isBeingDismissed || top.isBeingPresented) return NO;
    // This native delegate creates its interactive transition context in this
    // callback. Keep that work instead of bypassing it with a blanket YES.
    if ([previous isKindOfClass:NSClassFromString(@"YTSwipeNavigationTransitionController")] &&
        [previous respondsToSelector:_cmd]) return [previous gestureRecognizerShouldBegin:gesture];
    return YES; // UIKit's existing edge recognizer and transition target do the slide.
}
- (void)backGestureChanged:(UIGestureRecognizer *)gesture {
    if (gesture.state==UIGestureRecognizerStateBegan && RVSettingsOwnsNavigationTop(self.navigation))
        [self.navigation.topViewController.view endEditing:YES];
}
@end

static void RVSettingsAcquireBackGesture(UIViewController *owner) {
    UINavigationController *navigation=owner.navigationController;
    if (!navigation || navigation.topViewController!=owner || !RVSettingsOwnsNavigationTop(navigation)) return;
    UIGestureRecognizer *gesture=navigation.interactivePopGestureRecognizer;if (!gesture) return;
    RVSettingsBackGestureDelegate *lease=objc_getAssociatedObject(navigation,RVSettingsBackGestureKey);
    if (lease && (lease.gesture!=gesture || gesture.delegate!=lease)) {
        // Another native page/extension changed the delegate. Discard our old
        // lease without restoring stale state over its replacement.
        [lease.gesture removeTarget:lease action:@selector(backGestureChanged:)];
        objc_setAssociatedObject(navigation,RVSettingsBackGestureKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);lease=nil;
    }
    if (!lease) {
        lease=[RVSettingsBackGestureDelegate new];lease.navigation=navigation;lease.gesture=gesture;
        lease.previousDelegate=gesture.delegate;lease.previousEnabled=gesture.enabled;
        objc_setAssociatedObject(navigation,RVSettingsBackGestureKey,lease,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [gesture addTarget:lease action:@selector(backGestureChanged:)];gesture.delegate=lease;
    }
    gesture.enabled=navigation.viewControllers.count>1;
}
static void RVSettingsReleaseBackGesture(UINavigationController *navigation) {
    if (!navigation || RVSettingsOwnsNavigationTop(navigation)) return;
    RVSettingsBackGestureDelegate *lease=objc_getAssociatedObject(navigation,RVSettingsBackGestureKey);
    if (!lease) return;
    UIGestureRecognizer *gesture=lease.gesture;
    [gesture removeTarget:lease action:@selector(backGestureChanged:)];
    if (gesture.delegate==lease) {
        gesture.delegate=lease.previousDelegate;gesture.enabled=lease.previousEnabled;
    }
    objc_setAssociatedObject(navigation,RVSettingsBackGestureKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
@implementation RVSettingsContentController
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];UIViewController *owner=RVSettingsOwner(self);
    RVSettingsRefreshAppearance(self);
    self.backGestureNavigation=owner.navigationController;RVSettingsAcquireBackGesture(owner);
}
- (void)traitCollectionDidChange:(UITraitCollection *)previous {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [super traitCollectionDidChange:previous];
#pragma clang diagnostic pop
    if (self.isViewLoaded && [self.traitCollection hasDifferentColorAppearanceComparedToTraitCollection:previous]) RVSettingsRefreshAppearance(self);
}
- (void)viewDidDisappear:(BOOL)animated {
    [super viewDidDisappear:animated];
    // Wait for completion: changing enabled/delegate during an interactive
    // disappearance would cancel the in-progress swipe. Cancellation retains
    // the lease; the next didAppear re-enables the still-visible page.
    RVSettingsReleaseBackGesture(self.backGestureNavigation);self.backGestureNavigation=nil;
}
@end
