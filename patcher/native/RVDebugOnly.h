// A constrained bundle; unsupported features and hooks remain excluded.
#ifndef RV_DEBUGGER_ONLY
#define RV_DEBUGGER_ONLY 1
#endif
#include "RVBundleCatalog.h"
static BOOL RVDebuggerOnly(void) { return RV_DEBUGGER_ONLY != 0; }
static BOOL RVDebugBaselineFeature(NSString *key) {
    return [key isEqual:@"diagnostics"] || [RV_BUNDLE_SUPPORTED containsObject:key];
}
static BOOL RVBundleSettingAllowed(NSString *key) { return [RV_BUNDLE_SETTINGS containsObject:key]; }
static id RVDebugOnlySetting(NSString *key,NSDictionary *config,NSArray *features,id override) {
    if ([key isEqual:@"diagnostics"] || [RV_BUNDLE_FORCED containsObject:key]) return @YES;
    if (RVBundleSettingAllowed(key)) return override ?: config[key];
    id value=config[key];
    if ([features containsObject:key] || ([value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID())) return @NO;
    return value;
}
static BOOL RVDebugObserverContract(NSString *className,NSString *selectorName,NSString *signature,BOOL classMethod) {
    NSString *contract=[NSString stringWithFormat:@"%@|%@|%@|%@",className,classMethod ? @"+" : @"-",selectorName,signature];
    return [RV_BUNDLE_CONTRACTS containsObject:contract];
}
