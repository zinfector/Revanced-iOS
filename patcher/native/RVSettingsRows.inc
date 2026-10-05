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
        NSMutableArray *tools=[NSMutableArray array];NSArray *titles=@[@"Video tools",@"Import configuration",@"Copy configuration",@"Reset patch settings",@"Hook diagnostics",@"About"];
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
        [rows addObject:row];
    }
    if (searching && !rows.count) [rows addObject:@{@"title":@"No matching settings",@"disabled":@YES}];
    NSString *footer=mapKey ? @"Default removes this per-item override. Global settings then apply." : listKey ? @"Enabled categories are requested when the next video is opened." : group[@"hint"];
    if ([groupID isEqual:@"dearrow"]) return @[@{@"title":@"",@"footer":footer ?: @"",@"rows":rows},
        @{@"title":@"Current video and service",@"rows":@[@{@"title":@"Show original for current video",@"dearrow_action":@0},
        @{@"title":@"Use community version for current video",@"dearrow_action":@1},@{@"title":@"Clear DeArrow cache",@"dearrow_action":@2},
        @{@"title":@"Start DeArrow diagnostic capture",@"dearrow_action":@4},@{@"title":@"Probe service for current video",@"dearrow_action":@5},@{@"title":@"Copy DeArrow checkpoints",@"dearrow_action":@3}]}];
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
