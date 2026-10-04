// SPDX-License-Identifier: MIT
#include "RVSettingsCatalog.h"
#include <CoreFoundation/CoreFoundation.h>
static NSDictionary *RVPreferencesCatalog(void) {
    static NSDictionary *catalog;static dispatch_once_t once;
    dispatch_once(&once,^{ catalog=[NSJSONSerialization JSONObjectWithData:[RVSettingsCatalogJSON dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil]; });
    return catalog;
}
static NSDictionary *RVPreferenceDescriptor(NSString *key) { return RVPreferencesCatalog()[@"settings"][key]; }
static BOOL RVPreferenceNumber(id value,BOOL integer) {
    if (![value isKindOfClass:NSNumber.class] || CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID()) return NO;
    double number=[value doubleValue];return isfinite(number) && (!integer || floor(number)==number);
}
static BOOL RVPreferencePattern(NSString *value,NSString *pattern) {
    NSRegularExpression *regex=[NSRegularExpression regularExpressionWithPattern:pattern options:0 error:nil];
    NSTextCheckingResult *match=[regex firstMatchInString:value options:0 range:NSMakeRange(0,value.length)];
    return match && NSEqualRanges(match.range,NSMakeRange(0,value.length));
}
static BOOL RVPreferenceValid(id value,NSDictionary *rule) {
    if (!rule || !value) return NO;
    NSString *kind=rule[@"kind"];
    if ([kind isEqual:@"bool"]) return [value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID();
    if ([kind isEqual:@"choice"]) {
        if ([rule[@"integer"] boolValue] && !RVPreferenceNumber(value,YES)) return NO;
        if (![rule[@"integer"] boolValue] && ![value isKindOfClass:NSString.class]) return NO;
        return [rule[@"choices"] containsObject:value];
    }
    if ([kind isEqual:@"number"]) {
        if (!RVPreferenceNumber(value,[rule[@"integer"] boolValue])) return NO;
        double number=[value doubleValue];
        return ([rule[@"allow_zero"] boolValue] && number==0) || (number>=[rule[@"min"] doubleValue] && number<=[rule[@"max"] doubleValue]);
    }
    if ([kind isEqual:@"string"]) {
        if (![value isKindOfClass:NSString.class] || (rule[@"max_length"] && [value length]>[rule[@"max_length"] unsignedIntegerValue])) return NO;
        if (![value length] && [rule[@"allow_empty"] boolValue]) return YES;
        if (rule[@"pattern"] && !RVPreferencePattern(value,rule[@"pattern"])) return NO;
        if ([rule[@"url"] boolValue]) {
            NSURLComponents *url=[NSURLComponents componentsWithString:value];
            return [url.scheme isEqual:@"https"] && url.host.length && !url.user && !url.password && !url.fragment;
        }
        return YES;
    }
    if ([kind isEqual:@"string_list"] || [kind isEqual:@"number_list"]) {
        if (![value isKindOfClass:NSArray.class] || [value count]<[rule[@"list_min"] unsignedIntegerValue] || [value count]>[rule[@"list_max"] unsignedIntegerValue]) return NO;
        for (id entry in value) {
            if ([kind isEqual:@"string_list"]) {
                if (![entry isKindOfClass:NSString.class] || ![entry length] || [entry length]>[rule[@"max_length"] unsignedIntegerValue] || (rule[@"allowed"] && ![rule[@"allowed"] containsObject:entry])) return NO;
            } else if (!RVPreferenceNumber(entry,NO) || [entry doubleValue]<[rule[@"min"] doubleValue] || [entry doubleValue]>[rule[@"max"] doubleValue]) return NO;
        }
        return YES;
    }
    if ([kind isEqual:@"map"]) {
        if (![value isKindOfClass:NSDictionary.class]) return NO;
        for (id key in value) {
            id entry=value[key];
            if (![rule[@"map_keys"] containsObject:key] || ![entry isKindOfClass:NSString.class] ||
                (rule[@"choices"] && ![rule[@"choices"] containsObject:entry]) || (rule[@"pattern"] && !RVPreferencePattern(entry,rule[@"pattern"]))) return NO;
        }
        return YES;
    }
    return NO;
}
static id RVPreferenceStoredValue(id value,NSDictionary *rule) {
    // Keep exported integer preferences compatible with the CLI's JSON type checks.
    return [rule[@"integer"] boolValue] ? @([value longLongValue]) : value;
}
static NSDictionary *RVEffectivePreferences(void) {
    NSMutableDictionary *values=[NSMutableDictionary dictionary];
    for (NSString *key in RVPreferencesCatalog()[@"settings"]) {
        NSDictionary *rule=RVPreferenceDescriptor(key);id value=RVSetting(key);
        values[key]=RVPreferenceValid(value,rule) ? RVPreferenceStoredValue(value,rule) : rule[@"default"];
    }
    values[@"schema"]=@1;values[@"app_name"]=RVConfig[@"app_name"] ?: @"";return values;
}
static NSString *RVValidatePreferenceImport(id values) {
    if (![values isKindOfClass:NSDictionary.class]) return @"The configuration must be a JSON object.";
    for (id key in values) {
        if ([key isEqual:@"schema"]) { if (!RVPreferenceNumber(values[key],YES) || [values[key] intValue]!=1) return @"Unsupported configuration version.";continue; }
        if ([key isEqual:@"app_name"]) {
            id name=values[key];
            if (![name isKindOfClass:NSString.class] || [name length]>60 || [name rangeOfCharacterFromSet:[NSCharacterSet controlCharacterSet]].location!=NSNotFound) return @"Invalid app name.";
            continue; // Packaging-only metadata; never alter it at runtime.
        }
        NSDictionary *rule=RVPreferenceDescriptor(key);
        if (!rule) return @"The configuration contains an unknown setting.";
        if (!RVPreferenceValid(values[key],rule)) return [NSString stringWithFormat:@"Invalid value for %@.",rule[@"title"]];
    }
    return nil;
}
static BOOL RVSavePreference(NSString *key,id value) {
    if (!RVCompatible || !RVPreferenceValid(value,RVPreferenceDescriptor(key))) return NO;
    [NSUserDefaults.standardUserDefaults setObject:RVPreferenceStoredValue(value,RVPreferenceDescriptor(key)) forKey:[RVPreferencePrefix stringByAppendingString:key]];return YES;
}
static NSString *RVApplyPreferenceImport(id values) {
    NSString *error=RVValidatePreferenceImport(values);if (error) return error;
    if (!RVCompatible) return @"This app does not match the supported profile.";
    // Validate every field before applying any. Missing values keep their current settings.
    for (NSString *key in values) if (RVPreferenceDescriptor(key)) RVSavePreference(key,values[key]);
    return nil;
}
static void RVResetPreferences(void) {
    if (!RVCompatible) return;
    for (NSString *key in RVPreferencesCatalog()[@"settings"]) [NSUserDefaults.standardUserDefaults removeObjectForKey:[RVPreferencePrefix stringByAppendingString:key]];
}
static NSArray *RVSearchPreferences(NSString *query) {
    NSString *needle=[query stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (!needle.length) return @[];
    NSMutableArray *results=[NSMutableArray array];
    for (NSDictionary *group in RVPreferencesCatalog()[@"groups"]) for (NSString *key in group[@"keys"]) {
        NSDictionary *rule=RVPreferenceDescriptor(key);
        NSString *text=[NSString stringWithFormat:@"%@ %@ %@",group[@"title"],rule[@"title"],rule[@"hint"] ?: @""];
        if ([text rangeOfString:needle options:NSCaseInsensitiveSearch|NSDiacriticInsensitiveSearch].location!=NSNotFound) [results addObject:key];
    }
    return results;
}
