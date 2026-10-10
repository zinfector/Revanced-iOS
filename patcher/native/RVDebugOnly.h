// Debugger baseline: ignore saved patch preferences and install only observers.
#ifndef RV_DEBUGGER_ONLY
#define RV_DEBUGGER_ONLY 1
#endif
static BOOL RVDebuggerOnly(void) { return RV_DEBUGGER_ONLY != 0; }
static id RVDebugOnlySetting(NSString *key,NSDictionary *config,NSArray *features) {
    if ([key isEqual:@"diagnostics"]) return @YES;
    id value=config[key];
    if ([features containsObject:key] || ([value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID())) return @NO;
    return value;
}
static BOOL RVDebugObserverContract(NSString *className,NSString *selectorName,NSString *signature,BOOL classMethod) {
    if (classMethod) return NO;
    if ([className isEqual:@"YTPlayerViewController"])
        return [selectorName isEqual:@"playbackController:didLoadContentPlaybackData:"] && [signature isEqual:@"v@:@@"];
    return [className isEqual:@"YTWatchViewController"] && [signature isEqual:@"v@:B"] &&
        [@[@"viewDidAppear:",@"viewDidDisappear:"] containsObject:selectorName];
}
