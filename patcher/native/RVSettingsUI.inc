// SPDX-License-Identifier: MIT
#include "RVSettingsRows.inc"
#include "RVSettingsNativeUI.inc"

static NSNumber *RVPreferenceParseNumber(NSString *text) {
    NSScanner *scanner=[NSScanner scannerWithString:[text stringByReplacingOccurrencesOfString:@"," withString:@"."]];
    scanner.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    double value;
    return [scanner scanDouble:&value] && scanner.isAtEnd && isfinite(value) ? @(value) : nil;
}
static void RVPreferencesChanged(void) { RVSPRefreshUI();RVShortsRefreshNavigation();RVRefreshShortcuts();RVSegmentsChanged();RVNativeDislikeRefresh();RVAdPolicyChanged(); }

@interface RVPreferenceTextController : RVSettingsContentController
@property(nonatomic,strong) UITextView *editor;
@property(nonatomic,copy) NSString *initialText;
@property(nonatomic,copy) NSString *hint;
@property(nonatomic,copy) NSString *(^saveText)(NSString *);
@end
@implementation RVPreferenceTextController
- (void)viewDidLoad {
    [super viewDidLoad];RVSettingsRefreshAppearance(self);
    UILabel *hint=[UILabel new];hint.text=self.hint;hint.numberOfLines=0;hint.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];hint.textColor=RVSettingsColor(self,"textSecondary");
    self.editor=[UITextView new];self.editor.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];self.editor.adjustsFontForContentSizeCategory=YES;
    self.editor.backgroundColor=RVSettingsColor(self,"baseBackground");self.editor.textColor=RVSettingsColor(self,"textPrimary");
    self.editor.text=self.initialText ?: @"";self.editor.autocorrectionType=UITextAutocorrectionTypeNo;self.editor.autocapitalizationType=UITextAutocapitalizationTypeNone;
    hint.translatesAutoresizingMaskIntoConstraints=NO;self.editor.translatesAutoresizingMaskIntoConstraints=NO;
    [self.view addSubview:hint];[self.view addSubview:self.editor];
    UILayoutGuide *guide=self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[hint.topAnchor constraintEqualToAnchor:guide.topAnchor constant:12],
        [hint.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:16],[hint.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-16],
        [self.editor.topAnchor constraintEqualToAnchor:hint.bottomAnchor constant:8],[self.editor.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:12],
        [self.editor.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-12],[self.editor.bottomAnchor constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor]]];
    RVSettingsOwner(self).navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemSave target:self action:@selector(save)];
}
- (void)save {
    if ([self.editor.text lengthOfBytesUsingEncoding:NSUTF8StringEncoding]>128*1024) { [self showError:@"The text is too large (maximum 128 KB)."];return; }
    NSString *error=self.saveText ? self.saveText(self.editor.text) : @"No setting selected.";
    if (error) { [self showError:error];return; }
    RVPreferencesChanged();RVSettingsPop(self);
}
- (void)showError:(NSString *)message {
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Unable to save" message:message preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil]];[RVSettingsOwner(self) presentViewController:alert animated:YES completion:nil];
}
@end

@interface RVSettingsController : RVSettingsContentController <UISearchBarDelegate>
@property(nonatomic,copy) NSString *groupID,*mapKey,*listKey;
@property(nonatomic,strong) RVSettingsListController *list;
@property(nonatomic,strong) UISearchBar *searchBar;
@property(nonatomic) CGPoint rootScrollOffset;
@property(nonatomic) BOOL modalEntry;
@end
@implementation RVSettingsController
- (NSDictionary *)group {
    for (NSDictionary *group in RVPreferencesCatalog()[@"groups"]) if ([group[@"id"] isEqual:self.groupID]) return group;
    return nil;
}
- (BOOL)isRoot { return !self.groupID && !self.mapKey && !self.listKey; }
- (BOOL)isSearching { return [self.searchBar.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet].length>0; }
- (void)viewDidLoad {
    [super viewDidLoad];self.list=[RVSettingsListController new];__weak typeof(self) weakSelf=self;
    self.list.selected=^(NSDictionary *row) { [weakSelf selectRow:row]; };
    self.list.changed=^BOOL(NSDictionary *row,BOOL on) {
        BOOL saved=RVSettingsSaveToggle(row,on);if (saved) RVPreferencesChanged();return saved;
    };
    [self addChildViewController:self.list];self.list.view.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:self.list.view];
    [self.list didMoveToParentViewController:self];UILayoutGuide *safe=self.view.safeAreaLayoutGuide;
    NSLayoutYAxisAnchor *top=safe.topAnchor;
    if (self.isRoot) {
        self.searchBar=[UISearchBar new];self.searchBar.delegate=self;self.searchBar.placeholder=@"Search patch settings";
        self.searchBar.searchBarStyle=UISearchBarStyleMinimal;self.searchBar.autocapitalizationType=UITextAutocapitalizationTypeNone;
        self.searchBar.autocorrectionType=UITextAutocorrectionTypeNo;self.searchBar.translatesAutoresizingMaskIntoConstraints=NO;
        [self.view addSubview:self.searchBar];
        [NSLayoutConstraint activateConstraints:@[[self.searchBar.topAnchor constraintEqualToAnchor:safe.topAnchor],
            [self.searchBar.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:8],
            [self.searchBar.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-8]]];
        top=self.searchBar.bottomAnchor;
    }
    [NSLayoutConstraint activateConstraints:@[[self.list.view.topAnchor constraintEqualToAnchor:top],
        [self.list.view.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[self.list.view.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],
        [self.list.view.bottomAnchor constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor]]];
    [self applyAppearance];[self reloadRows];
}
- (void)viewWillAppear:(BOOL)animated { [super viewWillAppear:animated];[self applyAppearance];[self reloadRows]; }
- (void)viewWillDisappear:(BOOL)animated { [super viewWillDisappear:animated];[self.view endEditing:YES]; }
- (void)traitCollectionDidChange:(UITraitCollection *)previous {
    // Preserve the inherited UIKit callback with both the pinned iOS 16 SDK
    // and newer Xcode SDKs. Suppression is confined to this compatibility call.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [super traitCollectionDidChange:previous];
#pragma clang diagnostic pop
    if ([self.traitCollection hasDifferentColorAppearanceComparedToTraitCollection:previous]) { [self applyAppearance];[self reloadRows]; }
    if (![self.traitCollection.preferredContentSizeCategory isEqual:previous.preferredContentSizeCategory]) [self.list.collection.collectionViewLayout invalidateLayout];
}
- (void)applyAppearance {
    RVSettingsRefreshAppearance(self);self.searchBar.searchTextField.textColor=RVSettingsColor(self,"textPrimary");
    self.searchBar.tintColor=[UIColor colorWithRed:.24 green:.61 blue:.98 alpha:1];
    self.searchBar.searchTextField.backgroundColor=RVSettingsPageStyle(self)==1 ? [UIColor colorWithWhite:.15 alpha:1] : [UIColor colorWithWhite:.94 alpha:1];
}
- (void)done { [RVSettingsOwner(self) dismissViewControllerAnimated:YES completion:nil]; }
- (void)reloadRows { [self.list reloadSections:RVSettingsRows(self.groupID,self.mapKey,self.listKey,self.searchBar.text ?: @"")]; }
- (void)searchBarTextDidBeginEditing:(UISearchBar *)bar {
    if (!self.isSearching) self.rootScrollOffset=self.list.collection.contentOffset;
    [bar setShowsCancelButton:YES animated:YES];
}
- (void)searchBar:(UISearchBar *)bar textDidChange:(NSString *)text {
    (void)bar;(void)text;[self reloadRows];
    if (!self.isSearching) [self.list.collection setContentOffset:self.rootScrollOffset animated:NO];
    else [self.list.collection setContentOffset:CGPointMake(0,-self.list.collection.adjustedContentInset.top) animated:NO];
}
- (void)searchBarSearchButtonClicked:(UISearchBar *)bar { [bar resignFirstResponder]; }
- (void)searchBarCancelButtonClicked:(UISearchBar *)bar {
    bar.text=@"";[bar resignFirstResponder];[bar setShowsCancelButton:NO animated:YES];[self reloadRows];
    [self.list.collection setContentOffset:self.rootScrollOffset animated:NO];
}
- (void)searchBarTextDidEndEditing:(UISearchBar *)bar { if (!self.isSearching) [bar setShowsCancelButton:NO animated:YES]; }
- (void)message:(NSString *)title text:(NSString *)text {
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:title message:text preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil]];[self presentPreferenceAlert:alert];
}
- (void)presentPreferenceAlert:(UIAlertController *)alert {
    [self.view endEditing:YES];UIViewController *presenter=RVSettingsOwner(self);
    UIViewController *previous=presenter.presentedViewController;
    if ([previous isKindOfClass:UIAlertController.class]) {
        // Picker actions can open an editor or validation message while the picker is closing.
        [previous dismissViewControllerAnimated:YES completion:^{
            if (presenter.view.window && !presenter.presentedViewController) [presenter presentViewController:alert animated:YES completion:nil];
        }];
    } else if (!previous) [presenter presentViewController:alert animated:YES completion:nil];
}
- (void)write:(NSString *)key value:(id)value {
    if (!RVSavePreference(key,value)) { [self message:@"Invalid setting" text:@"The value is outside the supported range or format."];return; }
    RVPreferencesChanged();[self applyAppearance];[self reloadRows];
}
- (void)choose:(NSString *)key member:(NSString *)member {
    NSDictionary *rule=RVPreferenceDescriptor(key);NSArray *values=rule[@"choices"] ?: rule[@"options"];
    UIAlertController *picker=[UIAlertController alertControllerWithTitle:member ? RVPreferenceMemberTitle(member) : rule[@"title"] message:rule[@"hint"] preferredStyle:UIAlertControllerStyleActionSheet];
    void (^save)(id)=^(id value) {
        if (!member) { [self write:key value:value];return; }
        NSMutableDictionary *map=[(RVSetting(key) ?: @{}) mutableCopy];
        if (value==NSNull.null) [map removeObjectForKey:member];else map[member]=value;
        [self write:key value:map];
    };
    if (member) [picker addAction:[UIAlertAction actionWithTitle:@"Default" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { save(NSNull.null); }]];
    for (id value in values) [picker addAction:[UIAlertAction actionWithTitle:RVPreferenceValueTitle(value,rule) style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { save(value); }]];
    if (!values || [rule[@"kind"] isEqual:@"number"]) [picker addAction:[UIAlertAction actionWithTitle:@"Custom value" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) { [self editScalar:key member:member]; }]];
    [picker addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    picker.popoverPresentationController.sourceView=self.view;picker.popoverPresentationController.sourceRect=CGRectMake(CGRectGetMidX(self.view.bounds),CGRectGetMidY(self.view.bounds),1,1);
    [self presentPreferenceAlert:picker];
}
- (void)editScalar:(NSString *)key member:(NSString *)member {
    NSDictionary *rule=RVPreferenceDescriptor(key);BOOL numeric=[rule[@"kind"] isEqual:@"number"];
    NSString *hint=rule[@"hint"];
    if (numeric) hint=[NSString stringWithFormat:@"Supported range: %@–%@%@.",rule[@"min"],rule[@"max"],[rule[@"allow_zero"] boolValue] ? @"; 0 keeps native behavior" : @""];
    UIAlertController *editor=[UIAlertController alertControllerWithTitle:member ? RVPreferenceMemberTitle(member) : rule[@"title"] message:hint preferredStyle:UIAlertControllerStyleAlert];
    [editor addTextFieldWithConfigurationHandler:^(UITextField *field) {
        id value=member ? RVSetting(key)[member] : RVSetting(key);field.text=value ? [value description] : @"";
        field.autocapitalizationType=UITextAutocapitalizationTypeNone;field.autocorrectionType=UITextAutocorrectionTypeNo;
        field.keyboardType=numeric ? UIKeyboardTypeDecimalPad : [rule[@"url"] boolValue] ? UIKeyboardTypeURL : UIKeyboardTypeDefault;
    }];
    [editor addAction:[UIAlertAction actionWithTitle:@"Save" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSString *text=editor.textFields.firstObject.text ?: @"";id value=numeric ? RVPreferenceParseNumber(text) : text;
        if (member) { NSMutableDictionary *map=[(RVSetting(key) ?: @{}) mutableCopy];if (!text.length) [map removeObjectForKey:member];else map[member]=text;value=map; }
        [self write:key value:value];
    }]];
    [editor addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[self presentPreferenceAlert:editor];
}
- (void)editList:(NSString *)key {
    NSDictionary *rule=RVPreferenceDescriptor(key);BOOL numeric=[rule[@"kind"] isEqual:@"number_list"];
    RVPreferenceTextController *editor=[RVPreferenceTextController new];editor.title=rule[@"title"];
    editor.hint=numeric ? @"One playback speed per line, between 0.25 and 4. Up to 30 choices." : rule[@"hint"];
    NSMutableArray *lines=[NSMutableArray array];for (id value in RVSetting(key)) [lines addObject:[value description]];
    editor.initialText=[lines componentsJoinedByString:@"\n"];
    editor.saveText=^NSString *(NSString *text) {
        NSMutableArray *values=[NSMutableArray array];
        for (NSString *line in [text componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet]) {
            NSString *entry=[line stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];if (!entry.length) continue;
            id value=numeric ? RVPreferenceParseNumber(entry) : entry;if (!value) return @"A line is not a valid number.";[values addObject:value];
        }
        return RVSavePreference(key,values) ? nil : @"The list has unsupported values or too many entries.";
    };
    RVSettingsPush(self,editor);
}
- (void)diagnostics {
    NSString *text;@synchronized(RVStatus) { text=[RVStatus componentsJoinedByString:@"\n"]; }
    text=[RVDislikeCheckpointSummary() stringByAppendingString:text ?: @""];
    text=[RVSpeedCheckpointSummary() stringByAppendingString:text ?: @""];
    text=[RVAdSummary() stringByAppendingString:text ?: @""];
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Hook diagnostics" message:text preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Copy diagnostic report" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSArray *hooks;@synchronized(RVStatus) { hooks=[RVStatus copy]; }
        NSDictionary *report=@{@"patcher_version":RV_PORT_VERSION,@"release_flavor":@"merged-navigation-options",@"build_source_sha256":RV_BUILD_SOURCE_SHA256,@"profile_accepted":@(RVCompatible),@"youtube_version":NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"] ?: @"",@"ios_version":UIDevice.currentDevice.systemVersion,@"device_model":UIDevice.currentDevice.model,@"effective_config":RVEffectivePreferences(),@"hooks":hooks,@"authentication":RVAuthenticationReport(),@"sponsorblock":RVSponsorReport(),@"sponsorblock_prompts":RVSPReport(),@"shorts_layout":RVShortsReport(),@"return_youtube_dislike":RVNativeDislikeReport(),@"ryd_checkpoints":RVDislikeCheckpointReport(),@"playback_speed":RVSpeedReport(),@"adblock":RVAdReport()};
        NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
        if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Copy RYD checkpoints" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSData *data=[NSJSONSerialization dataWithJSONObject:RVDislikeCheckpointReport() options:NSJSONWritingPrettyPrinted error:nil];
        if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Copy speed checkpoints" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSDictionary *report=@{@"playback_speed":RVSpeedReport()};
        NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
        if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Copy ad-block checkpoints" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSData *data=[NSJSONSerialization dataWithJSONObject:RVAdReport() options:NSJSONWritingPrettyPrinted error:nil];
        if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Copy SponsorBlock prompt checkpoints" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSData *data=[NSJSONSerialization dataWithJSONObject:RVSPReport() options:NSJSONWritingPrettyPrinted error:nil];
        if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Copy navigation and Shorts checkpoints" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSData *data=[NSJSONSerialization dataWithJSONObject:RVShortsReport() options:NSJSONWritingPrettyPrinted error:nil];
        if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleCancel handler:nil]];[self presentPreferenceAlert:alert];
}
- (void)selectRow:(NSDictionary *)row {
    [self.view endEditing:YES];
    if (row[@"group"] || row[@"tool"]) {
        if (row[@"group"]) { RVSettingsController *page=[RVSettingsController new];page.groupID=row[@"group"];page.title=page.group[@"title"];RVSettingsPush(self,page);return; }
        NSUInteger tool=[row[@"tool"] unsignedIntegerValue];
        if (tool==0) { RVShowTools(RVSettingsOwner(self));return; }
        if (tool==1) {
            RVPreferenceTextController *editor=[RVPreferenceTextController new];editor.title=@"Import configuration";editor.hint=@"Paste a configuration exported by this iOS patcher. Only supplied settings change; all values are checked before saving. Android preference files use a different format.";
            editor.initialText=@"";editor.saveText=^NSString *(NSString *text) { NSData *data=[text dataUsingEncoding:NSUTF8StringEncoding];id values=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;return RVApplyPreferenceImport(values); };
            RVSettingsPush(self,editor);return;
        }
        if (tool==2) { NSData *data=[NSJSONSerialization dataWithJSONObject:RVEffectivePreferences() options:NSJSONWritingPrettyPrinted error:nil];if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];[self message:@"Configuration copied" text:@"You can paste it into another installation or the Windows patcher."];return; }
        if (tool==3) {
            UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Reset patch settings?" message:@"Restore the configuration bundled in this IPA. Accounts, credentials and service identities are retained." preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"Reset settings" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) { RVResetPreferences();RVPreferencesChanged();[self applyAppearance];[self reloadRows]; }]];
            [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[self presentPreferenceAlert:alert];return;
        }
        if (tool==4) { [self diagnostics];return; }
        [self message:[@"ReVanced iOS " stringByAppendingString:RV_PORT_VERSION] text:[NSString stringWithFormat:@"YouTube %@\n%lu patch switches and grouped preferences.\n%@",NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"] ?: @"",(unsigned long)RVFeatureKeys.count,RVCompatible ? @"Supported app profile." : @"Unsupported app: patches disabled."]];return;
    }
    if (!RVCompatible || [row[@"disabled"] boolValue] || [row[@"toggle"] boolValue]) return;
    NSString *key=row[@"key"];if (!key) return;
    if (self.mapKey) { [self choose:self.mapKey member:row[@"member"]];return; }
    NSDictionary *rule=RVPreferenceDescriptor(key);NSString *kind=rule[@"kind"];
    if ([kind isEqual:@"bool"]) return;
    if ([kind isEqual:@"map"] || rule[@"allowed"]) {
        RVSettingsController *page=[RVSettingsController new];
        if ([kind isEqual:@"map"]) page.mapKey=key;else page.listKey=key;
        page.title=rule[@"title"];RVSettingsPush(self,page);
    } else if ([kind hasSuffix:@"_list"]) [self editList:key];
    else if ([kind isEqual:@"string"]) [self editScalar:key member:nil];
    else [self choose:key member:nil];
}
@end

static BOOL RVPresentSettings(id presenter) {
    if (!RVCompatible || !NSThread.isMainThread || ![presenter isKindOfClass:UIViewController.class]) return NO;
    UIViewController *source=presenter;
    if (!source.isViewLoaded || !source.view.window || source.presentedViewController) return NO;
    RVSettingsController *content=[RVSettingsController new];content.title=@"ReVanced";
    UIViewController *page=RVSettingsWrap(content,source);
    if (RVCan(source,"showOrPushViewController:",@"v@:@")) ((void(*)(id,SEL,id))objc_msgSend)(source,sel_registerName("showOrPushViewController:"),page);
    else if (source.navigationController) [source.navigationController pushViewController:page animated:YES];
    else { content.modalEntry=YES;page.navigationItem.leftBarButtonItem=[[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"xmark"] style:UIBarButtonItemStylePlain target:content action:@selector(done)];[source presentViewController:[[UINavigationController alloc] initWithRootViewController:page] animated:YES completion:nil]; }
    return YES;
}
