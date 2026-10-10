// SPDX-License-Identifier: MIT
static id RVRuntimeRoleObject(NSString *role) {
    id player=RVCurrentPlayer,playback=RVCurrentPlayback;
    BOOL owned=player && playback && playback==RVPlaybackForPlayer(player) && RVOrdinaryContent(playback);
    if ([role isEqual:@"player"]) return owned ? player : nil;
    if ([role isEqual:@"playback"]) return owned ? playback : nil;
    if ([role isEqual:@"rolling_view"]) { UIView *view=RVRuntimeRollingView;return view.window ? view : nil; }
    return nil;
}
static NSString *RVRuntimeSessionToken(void) {
    id controller=RVCurrentPlayback;
    RVPlaybackSession *session=controller ? objc_getAssociatedObject(controller,RVSessionKey) : nil;
    return [NSString stringWithFormat:@"%p:%llu:%@:%@",(__bridge void *)controller,(unsigned long long)session.generation,
        session.videoID ?: @"",session.cpn ?: @""];
}
static BOOL RVDBGRuntimeCurrent(RVDBGHandle *handle) {
    if (!handle.runtimeRole) return YES;
    if (!handle.runtimeRoot || handle.runtimeRoot!=RVRuntimeRoleObject(handle.runtimeRole) || ![handle.runtimeSession isEqual:RVRuntimeSessionToken()]) return NO;
    return ![handle.runtimeRole isEqual:@"rolling_view"] || handle.runtimeGeneration==RVRuntimeRollingEpoch;
}
static NSDictionary *RVDBGRuntimeReference(id object,NSString *role,id root) {
    if (!object) return @{@"kind":@"nil"};
    RVDBGState *state=RVDBGState.shared;@synchronized(state) {
        RVDBGHandle *old=[state.reverse objectForKey:object];
        if (old && (![old.runtimeRole isEqual:role] || old.runtimeRoot!=root)) [state.reverse removeObjectForKey:object];
    }
    NSDictionary *reference=RVDBGReference(object,nil);NSString *identifier=reference[@"handle"];
    if (identifier && ![identifier isEqual:@"limit"]) {
        RVDBGState *state=RVDBGState.shared;@synchronized(state) {
            RVDBGHandle *handle=state.handles[identifier];handle.runtimeRole=role;handle.runtimeRoot=root;
            handle.runtimeSession=RVRuntimeSessionToken();handle.runtimeGeneration=RVRuntimeRollingEpoch;
        }
    }return reference;
}
static NSDictionary *RVDBGRuntimeOperation(NSString *operation,NSDictionary *args) {
    if ([operation isEqual:@"debug.runtime.bindings"]) {
        NSMutableDictionary *bindings=[NSMutableDictionary dictionary];
        for (NSString *role in @[@"player",@"playback",@"rolling_view"]) { id root=RVRuntimeRoleObject(role);bindings[role]=RVDBGRuntimeReference(root,role,root); }
        return @{@"ok":@YES,@"bindings":bindings,@"session_scoped":@YES,@"rolling_view_is_latest_mounted_callback":@YES,
            @"rolling_player_ownership_proven":@NO};
    }
    if ([operation isEqual:@"debug.runtime.field"]) {
        NSString *identifier=args[@"handle"],*owner=args[@"owner"],*name=args[@"name"];
        id object=RVDBGResolve(identifier,NULL);if (!object) return RVDBGFail(@"object_handle_expired");
        RVDBGHandle *handle=RVDBGState.shared.handles[identifier];
        if (!handle.runtimeRole || ![owner isKindOfClass:NSString.class] || ![name isKindOfClass:NSString.class]) return RVDBGFail(@"runtime_field_scope_required");
        NSDictionary *metadata=nil;Ivar field=RVRuntimeResolveField(object,RVRuntimeFieldSpec(owner,name),&metadata);
        if (!field) return RVDBGFail(@"field_contract_unavailable");
        return @{@"ok":@YES,@"layout":metadata,@"value":RVDBGRuntimeReference(object_getIvar(object,field),handle.runtimeRole,handle.runtimeRoot)};
    }
    if ([operation isEqual:@"debug.runtime.setting"]) {
        NSString *feature=args[@"feature"];if (![feature isKindOfClass:NSString.class] || !RVRuntimePolicyKnown(feature)) return RVDBGFail(@"unbound_policy_feature");
        NSString *key=[RVPreferencePrefix stringByAppendingString:feature];NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
        if (args[@"value"] || args[@"remove"]) {
            id expected=args[@"expected"];if (!expected || ![expected isEqual:[defaults objectForKey:key] ?: NSNull.null]) return RVDBGFail(@"setting_compare_failed");
            if ([args[@"remove"] isEqual:@YES] && !args[@"value"]) [defaults removeObjectForKey:key];
            else if ([args[@"value"] isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)args[@"value"])==CFBooleanGetTypeID()) [defaults setBool:[args[@"value"] boolValue] forKey:key];
            else return RVDBGFail(@"boolean_or_remove_required");
        }
        BOOL result=RVRuntimeSourceFeatureEnabled(feature);NSDictionary *policy;@synchronized(RVRuntimeSuite) { policy=RVRuntimePolicyResults[feature] ?: @{}; }
        return @{@"ok":@YES,@"feature":feature,@"override":[defaults objectForKey:key] ?: NSNull.null,
            @"effective":@(RVEnabled(feature)),@"policy_result":@(result),@"policy":policy};
    }
    if ([operation isEqual:@"debug.runtime.visibility"]) {
        id object=RVDBGResolve(args[@"handle"],NULL);RVDBGHandle *handle=RVDBGState.shared.handles[args[@"handle"]];
        if (!object || ![handle.runtimeRole isEqual:@"rolling_view"]) return RVDBGFail(@"visible_view_scope_required");
        int32_t hidden=rv_ios_get_hidden((__bridge void *)object);
        if (hidden<0) return RVDBGFail(@"generated_view_read_failed");
        if (args[@"value"]) {
            if (![args[@"expected"] isEqual:@(hidden)] || ![args[@"value"] isKindOfClass:NSNumber.class] || CFGetTypeID((__bridge CFTypeRef)args[@"value"])!=CFBooleanGetTypeID()) return RVDBGFail(@"visibility_compare_failed");
            if (rv_ios_set_hidden((__bridge void *)object,[args[@"value"] boolValue])!=0) return RVDBGFail(@"generated_view_write_failed");
        }
        return @{@"ok":@YES,@"hidden":@(rv_ios_get_hidden((__bridge void *)object)),@"provider":@"compiled_generated_ios_view"};
    }
    if ([operation isEqual:@"debug.runtime.report"]) {
        NSDictionary *policies;@synchronized(RVRuntimeSuite) { policies=[RVRuntimePolicyResults copy] ?: @{}; }
        return @{@"ok":@YES,@"schema":@"runtime-integration.v1",@"providers_linked_into":@"RVPort.dylib",@"preferences_count":@(rv_setting_count()),
            @"source_sha256":RV_BUILD_SOURCE_SHA256,@"policies":policies,@"field_contracts":RVRuntimeFieldCatalog(),
            @"rolling_events":[RVRuntimeRollingEvents copy] ?: @[],@"rolling_sequence":@(RVRuntimeRollingSequence),
            @"visual_animation_verified":@NO,@"runtime_verified":@NO};
    }return RVDBGFail(@"unsupported_runtime_operation");
}
