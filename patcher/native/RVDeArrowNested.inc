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
    }return changed;
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
    if (!input.dataSource || !input.item || !surface || RVObject(input.item,"entry")!=input.entry ||
        ![RVObject(RVAdRootElement(input.entry),"elementData") isEqual:input.source] || RVObject(node,"element")!=element) {
        RVDADiagCount(@"nested.rejected_owner_or_source");return;
    }
    NSArray<RVDAProjection *> *profiles=RVDAProfiles(input.source,nil);
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
    state.ownerSummary=@{@"strategy":@"independent_nested_child",@"data_source":NSStringFromClass([input.dataSource class]),
        @"factory_node":NSStringFromClass([node class]),@"owner":NSStringFromClass([input.owner class])};
    objc_setAssociatedObject(node,RVDAUIBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [RVDAUIBindings addObject:state];RVDAUIRequest(state);
    capture.inputVerified=RVDAUIFactoryInputAccepted(@[state],input.source);
    state.factorySerial=++RVDAUIFactorySerial;state.submittedData=input.source;
    RVDAUIAcceptFactory(state,node,capture);
    RVDADiagRecord(@"N04_nested_child_bound",state,nil,@{@"single_video":@YES,@"entry_identity_checked":@YES,
        @"native_element_identity_checked":@YES,@"serialized_source_checked":@YES});
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
static void RVDANestedInstall(void) {
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
            }return item;
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
            }return result;
        };
    });
}
