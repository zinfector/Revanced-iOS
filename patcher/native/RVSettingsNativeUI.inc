// SPDX-License-Identifier: MIT
// Own the content layout; reuse YouTube's title, back control, cells and headers.
// Declarations only: runtime ABI checks precede these calls. The init family
// tells ARC to consume the allocated receiver and adopt the retained result.
@interface NSObject (RVSettingsInitializers)
- (instancetype)initWithParentResponder:(id)responder;
- (instancetype)initWithTitle:(id)title items:(id)items;
@end
static BOOL RVUIRequire(NSString *name,NSString *selector,NSString *signature,BOOL classMethod) {
    Class cls=NSClassFromString(name);Class target=classMethod ? object_getClass(cls) : cls;
    BOOL valid=cls && [RVSignature(class_getInstanceMethod(target,NSSelectorFromString(selector))) isEqual:signature];
    if (!valid) RVLog([NSString stringWithFormat:@"Settings UI fallback: %@ %@",name,selector]);
    return valid;
}
static BOOL RVSettingsNativeRowsAvailable(void) {
    static BOOL available;static dispatch_once_t once;
    dispatch_once(&once,^{
        // &= deliberately checks/reports every contract, not just the first failure.
        available=YES;
        available &= RVUIRequire(@"YTSettingsSectionItem",@"itemWithTitle:titleDescription:accessibilityIdentifier:detailTextBlock:selectBlock:",@"@@:@@@@?@?",YES);
        available &= RVUIRequire(@"YTSettingsSectionItem",@"switchItemWithTitle:titleDescription:accessibilityIdentifier:switchOn:switchBlock:",@"@@:@@@B@?",YES);
        available &= RVUIRequire(@"YTSettingsSectionItem",@"setEnabled:",@"v@:B",NO);
        available &= RVUIRequire(@"YTSettingsSectionItem",@"setOn:",@"v@:B",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"initWithTitle:items:",@"@@:@@",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"updateCell:forIndex:",@"v@:@q",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"sizeForItemAtIndex:withSize:",@"{CGSize=dd}@:q{CGSize=dd}",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"classForHeader",@"#@:",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"sizeForHeaderWithSize:",@"{CGSize=dd}@:{CGSize=dd}",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"updateHeader:",@"v@:@",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"didSelectItemAtIndex:fromView:",@"v@:q@",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"setHeaderHidden:",@"v@:B",NO);
        available &= RVUIRequire(@"YTSettingsSectionController",@"setUseUpdatedSettingsColors:",@"v@:B",NO);
        available &= RVUIRequire(@"YTCardCollectionViewUIFormatter",@"setCellPositioningStyle:",@"v@:i",NO);
        available &= RVUIRequire(@"YTCardCollectionViewUIFormatter",@"updateCell:atIndexPath:sectionController:numberofSections:",@"v@:@@@Q",NO);
        available &= RVUIRequire(@"YTCardCollectionViewUIFormatter",@"updateSupplementaryItem:kind:sectionIndex:sectionController:numberOfSections:",@"v@:@@Q@Q",NO);
        available &= RVUIRequire(@"YTCardCollectionViewUIFormatter",@"insetForSectionAtIndex:sectionController:numberOfSections:",@"{UIEdgeInsets=dddd}@:Q@Q",NO);
        available &= RVUIRequire(@"YTCardCollectionViewUIFormatter",@"referenceSizeForHeaderWithSize:sectionIndex:sectionController:numberOfSections:",@"{CGSize=dd}@:{CGSize=dd}Q@Q",NO);
        available &= RVUIRequire(@"YTSettingsCell",@"pageStyleDidChange:",@"v@:q",NO);
        available &= RVUIRequire(@"YTSettingsCell",@"setParentResponder:",@"v@:@",NO);
        available &= [NSClassFromString(@"YTSettingsCell") isSubclassOfClass:UICollectionViewCell.class];
        available &= [NSClassFromString(@"YTFeedHeaderView") isSubclassOfClass:UICollectionReusableView.class];
        available &= RVUIRequire(@"YTFeedHeaderView",@"setCustomEdgePadding:",@"v@:d",NO);
        RVLog(available ? @"Settings UI: native YouTube rows" : @"Settings UI: plain UIKit rows");
    });return available;
}

@interface RVSettingsHostState : NSObject
@property(nonatomic,strong) UIViewController *content;
@property(nonatomic,weak) UIViewController *source;
@end
@implementation RVSettingsHostState @end
static const void *RVSettingsHostKey=&RVSettingsHostKey;
static Class RVSettingsHostClass;
static UIViewController *RVSettingsOwner(UIViewController *content) {
    UIViewController *parent=content.parentViewController;
    return objc_getAssociatedObject(parent,RVSettingsHostKey) ? parent : content;
}
#include "RVSettingsNavigation.inc"
static NSInteger RVSettingsPageStyle(UIViewController *content) {
    NSString *theme=RVSetting(@"theme");
    if ([theme isEqual:@"dark"]) return 1;
    if ([theme isEqual:@"light"]) return 0;
    UIViewController *owner=RVSettingsOwner(content);
    RVSettingsHostState *state=objc_getAssociatedObject(owner,RVSettingsHostKey);
    if (RVCan(state.source,"pageStyle",@"q@:")) {
        NSInteger style=((NSInteger(*)(id,SEL))objc_msgSend)(state.source,sel_registerName("pageStyle"));
        if (style==0 || style==1) return style;
    }
    return owner.traitCollection.userInterfaceStyle==UIUserInterfaceStyleDark ? 1 : 0;
}
static UIColor *RVSettingsColor(UIViewController *content,const char *token) {
    Class cls=NSClassFromString(@"YTCommonColorPalette");
    const char *factory=RVSettingsPageStyle(content)==1 ? "darkPalette" : "lightPalette";
    id palette=RVObject(cls,factory);id color=RVObject(palette,token);
    if ([color isKindOfClass:UIColor.class]) return color;
    BOOL dark=RVSettingsPageStyle(content)==1;
    if (!strcmp(token,"textSecondary")) return dark ? [UIColor colorWithWhite:.65 alpha:1] : [UIColor colorWithWhite:.4 alpha:1];
    if (!strcmp(token,"textPrimary")) return dark ? UIColor.whiteColor : UIColor.blackColor;
    if (!strcmp(token,"outline")) return [UIColor colorWithWhite:dark ? .25 : .85 alpha:1];
    return dark ? [UIColor colorWithWhite:15.0/255 alpha:1] : UIColor.whiteColor;
}
static void RVSettingsRefreshAppearance(UIViewController *content) {
    UIViewController *owner=RVSettingsOwner(content);NSString *theme=RVSetting(@"theme");
    UIUserInterfaceStyle style=[theme isEqual:@"dark"] ? UIUserInterfaceStyleDark : [theme isEqual:@"light"] ? UIUserInterfaceStyleLight : UIUserInterfaceStyleUnspecified;
    owner.overrideUserInterfaceStyle=style;content.overrideUserInterfaceStyle=style;
    content.view.backgroundColor=RVSettingsColor(content,"baseBackground");
    owner.view.backgroundColor=content.view.backgroundColor;
    // A child loads before its host is presented. Styling the shared native bar
    // then changes the screen underneath it; native viewWillAppear owns entry.
    if (owner.viewIfLoaded.window && (!owner.navigationController || owner.navigationController.visibleViewController==owner) &&
        RVCan(owner,"setupNavigationBarAppearance",@"v@:"))
        ((void(*)(id,SEL))objc_msgSend)(owner,sel_registerName("setupNavigationBarAppearance"));
}
static NSString *const RVSettingsHeaderGeometryScope=@"RVPort.SettingsHeaderGeometryScope";
static void RVSettingsInstallHeaderGeometry(void) {
    // Native browse insets use an app-global window, then fall back to a
    // status-bar lookup. Only while refreshing a settings header, measure its
    // actual attached window instead. The UIView category stays untouched.
    RVHook(@"YTHeaderView",@"yt_safeAreaInsetsForBrowse",@"{UIEdgeInsets=dddd}@:",NO,^id(IMP original,SEL selector) {
        return ^UIEdgeInsets(UIView *header) {
            UIEdgeInsets native=((UIEdgeInsets(*)(id,SEL))original)(header,selector);
            if (NSThread.currentThread.threadDictionary[RVSettingsHeaderGeometryScope]==header && header.window) {
                CGFloat top=header.window.safeAreaInsets.top;
                if (isfinite(top) && top>0) native.top=top;
            }
            return native;
        };
    });
}
static void RVSettingsRefreshNativeHeader(UIViewController *controller) {
    // YTHeaderView caches statusBarHeight/layoutHeight at construction. Once
    // attached, let its native status-area calculation refresh both together.
    // Do not shift title frames or add a device-specific top constant.
    if (!NSThread.isMainThread || !controller.viewIfLoaded.window) return;
    UIView *header=RVObject(controller,"yt_headerView");
    if (![header isKindOfClass:NSClassFromString(@"YTHeaderView")] || !header.window ||
        !RVCan(header,"resetHeaderParamsForStatusBarUpdate",@"v@:")) return;
    CGFloat before=header.intrinsicContentSize.height;
    NSMutableDictionary *thread=NSThread.currentThread.threadDictionary;id previous=thread[RVSettingsHeaderGeometryScope];
    thread[RVSettingsHeaderGeometryScope]=header;
    @try { ((void(*)(id,SEL))objc_msgSend)(header,sel_registerName("resetHeaderParamsForStatusBarUpdate")); }
    @finally { if (previous) thread[RVSettingsHeaderGeometryScope]=previous;else [thread removeObjectForKey:RVSettingsHeaderGeometryScope]; }
    CGFloat after=header.intrinsicContentSize.height;
    if (before==after) return; // Avoid a layout/invalidation feedback loop.
    [header invalidateIntrinsicContentSize];[header setNeedsLayout];[header.superview setNeedsLayout];
    [controller.parentViewController.viewIfLoaded setNeedsLayout];
    RVLog([NSString stringWithFormat:@"Settings header height refreshed: %.1f -> %.1f",before,after]);
}
static void RVSettingsHostLoad(id object,SEL selector) {
    (void)selector;((UIViewController *)object).view=[UIView new];
}
static void RVSettingsHostDidLoad(id object,SEL selector) {
    // Direct superclass IMP avoids recursing through the registered subclass.
    IMP original=class_getMethodImplementation(class_getSuperclass(RVSettingsHostClass),selector);
    ((void(*)(id,SEL))original)(object,selector);
    UIViewController *host=object;RVSettingsHostState *state=objc_getAssociatedObject(host,RVSettingsHostKey);
    UIViewController *child=state.content;if (!child) return;
    [host addChildViewController:child];child.view.translatesAutoresizingMaskIntoConstraints=NO;[host.view addSubview:child.view];
    UILayoutGuide *safe=host.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[child.view.topAnchor constraintEqualToAnchor:safe.topAnchor],
        [child.view.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[child.view.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],
        [child.view.bottomAnchor constraintEqualToAnchor:host.view.bottomAnchor]]];
    [child didMoveToParentViewController:host];RVSettingsRefreshAppearance(child);
}
static NSInteger RVSettingsHostPageStyle(id object,SEL selector) {
    (void)selector;RVSettingsHostState *state=objc_getAssociatedObject(object,RVSettingsHostKey);
    // Before state is attached, use the host's traits (native initialization may ask).
    return RVSettingsPageStyle(state.content ?: object);
}
static void RVSettingsHostDidAppear(id object,SEL selector,BOOL animated) {
    IMP original=class_getMethodImplementation(class_getSuperclass(RVSettingsHostClass),selector);
    ((void(*)(id,SEL,BOOL))original)(object,selector,animated);
    RVSettingsRefreshNativeHeader(object);
}
static void RVSettingsHostDidLayout(id object,SEL selector) {
    IMP original=class_getMethodImplementation(class_getSuperclass(RVSettingsHostClass),selector);
    ((void(*)(id,SEL))original)(object,selector);
    RVSettingsRefreshNativeHeader(object);
}
static Class RVSettingsNativeHostClass(void) {
    static dispatch_once_t once;dispatch_once(&once,^{
        BOOL valid=RVUIRequire(@"YTStyledViewController",@"initWithParentResponder:",@"@@:@",NO);
        valid &= RVUIRequire(@"YTStyledViewController",@"viewDidLoad",@"v@:",NO);
        valid &= RVUIRequire(@"YTStyledViewController",@"viewWillAppear:",@"v@:B",NO);
        valid &= RVUIRequire(@"YTStyledViewController",@"viewDidAppear:",@"v@:B",NO);
        valid &= RVUIRequire(@"YTStyledViewController",@"viewDidLayoutSubviews",@"v@:",NO);
        valid &= RVUIRequire(@"YTStyledViewController",@"setShouldSetupNavigationBarStyle:",@"v@:B",NO);
        valid &= RVUIRequire(@"YTStyledViewController",@"setupNavigationBarAppearance",@"v@:",NO);
        Class base=NSClassFromString(@"YTStyledViewController");
        if (!valid || ![base isSubclassOfClass:UIViewController.class] || NSClassFromString(@"RVPortNativeSettingsHost")) return;
        Class cls=objc_allocateClassPair(base,"RVPortNativeSettingsHost",0);if (!cls) return;
        BOOL added=class_addMethod(cls,@selector(loadView),(IMP)RVSettingsHostLoad,"v@:");
        added &= class_addMethod(cls,@selector(viewDidLoad),(IMP)RVSettingsHostDidLoad,"v@:");
        added &= class_addMethod(cls,@selector(viewDidAppear:),(IMP)RVSettingsHostDidAppear,"v@:B");
        added &= class_addMethod(cls,@selector(viewDidLayoutSubviews),(IMP)RVSettingsHostDidLayout,"v@:");
        added &= class_addMethod(cls,sel_registerName("pageStyle"),(IMP)RVSettingsHostPageStyle,"q@:");
        if (!added) { objc_disposeClassPair(cls);return; }
        RVSettingsHostClass=cls;objc_registerClassPair(cls);RVLog(@"Settings UI: native YouTube header");
    });return RVSettingsHostClass;
}
static UIViewController *RVSettingsWrap(UIViewController *content,UIViewController *source) {
    Class cls=RVSettingsNativeHostClass();if (!cls) return content;
    UIViewController *host=[[cls alloc] initWithParentResponder:source];
    if (![host isKindOfClass:cls]) return content;
    RVSettingsHostState *state=[RVSettingsHostState new];state.content=content;state.source=source;
    objc_setAssociatedObject(host,RVSettingsHostKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);host.title=content.title;
    ((void(*)(id,SEL,BOOL))objc_msgSend)(host,sel_registerName("setShouldSetupNavigationBarStyle:"),YES);
    return host;
}
static void RVSettingsPush(UIViewController *content,UIViewController *destination) {
    UIViewController *owner=RVSettingsOwner(content);[content.view endEditing:YES];
    RVSettingsHostState *state=objc_getAssociatedObject(owner,RVSettingsHostKey);
    UIViewController *page=RVSettingsWrap(destination,state.source ?: owner);
    if (owner.navigationController) [owner.navigationController pushViewController:page animated:YES];
    else if (RVCan(state.source,"showOrPushViewController:",@"v@:@")) ((void(*)(id,SEL,id))objc_msgSend)(state.source,sel_registerName("showOrPushViewController:"),page);
    else [owner presentViewController:[[UINavigationController alloc] initWithRootViewController:page] animated:YES completion:nil];
}
static void RVSettingsPop(UIViewController *content) {
    UIViewController *owner=RVSettingsOwner(content);
    if (owner.navigationController.viewControllers.count>1) [owner.navigationController popViewControllerAnimated:YES];
    else [owner dismissViewControllerAnimated:YES completion:nil];
}

@interface RVSettingsSupplement : UICollectionReusableView
@property(nonatomic,strong) UILabel *label;
@property(nonatomic,strong) UIView *separator;
@end
@implementation RVSettingsSupplement
- (instancetype)initWithFrame:(CGRect)frame {
    if ((self=[super initWithFrame:frame])) {
        self.label=[UILabel new];self.label.numberOfLines=0;self.label.adjustsFontForContentSizeCategory=YES;
        self.label.translatesAutoresizingMaskIntoConstraints=NO;[self addSubview:self.label];
        [NSLayoutConstraint activateConstraints:@[[self.label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:16],
            [self.label.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-16],[self.label.topAnchor constraintEqualToAnchor:self.topAnchor constant:8]]];
        self.separator=[UIView new];self.separator.translatesAutoresizingMaskIntoConstraints=NO;self.separator.hidden=YES;[self addSubview:self.separator];
        [NSLayoutConstraint activateConstraints:@[[self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],[self.separator.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
            [self.separator.heightAnchor constraintEqualToConstant:1]]];
    }return self;
}
@end
@interface RVSettingsFallbackCell : UICollectionViewCell
@property(nonatomic,strong) UILabel *titleLabel,*hintLabel,*valueLabel;
@property(nonatomic,strong) UISwitch *toggle;
@property(nonatomic,copy) BOOL (^changed)(BOOL);
@end
@implementation RVSettingsFallbackCell
- (instancetype)initWithFrame:(CGRect)frame {
    if ((self=[super initWithFrame:frame])) {
        self.titleLabel=[UILabel new];self.hintLabel=[UILabel new];self.valueLabel=[UILabel new];self.toggle=[UISwitch new];
        self.titleLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];self.hintLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
        self.valueLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
        for (UILabel *label in @[self.titleLabel,self.hintLabel,self.valueLabel]) { label.numberOfLines=0;label.adjustsFontForContentSizeCategory=YES; }
        UIStackView *text=[[UIStackView alloc] initWithArrangedSubviews:@[self.titleLabel,self.hintLabel]];text.axis=UILayoutConstraintAxisVertical;text.spacing=8;
        UIStackView *line=[[UIStackView alloc] initWithArrangedSubviews:@[text,self.valueLabel,self.toggle]];line.spacing=16;line.alignment=UIStackViewAlignmentCenter;
        [self.valueLabel setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        [text setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
        line.translatesAutoresizingMaskIntoConstraints=NO;[self.contentView addSubview:line];
        [NSLayoutConstraint activateConstraints:@[[line.topAnchor constraintEqualToAnchor:self.contentView.topAnchor constant:13],
            [line.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor constant:-13],[line.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor constant:16],
            [line.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor constant:-16],
            [self.valueLabel.widthAnchor constraintLessThanOrEqualToAnchor:self.contentView.widthAnchor multiplier:.3]]];
        self.toggle.onTintColor=[UIColor colorWithRed:.24 green:.61 blue:.98 alpha:1];[self.toggle addTarget:self action:@selector(toggled:) forControlEvents:UIControlEventValueChanged];
    }return self;
}
- (void)toggled:(UISwitch *)toggle { if (!self.changed || !self.changed(toggle.on)) [toggle setOn:!toggle.on animated:YES]; }
- (void)prepareForReuse { [super prepareForReuse];self.changed=nil; }
@end

@interface RVSettingsListController : UIViewController <UICollectionViewDataSource,UICollectionViewDelegateFlowLayout>
@property(nonatomic,strong) UICollectionView *collection;
@property(nonatomic,strong) NSArray *sections,*nativeSections;
@property(nonatomic,strong) id formatter;
@property(nonatomic) BOOL nativeRows;
@property(nonatomic) CGFloat layoutWidth;
@property(nonatomic,copy) void (^selected)(NSDictionary *);
@property(nonatomic,copy) BOOL (^changed)(NSDictionary *,BOOL);
- (void)reloadSections:(NSArray *)sections;
@end
@implementation RVSettingsListController
- (void)loadView {
    UICollectionViewFlowLayout *layout=[UICollectionViewFlowLayout new];layout.minimumLineSpacing=0;layout.minimumInteritemSpacing=0;
    self.collection=[[UICollectionView alloc] initWithFrame:CGRectZero collectionViewLayout:layout];self.view=self.collection;
    self.nativeRows=RVSettingsNativeRowsAvailable();self.collection.dataSource=self;self.collection.delegate=self;
    self.collection.keyboardDismissMode=UIScrollViewKeyboardDismissModeInteractive;
    self.collection.contentInset=UIEdgeInsetsMake(8,0,0,0); // Native Settings loadView.
    if (self.nativeRows) {
        self.formatter=[NSClassFromString(@"YTCardCollectionViewUIFormatter") new];
        ((void(*)(id,SEL,int))objc_msgSend)(self.formatter,sel_registerName("setCellPositioningStyle:"),2);
        [self.collection registerClass:NSClassFromString(@"YTSettingsCell") forCellWithReuseIdentifier:@"native.preference"];
        [self.collection registerClass:NSClassFromString(@"YTFeedHeaderView") forSupplementaryViewOfKind:UICollectionElementKindSectionHeader withReuseIdentifier:@"native.heading"];
    }
    [self.collection registerClass:RVSettingsFallbackCell.class forCellWithReuseIdentifier:@"fallback.preference"];
    [self.collection registerClass:RVSettingsSupplement.class forSupplementaryViewOfKind:UICollectionElementKindSectionHeader withReuseIdentifier:@"fallback.heading"];
    [self.collection registerClass:RVSettingsSupplement.class forSupplementaryViewOfKind:UICollectionElementKindSectionFooter withReuseIdentifier:@"footer"];
    self.collection.alwaysBounceVertical=YES;
}
- (NSInteger)pageStyle { return RVSettingsPageStyle(self.parentViewController ?: self); }
- (void)reloadSections:(NSArray *)sections {
    (void)self.view;self.sections=sections;NSMutableArray *native=[NSMutableArray array];__weak typeof(self) weakSelf=self;
    if (self.nativeRows) for (NSDictionary *section in sections) {
        NSMutableArray *items=[NSMutableArray array];
        for (NSDictionary *row in section[@"rows"]) {
            id item;Class cls=NSClassFromString(@"YTSettingsSectionItem");
            if ([row[@"toggle"] boolValue]) {
                BOOL (^toggle)(id,BOOL)=^BOOL(id cell,BOOL on) { (void)cell;return weakSelf.changed ? weakSelf.changed(row,on) : NO; };
                item=((id(*)(id,SEL,id,id,id,BOOL,id))objc_msgSend)(cls,sel_registerName("switchItemWithTitle:titleDescription:accessibilityIdentifier:switchOn:switchBlock:"),row[@"title"],row[@"hint"],row[@"identifier"],[row[@"on"] boolValue],toggle);
            } else {
                BOOL (^select)(id,NSUInteger)=^BOOL(id view,NSUInteger index) { (void)view;(void)index;if (!weakSelf.selected || [row[@"disabled"] boolValue]) return NO;weakSelf.selected(row);return YES; };
                id (^detail)(void)=^id { return row[@"detail"]; };
                item=((id(*)(id,SEL,id,id,id,id,id))objc_msgSend)(cls,sel_registerName("itemWithTitle:titleDescription:accessibilityIdentifier:detailTextBlock:selectBlock:"),row[@"title"],row[@"hint"],row[@"identifier"],detail,select);
            }
            if (![item isKindOfClass:cls]) { self.nativeRows=NO;break; }
            ((void(*)(id,SEL,BOOL))objc_msgSend)(item,sel_registerName("setEnabled:"),RVCompatible && ![row[@"disabled"] boolValue]);[items addObject:item];
        }
        if (!self.nativeRows) break;
        id nativeSection=[[NSClassFromString(@"YTSettingsSectionController") alloc] initWithTitle:[section[@"title"] length] ? section[@"title"] : nil items:items];
        if (![nativeSection isKindOfClass:NSClassFromString(@"YTSettingsSectionController")]) { self.nativeRows=NO;break; }
        ((void(*)(id,SEL,BOOL))objc_msgSend)(nativeSection,sel_registerName("setHeaderHidden:"),![section[@"title"] length]);
        ((void(*)(id,SEL,BOOL))objc_msgSend)(nativeSection,sel_registerName("setUseUpdatedSettingsColors:"),YES);[native addObject:nativeSection];
    }
    self.nativeSections=self.nativeRows ? native : nil;self.collection.backgroundColor=RVSettingsColor(self.parentViewController ?: self,"baseBackground");
    [self.collection.collectionViewLayout invalidateLayout];[self.collection reloadData];
}
- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)view { (void)view;return self.sections.count; }
- (NSInteger)collectionView:(UICollectionView *)view numberOfItemsInSection:(NSInteger)section { (void)view;return [self.sections[section][@"rows"] count]; }
- (NSDictionary *)row:(NSIndexPath *)index { return self.sections[index.section][@"rows"][index.item]; }
- (UICollectionViewCell *)collectionView:(UICollectionView *)view cellForItemAtIndexPath:(NSIndexPath *)index {
    UICollectionViewCell *cell=[view dequeueReusableCellWithReuseIdentifier:self.nativeRows ? @"native.preference" : @"fallback.preference" forIndexPath:index];
    NSDictionary *row=[self row:index];cell.accessibilityIdentifier=row[@"identifier"];cell.backgroundColor=view.backgroundColor;
    if (self.nativeRows) {
        ((void(*)(id,SEL,id,NSInteger))objc_msgSend)(self.nativeSections[index.section],sel_registerName("updateCell:forIndex:"),cell,index.item);
        RVSetObject(cell,"setParentResponder:",RVSettingsOwner(self.parentViewController ?: self));
        ((void(*)(id,SEL,NSInteger))objc_msgSend)(cell,sel_registerName("pageStyleDidChange:"),self.pageStyle);
        ((void(*)(id,SEL,id,id,id,NSUInteger))objc_msgSend)(self.formatter,sel_registerName("updateCell:atIndexPath:sectionController:numberofSections:"),cell,index,self.nativeSections[index.section],self.sections.count);
    } else {
        RVSettingsFallbackCell *fallback=(id)cell;fallback.titleLabel.text=row[@"title"];fallback.hintLabel.text=row[@"hint"];fallback.hintLabel.hidden=![row[@"hint"] length];
        fallback.valueLabel.text=row[@"detail"];fallback.valueLabel.hidden=![row[@"detail"] length];fallback.toggle.hidden=![row[@"toggle"] boolValue];
        fallback.toggle.on=[row[@"on"] boolValue];fallback.toggle.enabled=RVCompatible;
        fallback.titleLabel.textColor=RVSettingsColor(self.parentViewController ?: self,"textPrimary");
        fallback.hintLabel.textColor=fallback.valueLabel.textColor=RVSettingsColor(self.parentViewController ?: self,"textSecondary");
        fallback.toggle.accessibilityLabel=row[@"title"];__weak typeof(self) weakSelf=self;
        fallback.changed=^BOOL(BOOL on) { return weakSelf.changed ? weakSelf.changed(row,on) : NO; };
    }return cell;
}
- (CGSize)collectionView:(UICollectionView *)view layout:(UICollectionViewLayout *)layout sizeForItemAtIndexPath:(NSIndexPath *)index {
    (void)layout;CGFloat width=MAX(1,CGRectGetWidth(view.bounds)-view.adjustedContentInset.left-view.adjustedContentInset.right);
    if (self.nativeRows) {
        CGSize size=((CGSize(*)(id,SEL,NSInteger,CGSize))objc_msgSend)(self.nativeSections[index.section],sel_registerName("sizeForItemAtIndex:withSize:"),index.item,CGSizeMake(width,CGFLOAT_MAX));
        if (isfinite(size.height) && size.height>0) return CGSizeMake(width,size.height);
    }
    NSDictionary *row=[self row:index];CGFloat textWidth=MAX(40,width-32-([row[@"toggle"] boolValue] ? 67 : [row[@"detail"] length] ? width*.3+16 : 0));
    CGFloat height=[row[@"title"] boundingRectWithSize:CGSizeMake(textWidth,CGFLOAT_MAX) options:NSStringDrawingUsesLineFragmentOrigin attributes:@{NSFontAttributeName:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]} context:nil].size.height;
    if ([row[@"hint"] length]) height+=8+[row[@"hint"] boundingRectWithSize:CGSizeMake(textWidth,CGFLOAT_MAX) options:NSStringDrawingUsesLineFragmentOrigin attributes:@{NSFontAttributeName:[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]} context:nil].size.height;
    return CGSizeMake(width,MAX(52,ceil(height)+26));
}
- (void)collectionView:(UICollectionView *)view didSelectItemAtIndexPath:(NSIndexPath *)index {
    [view deselectItemAtIndexPath:index animated:YES];NSDictionary *row=[self row:index];if ([row[@"disabled"] boolValue] || [row[@"toggle"] boolValue]) return;
    if (self.nativeRows) ((void(*)(id,SEL,NSInteger,id))objc_msgSend)(self.nativeSections[index.section],sel_registerName("didSelectItemAtIndex:fromView:"),index.item,[view cellForItemAtIndexPath:index]);
    else if (self.selected) self.selected(row);
}
- (CGSize)collectionView:(UICollectionView *)view layout:(UICollectionViewLayout *)layout referenceSizeForHeaderInSection:(NSInteger)section {
    (void)layout;CGFloat width=CGRectGetWidth(view.bounds);if (![self.sections[section][@"title"] length]) return CGSizeZero;
    if (self.nativeRows) {
        id nativeSection=self.nativeSections[section];
        CGSize size=((CGSize(*)(id,SEL,CGSize))objc_msgSend)(nativeSection,sel_registerName("sizeForHeaderWithSize:"),CGSizeMake(width,CGFLOAT_MAX));
        return ((CGSize(*)(id,SEL,CGSize,NSUInteger,id,NSUInteger))objc_msgSend)(self.formatter,
            sel_registerName("referenceSizeForHeaderWithSize:sectionIndex:sectionController:numberOfSections:"),size,section,nativeSection,self.sections.count);
    }
    CGFloat height=[self.sections[section][@"title"] boundingRectWithSize:CGSizeMake(MAX(1,width-32),CGFLOAT_MAX) options:NSStringDrawingUsesLineFragmentOrigin attributes:@{NSFontAttributeName:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]} context:nil].size.height;
    return CGSizeMake(width,ceil(height)+24);
}
- (UIEdgeInsets)collectionView:(UICollectionView *)view layout:(UICollectionViewLayout *)layout insetForSectionAtIndex:(NSInteger)section {
    (void)view;(void)layout;
    if (self.nativeRows) return ((UIEdgeInsets(*)(id,SEL,NSUInteger,id,NSUInteger))objc_msgSend)(self.formatter,
        sel_registerName("insetForSectionAtIndex:sectionController:numberOfSections:"),section,self.nativeSections[section],self.sections.count);
    return UIEdgeInsetsMake(0,0,24,0);
}
- (CGSize)collectionView:(UICollectionView *)view layout:(UICollectionViewLayout *)layout referenceSizeForFooterInSection:(NSInteger)section {
    (void)layout;NSString *text=self.sections[section][@"footer"];CGFloat width=CGRectGetWidth(view.bounds);
    CGFloat separator=section+1<(NSInteger)self.sections.count ? 1 : 0;
    if (!text.length) return CGSizeMake(width,separator);
    CGFloat height=[text boundingRectWithSize:CGSizeMake(MAX(1,width-32),CGFLOAT_MAX) options:NSStringDrawingUsesLineFragmentOrigin attributes:@{NSFontAttributeName:[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]} context:nil].size.height;
    return CGSizeMake(width,ceil(height)+24+separator);
}
- (UICollectionReusableView *)collectionView:(UICollectionView *)view viewForSupplementaryElementOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)index {
    BOOL header=[kind isEqual:UICollectionElementKindSectionHeader];UICollectionReusableView *supplement=[view dequeueReusableSupplementaryViewOfKind:kind withReuseIdentifier:header ? self.nativeRows ? @"native.heading" : @"fallback.heading" : @"footer" forIndexPath:index];
    supplement.backgroundColor=view.backgroundColor;
    if (header && self.nativeRows) {
        ((void(*)(id,SEL,id))objc_msgSend)(self.nativeSections[index.section],sel_registerName("updateHeader:"),supplement);
        ((void(*)(id,SEL,id,id,NSUInteger,id,NSUInteger))objc_msgSend)(self.formatter,
            sel_registerName("updateSupplementaryItem:kind:sectionIndex:sectionController:numberOfSections:"),supplement,kind,index.section,self.nativeSections[index.section],self.sections.count);
        ((void(*)(id,SEL,double))objc_msgSend)(supplement,sel_registerName("setCustomEdgePadding:"),16);
        if (RVCan(supplement,"pageStyleDidChange:",@"v@:q")) ((void(*)(id,SEL,NSInteger))objc_msgSend)(supplement,sel_registerName("pageStyleDidChange:"),self.pageStyle);
    } else {
        RVSettingsSupplement *label=(id)supplement;label.label.text=self.sections[index.section][header ? @"title" : @"footer"];
        label.label.hidden=!label.label.text.length;
        label.separator.hidden=header || index.section+1>=(NSInteger)self.sections.count;
        label.separator.backgroundColor=RVSettingsColor(self.parentViewController ?: self,"outline");
        label.label.font=[UIFont preferredFontForTextStyle:header ? UIFontTextStyleHeadline : UIFontTextStyleFootnote];
        label.label.textColor=RVSettingsColor(self.parentViewController ?: self,header ? "textPrimary" : "textSecondary");
    }return supplement;
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];CGFloat width=CGRectGetWidth(self.collection.bounds);
    if (width!=self.layoutWidth) { self.layoutWidth=width;[self.collection.collectionViewLayout invalidateLayout]; }
}
@end
