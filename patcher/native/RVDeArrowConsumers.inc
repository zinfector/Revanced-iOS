// SPDX-License-Identifier: MIT
// Observe actual native descendants after construction, outside component locks.
// Do not query component.node: that accessor may trigger lazy materialization.
static id RVDAUIConsumerRoot(id node) {
    id component=RVObject(RVObject(node,"nodeController"),"owningComponent");
    NSMutableSet *seen=[NSMutableSet set];
    for (NSUInteger depth=0;component && depth<24;depth++) {
        NSValue *identity=[NSValue valueWithNonretainedObject:component];if ([seen containsObject:identity]) return nil;[seen addObject:identity];
        if ([RVDAResourceName(RVObject(component,"templateURI")) isEqual:RVDALiveCardKey()]) return component;
        id parent=RVObject(component,"parent");
        component=parent ?: RVObject(component,"owningComponent");
    }return nil;
}
static BOOL RVDAUIConsumerOwned(id node,RVDAUIBinding *binding) {
    id component=RVDAUIConsumerRoot(node);return component && component==binding.nativeRootComponent;
}
static void RVDAUIBindConsumerRoot(RVDAUIBinding *binding,id root) {
    // Only called on the node returned by this entry's native factory block.
    // A second instance of the same video elsewhere cannot supply permission.
    NSMutableArray *queue=[NSMutableArray arrayWithObject:root];
    for (NSUInteger index=0;index<queue.count && index<256;index++) {
        id node=queue[index],component=RVDAUIConsumerRoot(node);
        NSData *data=RVObject(RVObject(component,"element"),"data");
        NSString *video=RVDAText(RVDAPath(RVDAPath(data,@[@1,@168777401,@5]),binding.profile.identityPath));
        if (component && [video isEqual:binding.video]) { binding.nativeRootComponent=component;return; }
        id children=RVObject(node,"subnodes");if ([children isKindOfClass:NSArray.class])
            for (id child in children) if (queue.count<256 && [queue indexOfObjectIdenticalTo:child]==NSNotFound) [queue addObject:child];
    }
    binding.nativeRootComponent=nil;
}
static void RVDAUIObserveConsumers(RVDAUIBinding *binding,id root,uint64_t generation,NSUInteger attempt) {
    if (!NSThread.isMainThread || !binding.profile.liveCard || binding.generation!=generation || !RVDAUICurrent(binding)) return;
    Class textClass=NSClassFromString(@"ELMTextNode"),imageClass=NSClassFromString(@"ELMImageNode");
    NSData *model=RVDAPath(binding.profile.source,[binding.profile.componentPath arrayByAddingObject:@5]);
    NSArray *urls=RVDALiveCardImageURLs(RVDAPath(model,@[@232954548,@18,@1,@1,@1]),binding.video);
    NSMutableArray *queue=[NSMutableArray arrayWithObject:root];NSUInteger owned=0,texts=0,images=0;
    BOOL titleFound=NO,imageFound=NO,titleApplied=NO,imageApplied=NO;
    NSString *expected=RVDAUITitle(binding);NSData *expectedBytes=binding.imageBytes;
    for (NSUInteger index=0;index<queue.count && index<256;index++) {
        id node=queue[index];BOOL text=textClass && [node isKindOfClass:textClass],image=imageClass && [node isKindOfClass:imageClass];
        if ((text || image) && RVDAUIConsumerOwned(node,binding)) {
            owned++;
            if (text) {
                texts++;id attributed=RVObject(node,"attributedText");NSString *value=[attributed isKindOfClass:NSAttributedString.class] ? [attributed string] : nil;
                titleFound|=[value isEqual:binding.originalTitle];titleApplied|=expected && [value isEqual:expected];
            }
            if (image) {
                images++;id value=RVObject(node,"URL");NSString *url=[value isKindOfClass:NSURL.class] ? [value absoluteString] : [value isKindOfClass:NSString.class] ? value : nil;
                imageFound|=url && [urls containsObject:url];
                NSData *elementData=RVObject(RVObject(node,"element"),"data");
                NSData *imageInput=RVDAPath(elementData,@[@1,@158796345]);
                // ImageType.sources1 -> ImageSource.imageData2. The template
                // passes Image.sourcesArray into this repeated field directly.
                // Compare the actual encoded input, not the node's existence.
                NSArray *sources=RVVoteFields(imageInput);NSUInteger count=0;NSData *bytes=nil;
                for (RVVoteField *source in sources) if (source.number==1) { count++;if (source.wire==2) bytes=RVDAChild(source.value,2); }
                imageApplied|=expectedBytes && count==1 && [bytes isEqual:expectedBytes];
            }
        }
        id children=RVObject(node,"subnodes");if ([children isKindOfClass:NSArray.class])
            for (id child in children) if (queue.count<256 && [queue indexOfObjectIdenticalTo:child]==NSNotFound) [queue addObject:child];
    }
    BOOL changed=(!binding.titleConsumerVerified && titleFound) || (!binding.imageConsumerVerified && imageFound);
    binding.titleConsumerVerified|=titleFound;binding.imageConsumerVerified|=imageFound;binding.consumerObservations++;
    NSString *blocker=!owned ? @"live_native_owned_descendants_missing" : !binding.titleConsumerVerified ? @"live_native_headline_unconfirmed" :
        !binding.imageConsumerVerified ? @"live_native_thumbnail_unconfirmed" : nil;
    binding.consumerCheckpoint=@{@"attempt":@(attempt+1),@"nodes":@(queue.count),@"factory_root_bound":@(binding.nativeRootComponent!=nil),@"owned_nodes":@(owned),@"text_nodes":@(texts),@"image_nodes":@(images),
        @"title_confirmed":@(binding.titleConsumerVerified),@"thumbnail_confirmed":@(binding.imageConsumerVerified),
        @"title_replacement_consumed":@(titleApplied),@"image_replacement_consumed":@(imageApplied),@"blocker":blocker ?: @"ready"};
    RVDADiagRecord(@"D04_live_native_consumers",binding,blocker,binding.consumerCheckpoint);
    if (titleApplied || imageApplied) RVDADiagRecord(@"D09_live_native_replacement_consumed",binding,nil,
        @{@"title":@(titleApplied),@"image":@(imageApplied),@"generation_checked":@YES});
    if (changed) RVDAUIRefresh(binding);
    if (attempt<3 && (blocker || (expected && !titleApplied) || (expectedBytes && !imageApplied))) {
        __weak RVDAUIBinding *weak=binding;__weak id weakRoot=root;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(0.15*NSEC_PER_SEC)),dispatch_get_main_queue(),^{
            RVDAUIBinding *current=weak;id node=weakRoot;if (current && node) RVDAUIObserveConsumers(current,node,generation,attempt+1);
        });
    }
}
