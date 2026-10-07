// SPDX-License-Identifier: MIT
// Shared rows and callbacks for the native and UIKit settings renderers.
#include "RVSettingsModel.inc"

static NSString *RVPreferenceValueTitle(id value,NSDictionary *rule) {
    NSUInteger index=[rule[@"choices"] indexOfObject:value];NSArray *labels=rule[@"choice_labels"];
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
    NSDictionary *names=@{@"home":@"Home",@"subscriptions":@"Subscriptions",@"library":@"Library",@"player":@"Watch title",@"search":@"Search",@"related":@"Related videos",
        @"sponsor":@"Sponsor",@"selfpromo":@"Self-promotion",@"interaction":@"Interaction reminder",@"intro":@"Intro",@"outro":@"Outro",
        @"preview":@"Preview",@"music_offtopic":@"Off-topic music",@"filler":@"Filler",@"hook":@"Hook",@"poi_highlight":@"Highlight"};
    return names[key] ?: key.capitalizedString;
}
static id RVSettingsEffectiveValue(NSString *key) {
    NSDictionary *rule=RVPreferenceDescriptor(key);id value=RVSetting(key);
    return RVPreferenceValid(value,rule) ? value : rule[@"default"];
}
static NSArray *RVSettingsRows(NSString *groupID,NSString *mapKey,NSString *listKey,NSString *query) {
    NSDictionary *group=nil;
    for (NSDictionary *candidate in RVPreferencesCatalog()[@"groups"]) if ([candidate[@"id"] isEqual:groupID]) { group=candidate;break; }
    BOOL root=!groupID && !mapKey && !listKey;
    NSString *trimmed=[query stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    BOOL searching=root && trimmed.length>0;
    if (root && !searching) {
        NSMutableArray *groups=[NSMutableArray array];
        for (NSDictionary *item in RVPreferencesCatalog()[@"groups"])
            [groups addObject:@{@"title":item[@"title"],@"group":item[@"id"]}];
        NSMutableArray *tools=[NSMutableArray array];NSArray *titles=@[@"Video tools",@"Import configuration",@"Copy configuration",@"Reset patch settings",@"Hook diagnostics",@"About",@"USB diagnostics"];
        for (NSUInteger index=0;index<titles.count;index++) [tools addObject:@{@"title":titles[index],@"tool":@(index)}];
        return @[@{@"title":@"Patch settings",@"rows":groups},@{@"title":@"Tools and configuration",@"rows":tools}];
    }
    NSArray *keys=searching ? RVSearchPreferences(trimmed) : mapKey ? RVPreferenceDescriptor(mapKey)[@"map_keys"] : listKey ? RVPreferenceDescriptor(listKey)[@"allowed"] : group[@"keys"];
    NSMutableArray *rows=[NSMutableArray array];
    for (NSString *key in keys) {
        NSString *preference=mapKey ?: listKey ?: key;NSDictionary *rule=RVPreferenceDescriptor(preference);
        NSMutableDictionary *row=[@{@"title":(mapKey || listKey) ? RVPreferenceMemberTitle(key) : rule[@"title"],@"key":preference,
            @"identifier":[NSString stringWithFormat:@"rvport.pref.%@%@%@",preference,(mapKey || listKey) ? @"." : @"",(mapKey || listKey) ? key : @""]} mutableCopy];
        if (mapKey || listKey) row[@"member"]=key;
        NSString *hint=rule[@"hint"] ?: @"";
        if (searching) {
            NSString *context=@"";
            for (NSDictionary *candidate in RVPreferencesCatalog()[@"groups"]) if ([candidate[@"keys"] containsObject:key]) { context=candidate[@"title"];break; }
            hint=hint.length ? [NSString stringWithFormat:@"%@\n%@",context,hint] : context;
        }
        if (hint.length && !mapKey && !listKey) row[@"hint"]=hint;
        if (listKey || (!mapKey && [rule[@"kind"] isEqual:@"bool"])) {
            row[@"toggle"]=@YES;row[@"on"]=listKey ? @([RVSettingsEffectiveValue(listKey) containsObject:key]) : RVSettingsEffectiveValue(key);
        } else {
            id value=mapKey ? RVSettingsEffectiveValue(mapKey)[key] : RVSettingsEffectiveValue(key);
            row[@"detail"]=mapKey && !value ? @"Default" : RVPreferenceValueTitle(value,rule);
        }
        if ([key isEqual:@"dearrow_store_selected_votes"]) {
            [row removeObjectForKey:@"toggle"];row[@"disabled"]=@YES;row[@"detail"]=@"Contribution UI pending";
        }
        [rows addObject:row];
    }
    if (searching && !rows.count) [rows addObject:@{@"title":@"No matching settings",@"disabled":@YES}];
    NSString *footer=mapKey ? @"Default removes this per-item override. Global settings then apply." : listKey ? ([listKey isEqual:@"dearrow_casual_categories"] ? @"Selected categories keep the original title when community votes identify a match." : @"Enabled categories are requested when the next video is opened.") : group[@"hint"];
    if ([groupID isEqual:@"dearrow"] && !mapKey && !listKey) {
        NSDictionary *frameRule=RVPreferenceDescriptor(@"thumbnail_frame");
        [rows addObject:@{@"title":frameRule[@"title"],@"key":@"thumbnail_frame",@"identifier":@"rvport.pref.thumbnail_frame",@"detail":RVPreferenceValueTitle(RVSettingsEffectiveValue(@"thumbnail_frame"),frameRule)}];
        NSArray *titles=@[@"Mode",@"Titles",@"Thumbnails",@"Display",@"Original and custom",@"Service and diagnostics"];
        NSArray *keys=@[@[@"dearrow_mode",@"dearrow_casual_categories"],
            @[@"dearrow_titles",@"dearrow_use_crowdsourced_titles",@"dearrow_title_format",@"dearrow_format_original_titles",@"dearrow_format_custom_titles",@"dearrow_remove_emojis",@"dearrow_capitalize_non_english",@"dearrow_ignore_translated_titles",@"dearrow_title_modes"],
            @[@"dearrow_thumbnails",@"dearrow_fallback",@"thumbnail_frame",@"dearrow_upcoming_avatar",@"dearrow_ignore_ab_thumbnails",@"dearrow_thumbnail_modes"],
            @[@"dearrow_hide_while_fetching",@"dearrow_title_max_lines",@"dearrow_thumbnail_saturation",@"dearrow_settings_theme"],
            @[@"dearrow_default_original",@"dearrow_always_show_original",@"dearrow_peek_interaction",@"dearrow_show_original_for_formatted",@"dearrow_store_selected_votes",@"dearrow_guideline_help"],
            @[@"dearrow_branding_url",@"dearrow_url"]];
        NSMutableArray *sections=[NSMutableArray array];
        for (NSUInteger index=0;index<titles.count;index++) {
            NSMutableArray *items=[NSMutableArray array];for (NSString *key in keys[index]) for (NSDictionary *row in rows) if ([row[@"key"] isEqual:key]) [items addObject:row];
            [sections addObject:@{@"title":titles[index],@"rows":items}];
        }
        NSMutableArray *actions=[NSMutableArray array];
        if (RVDAUIOriginalActionsAvailable()) [actions addObjectsFromArray:@[@{@"title":@"Show original for current video",@"dearrow_action":@0},@{@"title":@"Use custom version for current video",@"dearrow_action":@1}]];
        if (RVEnabled(@"dearrow_peek_interaction")) [actions addObject:@{@"title":@"Preview original and custom details",@"dearrow_action":@6}];
        if (RVEnabled(@"dearrow_guideline_help")) [actions addObject:@{@"title":@"DeArrow guidelines and help",@"dearrow_action":@7}];
        [actions addObjectsFromArray:@[@{@"title":@"Clear DeArrow cache",@"dearrow_action":@2},@{@"title":@"Start DeArrow diagnostic capture",@"dearrow_action":@4},@{@"title":@"Probe service for current video",@"dearrow_action":@5},@{@"title":@"Copy DeArrow checkpoints",@"dearrow_action":@3}]];
        [sections addObject:@{@"title":@"Current video and troubleshooting",@"footer":footer ?: @"",@"rows":actions}];return sections;
    }
    return @[@{@"title":@"",@"footer":footer ?: @"",@"rows":rows}];
}
static BOOL RVSettingsSaveToggle(NSDictionary *row,BOOL on) {
    NSString *key=row[@"key"],*member=row[@"member"];id value=@(on);
    if (!row[@"toggle"] || !key) return NO;
    if (member.length) {
        NSMutableArray *items=[RVSettingsEffectiveValue(key) mutableCopy];
        [items removeObject:member];if (on) [items addObject:member];value=items;
    }
    return RVSavePreference(key,value);
}
