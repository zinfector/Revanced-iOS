// SPDX-License-Identifier: MIT
#include "RVSettingsModel.inc"

static const void *RVPreferenceToggleKey=&RVPreferenceToggleKey;
static NSString *RVPreferenceValueTitle(id value,NSDictionary *rule) {
    NSUInteger index=[rule[@"choices"] indexOfObject:value];
    NSArray *labels=rule[@"choice_labels"];
    if (labels && index<labels.count) return labels[index];
    if ([value isKindOfClass:NSDictionary.class]) return [NSString stringWithFormat:@"%lu overrides",(unsigned long)[value count]];
    if ([value isKindOfClass:NSArray.class]) return [NSString stringWithFormat:@"%lu choices",(unsigned long)[value count]];
    if ([value isKindOfClass:NSNumber.class]) {
        if ([value doubleValue]==0 && rule[@"zero_label"]) return rule[@"zero_label"];
        return [NSString stringWithFormat:@"%@%@%@",value,rule[@"unit"] ? @" " : @"",rule[@"unit"] ?: @""];
    }
    return [value length] ? value : @"Native / default";
}
static NSString *RVPreferenceMemberTitle(NSString *key) {
    NSDictionary *names=@{@"home":@"Home",@"subscriptions":@"Subscriptions",@"library":@"Library",@"player":@"Player",@"search":@"Search",
        @"sponsor":@"Sponsor",@"selfpromo":@"Self-promotion",@"interaction":@"Interaction reminder",@"intro":@"Intro",@"outro":@"Outro",
        @"preview":@"Preview",@"music_offtopic":@"Off-topic music",@"filler":@"Filler",@"hook":@"Hook",@"poi_highlight":@"Highlight"};
    return names[key] ?: key.capitalizedString;
}
static NSNumber *RVPreferenceParseNumber(NSString *text) {
    NSScanner *scanner=[NSScanner scannerWithString:[text stringByReplacingOccurrencesOfString:@"," withString:@"."]];
    scanner.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    double value;
    return [scanner scanDouble:&value] && scanner.isAtEnd && isfinite(value) ? @(value) : nil;
}
static void RVPreferencesChanged(void) { RVRefreshShortcuts();RVSegmentsChanged(); }

@interface RVPreferenceTextController : UIViewController
@property(nonatomic,strong) UITextView *editor;
@property(nonatomic,copy) NSString *initialText;
@property(nonatomic,copy) NSString *hint;
@property(nonatomic,copy) NSString *(^saveText)(NSString *);
@end
@implementation RVPreferenceTextController
- (void)viewDidLoad {
    [super viewDidLoad];self.view.backgroundColor=UIColor.systemBackgroundColor;
    UILabel *hint=[UILabel new];hint.text=self.hint;hint.numberOfLines=0;hint.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];hint.textColor=UIColor.secondaryLabelColor;
    self.editor=[UITextView new];self.editor.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];self.editor.adjustsFontForContentSizeCategory=YES;
    self.editor.text=self.initialText ?: @"";self.editor.autocorrectionType=UITextAutocorrectionTypeNo;self.editor.autocapitalizationType=UITextAutocapitalizationTypeNone;
    hint.translatesAutoresizingMaskIntoConstraints=NO;self.editor.translatesAutoresizingMaskIntoConstraints=NO;
    [self.view addSubview:hint];[self.view addSubview:self.editor];
    UILayoutGuide *guide=self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[hint.topAnchor constraintEqualToAnchor:guide.topAnchor constant:12],
        [hint.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:16],[hint.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-16],
        [self.editor.topAnchor constraintEqualToAnchor:hint.bottomAnchor constant:8],[self.editor.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:12],
        [self.editor.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-12],[self.editor.bottomAnchor constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor]]];
    self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemSave target:self action:@selector(save)];
}
- (void)save {
    if ([self.editor.text lengthOfBytesUsingEncoding:NSUTF8StringEncoding]>128*1024) { [self showError:@"The text is too large (maximum 128 KB)."];return; }
    NSString *error=self.saveText ? self.saveText(self.editor.text) : @"No setting selected.";
    if (error) { [self showError:error];return; }
    RVPreferencesChanged();[self.navigationController popViewControllerAnimated:YES];
}
- (void)showError:(NSString *)message {
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Unable to save" message:message preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil]];[self presentViewController:alert animated:YES completion:nil];
}
@end

@interface RVSettingsController : UITableViewController <UISearchResultsUpdating>
@property(nonatomic,copy) NSString *groupID;
@property(nonatomic,copy) NSString *mapKey;
@property(nonatomic,copy) NSString *listKey;
@property(nonatomic,strong) NSArray *keys;
@property(nonatomic,strong) NSArray *searchKeys;
@property(nonatomic,strong) UISearchController *search;
@property(nonatomic) BOOL modalEntry;
@end
@implementation RVSettingsController
- (NSDictionary *)group {
    for (NSDictionary *group in RVPreferencesCatalog()[@"groups"]) if ([group[@"id"] isEqual:self.groupID]) return group;
    return nil;
}
- (BOOL)isRoot { return !self.groupID && !self.mapKey && !self.listKey; }
- (BOOL)isSearching { return self.search.isActive && [self.search.searchBar.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet].length>0; }
- (void)viewDidLoad {
    [super viewDidLoad];self.tableView.rowHeight=UITableViewAutomaticDimension;self.tableView.estimatedRowHeight=64;
    if (self.isRoot) {
        self.title=@"ReVanced";self.definesPresentationContext=YES;
        self.search=[[UISearchController alloc] initWithSearchResultsController:nil];self.search.searchResultsUpdater=self;
        self.search.obscuresBackgroundDuringPresentation=NO;self.search.searchBar.placeholder=@"Search patch settings";
        self.navigationItem.searchController=self.search;self.navigationItem.hidesSearchBarWhenScrolling=NO;
        if (self.modalEntry) self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done)];
    } else if (self.mapKey) { self.title=RVPreferenceDescriptor(self.mapKey)[@"title"];self.keys=RVPreferenceDescriptor(self.mapKey)[@"map_keys"]; }
    else if (self.listKey) { self.title=RVPreferenceDescriptor(self.listKey)[@"title"];self.keys=RVPreferenceDescriptor(self.listKey)[@"allowed"]; }
    else { self.title=self.group[@"title"];self.keys=self.group[@"keys"]; }
    [self applyAppearance];
}
- (void)viewWillAppear:(BOOL)animated { [super viewWillAppear:animated];[self applyAppearance];[self.tableView reloadData]; }
- (void)applyAppearance {
    NSString *theme=RVSetting(@"theme");self.overrideUserInterfaceStyle=[theme isEqual:@"dark"] ? UIUserInterfaceStyleDark : [theme isEqual:@"light"] ? UIUserInterfaceStyleLight : UIUserInterfaceStyleUnspecified;
}
- (void)done { [self dismissViewControllerAnimated:YES completion:nil]; }
- (void)updateSearchResultsForSearchController:(UISearchController *)searchController { self.searchKeys=RVSearchPreferences(searchController.searchBar.text ?: @"");[self.tableView reloadData]; }
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return self.isRoot && !self.isSearching ? 2 : 1; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    if (self.isSearching) return self.searchKeys.count;
    if (self.isRoot) return section==0 ? [RVPreferencesCatalog()[@"groups"] count] : 6;
    return self.keys.count;
}
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section { return self.isRoot && !self.isSearching ? (section==0 ? @"Patch settings" : @"Tools and configuration") : nil; }
- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    if (self.isRoot && !self.isSearching) return section==0 ? @"Changes are saved on this device. Reopen the affected video or screen to apply them. Restart after changing authentication settings." : @"Reset restores the configuration bundled in this IPA and leaves accounts and service identities intact.";
    if (self.mapKey) return @"Default removes this per-item override. Global settings then apply.";
    if (self.listKey) return @"Enabled categories are requested when the next video is opened.";
    return self.group[@"hint"];
}
- (NSString *)keyAtIndex:(NSIndexPath *)index { return self.isSearching ? self.searchKeys[index.row] : self.keys[index.row]; }
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)index {
    UITableViewCell *cell=[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    cell.textLabel.numberOfLines=0;cell.detailTextLabel.numberOfLines=0;cell.textLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    cell.detailTextLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];cell.detailTextLabel.textColor=UIColor.secondaryLabelColor;
    if (self.isRoot && !self.isSearching) {
        if (index.section==0) { NSDictionary *group=RVPreferencesCatalog()[@"groups"][index.row];cell.textLabel.text=group[@"title"];cell.detailTextLabel.text=[NSString stringWithFormat:@"%lu settings",(unsigned long)[group[@"keys"] count]]; }
        else cell.textLabel.text=@[@"Video tools",@"Import configuration",@"Copy configuration",@"Reset patch settings",@"Hook diagnostics",@"About"] [index.row];
        cell.accessoryType=UITableViewCellAccessoryDisclosureIndicator;return cell;
    }
    NSString *key=[self keyAtIndex:index];
    if (self.mapKey) { id value=RVSetting(self.mapKey)[key];cell.textLabel.text=RVPreferenceMemberTitle(key);cell.detailTextLabel.text=value ? RVPreferenceValueTitle(value,RVPreferenceDescriptor(self.mapKey)) : @"Default";cell.accessoryType=UITableViewCellAccessoryDisclosureIndicator;return cell; }
    NSDictionary *rule=RVPreferenceDescriptor(self.listKey ?: key);
    cell.textLabel.text=self.listKey ? RVPreferenceMemberTitle(key) : rule[@"title"];cell.accessibilityIdentifier=[@"rvport.pref." stringByAppendingString:self.listKey ?: key];
    if (self.listKey || [rule[@"kind"] isEqual:@"bool"]) {
        UISwitch *toggle=[UISwitch new];toggle.on=self.listKey ? [RVSetting(self.listKey) containsObject:key] : [RVSetting(key) boolValue];toggle.enabled=RVCompatible;
        objc_setAssociatedObject(toggle,RVPreferenceToggleKey,@{@"key":self.listKey ?: key,@"entry":self.listKey ? key : @""},OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        toggle.accessibilityLabel=cell.textLabel.text;[toggle addTarget:self action:@selector(toggled:) forControlEvents:UIControlEventValueChanged];cell.accessoryView=toggle;cell.selectionStyle=UITableViewCellSelectionStyleNone;
        cell.detailTextLabel.text=rule[@"hint"];
    } else {
        NSString *value=RVPreferenceValueTitle(RVSetting(key) ?: rule[@"default"],rule);
        cell.detailTextLabel.text=[rule[@"hint"] length] ? [NSString stringWithFormat:@"%@\n%@",value,rule[@"hint"]] : value;
        cell.accessoryType=UITableViewCellAccessoryDisclosureIndicator;
    }
    cell.userInteractionEnabled=RVCompatible;return cell;
}
- (void)toggled:(UISwitch *)toggle {
    NSDictionary *target=objc_getAssociatedObject(toggle,RVPreferenceToggleKey);NSString *key=target[@"key"],*entry=target[@"entry"];
    id value=@(toggle.on);
    if (entry.length) {
        NSMutableArray *items=[(RVSetting(key) ?: @[]) mutableCopy];
        [items removeObject:entry];if (toggle.on) [items addObject:entry];value=items;
    }
    if (!RVSavePreference(key,value)) toggle.on=!toggle.on;
    RVPreferencesChanged();
}
- (void)message:(NSString *)title text:(NSString *)text {
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:title message:text preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil]];[self presentPreferenceAlert:alert];
}
- (void)presentPreferenceAlert:(UIAlertController *)alert {
    // An active UISearchController owns presentation while editing a search result.
    UIViewController *presenter=self.search.isActive && self.search.isViewLoaded && self.search.view.window ? self.search : self;
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
    RVPreferencesChanged();[self applyAppearance];[self.tableView reloadData];
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
    [self.navigationController pushViewController:editor animated:YES];
}
- (void)diagnostics {
    NSString *text;@synchronized(RVStatus) { text=[RVStatus componentsJoinedByString:@"\n"]; }
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Hook diagnostics" message:text preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Copy diagnostic report" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        NSArray *hooks;@synchronized(RVStatus) { hooks=[RVStatus copy]; }
        NSDictionary *report=@{@"patcher_version":@"0.3.7",@"profile_accepted":@(RVCompatible),@"youtube_version":NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"] ?: @"",@"ios_version":UIDevice.currentDevice.systemVersion,@"device_model":UIDevice.currentDevice.model,@"effective_config":RVEffectivePreferences(),@"hooks":hooks,@"authentication":RVAuthenticationReport(),@"sponsorblock":RVSponsorReport()};
        NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
        if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleCancel handler:nil]];[self presentPreferenceAlert:alert];
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)index {
    [tableView deselectRowAtIndexPath:index animated:YES];
    if (self.isRoot && !self.isSearching) {
        if (index.section==0) { RVSettingsController *page=[[RVSettingsController alloc] initWithStyle:UITableViewStyleInsetGrouped];page.groupID=RVPreferencesCatalog()[@"groups"][index.row][@"id"];[self.navigationController pushViewController:page animated:YES];return; }
        if (index.row==0) { RVShowTools(self);return; }
        if (index.row==1) {
            RVPreferenceTextController *editor=[RVPreferenceTextController new];editor.title=@"Import configuration";editor.hint=@"Paste a configuration exported by this iOS patcher. Only supplied settings change; all values are checked before saving. Android preference files use a different format.";
            editor.initialText=@"";editor.saveText=^NSString *(NSString *text) { NSData *data=[text dataUsingEncoding:NSUTF8StringEncoding];id values=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;return RVApplyPreferenceImport(values); };
            [self.navigationController pushViewController:editor animated:YES];return;
        }
        if (index.row==2) { NSData *data=[NSJSONSerialization dataWithJSONObject:RVEffectivePreferences() options:NSJSONWritingPrettyPrinted error:nil];if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];[self message:@"Configuration copied" text:@"You can paste it into another installation or the Windows patcher."];return; }
        if (index.row==3) {
            UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Reset patch settings?" message:@"Restore the configuration bundled in this IPA. Accounts, credentials and service identities are retained." preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"Reset settings" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) { RVResetPreferences();RVPreferencesChanged();[self applyAppearance];[self.tableView reloadData]; }]];
            [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[self presentPreferenceAlert:alert];return;
        }
        if (index.row==4) { [self diagnostics];return; }
        [self message:@"ReVanced iOS 0.3.7" text:[NSString stringWithFormat:@"YouTube %@\n80 patch switches and grouped preferences.\n%@",NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"] ?: @"",RVCompatible ? @"Supported app profile." : @"Unsupported app: patches disabled."]];return;
    }
    if (!RVCompatible || self.listKey) return;
    NSString *key=[self keyAtIndex:index];
    if (self.mapKey) { [self choose:self.mapKey member:key];return; }
    NSDictionary *rule=RVPreferenceDescriptor(key);NSString *kind=rule[@"kind"];
    if ([kind isEqual:@"bool"]) return;
    if ([kind isEqual:@"map"] || rule[@"allowed"]) {
        RVSettingsController *page=[[RVSettingsController alloc] initWithStyle:UITableViewStyleInsetGrouped];
        if ([kind isEqual:@"map"]) page.mapKey=key;else page.listKey=key;
        [self.navigationController pushViewController:page animated:YES];
    } else if ([kind hasSuffix:@"_list"]) [self editList:key];
    else if ([kind isEqual:@"string"]) [self editScalar:key member:nil];
    else [self choose:key member:nil];
}
@end

static BOOL RVPresentSettings(id presenter) {
    if (!RVCompatible || !NSThread.isMainThread || ![presenter isKindOfClass:UIViewController.class]) return NO;
    UIViewController *source=presenter;
    if (!source.isViewLoaded || !source.view.window || source.presentedViewController) return NO;
    RVSettingsController *page=[[RVSettingsController alloc] initWithStyle:UITableViewStyleInsetGrouped];
    if (RVCan(source,"showOrPushViewController:",@"v@:@")) ((void(*)(id,SEL,id))objc_msgSend)(source,sel_registerName("showOrPushViewController:"),page);
    else if (source.navigationController) [source.navigationController pushViewController:page animated:YES];
    else { page.modalEntry=YES;[source presentViewController:[[UINavigationController alloc] initWithRootViewController:page] animated:YES completion:nil]; }
    return YES;
}
