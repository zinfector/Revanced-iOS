// SPDX-License-Identifier: MIT
// Native YouTube settings entry. UI presentation is supplied by RVSettingsUI.inc.
static BOOL RVPresentSettings(id presenter);
static void RVSettingsRefreshNativeHeader(UIViewController *controller);
static void RVSettingsInstallHeaderGeometry(void);
static NSString *const RVSettingsBuildScope=@"RVPort.SettingsBuildScope";
static const void *RVSettingsOwnedSectionKey=&RVSettingsOwnedSectionKey;
static const NSUInteger RVSettingsCategory=0x52565054; // Fits the native UInt32 category path.
static BOOL RVEnsureNativeSettings(id controller) {
    if (!RVCompatible || !RVCan(controller,"settingsSectionControllers",@"@@:") ||
        !RVCan(controller,"setSectionItems:forCategory:title:icon:titleDescription:headerHidden:",@"v@:@Q@@@B")) return NO;
    id sections=RVObject(controller,"settingsSectionControllers");
    if (![sections isKindOfClass:NSDictionary.class]) return NO;
    if (sections[@(RVSettingsCategory)]) return NO; // Never overwrite a native/third-party category.
    id itemClass=NSClassFromString(@"YTSettingsSectionItem");
    const char *factory="itemWithTitle:titleDescription:accessibilityIdentifier:detailTextBlock:selectBlock:";
    if (!RVCan(itemClass,factory,@"@@:@@@@?@?")) return NO;
    __weak id weakController=controller;
    BOOL (^selection)(id,NSUInteger)=^BOOL(id view,NSUInteger index) {
        (void)view;(void)index;return RVPresentSettings(weakController);
    };
    id item=((id(*)(id,SEL,id,id,id,id,id))objc_msgSend)(itemClass,sel_registerName(factory),@"ReVanced",@"Customize patches and playback",@"rvport.settings",nil,selection);
    if (![item isKindOfClass:itemClass]) return NO;
    id icon=RVSettingsEntryIcon();
    if (icon && RVCan(item,"setSettingIcon:",@"v@:@"))
        ((void(*)(id,SEL,id))objc_msgSend)(item,sel_registerName("setSettingIcon:"),icon);
    ((void(*)(id,SEL,id,NSUInteger,id,id,id,BOOL))objc_msgSend)(controller,
        sel_registerName("setSectionItems:forCategory:title:icon:titleDescription:headerHidden:"),@[item],RVSettingsCategory,nil,icon,nil,YES);
    id section=RVObject(controller,"settingsSectionControllers")[@(RVSettingsCategory)];
    if (section) objc_setAssociatedObject(section,RVSettingsOwnedSectionKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return section!=nil;
}
static BOOL RVSettingsScopeActive(void) {
    id controller=NSThread.currentThread.threadDictionary[RVSettingsBuildScope];
    id sections=RVObject(controller,"settingsSectionControllers");
    id section=[sections isKindOfClass:NSDictionary.class] ? sections[@(RVSettingsCategory)] : nil;
    return RVCompatible && section && [objc_getAssociatedObject(section,RVSettingsOwnedSectionKey) boolValue];
}
static id RVSettingsCategoryOrder(id native) {
    if (!RVSettingsScopeActive() || ![native isKindOfClass:NSArray.class] || [native containsObject:@(RVSettingsCategory)]) return native;
    for (id entry in native) if (![entry isKindOfClass:NSNumber.class]) return native;
    return [native arrayByAddingObject:@(RVSettingsCategory)];
}
static void RVInstallSettingsBridge(void) {
    RVSettingsInstallHeaderGeometry();
    RVHook(@"YTSettingsViewController",@"viewDidAppear:",@"v@:B",NO,^id(IMP original,SEL selector) {
        return ^(UIViewController *controller,BOOL animated) {
            ((void(*)(id,SEL,BOOL))original)(controller,selector,animated);
            RVSettingsRefreshNativeHeader(controller);
        };
    });
    RVHook(@"YTSettingsViewController",@"viewDidLayoutSubviews",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^(UIViewController *controller) {
            ((void(*)(id,SEL))original)(controller,selector);
            RVSettingsRefreshNativeHeader(controller);
        };
    });
    RVHook(@"YTSettingsViewController",@"setSectionControllers",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^(id object) {
            RVEnsureNativeSettings(object);
            NSMutableDictionary *thread=NSThread.currentThread.threadDictionary;
            id previous=thread[RVSettingsBuildScope];thread[RVSettingsBuildScope]=object;
            @try { ((void(*)(id,SEL))original)(object,selector); }
            @finally { if (previous) thread[RVSettingsBuildScope]=previous;else [thread removeObjectForKey:RVSettingsBuildScope]; }
        };
    });
    RVHook(@"YTSettingsViewController",@"viewDidLoad",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^(id object) {
            ((void(*)(id,SEL))original)(object,selector);
            if (RVEnsureNativeSettings(object) && RVCan(object,"setSectionControllers",@"v@:")) ((void(*)(id,SEL))objc_msgSend)(object,sel_registerName("setSectionControllers"));
        };
    });
    RVHook(@"YTAppSettingsPresentationData",@"settingsCategoryOrder",@"@@:",YES,^id(IMP original,SEL selector) {
        return ^id(id object) { return RVSettingsCategoryOrder(((id(*)(id,SEL))original)(object,selector)); };
    });
    RVHook(@"YTSettingsGroupData",@"orderedCategories",@"@@:",NO,^id(IMP original,SEL selector) {
        return ^id(id object) {
            id native=((id(*)(id,SEL))original)(object,selector);
            if (!RVCan(object,"type",@"Q@:") || ((NSUInteger(*)(id,SEL))objc_msgSend)(object,sel_registerName("type"))!=2) return native;
            return RVSettingsCategoryOrder(native);
        };
    });
}
