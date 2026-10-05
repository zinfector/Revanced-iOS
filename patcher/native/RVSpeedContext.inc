// SPDX-License-Identifier: MIT
// Keep native speed plumbing independent of the vote-layout default policy.
static _Thread_local NSUInteger RVSpeedNativeConfigurationScope;
static id RVSpeedOriginalColdGroup;
static BOOL RVSpeedOriginalColdGroupPopulated;
static NSMutableDictionary *RVSpeedContextCounts,*RVSpeedContextState;
static BOOL RVSpeedFirstLaunchPolicyActive(void);
static void RVSpeedContextValue(NSString *event,NSDictionary *values) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(UIImage.class) {
        if (!RVSpeedContextCounts) RVSpeedContextCounts=[NSMutableDictionary dictionary];
        if (!RVSpeedContextState) RVSpeedContextState=[NSMutableDictionary dictionary];
        RVSpeedContextCounts[event]=@([RVSpeedContextCounts[event] unsignedLongLongValue]+1);
        [RVSpeedContextState addEntriesFromDictionary:values ?: @{}];
    }
}
static void RVSpeedCaptureColdGroup(id group,BOOL replace) {
    Class cls=NSClassFromString(@"YTIColdConfigGroup");
    if (!cls || ![group isKindOfClass:cls] || ![group conformsToProtocol:@protocol(NSCopying)]) return;
    @synchronized(UIImage.class) {
        if (RVSpeedOriginalColdGroupPopulated && !replace) return;
        // Keep a private native copy before the loader supplies the UI defaults.
        RVSpeedOriginalColdGroup=[group copy];
        RVSpeedOriginalColdGroupPopulated=[RVObject(group,"data") length]>0;
    }
}
static id RVSpeedSavedColdGroup(void) { @synchronized(UIImage.class) { return RVSpeedOriginalColdGroup; } }
static NSDictionary *RVSpeedContextReport(void) {
    @synchronized(UIImage.class) {
        return @{@"revision":@"speed-context-1",@"first_launch_policy_active":@(RVSpeedFirstLaunchPolicyActive()),
            @"saved_native_group_available":@(RVSpeedOriginalColdGroup!=nil),
            @"saved_native_group_populated":@(RVSpeedOriginalColdGroupPopulated),
            @"configuration_preservation":@"native_speed_and_registry_getters_only",
            @"counts":RVSpeedContextCounts.copy ?: @{},@"state":RVSpeedContextState.copy ?: @{},@"device_verified":@NO};
    }
}
static id RVSpeedResponderRegistry(id responder) {
    Class provider=NSClassFromString(@"YTResponderProvider");Protocol *protocol=NSProtocolFromString(@"YTBlockRegistryProvider");
    if (!NSThread.isMainThread || !RVSpeedVisiblePlayer() || !responder || !protocol ||
        !RVCan(provider,"providerForProtocol:firstResponder:optional:",@"@@:@@B")) return nil;
    id owner=((id(*)(id,SEL,id,id,BOOL))objc_msgSend)(provider,sel_registerName("providerForProtocol:firstResponder:optional:"),protocol,responder,YES);
    id player=RVSpeedVisiblePlayer();
    BOOL matches=owner==RVCurrentWatch || owner==player || RVObject(owner,"playerViewController")==player;
    id registry=RVObject(owner,"blockRegistry"),container=RVObject(registry,"getContainer");
    RVSpeedContextValue(@"speed_registry_resolved",@{@"responder_present":@YES,@"provider_present":@(owner!=nil),
        @"provider_matches_current_player":@(matches),@"registry_present":@(registry!=nil),@"container_present":@(container!=nil)});
    return matches && container ? registry : nil;
}
static void RVInstallSpeedContext(void) {
    // Both typed accessors and YTColdConfig are used by native/C++ consumers.
    NSArray *flags=@[
        @[@"YTColdConfig",@"iosClientGlobalConfigEnableBlockRegistryProviderForPlaybackRateSelector"],
        @[@"YTColdConfigIosClientGlobalConfigImpl",@"enableBlockRegistryProviderForPlaybackRateSelector"],
        @[@"YTColdConfig",@"clientInfraClientConfigEnablePlaybackSpeedSheetWithXplatPlayerAdapter"],
        @[@"YTColdConfigClientInfraClientConfigImpl",@"enablePlaybackSpeedSheetWithXplatPlayerAdapter"],
        @[@"YTColdConfig",@"shortsConsumptionClientGlobalConfigIosEnableBottomSheetBlocksRegistryIntegration"],
        @[@"YTColdConfigShortsConsumptionClientGlobalConfigImpl",@"iosEnableBottomSheetBlocksRegistryIntegration"]];
    for (NSArray *target in flags) {
        NSString *key=[NSString stringWithFormat:@"%@.%@",target[0],target[1]];
        RVHook(target[0],target[1],@"B@:",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id config) {
                BOOL native;RVSpeedNativeConfigurationScope++;
                @try { native=((BOOL(*)(id,SEL))original)(config,sel); }
                @finally { RVSpeedNativeConfigurationScope--; }
                RVSpeedContextValue(@"native_configuration_read",@{key:@(native)});return native;
            };
        });
    }
    RVHook(@"YTIPlaybackRateSelectorMenuSheetViewModel",@"playbackRateSelectorMenuSheetFillerBlockWithParentResponder:",@"@@:@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id responder) {
            id result=((id(*)(id,SEL,id))original)(cls,sel,responder);
            RVSpeedContextValue(@"speed_filler_factory_returned",@{@"native_filler_present":@(result!=nil),@"factory_on_main_thread":@(NSThread.isMainThread)});
            id registry=RVSpeedResponderRegistry(responder);
            if (result || !RVCompatible) return result;
            id container=RVObject(registry,"getContainer");
            Class filler=NSClassFromString(@"YTBAPlaybackRateSelectorMenuSheetFillerBlock");
            if (!container || !RVCan(filler,"createInContainer:error:",@"@@:@^@")) return result;
            NSError *error=nil;
            id recovered=((id(*)(id,SEL,id,NSError **))objc_msgSend)(filler,sel_registerName("createInContainer:error:"),container,&error);
            BOOL valid=[recovered isKindOfClass:filler] && !error;
            RVSpeedContextValue(@"speed_filler_context_recovery",@{@"recovered_filler_present":@(valid),@"filler_error_present":@(error!=nil),@"filler_error_code":@(error.code)});
            return valid ? recovered : result;
        };
    });
    for (NSString *name in @[@"getShowPlaybackRateSelectorMenuCommand:error:",@"getPlaybackRateOverflowMenu:error:"]) {
        RVHook(@"YTBAPlaybackRateSelectorMenuSheetFillerBlock",name,@"@@:@^@",NO,^id(IMP original,SEL sel) {
            return ^id(id filler,id args,NSError **error) {
                RVSpeedContextValue(@"speed_sheet_command_entered",@{@"sheet_method":name});
                NSError *nativeError=nil;
                id input=[name isEqual:@"getShowPlaybackRateSelectorMenuCommand:error:"] ? RVSpeedThreeXSheet(args) : RVSpeedThreeXOverflow(args);
                id result=((id(*)(id,SEL,id,NSError **))original)(filler,sel,input,error ? &nativeError : NULL);
                if (error && nativeError) *error=nativeError;
                RVSpeedContextValue(@"speed_sheet_command_returned",@{@"sheet_result_present":@(result!=nil),
                    @"sheet_error_observable":@(error!=NULL),@"sheet_error_present":@(nativeError!=nil),@"sheet_error_code":@(nativeError.code)});
                if (NSThread.isMainThread) RVSpeedCheckpoint(@"speed_sheet_command_returned",RVCurrentPlayer,@{@"method":name,@"result_present":@(result!=nil)});
                return result;
            };
        });
    }
    RVHook(@"YTIPlaybackRateSelectorMenuSheetViewModel",@"playbackRateSelectorShowSheetCommandWithParentResponder:",@"@@:@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id responder) {
            if (NSThread.isMainThread) RVSpeedPickerOpened(RVSpeedVisiblePlayer(),@"native_speed_sheet_factory");
            id result=((id(*)(id,SEL,id))original)(cls,sel,responder);
            RVSpeedContextValue(@"speed_show_factory_returned",@{@"show_command_present":@(result!=nil)});return result;
        };
    });
    // Wrapper dispatch can invoke a C++ client without entering PlayerBlockImpl.
    for (NSString *name in @[@"getPlaybackRate:error:",@"getPlaybackRateModel:error:",@"setPlaybackRate:error:",@"setPlaybackRateModel:error:"]) {
        BOOL selection=[name hasPrefix:@"set"];
        RVHook(@"YTBAPlayerBlock",name,@"@@:@^@",NO,^id(IMP original,SEL sel) {
            return ^id(id block,id args,NSError **error) {
                BOOL main=NSThread.isMainThread,previous=RVSpeedBridgeCapture;
                RVSpeedContextValue(selection ? @"wrapper_selection_entered" : @"wrapper_rate_read_entered",@{@"wrapper_method":name,@"wrapper_on_main_thread":@(main)});
                if (main && selection) RVSpeedBridgeCapture=YES;
                id result;NSError *nativeError=nil;
                @try { result=((id(*)(id,SEL,id,NSError **))original)(block,sel,args,error ? &nativeError : NULL); }
                @finally { if (main) RVSpeedBridgeCapture=previous; }
                if (error && nativeError) *error=nativeError;
                NSString *field=[name isEqual:@"getPlaybackRate:error:"] ? @"playbackRate" : @"activeRate";
                float rate=selection ? NAN : RVSpeedFloat(result,field.UTF8String);
                RVSpeedContextValue(selection ? @"wrapper_selection_returned" : @"wrapper_rate_read_returned",@{[name stringByAppendingString:@".wrapper_result"]:@{
                    @"wrapper_result_present":@(result!=nil),@"wrapper_rate_available":@(isfinite(rate)),@"wrapper_rate":@(isfinite(rate) ? rate : 0),
                    @"wrapper_error_observable":@(error!=NULL),@"wrapper_error_present":@(nativeError!=nil),@"wrapper_error_code":@(nativeError.code)}});
                if (!main) RVSpeedContextValue(selection ? @"wrapper_off_main_selection" : @"wrapper_off_main_rate_read",nil);
                if (main) RVSpeedCheckpoint(selection ? @"wrapper_selection_returned" : @"wrapper_rate_read_returned",RVCurrentPlayer,@{@"method":name,@"result_present":@(result!=nil),@"rate_available":@(isfinite(rate)),@"rate":@(isfinite(rate) ? rate : 0)});
                return result;
            };
        });
    }
    // Observation only. These totals describe all Elements contexts, not only speed sheets.
    RVHook(@"YTELMContextFactory",@"contextWithResponder:",@"@@:@",NO,^id(IMP original,SEL sel) {
        return ^id(id factory,id responder) {
            id result=((id(*)(id,SEL,id))original)(factory,sel,responder);
            RVSpeedContextValue(@"element_context_without_explicit_registry",nil);return result;
        };
    });
    RVHook(@"YTELMContextFactory",@"contextWithResponder:blockRegistry:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id factory,id responder,id registry) {
            id result=((id(*)(id,SEL,id,id))original)(factory,sel,responder,registry);
            RVSpeedContextValue(registry ? @"element_context_with_registry" : @"element_context_with_nil_registry",nil);return result;
        };
    });
}
