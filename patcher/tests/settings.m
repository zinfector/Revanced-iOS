// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
static BOOL RVCompatible=YES;
static NSDictionary *RVConfig;
static NSString *const RVPreferencePrefix=@"RVPort.";
static NSMutableArray *RVStatus;
static NSUserDefaults *TestDefaults;
static id RVSetting(NSString *key) { return [TestDefaults objectForKey:[RVPreferencePrefix stringByAppendingString:key]] ?: RVConfig[key]; }
static void RVLog(NSString *value) { [RVStatus addObject:value]; }
static NSUserDefaults *FixtureDefaults(id object,SEL selector) { (void)object;(void)selector;return TestDefaults; }
#include "../native/RVRuntime.inc"
#include "../native/RVSettingsRows.inc"
static id FixtureIcon;
static id RVSettingsEntryIcon(void) { return FixtureIcon; }
static id PresentedController;
static BOOL CanPresent=YES;
static BOOL RVPresentSettings(id controller) { if (!controller || !CanPresent || !RVCompatible) return NO;PresentedController=controller;return YES; }
#include "../native/RVSettingsBridge.inc"

static BOOL MalformedItem,ThrowBuild;
static NSArray *NativeOrder,*LinearObserved,*GroupedObserved,*AccountObserved,*BackgroundObserved;
static id NestedController;
@interface YTSettingsSectionItem : NSObject
@property(nonatomic,copy) NSString *title,*summary,*identifier;
@property(nonatomic,copy) BOOL (^selectBlock)(id,NSUInteger);
@property(nonatomic,strong) id settingIcon;
+ (id)itemWithTitle:(id)title titleDescription:(id)description accessibilityIdentifier:(id)identifier detailTextBlock:(id(^)(void))detail selectBlock:(BOOL(^)(id,NSUInteger))select;
@end
@implementation YTSettingsSectionItem
+ (id)itemWithTitle:(id)title titleDescription:(id)description accessibilityIdentifier:(id)identifier detailTextBlock:(id(^)(void))detail selectBlock:(BOOL(^)(id,NSUInteger))select {
    assert(detail==nil);
    if (MalformedItem) return [NSObject new];
    YTSettingsSectionItem *item=[self new];item.title=title;item.summary=description;item.identifier=identifier;item.selectBlock=select;return item;
}
@end
@interface FixtureSection : NSObject
@property(nonatomic,strong) NSArray *items;
@property(nonatomic,strong) id icon;
@end
@implementation FixtureSection @end
@interface YTAppSettingsPresentationData : NSObject
+ (id)settingsCategoryOrder;
@end
@implementation YTAppSettingsPresentationData
+ (id)settingsCategoryOrder { return NativeOrder; }
@end
@interface YTSettingsGroupData : NSObject
@property(nonatomic) NSUInteger type;
- (id)orderedCategories;
@end
@implementation YTSettingsGroupData
- (id)orderedCategories { return NativeOrder; }
@end
@interface YTSettingsViewController : NSObject
@property(nonatomic,strong) id settingsSectionControllers;
@property(nonatomic) NSUInteger setCalls,loadCalls,sectionWrites;
- (void)setSectionItems:(id)items forCategory:(NSUInteger)category title:(id)title icon:(id)icon titleDescription:(id)description headerHidden:(bool)hidden;
- (void)setSectionControllers;
- (void)viewDidLoad;
@end
@implementation YTSettingsViewController
- (instancetype)init { if ((self=[super init])) _settingsSectionControllers=[NSMutableDictionary dictionary];return self; }
- (void)setSectionItems:(id)items forCategory:(NSUInteger)category title:(id)title icon:(id)icon titleDescription:(id)description headerHidden:(bool)hidden {
    assert(category==RVSettingsCategory && !title && icon==FixtureIcon && !description && hidden);
    assert([items[0] settingIcon]==FixtureIcon);
    self.sectionWrites++;FixtureSection *section=[FixtureSection new];section.items=items;section.icon=icon;self.settingsSectionControllers[@(category)]=section;
}
- (void)setSectionControllers {
    self.setCalls++;
    assert(NSThread.currentThread.threadDictionary[RVSettingsBuildScope]==self);
    if (ThrowBuild) @throw [NSException exceptionWithName:@"SettingsFixture" reason:nil userInfo:nil];
    if (NestedController) {
        id nested=NestedController;NestedController=nil;[nested setSectionControllers];
        assert(NSThread.currentThread.threadDictionary[RVSettingsBuildScope]==self);
    }
    LinearObserved=[YTAppSettingsPresentationData settingsCategoryOrder];
    YTSettingsGroupData *group=[YTSettingsGroupData new];group.type=2;GroupedObserved=[group orderedCategories];
    group.type=1;AccountObserved=[group orderedCategories];
    dispatch_semaphore_t done=dispatch_semaphore_create(0);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT,0),^{ @autoreleasepool {
        BackgroundObserved=[YTAppSettingsPresentationData settingsCategoryOrder];dispatch_semaphore_signal(done);
    } });
    assert(!dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC)));
}
- (void)viewDidLoad { self.loadCalls++; }
@end

int main(void) { @autoreleasepool {
    NSString *suite=[@"RVPort.SettingsFixture." stringByAppendingString:NSUUID.UUID.UUIDString];
    TestDefaults=[[NSUserDefaults alloc] initWithSuiteName:suite];
    Method defaults=class_getClassMethod(NSUserDefaults.class,@selector(standardUserDefaults));
    IMP originalDefaults=method_setImplementation(defaults,(IMP)FixtureDefaults);
    NSMutableDictionary *config=[NSMutableDictionary dictionary];
    NSDictionary *catalog=RVPreferencesCatalog();assert([catalog[@"groups"] count]==13 && [catalog[@"settings"] count]==116);
    for (NSString *key in catalog[@"settings"]) { NSDictionary *rule=RVPreferenceDescriptor(key);assert(RVPreferenceValid(rule[@"default"],rule));config[key]=rule[@"default"]; }
    config[@"schema"]=@1;config[@"app_name"]=@"Original app";RVConfig=config;
    FixtureIcon=[NSObject new];
    NSArray *rootRows=RVSettingsRows(nil,nil,nil,nil);
    assert([rootRows[0][@"rows"] count]==13 && [rootRows[1][@"rows"] count]==6);
    assert([rootRows isEqual:RVSettingsRows(nil,nil,nil,@" \n ")]);
    NSMutableSet *rowKeys=[NSMutableSet set];
    for (NSDictionary *group in catalog[@"groups"]) {
        NSArray *rows=RVSettingsRows(group[@"id"],nil,nil,nil)[0][@"rows"];
        assert(rows.count==[group[@"keys"] count]);
        for (NSDictionary *row in rows) { assert(![rowKeys containsObject:row[@"key"]]);[rowKeys addObject:row[@"key"]]; }
    }
    assert(rowKeys.count==[catalog[@"settings"] count]);
    NSArray *mapRows=RVSettingsRows(nil,@"sponsor_behaviors",nil,nil)[0][@"rows"];
    assert(mapRows.count==[RVPreferenceDescriptor(@"sponsor_behaviors")[@"map_keys"] count]);
    for (NSDictionary *row in mapRows) assert([row[@"key"] isEqual:@"sponsor_behaviors"] && row[@"member"] && row[@"detail"]);
    NSArray *listRows=RVSettingsRows(nil,nil,@"sponsor_categories",nil)[0][@"rows"];
    NSDictionary *toggleRow=listRows[0];assert([toggleRow[@"toggle"] boolValue]);
    assert(RVSettingsSaveToggle(toggleRow,NO));assert(![RVSetting(@"sponsor_categories") containsObject:toggleRow[@"member"]]);
    assert(RVSettingsSaveToggle(toggleRow,YES));assert([RVSetting(@"sponsor_categories") containsObject:toggleRow[@"member"]]);
    RVCompatible=NO;assert(!RVSettingsSaveToggle(toggleRow,NO));RVCompatible=YES;
    assert([RVSetting(@"sponsor_categories") containsObject:toggleRow[@"member"]]);
    assert(!RVSettingsSaveToggle(rootRows[0][@"rows"][0],YES));
    NSArray *matches=RVSettingsRows(nil,nil,nil,@"default speed")[0][@"rows"];
    assert(matches.count && [matches[0][@"hint"] length] && matches[0][@"key"]);
    assert([RVSettingsRows(nil,nil,nil,@"no_such_preference_xyz")[0][@"rows"][0][@"disabled"] boolValue]);
    [TestDefaults setObject:@"invalid" forKey:@"RVPort.default_speed"];
    assert([RVSettingsEffectiveValue(@"default_speed") isEqual:RVPreferenceDescriptor(@"default_speed")[@"default"]]);
    RVResetPreferences();
    assert(RVPreferenceValid(@YES,RVPreferenceDescriptor(@"video_ads")));
    assert(!RVPreferenceValid(@1,RVPreferenceDescriptor(@"video_ads")));
    assert(!RVPreferenceValid(@YES,RVPreferenceDescriptor(@"default_speed")));
    assert(!RVPreferenceValid(@(NAN),RVPreferenceDescriptor(@"default_speed")));
    assert(!RVPreferenceValid(@.24,RVPreferenceDescriptor(@"default_speed")));
    assert(!RVPreferenceValid(@4.1,RVPreferenceDescriptor(@"default_speed")));
    assert(RVPreferenceValid(@2.25,RVPreferenceDescriptor(@"default_speed")));
    assert(!RVPreferenceValid(@144.5,RVPreferenceDescriptor(@"wifi_quality")));
    assert(!RVPreferenceValid(@YES,RVPreferenceDescriptor(@"default_quality")));
    assert(RVPreferenceValid(@-1,RVPreferenceDescriptor(@"wifi_quality")));
    assert(!RVPreferenceValid(@169,RVPreferenceDescriptor(@"miniplayer_min_dimension_points")));
    assert(RVPreferenceValid(@0,RVPreferenceDescriptor(@"miniplayer_min_dimension_points")));
    assert(!RVPreferenceValid(@1.1,RVPreferenceDescriptor(@"overlay_opacity")));
    assert(!RVPreferenceValid(@"#GG1122",RVPreferenceDescriptor(@"seekbar_color")));
    assert(RVPreferenceValid(@"",RVPreferenceDescriptor(@"seekbar_color")));
    assert(RVPreferenceValid(@"#aB1234",RVPreferenceDescriptor(@"seekbar_color")));
    assert(!RVPreferenceValid(@"http://example.test/a",RVPreferenceDescriptor(@"thumbnail_proxy_url")));
    assert(!RVPreferenceValid(@"https://user:secret@example.test/a",RVPreferenceDescriptor(@"thumbnail_proxy_url")));
    assert(!RVPreferenceValid(@"https://example.test/a#fragment",RVPreferenceDescriptor(@"dearrow_url")));
    assert(!RVPreferenceValid(@"",RVPreferenceDescriptor(@"dearrow_url")));
    assert(RVPreferenceValid(@"https://example.test/a",RVPreferenceDescriptor(@"dearrow_url")));
    assert(!RVPreferenceValid(@"21.39",RVPreferenceDescriptor(@"client_version")));
    assert(!RVPreferenceValid(@[],RVPreferenceDescriptor(@"custom_speeds")));
    assert(!RVPreferenceValid(@[@YES],RVPreferenceDescriptor(@"custom_speeds")));
    assert(!RVPreferenceValid(@[@""],RVPreferenceDescriptor(@"feed_patterns")));
    assert(!RVPreferenceValid(@[@"unknown"],RVPreferenceDescriptor(@"sponsor_categories")));
    assert(!RVPreferenceValid(@{@"sponsor":@"mute"},RVPreferenceDescriptor(@"sponsor_behaviors")));
    assert(!RVPreferenceValid(@{@"sponsor":@""},RVPreferenceDescriptor(@"sponsor_colors")));
    assert(!RVPreferenceValid(@{@"unknown":@"stills"},RVPreferenceDescriptor(@"thumbnail_modes")));
    assert(RVPreferenceValid(@{@"home":@"dearrow-stills"},RVPreferenceDescriptor(@"thumbnail_modes")));
    assert(RVValidatePreferenceImport(@{@"schema":@YES}));assert(RVValidatePreferenceImport(@{@"schema":@2}));
    assert(RVValidatePreferenceImport(@{@"app_name":@42}));assert(RVValidatePreferenceImport(@[]));
    assert(!RVValidatePreferenceImport(RVEffectivePreferences()));
    assert([RVSearchPreferences(@"KeYcHaIn") containsObject:@"sideload_auth_keychain"]);
    assert([RVSearchPreferences(@"miniplayer") count]>=8);assert(!RVSearchPreferences(@"  ").count);
    assert(!RVSearchPreferences(@"not-a-setting-xyz").count);
    [TestDefaults setObject:@"synthetic account" forKey:@"SSO.account"];
    [TestDefaults setObject:@"synthetic service identity" forKey:@"RVPort.sponsorIdentity"];
    assert(RVSavePreference(@"video_ads",@NO));assert(![RVSetting(@"video_ads") boolValue]);
    assert(RVApplyPreferenceImport(@{@"video_ads":@YES,@"default_speed":@999}));assert(![RVSetting(@"video_ads") boolValue]);
    assert(RVApplyPreferenceImport(@{@"unknown_key":@YES}));
    assert(!RVApplyPreferenceImport(@{@"default_speed":@1.5,@"app_name":@"ignored runtime name"}));
    assert([RVSetting(@"default_speed") doubleValue]==1.5 && ![RVSetting(@"video_ads") boolValue]);
    assert([RVEffectivePreferences()[@"app_name"] isEqual:@"Original app"]);
    assert(!RVApplyPreferenceImport(@{@"screen_width_points":@1920.0,@"screen_height_points":@1080.0,@"wifi_quality":@144.0}));
    NSData *export=[NSJSONSerialization dataWithJSONObject:RVEffectivePreferences() options:0 error:nil];assert(export);
    printf("Settings export: %s\n",[[[NSString alloc] initWithData:export encoding:NSUTF8StringEncoding] UTF8String]);
    RVCompatible=NO;assert(!RVSavePreference(@"video_ads",@YES));assert(RVApplyPreferenceImport(@{}));RVResetPreferences();assert(![RVSetting(@"video_ads") boolValue]);RVCompatible=YES;
    RVResetPreferences();assert([RVSetting(@"video_ads") boolValue]==[config[@"video_ads"] boolValue]);
    assert([[TestDefaults objectForKey:@"SSO.account"] isEqual:@"synthetic account"]);
    assert([[TestDefaults objectForKey:@"RVPort.sponsorIdentity"] isEqual:@"synthetic service identity"]);
    [TestDefaults setObject:@999 forKey:@"RVPort.default_speed"];
    assert([RVEffectivePreferences()[@"default_speed"] isEqual:config[@"default_speed"]]);RVResetPreferences();
    RVStatus=[NSMutableArray array];NativeOrder=@[@1,@2,@4];RVInstallSettingsBridge();assert(RVStatus.count==4);
    for (NSString *status in RVStatus) assert([status hasPrefix:@"Installed "]);
    assert([YTAppSettingsPresentationData settingsCategoryOrder]==NativeOrder);
    YTSettingsViewController *root=[YTSettingsViewController new];[root viewDidLoad];
    assert(root.loadCalls==1 && root.setCalls==1 && root.sectionWrites==1);
    assert(LinearObserved.count==4 && [LinearObserved.lastObject unsignedIntegerValue]==RVSettingsCategory);
    assert(GroupedObserved.count==4 && [GroupedObserved.lastObject unsignedIntegerValue]==RVSettingsCategory);
    assert(AccountObserved==NativeOrder && BackgroundObserved==NativeOrder && NativeOrder.count==3);
    FixtureSection *section=root.settingsSectionControllers[@(RVSettingsCategory)];
    YTSettingsSectionItem *item=section.items[0];assert([item.title isEqual:@"ReVanced"] && [item.identifier isEqual:@"rvport.settings"]);
    assert(item.selectBlock(nil,0) && PresentedController==root);CanPresent=NO;assert(!item.selectBlock(nil,0));CanPresent=YES;
    [root setSectionControllers];assert(root.sectionWrites==1 && root.setCalls==2);
    assert(!NSThread.currentThread.threadDictionary[RVSettingsBuildScope]);
    NestedController=[YTSettingsViewController new];[root setSectionControllers];assert(!NSThread.currentThread.threadDictionary[RVSettingsBuildScope]);
    ThrowBuild=YES;@try { [root setSectionControllers];assert(false); } @catch (NSException *exception) { assert([exception.name isEqual:@"SettingsFixture"]); }ThrowBuild=NO;
    assert(!NSThread.currentThread.threadDictionary[RVSettingsBuildScope]);assert([YTAppSettingsPresentationData settingsCategoryOrder]==NativeOrder);
    NativeOrder=@[@1,@(RVSettingsCategory)];[root setSectionControllers];assert(LinearObserved==NativeOrder && GroupedObserved==NativeOrder);
    NativeOrder=@[@"invalid shape"];[root setSectionControllers];assert(LinearObserved==NativeOrder);
    NativeOrder=@[@1,@2,@4];
    YTSettingsViewController *foreign=[YTSettingsViewController new];NSObject *existing=[NSObject new];foreign.settingsSectionControllers[@(RVSettingsCategory)]=existing;
    [foreign setSectionControllers];assert(foreign.sectionWrites==0 && foreign.settingsSectionControllers[@(RVSettingsCategory)]==existing && LinearObserved==NativeOrder);
    YTSettingsViewController *malformed=[YTSettingsViewController new];malformed.settingsSectionControllers=NSNull.null;[malformed setSectionControllers];assert(!malformed.sectionWrites && LinearObserved==NativeOrder);
    MalformedItem=YES;YTSettingsViewController *badFactory=[YTSettingsViewController new];[badFactory setSectionControllers];assert(!badFactory.sectionWrites && LinearObserved==NativeOrder);MalformedItem=NO;
    RVCompatible=NO;YTSettingsViewController *off=[YTSettingsViewController new];[off setSectionControllers];assert(!off.sectionWrites && LinearObserved==NativeOrder);assert(!item.selectBlock(nil,0));RVCompatible=YES;
    __weak id weakRoot;
    YTSettingsSectionItem *orphan;
    @autoreleasepool { YTSettingsViewController *temporary=[YTSettingsViewController new];weakRoot=temporary;[temporary setSectionControllers];FixtureSection *owned=temporary.settingsSectionControllers[@(RVSettingsCategory)];orphan=owned.items[0]; }
    assert(!weakRoot && !orphan.selectBlock(nil,0));
    method_setImplementation(defaults,originalDefaults);[TestDefaults removePersistentDomainForName:suite];
    puts("Native settings model and menu bridge contract checks passed.");
}return 0; }
