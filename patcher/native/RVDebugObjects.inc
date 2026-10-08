// SPDX-License-Identifier: MIT
// Calls use reviewed fixed signatures; addresses never become arbitrary objects.
static NSArray *RVDBGGetterNames(void) {
    return @[@"parent",@"owningComponent",@"nodeController",@"element",@"childElements",@"materializedInstance",@"children",@"subnodes",@"yogaChildren",@"templateURI",@"attributedText",@"URL",@"image",@"node",@"view",@"layer",@"text",@"titleLabel",@"metadataView",@"isNodeLoaded",@"isNodeOnControllerLoaded",@"isLayerBacked",@"isHidden",@"isEnabled",@"instance"];
}
static NSDictionary *RVDBGGet(id object,NSString *selector,RVDAUIBinding *scope) {
    if (![RVDBGGetterNames() containsObject:selector]) return RVDBGFail(@"getter_not_reviewed");
    if ([@[@"node",@"view",@"layer"] containsObject:selector] && !RVBool(object,"isNodeLoaded") && !RVBool(object,"isNodeOnControllerLoaded")) return RVDBGFail(@"lazy_materialization_blocked");
    SEL sel=NSSelectorFromString(selector);
    if (RVCan(object,selector.UTF8String,@"@@:")) return @{@"ok":@YES,@"value":RVDBGReference(((id(*)(id,SEL))objc_msgSend)(object,sel),scope),@"encoding":@"@@:"};
    if (RVCan(object,selector.UTF8String,@"B@:")) return @{@"ok":@YES,@"value":@(((BOOL(*)(id,SEL))objc_msgSend)(object,sel)),@"encoding":@"B@:"};
    if ([selector isEqual:@"instance"] && RVCan(object,"instance",@"r^v@:")) return @{@"ok":@YES,@"address":RVDBGHex((uint64_t)((const void *(*)(id,SEL))objc_msgSend)(object,sel)),@"encoding":@"r^v@:"};
    return RVDBGFail(@"reviewed_getter_signature_unavailable");
}
static NSDictionary *RVDBGInvoke(id object,NSDictionary *args,RVDAUIBinding *scope) {
    NSString *selector=args[@"selector"];if (![selector isKindOfClass:NSString.class]) return RVDBGFail(@"selector_required");
    if ([RVDBGGetterNames() containsObject:selector]) { if (args[@"value"]) return RVDBGFail(@"getter_takes_no_arguments");return RVDBGGet(object,selector,scope); }
    SEL sel=NSSelectorFromString(selector);BOOL node=[object isKindOfClass:NSClassFromString(@"ASDisplayNode")],view=[object isKindOfClass:UIView.class];
    if ([@[@"setNeedsLayout",@"setNeedsDisplay"] containsObject:selector]) {
        if (!(node || view) || !RVCan(object,selector.UTF8String,@"v@:") || args[@"value"]) return RVDBGFail(@"void_call_contract_mismatch");
        ((void(*)(id,SEL))objc_msgSend)(object,sel);return @{@"ok":@YES,@"return_type":@"void"};
    }
    if ([@[@"setHidden:",@"setUserInteractionEnabled:"] containsObject:selector]) {
        NSUInteger value=0;if (!(node || view) || !RVCan(object,selector.UTF8String,@"v@:B") || ![args[@"value"] isKindOfClass:NSNumber.class] || !RVDBGBound(@([args[@"value"] doubleValue]),0,1,&value)) return RVDBGFail(@"bool_setter_contract_mismatch");
        ((void(*)(id,SEL,BOOL))objc_msgSend)(object,sel,(BOOL)value);return @{@"ok":@YES,@"return_type":@"void"};
    }
    if ([selector isEqual:@"setAlpha:"]) {
        if (!view || !RVCan(object,"setAlpha:",@"v@:d") || ![args[@"value"] isKindOfClass:NSNumber.class]) return RVDBGFail(@"double_setter_contract_mismatch");
        double value=[args[@"value"] doubleValue];if (!isfinite(value) || value<0 || value>1) return RVDBGFail(@"alpha_range");
        ((void(*)(id,SEL,double))objc_msgSend)(object,sel,value);return @{@"ok":@YES,@"return_type":@"void"};
    }
    if ([selector isEqual:@"setText:"] || [selector isEqual:@"setAttributedText:"]) {
        BOOL label=[object isKindOfClass:UILabel.class],text=[object isKindOfClass:NSClassFromString(@"ASTextNode")];
        NSString *value=args[@"value"];if (!(label || (text && [selector isEqual:@"setAttributedText:"])) || !RVCan(object,selector.UTF8String,@"v@:@") || ![value isKindOfClass:NSString.class] || [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding]>4096) return RVDBGFail(@"text_setter_contract_mismatch");
        id argument=[selector isEqual:@"setAttributedText:"] ? [[NSAttributedString alloc] initWithString:value] : value;
        ((void(*)(id,SEL,id))objc_msgSend)(object,sel,argument);return @{@"ok":@YES,@"return_type":@"void"};
    }
    return RVDBGFail(@"invocation_adapter_not_registered");
}
static NSArray *RVDBGFunctions(void) {
    NSMutableArray *functions=[NSMutableArray array];
    for (NSArray *item in @[@[@"getpid",@"int(void)"],@[@"CFAbsoluteTimeGetCurrent",@"double(void)"],@[@"mach_absolute_time",@"uint64(void)"],@[@"mach_timebase_info",@"kern_return_t(timebase_info_out)"]]) {
        void *pointer=dlsym(RTLD_DEFAULT,[item[0] UTF8String]);Dl_info info;
        if (!pointer || !dladdr(pointer,&info) || !info.dli_fbase) continue;
        NSString *uuid=RVDBGImageUUID(info.dli_fbase);if (!uuid) continue;
        [functions addObject:@{@"name":item[0],@"prototype":item[1],@"address":RVDBGHex((uint64_t)pointer),@"image_uuid":uuid,@"offset":RVDBGHex((uint64_t)pointer-(uint64_t)info.dli_fbase),@"queue":@"main"}];
    }return functions;
}
static NSDictionary *RVDBGCall(NSDictionary *args) {
    NSString *name=args[@"function"];if (![name isKindOfClass:NSString.class]) return RVDBGFail(@"function_required");NSDictionary *registered=nil;
    for (NSDictionary *item in RVDBGFunctions()) if ([item[@"name"] isEqual:name]) { registered=item;break; }
    if (!registered) return RVDBGFail(@"function_prototype_not_registered");
    for (NSString *key in @[@"image_uuid",@"offset",@"address",@"prototype"]) if (args[key] && ![registered[key] isEqual:args[key]]) return RVDBGFail(@"function_identity_or_prototype_mismatch");
    if ([name isEqual:@"getpid"]) return @{@"ok":@YES,@"value":@(getpid()),@"prototype":registered[@"prototype"]};
    if ([name isEqual:@"CFAbsoluteTimeGetCurrent"]) return @{@"ok":@YES,@"value":@(CFAbsoluteTimeGetCurrent()),@"prototype":registered[@"prototype"]};
    if ([name isEqual:@"mach_absolute_time"]) return @{@"ok":@YES,@"value":RVDBGHex(mach_absolute_time()),@"prototype":registered[@"prototype"]};
    mach_timebase_info_data_t timebase={0};kern_return_t result=mach_timebase_info(&timebase);
    return @{@"ok":@(result==KERN_SUCCESS),@"kernel_status":@(result),@"numer":@(timebase.numer),@"denom":@(timebase.denom),@"prototype":registered[@"prototype"]};
}
static NSDictionary *RVDBGObjectOperation(NSString *operation,NSDictionary *args) {
    RVDAUIBinding *scope=nil;id object=RVDBGResolve(args[@"handle"],&scope);if (!object) return RVDBGFail(@"object_handle_expired");
    if ([operation isEqual:@"debug.get"]) {
        if (![args[@"selector"] isKindOfClass:NSString.class]) return RVDBGFail(@"selector_required");return RVDBGGet(object,args[@"selector"],scope);
    }
    if ([operation isEqual:@"debug.invoke"]) return RVDBGInvoke(object,args,scope);
    if ([operation isEqual:@"debug.retain"]) {
        RVDBGState *state=RVDBGState.shared;@synchronized(state) {
            NSUInteger count=0;for (RVDBGHandle *handle in state.handles.allValues) count+=handle.retained!=nil;
            if (count>=32) return RVDBGFail(@"retain_lease_limit");state.handles[args[@"handle"]].retained=object;
        }return @{@"ok":@YES,@"retained":@YES};
    }
    if ([operation isEqual:@"debug.release"]) { RVDBGState *state=RVDBGState.shared;@synchronized(state) { state.handles[args[@"handle"]].retained=nil; }return @{@"ok":@YES,@"retained":@NO}; }
    if ([operation isEqual:@"debug.value"]) {
        if ([object isKindOfClass:NSString.class]) { NSString *text=object;return @{@"ok":@YES,@"kind":@"string",@"value":[text substringToIndex:MIN(text.length,4096u)],@"truncated":@(text.length>4096)}; }
        if ([object isKindOfClass:NSAttributedString.class]) { NSString *text=[object string];return @{@"ok":@YES,@"kind":@"attributed_string",@"value":[text substringToIndex:MIN(text.length,4096u)],@"truncated":@(text.length>4096)}; }
        if ([object isKindOfClass:NSNumber.class]) { double value=[object doubleValue];return isfinite(value) ? @{@"ok":@YES,@"kind":@"number",@"value":object} : RVDBGFail(@"nonfinite_number"); }
        if ([object isKindOfClass:NSData.class]) { NSData *data=object;return @{@"ok":@YES,@"kind":@"data",@"bytes":[[data subdataWithRange:NSMakeRange(0,MIN(data.length,65536u))] base64EncodedStringWithOptions:0],@"truncated":@(data.length>65536)}; }
        if ([object isKindOfClass:NSURL.class]) return @{@"ok":@YES,@"kind":@"url",@"value":[object absoluteString] ?: @""};
        return RVDBGFail(@"value_type_not_reviewed");
    }
    NSUInteger cursor=0;if (args[@"cursor"] && !RVDBGBound(args[@"cursor"],0,100000,&cursor)) return RVDBGFail(@"invalid_object_cursor");
    if ([operation isEqual:@"debug.items"]) {
        if (![object isKindOfClass:NSArray.class]) return RVDBGFail(@"array_required");NSArray *array=object;NSMutableArray *items=[NSMutableArray array];
        for (NSUInteger index=cursor;index<array.count && index<cursor+64;index++) [items addObject:@{@"index":@(index),@"value":RVDBGReference(array[index],scope)}];
        return @{@"ok":@YES,@"items":items,@"count":@(array.count),@"next_cursor":@(cursor+items.count),@"complete":@(cursor+items.count>=array.count)};
    }
    NSMutableArray *methods=[NSMutableArray array],*fields=[NSMutableArray array],*classes=[NSMutableArray array];NSUInteger methodIndex=0;
    for (Class cls=object_getClass(object);cls && classes.count<12;cls=class_getSuperclass(cls)) {
        [classes addObject:NSStringFromClass(cls)];unsigned int count=0;Method *list=class_copyMethodList(cls,&count);
        for (unsigned int index=0;index<count;index++,methodIndex++) if (methodIndex>=cursor && methods.count<128) {
            const char *encoding=method_getTypeEncoding(list[index]);[methods addObject:@{@"selector":NSStringFromSelector(method_getName(list[index])),@"encoding":encoding ? [NSString stringWithUTF8String:encoding] : @"",@"declaring_class":NSStringFromClass(cls)}];
        }free(list);
        Ivar *ivars=class_copyIvarList(cls,&count);
        for (unsigned int index=0;index<count && fields.count<128;index++) {
            const char *name=ivar_getName(ivars[index]),*encoding=ivar_getTypeEncoding(ivars[index]);
            [fields addObject:@{@"name":name ? [NSString stringWithUTF8String:name] : @"",@"encoding":encoding ? [NSString stringWithUTF8String:encoding] : @"",@"offset":RVDBGHex((uint64_t)ivar_getOffset(ivars[index])),@"declaring_class":NSStringFromClass(cls)}];
        }free(ivars);
    }
    NSMutableDictionary *getters=[NSMutableDictionary dictionary];
    for (NSString *selector in @[@"parent",@"owningComponent",@"nodeController",@"element",@"isNodeLoaded",@"isNodeOnControllerLoaded",@"isLayerBacked",@"isHidden",@"isEnabled"])
        if (RVCan(object,selector.UTF8String,@"@@:") || RVCan(object,selector.UTF8String,@"B@:")) getters[selector]=RVDBGGet(object,selector,scope);
    return @{@"ok":@YES,@"getters":getters,@"object":RVDBGReference(object,scope),@"classes":classes,@"instance_size":@(class_getInstanceSize(object_getClass(object))),@"methods":methods,@"ivars":fields,@"next_cursor":@(cursor+methods.count),@"complete":@(cursor+methods.count>=methodIndex),@"reviewed_getters":RVDBGGetterNames()};
}
static NSDictionary *RVDBGDeArrow(NSString *operation,NSDictionary *args) {
    if ([operation isEqual:@"debug.dearrow.bindings"]) {
        NSUInteger cursor=0,generation=0;
        if (args[@"cursor"] && !RVDBGBound(args[@"cursor"],0,65536,&cursor)) return RVDBGFail(@"binding_cursor_invalid");
        if (args[@"generation"] && !RVDBGBound(args[@"generation"],1,9007199254740991ULL,&generation)) return RVDBGFail(@"binding_generation_invalid");
        NSMutableArray *current=[NSMutableArray array],*bindings=[NSMutableArray array];
        for (RVDAUIBinding *binding in RVDAUIDiagnosticBindings()) if (RVDAUICurrent(binding) && (!generation || binding.generation==generation)) [current addObject:binding];
        for (NSUInteger index=cursor;index<current.count && bindings.count<64;index++) {
            RVDAUIBinding *binding=current[index];
            NSDictionary *fallback=@{@"phase":binding.fallbackPhase ?: @"idle",@"reason":binding.fallbackResponseReason ?: @"none",@"http":@(binding.fallbackHTTPStatus),@"cached_http":@(binding.fallbackCachedHTTPStatus),@"retries":@(binding.fallbackRetries),@"cycles":@(binding.fallbackCycles),@"duration_source":binding.durationSource ?: @"unresolved",@"timestamp_requested":binding.fallbackTimestampRequested ?: NSNull.null,@"timestamp_returned":binding.fallbackTimestampReturned ?: NSNull.null,@"cooldown_seconds":@(MAX(0,binding.fallbackNextAttempt-RVDANow()))};
            [bindings addObject:@{@"binding":RVDBGReference(binding,binding),@"root":RVDBGReference(binding.nativeFactoryRoot,binding),@"controller":RVDBGReference(binding.controller,binding),@"surface":binding.surface ?: @"unknown",@"route":binding.route ?: @"unknown",@"factory_proven":@(binding.factoryProvenanceVerified),@"roles":binding.consumerCheckpoint ?: @{},@"visible_now":@(RVDAUIFallbackVisible(binding)),@"video_hash":RVDADiagVideo(binding.video),@"image_origin":binding.imageOrigin ?: @"native_original",@"encoded_image_ready":@(binding.imageBytes!=nil),@"image_blocker":binding.imageBlocker ?: @"policy_off",@"fallback":fallback}];
        }return @{@"ok":@YES,@"bindings":bindings,@"count":@(current.count),@"next_cursor":@(cursor+bindings.count),@"complete":@(cursor+bindings.count>=current.count),@"order":@"current_visible_pending_then_recent"};
    }
    RVDAUIBinding *binding=RVDBGResolve(args[@"handle"],NULL);
    if (![binding isKindOfClass:RVDAUIBinding.class] || !RVDAUICurrent(binding)) return RVDBGFail(@"binding_handle_expired");
    if ([operation isEqual:@"debug.dearrow.refresh"]) { RVDAUIRefresh(binding);return @{@"ok":@YES,@"refresh_queued":@YES,@"generation":@(binding.generation)}; }
    id root=binding.nativeFactoryRoot;if (!root) return RVDBGFail(@"factory_root_retired");
    if ([operation isEqual:@"debug.native.roles"]) { RVDAUIObserveConsumers(binding,root,binding.generation,binding.consumerPasses);return @{@"ok":@YES,@"roles":binding.consumerCheckpoint ?: @{},@"generation":@(binding.generation)}; }
    NSUInteger cursor=0;if (args[@"cursor"] && !RVDBGBound(args[@"cursor"],0,256,&cursor)) return RVDBGFail(@"graph_cursor_invalid");
    NSArray *graph=RVDAUINativeGraph(binding,root);NSMutableArray *nodes=[NSMutableArray array];
    for (NSUInteger index=cursor;index<graph.count && index<cursor+32;index++) {
        id object=graph[index];NSMutableDictionary *edges=[NSMutableDictionary dictionary];
        for (NSString *edge in @[@"parent",@"owningComponent",@"nodeController",@"materializedInstance"]) {
            Class component=NSClassFromString(@"ELMComponent"),controller=NSClassFromString(@"ASNodeController"),node=NSClassFromString(@"ASDisplayNode");
            BOOL typed=(component && [object isKindOfClass:component]) || (controller && [object isKindOfClass:controller]) || (node && [object isKindOfClass:node]);
            if (typed && RVCan(object,edge.UTF8String,@"@@:")) edges[edge]=RVDBGReference(RVObject(object,edge.UTF8String),binding);
        }
        NSDictionary *ancestry=nil;RVDAUIResolveConsumerRoot(object,binding,&ancestry);
        [nodes addObject:@{@"index":@(index),@"object":RVDBGReference(object,binding),@"edges":edges,@"ancestry":ancestry ?: @{}}];
    }return @{@"ok":@YES,@"nodes":nodes,@"count":@(graph.count),@"next_cursor":@(cursor+nodes.count),@"complete":@(cursor+nodes.count>=graph.count),@"truncated":@(graph.count>=256)};
}
