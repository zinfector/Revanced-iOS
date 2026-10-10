// SPDX-License-Identifier: MIT
// Per-child provenance from an Objective-C serialized renderer boundary. No
// collection getters, opaque C++ ABI calls or shared shelf projections.
static void RVDANestedRestore(RVDAUIBinding *binding) {
    if (!binding.nestedInput || !RVDAUICurrent(binding)) return;
    id title=binding.nestedEditedTitle,image=binding.nestedEditedImage;
    id text=RVObject(title,"attributedText");
    if (binding.nestedOriginalText && [text isKindOfClass:NSAttributedString.class] &&
        [[text string] isEqual:binding.nestedAppliedTitle] && RVCan(title,"setAttributedText:",@"v@:@"))
        RVSetObject(title,"setAttributedText:",binding.nestedOriginalText);
    if (binding.nativeImage && RVObject(image,"image")==binding.nestedAppliedImage && RVCan(image,"setImage:",@"v@:@"))
        RVSetObject(image,"setImage:",binding.nativeImage);
    binding.nestedEditedTitle=nil;binding.nestedEditedImage=nil;
    binding.nestedOriginalText=nil;binding.nestedAppliedTitle=nil;binding.nestedAppliedImage=nil;binding.nativeImage=nil;
}
static BOOL RVDANestedPresent(RVDAUIBinding *binding,NSUInteger titleMatches,NSUInteger imageMatches) {
    if (!binding.nestedInput || !RVDAUICurrent(binding) || !binding.factoryProvenanceVerified || !binding.nativeRootComponent) return NO;
    BOOL changed=NO;id title=binding.nativeTitleNode,image=binding.nativeThumbnailNode;
    // A role is writable only with one original match in this child's bounded
    // graph and the same independently proven ELMComponent ownership.
    NSString *replacement=RVDAUITitle(binding);
    id text=RVObject(title,"attributedText");
    BOOL originalText=[text isKindOfClass:NSAttributedString.class] && [[text string] isEqual:binding.originalTitle];
    BOOL heldText=title==binding.nestedEditedTitle && [text isKindOfClass:NSAttributedString.class] && [[text string] isEqual:binding.nestedAppliedTitle];
    if (replacement && (titleMatches==1 || heldText) && RVBool(title,"isNodeLoaded") &&
        RVDAUIResolveConsumerRoot(title,binding,NULL)==binding.nativeRootComponent &&
        (originalText || heldText) && ![[text string] isEqual:replacement] &&
        RVCan(title,"setAttributedText:",@"v@:@")) {
        NSAttributedString *baseline=heldText ? binding.nestedOriginalText : text;
        NSDictionary *attributes=[baseline length] ? [baseline attributesAtIndex:0 effectiveRange:NULL] : @{};
        NSAttributedString *next=[[NSAttributedString alloc] initWithString:replacement attributes:attributes];
        binding.nestedOriginalText=baseline;binding.nestedAppliedTitle=replacement;binding.nestedEditedTitle=title;
        RVSetObject(title,"setAttributedText:",next);changed=YES;
    }
    if (!replacement && heldText && binding.nestedOriginalText && RVCan(title,"setAttributedText:",@"v@:@")) {
        RVSetObject(title,"setAttributedText:",binding.nestedOriginalText);
        binding.nestedEditedTitle=nil;binding.nestedOriginalText=nil;binding.nestedAppliedTitle=nil;changed=YES;
    }
    UIImage *ready=RVDAUIImage(binding);
    if (!ready && binding.nativeImage && image==binding.nestedEditedImage && RVObject(image,"image")==binding.nestedAppliedImage && RVCan(image,"setImage:",@"v@:@")) {
        RVSetObject(image,"setImage:",binding.nativeImage);
        binding.nestedEditedImage=nil;binding.nestedAppliedImage=nil;binding.nativeImage=nil;changed=YES;
    }
    if (ready && ![binding.imageOrigin isEqual:@"pending"] && imageMatches==1 && RVBool(image,"isNodeLoaded") &&
        RVDAUIResolveConsumerRoot(image,binding,NULL)==binding.nativeRootComponent &&
        [binding.profile.imageURLs containsObject:RVDAUIConsumerURL(image) ?: @""] && RVCan(image,"setImage:",@"v@:@")) {
        UIImage *current=RVObject(image,"image");
        if ([current isKindOfClass:UIImage.class] && current!=ready) {
            // Native image loads may run again. Save the newest original only
            // when it is not our previous replacement, then reapply ready pixels.
            if (current!=binding.nestedAppliedImage) binding.nativeImage=current;
            binding.nestedAppliedImage=ready;binding.nestedEditedImage=image;
            RVSetObject(image,"setImage:",ready);changed=YES;
        }
    }
    if (changed) {
        binding.materializations++;
        RVDADiagRecord(@"N06_nested_owned_pixels_applied",binding,nil,
            @{@"title":@(binding.nestedEditedTitle!=nil),@"image":@(binding.nestedEditedImage!=nil),@"shared_model_modified":@NO});
    }
    return changed;
}
static void RVDANestedRefresh(RVDAUIBinding *binding) {
    if (!RVDAUICurrent(binding)) return;
    if (RVDAUIOriginal(binding) || !binding.parts) { RVDANestedRestore(binding);return; }
    if (!binding.factoryProvenanceVerified && binding.nestedNativeController) {
        RVDAUIFactoryCapture *capture=[RVDAUIFactoryCapture new];capture.input=binding.nestedInput.source;
        capture.element=binding.nestedElement;capture.controller=binding.nestedNativeController;capture.callbacks=1;
        capture.inputVerified=RVDAUIFactoryInputAccepted(@[binding],capture.input);
        RVDAUIAcceptFactory(binding,binding.controller,capture);
    } else RVDAUIScheduleConsumers(binding,0);
}
static void RVDANestedAccept(id node,id element,RVDANestedInput *input,RVDAUIFactoryCapture *capture) {
    if (!NSThread.isMainThread || !node || !input) return;
    RVDAUIBinding *prior=objc_getAssociatedObject(node,RVDAUIBindingKey);
    if (prior) {
        if (!prior.nestedInput || RVDAUICurrent(prior)) { RVDADiagCount(@"nested.already_bound");return; }
        // A reused native cell must retire the obsolete child's asynchronous
        // work. Native reconfiguration owns restoring changed-content baselines.
        [prior.request cancel];[prior.fallbackTransfer cancel];RVDAUIRetireConsumers(prior);
        objc_setAssociatedObject(node,RVDAUIBindingKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        RVDADiagCount(@"nested.reused_cell_rebound");
    }
    NSString *surface=RVDAUIOwnerSurface(input.owner,NO,NULL);
    if (RVDAUIDiagnosticsArmed) RVDALiveCaptureRoot(input.source,@"RVDANestedCollectionItem",surface,nil);
    BOOL ownerValid=input.nativeCollection ? input.collection && input.owner && RVObject(input.owner,"entry")==input.entry &&
        RVObject(input.collection,"element")==input.collectionElement &&
        [RVObject(RVAdRootElement(input.entry),"elementData") isEqual:input.parentSource] :
        input.dataSource && input.item && RVObject(input.item,"entry")==input.entry &&
        [RVObject(RVAdRootElement(input.entry),"elementData") isEqual:input.source];
    if (!ownerValid || !surface || RVObject(node,"element")!=element) {
        RVDADiagCount(@"nested.rejected_owner_or_source");return;
    }
    NSArray<RVDAProjection *> *profiles=input.verifiedProfile ? @[input.verifiedProfile] : RVDAProfiles(input.source,nil);
    // Never interpret a shelf container as one video, or give multiple children
    // a shared native root. Only reviewed single-card semantics are admitted.
    RVDAProjection *profile=profiles.count==1 ? profiles.firstObject : nil;
    if (!profile.liveCard || profile.watchTitle || profile.shelfCard) {
        RVDADiagCount(@"nested.rejected_unreviewed_child_profile");return;
    }
    if (capture.callbacks!=1 || !capture.controller || capture.element!=element) {
        RVDADiagCount(@"nested.rejected_factory_callback_contract");return;
    }
    RVDAUIBinding *state=[RVDAUIBinding new];state.controller=node;state.originalEntry=input.entry;
    state.nestedInput=input;state.nestedElement=element;state.nestedNativeController=capture.controller;
    state.index=-1;state.profile=profile;state.modern=YES;state.video=profile.video;state.surface=surface;state.route=profile.route;
    state.originalTitle=profile.originalTitle;state.untranslatedTitle=profile.untranslatedTitle;
    state.originalImageURL=profile.originalImageURL;state.duration=profile.duration;state.livestream=profile.livestream;
    state.ownerSummary=@{@"strategy":@"independent_nested_child",@"data_source":input.nativeCollection ? @"ELMCollectionNode" : NSStringFromClass([input.dataSource class]),
        @"factory_node":NSStringFromClass([node class]),@"owner":NSStringFromClass([input.owner class])};
    objc_setAssociatedObject(node,RVDAUIBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [RVDAUIBindings addObject:state];RVDAUIRequest(state);
    capture.inputVerified=RVDAUIFactoryInputAccepted(@[state],input.source);
    state.factorySerial=++RVDAUIFactorySerial;state.submittedData=input.source;
    RVDAUIAcceptFactory(state,node,capture);
    RVDADiagRecord(@"N04_nested_child_bound",state,nil,@{@"single_video":@YES,@"entry_identity_checked":@YES,
        @"native_element_identity_checked":@YES,@"serialized_source_checked":@YES,
        @"source_join":input.nativeCollection ? @"unique_video_and_template_in_owned_viewmodel" : @"item_entry_identity"});
}
static RVDANestedInput *RVDANestedRecordItem(id dataSource,id item) {
    id entry=RVObject(item,"entry"),renderer=RVAdRootElement(entry);
    NSData *source=RVObject(renderer,"elementData");id options=RVObject(renderer,"compatibilityOptions");
    if (![source isKindOfClass:NSData.class] || !source.length || source.length>128*1024 ||
        !RVCan(options,"hasAdLoggingData",@"B@:") || RVBool(options,"hasAdLoggingData")) {
        RVDADiagCount(@"nested.rejected_input_contract");return nil;
    }
    RVDANestedOwner *owner=objc_getAssociatedObject(dataSource,RVDANestedOwnerKey);
    RVDANestedInput *prior=objc_getAssociatedObject(item,RVDANestedInputKey);
    if (prior.dataSource==dataSource && prior.entry==entry && prior.owner==owner.controller && [prior.source isEqual:source]) return prior;
    RVDANestedInput *input=[RVDANestedInput new];input.dataSource=dataSource;input.item=item;
    input.owner=owner.controller;input.entry=entry;input.source=[source copy];
    if (RVDAUIDiagnosticsArmed) dispatch_async(dispatch_get_main_queue(),^{
        RVDALiveCaptureRoot(input.source,@"RVDANestedCollectionItem",RVDAUIOwnerSurface(input.owner,NO,NULL),nil);
    });
    RVDADiagCount(@"nested.serialized_child_provenance_recorded");return input;
}
// The native static collection route has no YT/ELMN data-source callback.
// Observe the native no-argument child factory, then join one independently
// serialized viewModel to that child. Context -> parentResponder proves the
// shelf's YT owner; native video identity + template proves the child join.
@interface RVDANativeChildObservation : NSObject
@property(nonatomic,weak) id collection,node;
@property(nonatomic,weak) id nativeController;
@property(nonatomic,strong) id element;
@property(nonatomic) NSUInteger callbacks;
@property(nonatomic) NSUInteger attempts;
@property(nonatomic) BOOL pending;
@end
@implementation RVDANativeChildObservation @end
static const void *RVDANativeChildKey=&RVDANativeChildKey;
static NSUInteger RVDANativeChildPending;
static NSHashTable *RVDANativeChildDeferred;
static const void *RVDANativeShelfCaptureEpochKey=&RVDANativeShelfCaptureEpochKey;
static id RVDANativeShelfOwner(id context,NSData **source) {
    // The native cell context may be a forwarding proxy. Unwrap only that
    // reviewed class through its fixed Objective-C forwarding method.
    Class proxy=NSClassFromString(@"ELMCollectionCellContextProxy");
    for (NSUInteger depth=0;proxy && [context isKindOfClass:proxy] && depth<2;depth++) {
        if (!RVCan(context,"forwardingTargetForSelector:",@"@@::")) return nil;
        context=((id(*)(id,SEL,SEL))objc_msgSend)(context,sel_registerName("forwardingTargetForSelector:"),sel_registerName("parentResponder"));
    }
    id owner=RVAdResolvedObject(context,"parentResponder");
    for (NSUInteger depth=0;owner && depth<12;depth++,owner=RVObject(owner,"parentResponder")) {
        id entry=RVObject(owner,"entry"),renderer=RVAdRootElement(entry);
        NSData *data=RVObject(renderer,"elementData");
        if (![data isKindOfClass:NSData.class] || !data.length || data.length>2*1024*1024) continue;
        // Defer template/model parsing to the utility queue. Unsupported
        // parent resources are captured read-only and cannot authorize a child.
        id options=RVAdResolvedObject(renderer,"compatibilityOptions");
        if (!RVAdResolve(options,"hasAdLoggingData",@"B@:") || RVBool(options,"hasAdLoggingData")) return nil;
        if (source) *source=[data copy];return owner;
    }
    return nil;
}
static NSString *RVDANativeChildVideo(id element,NSUInteger *length,NSString **blocker) {
    // Reviewed Objective-C bridge reads the immutable Element arena. It never
    // materializes controllers. Run off-main and never export its model text.
    NSString *text=RVObject(element,"protoText");
    if (length) *length=[text isKindOfClass:NSString.class] ? text.length : 0;
    if (![text isKindOfClass:NSString.class] || text.length>128*1024) { if (blocker) *blocker=@"native_child_text_unavailable_or_large";return nil; }
    static NSRegularExpression *ids;static dispatch_once_t once;
    dispatch_once(&once,^{ ids=[NSRegularExpression regularExpressionWithPattern:@"(?:video_id|videoId|videoID)\\s*:\\s*\"([A-Za-z0-9_-]{11})\"" options:0 error:nil]; });
    NSArray *matches=[ids matchesInString:text options:0 range:NSMakeRange(0,text.length)];
    if (!matches.count || matches.count>32) { if (blocker) *blocker=@"native_child_video_identity_missing";return nil; }
    NSString *video=[text substringWithRange:[matches[0] rangeAtIndex:1]];
    for (NSTextCheckingResult *match in matches) if (![[text substringWithRange:[match rangeAtIndex:1]] isEqual:video]) {
        if (blocker) *blocker=@"native_child_video_identity_ambiguous";return nil;
    }
    return video;
}
static void RVDANativeChildPrepare(RVDANativeChildObservation *observation) {
    if (!NSThread.isMainThread || observation.pending || observation.attempts>=2 || !observation.element || !observation.nativeController || !observation.node || !observation.collection) return;
    if (objc_getAssociatedObject(observation.node,RVDAUIBindingKey)) return;
    if (RVDANativeChildPending>=8 || UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) {
        if (!RVDANativeChildDeferred) RVDANativeChildDeferred=[NSHashTable weakObjectsHashTable];
        if (RVDANativeChildDeferred.count<64) [RVDANativeChildDeferred addObject:observation];
        RVDADiagCount(@"nested.native_child_queue_deferred");return;
    }
    observation.attempts++;id node=observation.node,collection=observation.collection;
    RVDAUIFactoryCapture *capture=[RVDAUIFactoryCapture new];capture.controller=observation.nativeController;
    capture.element=observation.element;capture.callbacks=observation.callbacks;
    if (capture.callbacks!=1 || !capture.controller || !capture.element || RVObject(node,"element")!=capture.element ||
        RVDAUIElementIdentity(RVObject(capture.controller,"element"))!=RVDAUIElementIdentity(capture.element) || !RVDAUIElementIdentity(capture.element)) {
        RVDADiagCount(@"nested.native_child_factory_contract_missing");return;
    }
    NSData *parentSource=nil;id owner=RVDANativeShelfOwner(RVObject(capture.controller,"context"),&parentSource);
    NSString *surface=RVDAUIOwnerSurface(owner,NO,NULL),*route=RVDAResourceName(RVObject(capture.controller,"templateURI"));
    id collectionElement=RVObject(collection,"element");
    if (!owner || !surface || !collectionElement || !route) {
        RVDADiagRecord(@"N10_native_collection_owner",nil,@"owned_shelf_context_unresolved",
            @{@"context_class":RVObject(capture.controller,"context") ? NSStringFromClass([RVObject(capture.controller,"context") class]) : @"absent",
              @"parent_owner":@(owner!=nil),@"surface":surface ?: @"unknown",@"route":route ?: @"unknown"});
        if (observation.attempts<2) dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC/5),dispatch_get_main_queue(),^{ RVDANativeChildPrepare(observation); });
        else observation.element=nil;return;
    }
    RVDADiagRecord(@"N10_native_collection_owner",nil,nil,@{@"surface":surface,@"route":route,
        @"owner_class":NSStringFromClass([owner class]),@"child_element_identity_checked":@YES,@"callbacks":@(capture.callbacks)});
    id entry=RVObject(owner,"entry");uint64_t revision=RVDAUISettingsRevision;
    __weak id weakOwner=owner,weakCollection=collection,weakNode=node;
    observation.pending=YES;RVDANativeChildPending++;
    static dispatch_queue_t queue;static dispatch_once_t once;
    dispatch_once(&once,^{ queue=dispatch_queue_create("RVPort.DeArrow.NativeShelfChild",dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,QOS_CLASS_UTILITY,0)); });
    dispatch_async(queue,^{ @autoreleasepool {
        NSString *blocker=nil;NSUInteger textLength=0;NSString *video=RVDANativeChildVideo(capture.element,&textLength,&blocker);
        NSData *component=RVDAPath(parentSource,@[@1,@168777401]);NSString *key=RVDATemplate(component);
        static NSMutableArray *parsedShelves;if (!parsedShelves) parsedShelves=[NSMutableArray array];
        NSArray *children=nil;for (NSDictionary *cached in parsedShelves) if ([cached[@"source"] isEqual:parentSource]) { children=cached[@"children"];break; }
        if (!children) {
            NSMutableArray *parsed=[NSMutableArray array];
            for (NSDictionary *child in RVDAShelfViewModels(RVDAChild(component,5),key)) {
                NSArray *profiles=RVDAProfiles(child[@"element"],nil);RVDAProjection *profile=profiles.count==1 ? profiles.firstObject : nil;
                NSMutableDictionary *item=[child mutableCopy];if (profile) item[@"profile"]=profile;
                item[@"blocker"]=profile ? @"ready" : RVDAProfileFailure(child[@"element"]) ?: @"unreviewed";[parsed addObject:item.copy];
            }
            children=parsed.copy;[parsedShelves addObject:@{@"source":parentSource,@"children":children}];
            while (parsedShelves.count>2) [parsedShelves removeObjectAtIndex:0];
        }
        RVDAProjection *chosen=nil;NSUInteger matches=0,verified=0;NSMutableArray *samples=[NSMutableArray array];
        for (NSDictionary *child in children) {
            RVDAProjection *profile=child[@"profile"];
            BOOL accepted=profile.liveCard && !profile.watchTitle && !profile.shelfCard;
            if (accepted) verified++;
            if (accepted && [profile.video isEqual:video] && [profile.route isEqual:route]) { matches++;chosen=profile; }
            if (samples.count<4) [samples addObject:@{@"occurrence":child[@"occurrence"],@"video_hash":RVDADiagVideo(profile.video),
                @"route":profile.route ?: RVDATemplate(RVDAPath(child[@"element"],@[@1,@168777401])) ?: @"unknown",
                @"blocker":accepted ? @"ready" : child[@"blocker"],@"child_bytes":@([child[@"element"] length])}];
        }
        if (!blocker && matches!=1) blocker=matches ? @"shelf_child_join_ambiguous" : @"shelf_child_join_missing";
        dispatch_async(dispatch_get_main_queue(),^{
            RVDANativeChildPending--;observation.pending=NO;
            if (!blocker || observation.attempts>=2) observation.element=nil;
            id currentNode=weakNode,currentOwner=weakOwner,currentCollection=weakCollection;
            RVDADiagRecord(@"N11_viewmodel_child_join",nil,blocker,
                @{@"video_hash":RVDADiagVideo(video),@"surface":surface,@"route":route,@"children":@(children.count),
                  @"verified_children":@(verified),@"matching_children":@(matches),@"native_text_length":@(textLength),@"samples":samples});
            if (RVDAUIDiagnosticsArmed) {
                if ([objc_getAssociatedObject(entry,RVDANativeShelfCaptureEpochKey) unsignedLongLongValue]!=RVDALiveEpoch) {
                    objc_setAssociatedObject(entry,RVDANativeShelfCaptureEpochKey,@(RVDALiveEpoch),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                    RVDALiveCaptureRoot(parentSource,@"RVDANestedNativeShelfOwner",surface,nil);
                }
                if (chosen) RVDALiveCaptureRoot(chosen.source,@"RVDANestedNativeViewModelChild",surface,nil);
            }
            while (RVDANativeChildPending<8 && RVDANativeChildDeferred.count && UIApplication.sharedApplication.applicationState==UIApplicationStateActive) {
                RVDANativeChildObservation *next=RVDANativeChildDeferred.anyObject;[RVDANativeChildDeferred removeObject:next];
                RVDANativeChildPrepare(next);
            }
            if (blocker || revision!=RVDAUISettingsRevision || !currentNode || !currentOwner || !currentCollection ||
                RVObject(currentOwner,"entry")!=entry || RVObject(currentCollection,"element")!=collectionElement ||
                ![RVObject(RVAdRootElement(entry),"elementData") isEqual:parentSource] || RVObject(currentNode,"element")!=capture.element) return;
            RVDANestedInput *input=[RVDANestedInput new];input.nativeCollection=YES;input.owner=currentOwner;input.collection=currentCollection;
            input.collectionElement=collectionElement;input.entry=entry;input.parentSource=parentSource;input.source=chosen.source;input.verifiedProfile=chosen;
            capture.input=chosen.source;RVDANestedAccept(currentNode,capture.element,input,capture);
        });
    }});
}
static void RVDANestedInstall(void) {
    [NSNotificationCenter.defaultCenter addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
        while (RVDANativeChildPending<8 && RVDANativeChildDeferred.count) {
            RVDANativeChildObservation *next=RVDANativeChildDeferred.anyObject;[RVDANativeChildDeferred removeObject:next];RVDANativeChildPrepare(next);
        }
    }];
    RVDAUIHook(@"ELMCollectionNode",@"collectionNode:nodeBlockForItemAtIndexPath:",@"@?@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id collection,id nativeCollection,id indexPath) {
            id (^native)(void)=((id(*)(id,SEL,id,id))original)(collection,sel,nativeCollection,indexPath);
            RVDADiagCount(@"nested.native_collection_factory_observed");if (!native) return native;
            __weak id weakCollection=collection;
            // Native block descriptor is @"ELMCellNode"8@?0. Never inspect its
            // opaque C++ captures or change the native arguments/result.
            return [^id(void) {
                if (RVDAUIFactoryScope) return native();
                RVDAUIFactoryCapture *capture=[RVDAUIFactoryCapture new];id node=nil;
                RVDAUIFactoryScope=capture;
                @try { node=native(); } @finally { RVDAUIFactoryScope=nil; }
                if (node && capture.callbacks==1 && capture.element && capture.controller) {
                    RVDANativeChildObservation *observation=[RVDANativeChildObservation new];
                    observation.collection=weakCollection;observation.node=node;observation.nativeController=capture.controller;
                    observation.element=capture.element;observation.callbacks=capture.callbacks;
                    objc_setAssociatedObject(node,RVDANativeChildKey,observation,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                    dispatch_async(dispatch_get_main_queue(),^{ RVDANativeChildPrepare(observation); });
                } else RVDADiagCount(@"nested.native_collection_callback_missing");
                return node;
            } copy];
        };
    });
    RVDAUIHook(@"ELMCollectionNode",@"collectionNode:willDisplayItemWithNode:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id collection,id nativeCollection,id node) {
            ((void(*)(id,SEL,id,id))original)(collection,sel,nativeCollection,node);
            RVDANativeChildObservation *observation=objc_getAssociatedObject(node,RVDANativeChildKey);
            if (observation && observation.collection==collection) dispatch_async(dispatch_get_main_queue(),^{ RVDANativeChildPrepare(observation); });
        };
    });

    RVDAUIHook(@"YTInnerTubeCollectionNodeSectionDataSource",@"initWithSection:controller:service:context:",@"@@:@@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id section,id controller,id service,id context) {
            id result=((id(*)(id,SEL,id,id,id,id))original)(object,sel,section,controller,service,context);
            if (result) { RVDANestedOwner *owner=[RVDANestedOwner new];owner.controller=controller;
                objc_setAssociatedObject(result,RVDANestedOwnerKey,owner,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
            return result;
        };
    });
    RVDAUIHook(@"YTInnerTubeCollectionNodeSectionDataSource",@"elementForItem:",@"@@:@",NO,^id(IMP original,SEL sel) {
        return ^id(id dataSource,id item) {
            id element=((id(*)(id,SEL,id))original)(dataSource,sel,item);
            if (!element) return element;
            RVDADiagCount(@"nested.element_for_item_observed");
            RVDANestedInput *input=RVDANestedRecordItem(dataSource,item);
            if (input) objc_setAssociatedObject(element,RVDANestedInputKey,input,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            return element;
        };
    });
    // The compatibility collection path returns YTCellController items and
    // lets their normal ELM block materialize the child. Record that exact item
    // instead of requiring a top-level YTSectionController/index contract.
    RVDAUIHook(@"YTInnerTubeCollectionNodeSectionDataSource",@"itemAtIndex:",@"@@:q",NO,^id(IMP original,SEL sel) {
        return ^id(id dataSource,NSInteger index) {
            id item=((id(*)(id,SEL,NSInteger))original)(dataSource,sel,index);
            RVDADiagCount(@"nested.collection_item_observed");
            if (item) {
                RVDANestedInput *input=RVDANestedRecordItem(dataSource,item);
                objc_setAssociatedObject(item,RVDANestedInputKey,input,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            }
            return item;
        };
    });
    // Read-only boundary for the modern diffable route. Its returned element
    // getter has a C++ aggregate ABI, so it cannot authorize our ELM adapter.
    RVDAUIHook(@"ELMNCollectionDataSource",@"diffableItemForSerializedElement:index:",@"@@:@Q",NO,^id(IMP original,SEL sel) {
        return ^id(id dataSource,id bytes,NSUInteger index) {
            id result=((id(*)(id,SEL,id,NSUInteger))original)(dataSource,sel,bytes,index);
            RVDADiagCount(@"nested.diffable_serialized_child_observed");
            if (RVDAUIDiagnosticsArmed && [bytes isKindOfClass:NSData.class] && [bytes length]<=128*1024) {
                NSData *snapshot=[bytes copy];dispatch_async(dispatch_get_main_queue(),^{
                    RVDALiveCaptureRoot(snapshot,@"RVDANestedDiffableItem",nil,nil);
                });
            }
            return result;
        };
    });
}
