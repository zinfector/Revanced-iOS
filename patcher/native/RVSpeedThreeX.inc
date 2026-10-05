// SPDX-License-Identifier: MIT
// Turn only the 300-percent speed upsell into the native ordinary rate option.
static _Thread_local BOOL RVSpeedThreeXCopying;
static _Thread_local NSUInteger RVSpeedThreeXSerializationDepth;
static atomic_uint RVSpeedThreeXConfigs,RVSpeedThreeXOptions,RVSpeedThreeXSheets,RVSpeedThreeXSerializations;
static BOOL RVSpeedThreeXEnabled(void) { return !RVSpeedThreeXCopying && RVEnabled(@"custom_speed_menu"); }
static BOOL RVSpeedThreeXClass(id value,NSString *name) {
    Class cls=NSClassFromString(name);return cls && [value isKindOfClass:cls] && [value conformsToProtocol:@protocol(NSCopying)];
}
static id RVSpeedThreeXConfig(id config) {
    if (!RVSpeedThreeXEnabled() || !RVSpeedThreeXClass(config,@"YTIGranularVariableSpeedConfig")) return config;
    id copy=[config copy];BOOL changed=NO,previous=RVSpeedThreeXCopying;RVSpeedThreeXCopying=YES;
    @try {
        int maximum=RVInt(copy,"maximumPlaybackRate");
        if (maximum>=25 && maximum<300 && RVCan(copy,"setMaximumPlaybackRate:",@"v@:i")) {
            ((void(*)(id,SEL,int))objc_msgSend)(copy,sel_registerName("setMaximumPlaybackRate:"),300);changed=YES;
        }
        id values=RVObject(copy,"defaultPlaybackRateOptionsArray");
        if ([values isKindOfClass:NSArray.class] && [values count]<=64) {
            NSMutableArray *options=[values mutableCopy];BOOL found=NO;
            for (NSUInteger i=0;i<options.count;i++) {
                id option=options[i];if (!RVSpeedThreeXClass(option,@"YTIGranularVariableSpeedConfig_PlaybackRateOption") || RVInt(option,"value")!=300) continue;
                found=YES;
                if (RVBool(option,"isPremiumUpsell") && RVCan(option,"setIsPremiumUpsell:",@"v@:B")) {
                    id regular=[option copy];((void(*)(id,SEL,BOOL))objc_msgSend)(regular,sel_registerName("setIsPremiumUpsell:"),NO);
                    options[i]=regular;changed=YES;atomic_fetch_add(&RVSpeedThreeXOptions,1);
                }
            }
            Class factory=NSClassFromString(@"YTIPlaybackRateSelectorMenuSheetViewModel");
            if (!found && maximum>=25 && maximum<=400 && RVCan(factory,"playbackRateOptionWithRate:displayName:",@"@@:i@")) {
                id option=((id(*)(id,SEL,int,id))objc_msgSend)(factory,sel_registerName("playbackRateOptionWithRate:displayName:"),300,@"3.0");
                if (RVSpeedThreeXClass(option,@"YTIGranularVariableSpeedConfig_PlaybackRateOption")) { [options addObject:option];changed=YES; }
            }
            if (changed) RVSetObject(copy,"setDefaultPlaybackRateOptionsArray:",options);
        }
    } @finally { RVSpeedThreeXCopying=previous; }
    if (changed) atomic_fetch_add(&RVSpeedThreeXConfigs,1);return changed ? copy : config;
}
static id RVSpeedThreeXRenderer(id renderer) {
    if (!RVSpeedThreeXClass(renderer,@"YTIRenderer") || !RVCan(renderer,"getExtension:",@"@@:@")) return renderer;
    id extensions=RVObject(renderer,"extensionsCurrentlySet");
    if (![extensions isKindOfClass:NSArray.class] || [extensions count]>32) return renderer;
    for (id extension in extensions) {
        id option=((id(*)(id,SEL,id))objc_msgSend)(renderer,sel_registerName("getExtension:"),extension);
        if (!RVSpeedThreeXClass(option,@"YTIPlaybackRateOptionViewModel")) continue;
        float rate=RVSpeedFloat(RVObject(option,"optionValue"),"floatValue");
        if (!isfinite(rate) || fabsf(rate-3.f)>.0001f) continue;
        // Ordinary options built at 0x10346c3dc have optionValue and no onTap.
        // Removing the special command restores that native selection fallback.
        if (!RVBool(option,"hasOnTap") && ![RVObject(option,"customTitle") length] && ![RVObject(option,"customDescription") length]) continue;
        if (!RVCan(option,"setOnTap:",@"v@:@") || !RVCan(renderer,"setExtension:value:",@"v@:@@")) continue;
        id regular=[option copy],copy=[renderer copy];
        RVSetObject(regular,"setOnTap:",nil);RVSetObject(regular,"setCustomTitle:",nil);RVSetObject(regular,"setCustomDescription:",nil);
        ((void(*)(id,SEL,id,id))objc_msgSend)(copy,sel_registerName("setExtension:value:"),extension,regular);
        atomic_fetch_add(&RVSpeedThreeXOptions,1);return copy;
    }return renderer;
}
static id RVSpeedThreeXSheet(id sheet) {
    if (!RVSpeedThreeXEnabled() || !RVSpeedThreeXClass(sheet,@"YTIPlaybackRateSelectorMenuSheetViewModel")) return sheet;
    id copy=[sheet copy];BOOL changed=NO;
    float maximum=RVSpeedFloat(sheet,"maximumPlaybackRate");
    if (isfinite(maximum) && maximum>=.25f && maximum<3.f && RVCan(copy,"setMaximumPlaybackRate:",@"v@:f")) {
        ((void(*)(id,SEL,float))objc_msgSend)(copy,sel_registerName("setMaximumPlaybackRate:"),3.f);changed=YES;
    }
    for (NSArray *field in @[@[@"defaultPlaybackRateOptionsArray",@"setDefaultPlaybackRateOptionsArray:"],@[@"defaultPlaybackRateChipLabelsArray",@"setDefaultPlaybackRateChipLabelsArray:"]]) {
        id values=RVObject(sheet,[field[0] UTF8String]);if (![values isKindOfClass:NSArray.class] || [values count]>64) continue;
        NSMutableArray *options=[values mutableCopy];BOOL replaced=NO;
        for (NSUInteger i=0;i<options.count;i++) { id regular=RVSpeedThreeXRenderer(options[i]);if (regular!=options[i]) { options[i]=regular;replaced=YES; } }
        if (replaced) { RVSetObject(copy,[field[1] UTF8String],options);changed=YES; }
    }
    if (changed) atomic_fetch_add(&RVSpeedThreeXSheets,1);return changed ? copy : sheet;
}
static id RVSpeedThreeXOverflow(id request) {
    if (!RVSpeedThreeXEnabled() || !RVSpeedThreeXClass(request,@"YTBAGetPlaybackRateOverflowMenuRequest")) return request;
    id sheet=RVObject(request,"viewModel"),regular=RVSpeedThreeXSheet(sheet);
    if (regular==sheet || !RVCan(request,"setViewModel:",@"v@:@")) return request;
    id copy=[request copy];RVSetObject(copy,"setViewModel:",regular);return copy;
}
static id RVSpeedThreeXPlayerConfig(id config) {
    if (!RVSpeedThreeXEnabled() || !RVSpeedThreeXClass(config,@"YTIPlayerConfig") || !RVBool(config,"hasGranularVariableSpeedConfig")) return config;
    id native=RVObject(config,"granularVariableSpeedConfig");
    if (!RVSpeedThreeXClass(native,@"YTIGranularVariableSpeedConfig") || !RVCan(config,"setGranularVariableSpeedConfig:",@"v@:@")) return config;
    id copy=[config copy];RVSetObject(copy,"setGranularVariableSpeedConfig:",RVSpeedThreeXConfig(native));return copy;
}
static NSDictionary *RVSpeedThreeXReport(void) {
    return @{@"revision":@"regular-three-x-1",@"enabled":@(RVEnabled(@"custom_speed_menu")),@"target_rate":@3,
        @"configured_maximum_percent":@300,@"configs_rewritten":@(atomic_load(&RVSpeedThreeXConfigs)),
        @"options_rewritten":@(atomic_load(&RVSpeedThreeXOptions)),@"sheets_rewritten":@(atomic_load(&RVSpeedThreeXSheets)),
        @"serialized_inputs_rewritten":@(atomic_load(&RVSpeedThreeXSerializations)),@"native_engine_limits_preserved":@YES,@"device_verified":@NO};
}
static void RVInstallSpeedThreeX(void) {
    RVHook(@"YTIGranularVariableSpeedConfig_PlaybackRateOption",@"isPremiumUpsell",@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id option) { BOOL native=((BOOL(*)(id,SEL))original)(option,sel);return RVSpeedThreeXEnabled() && RVInt(option,"value")==300 ? NO : native; };
    });
    RVHook(@"YTIGranularVariableSpeedConfig",@"maximumPlaybackRate",@"i@:",NO,^id(IMP original,SEL sel) {
        return ^int(id config) { int native=((int(*)(id,SEL))original)(config,sel);return RVSpeedThreeXEnabled() && native>=25 && native<300 ? 300 : native; };
    });
    RVHook(@"YTHotConfig",@"maximumPlaybackRate",@"f@:",NO,^id(IMP original,SEL sel) {
        return ^float(id config) { float native=((float(*)(id,SEL))original)(config,sel);return RVSpeedThreeXEnabled() && isfinite(native) && native>=.25f && native<3.f ? 3.f : native; };
    });
    RVHook(@"YTIPlayerConfig",@"granularVariableSpeedConfig",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id config) { return RVSpeedThreeXConfig(((id(*)(id,SEL))original)(config,sel)); };
    });
    RVHook(@"YTIPlaybackRateSelectorMenuSheetViewModel",@"defaultGranularVariableSpeedConfig",@"@@:",YES,^id(IMP original,SEL sel) {
        return ^id(id cls) { return RVSpeedThreeXConfig(((id(*)(id,SEL))original)(cls,sel)); };
    });
    RVHook(@"YTIPlaybackRateSelectorMenuSheetViewModel",@"playbackRateSelectorMenuSheetViewModelWithConfig:",@"@@:@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id config) { return RVSpeedThreeXSheet(((id(*)(id,SEL,id))original)(cls,sel,RVSpeedThreeXConfig(config))); };
    });
    // C++ block clients consume serialized GPB storage, bypassing field getters.
    for (NSString *name in @[@"YTIGranularVariableSpeedConfig",@"YTIPlayerConfig",@"YTIPlaybackRateSelectorMenuSheetViewModel",@"YTBAGetPlaybackRateOverflowMenuRequest"])
        RVHook(name,@"data",@"@@:",NO,^id(IMP original,SEL sel) {
            return ^id(id message) {
                if (!RVSpeedThreeXEnabled() || RVSpeedThreeXSerializationDepth) return ((id(*)(id,SEL))original)(message,sel);
                RVSpeedThreeXSerializationDepth++;id result;
                @try {
                    id regular=[name isEqual:@"YTIGranularVariableSpeedConfig"] ? RVSpeedThreeXConfig(message) :
                        [name isEqual:@"YTIPlayerConfig"] ? RVSpeedThreeXPlayerConfig(message) :
                        [name isEqual:@"YTIPlaybackRateSelectorMenuSheetViewModel"] ? RVSpeedThreeXSheet(message) : RVSpeedThreeXOverflow(message);
                    if (regular!=message) atomic_fetch_add(&RVSpeedThreeXSerializations,1);
                    result=((id(*)(id,SEL))original)(regular,sel);
                } @finally { RVSpeedThreeXSerializationDepth--; }
                return result;
            };
        });
}
