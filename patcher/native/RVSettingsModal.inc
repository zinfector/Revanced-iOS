// SPDX-License-Identifier: MIT
// Own only the settings modal stack. A stock bar is not a YouTube header, and
// UIKit's default scroll-edge appearance becomes transparent at the list top.
static const void *RVSettingsModalKey=&RVSettingsModalKey;
@interface RVSettingsModalState : NSObject
@property(nonatomic,weak) UINavigationController *navigation;
- (void)close:(id)sender;
@end
@implementation RVSettingsModalState
- (void)close:(id)sender {
    (void)sender;[self.navigation.view endEditing:YES];
    [self.navigation dismissViewControllerAnimated:YES completion:nil];
}
@end

static void RVSettingsRefreshModalAppearance(UIViewController *content) {
    UIViewController *owner=RVSettingsOwner(content);UINavigationController *navigation=owner.navigationController;
    if (!objc_getAssociatedObject(navigation,RVSettingsModalKey)) return;
    UIColor *background=RVSettingsColor(content,"baseBackground"),*foreground=RVSettingsColor(content,"textPrimary");
    UINavigationBarAppearance *appearance=[UINavigationBarAppearance new];
    [appearance configureWithOpaqueBackground];appearance.backgroundColor=background;
    appearance.backgroundEffect=nil;appearance.shadowColor=UIColor.clearColor;
    appearance.titleTextAttributes=@{NSForegroundColorAttributeName:foreground};
    appearance.largeTitleTextAttributes=appearance.titleTextAttributes;
    // Per-item overrides take precedence over bar defaults. Cover both, keeping
    // hidden pages from restyling the shared bar during loading or transitions.
    UINavigationItem *item=owner.navigationItem;
    item.standardAppearance=appearance;item.scrollEdgeAppearance=appearance;
    item.compactAppearance=appearance;item.compactScrollEdgeAppearance=appearance;
    for (UIBarButtonItem *button in item.leftBarButtonItems) { button.tintColor=foreground;button.customView.tintColor=foreground; }
    for (UIBarButtonItem *button in item.rightBarButtonItems) { button.tintColor=foreground;button.customView.tintColor=foreground; }
    RVSetObject(RVObject(owner,"titleButton"),"setCustomTitleColor:",foreground);
    if (navigation.topViewController!=owner || navigation.visibleViewController!=owner) return;
    navigation.overrideUserInterfaceStyle=RVSettingsPageStyle(content)==1 ? UIUserInterfaceStyleDark : UIUserInterfaceStyleLight;
    UINavigationBar *bar=navigation.navigationBar;
    bar.standardAppearance=appearance;bar.scrollEdgeAppearance=appearance;
    bar.compactAppearance=appearance;bar.compactScrollEdgeAppearance=appearance;
    bar.backgroundColor=background;bar.tintColor=foreground;
    navigation.viewIfLoaded.backgroundColor=background;
}

static UINavigationController *RVSettingsModalNavigation(UIViewController *page,UIViewController *source) {
    static BOOL native;static dispatch_once_t once;
    dispatch_once(&once,^{
        native=[NSClassFromString(@"YTNavigationController") isSubclassOfClass:UINavigationController.class] &&
            [NSClassFromString(@"YTNavigationBar") isSubclassOfClass:UINavigationBar.class];
        native &= RVUIRequire(@"YTNavigationController",@"initWithParentResponder:",@"@@:@",NO);
        native &= RVUIRequire(@"YTNavigationBar",@"setupStyle",@"v@:",NO);
    });
    UINavigationController *navigation=nil;
    if (native) {
        id candidate=[[NSClassFromString(@"YTNavigationController") alloc] initWithParentResponder:source];
        if ([candidate isKindOfClass:UINavigationController.class] &&
            [((UINavigationController *)candidate).navigationBar isKindOfClass:NSClassFromString(@"YTNavigationBar")]) navigation=candidate;
    }
    if (!navigation) navigation=[[UINavigationController alloc] initWithNavigationBarClass:nil toolbarClass:nil];
    RVSettingsModalState *state=[RVSettingsModalState new];state.navigation=navigation;
    objc_setAssociatedObject(navigation,RVSettingsModalKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    // Match YouTube's native default and keep close available in the fallback.
    navigation.modalPresentationStyle=UIModalPresentationPageSheet;
    page.navigationItem.leftBarButtonItem=[[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"xmark"]
        style:UIBarButtonItemStylePlain target:state action:@selector(close:)];
    page.navigationItem.leftBarButtonItem.accessibilityLabel=@"Close ReVanced settings";
    [navigation setViewControllers:@[page] animated:NO];
    RVSettingsHostState *host=objc_getAssociatedObject(page,RVSettingsHostKey);
    RVSettingsRefreshModalAppearance(host.content ?: page);
    RVLog([NSString stringWithFormat:@"Settings modal: %@, bar %@, opaque standard/scroll-edge appearance",
        NSStringFromClass(navigation.class),NSStringFromClass(navigation.navigationBar.class)]);
    return navigation;
}
