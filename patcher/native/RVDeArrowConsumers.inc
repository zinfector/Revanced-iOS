// SPDX-License-Identifier: MIT
// Native keyed callbacks on one accepted factory's already-created descendants.
@implementation RVDAUIConsumerLease
- (void)retire {
    id target=self.controller;NSString *key=self.key;self.controller=nil;
    if (!target || !key) return;
    void (^remove)(void)=^{
        for (NSString *name in @[@"setOnNodeDidLoad:forKey:",@"setOnNodeDidLayout:forKey:"])
            if (RVCan(target,name.UTF8String,@"v@:@?@")) ((void(*)(id,SEL,id,id))objc_msgSend)(target,NSSelectorFromString(name),nil,key);
    };
    if (NSThread.isMainThread) remove();else dispatch_async(dispatch_get_main_queue(),remove);
}
- (void)dealloc { [self retire]; }
@end
static void RVDAUIRetireConsumers(RVDAUIBinding *binding) {
    for (RVDAUIConsumerLease *lease in binding.consumerLeases) [lease retire];
    [binding.consumerLeases removeAllObjects];binding.consumerEpoch++;
    binding.consumerQueued=NO;binding.consumerPasses=0;binding.nativeRootComponent=nil;
}
static id RVDAUIConsumerRoot(id node,RVDAUIBinding *binding) {
    id component=RVObject(RVObject(node,"nodeController"),"owningComponent");
    NSMutableSet *seen=[NSMutableSet set];
    for (NSUInteger depth=0;component && depth<24;depth++) {
        NSValue *identity=[NSValue valueWithNonretainedObject:component];if ([seen containsObject:identity]) return nil;[seen addObject:identity];
        if ([RVDAResourceName(RVObject(component,"templateURI")) isEqual:binding.profile.route]) return component;
        component=RVObject(component,"parent") ?: RVObject(component,"owningComponent");
    }return nil;
}
static BOOL RVDAUIConsumerOwned(id node,RVDAUIBinding *binding) {
    return binding.nativeRootComponent && RVDAUIConsumerRoot(node,binding)==binding.nativeRootComponent;
}
static void RVDAUIBindConsumerRoot(RVDAUIBinding *binding,id root) {
    if (!root || !RVDAUICurrent(binding)) return;
    NSMutableArray *queue=[NSMutableArray arrayWithObject:root];
    for (NSUInteger index=0;index<queue.count && index<256;index++) {
        id node=queue[index],component=RVDAUIConsumerRoot(node,binding);
        NSData *data=RVObject(RVObject(component,"element"),"data"),*model=RVDAPath(data,@[@1,@168777401,@5]);
        BOOL matches=NO;
        if (binding.profile.watchTitle) {
            NSData *original=RVDAPath(binding.profile.source,[binding.profile.componentPath arrayByAddingObject:@5]);
            NSData *title=RVDAPath(original,binding.profile.titlePath);
            // Even a projected watch root must retain every other native branch.
            NSData *restored=title ? RVDASetPath(model,binding.profile.titlePath,title) : nil;
            matches=restored && [restored isEqual:original];
        } else matches=[RVDAText(RVDAPath(model,binding.profile.identityPath)) isEqual:binding.video];
        if (component && matches) { binding.nativeRootComponent=component;return; }
        id children=RVObject(node,"subnodes");if ([children isKindOfClass:NSArray.class])
            for (id child in children) if (queue.count<256 && [queue indexOfObjectIdenticalTo:child]==NSNotFound) [queue addObject:child];
    }binding.nativeRootComponent=nil;
}
static void RVDAUIObserveConsumers(RVDAUIBinding *binding,id root,uint64_t generation,NSUInteger attempt);
static void RVDAUIScheduleConsumers(RVDAUIBinding *binding,NSTimeInterval delay) {
    if (!NSThread.isMainThread || !binding.profile.liveCard || !binding.parts || RVDAUIOriginal(binding) ||
        binding.consumerQueued || binding.consumerPasses>=32 || !binding.nativeFactoryRoot || !RVDAUICurrent(binding)) return;
    binding.consumerQueued=YES;uint64_t generation=binding.generation,epoch=binding.consumerEpoch;__weak RVDAUIBinding *weak=binding;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(delay*NSEC_PER_SEC)),dispatch_get_main_queue(),^{
        RVDAUIBinding *current=weak;if (!current || current.generation!=generation || current.consumerEpoch!=epoch) return;
        current.consumerQueued=NO;
        if (!current.parts || RVDAUIOriginal(current) || !RVDAUICurrent(current)) { RVDAUIRetireConsumers(current);return; }
        id node=current.nativeFactoryRoot;if (!node) { RVDAUIRetireConsumers(current);return; }
        current.consumerPasses++;RVDAUIObserveConsumers(current,node,generation,current.consumerPasses-1);
    });
}
static void RVDAUIRegisterConsumer(RVDAUIBinding *binding,id node) {
    if (binding.consumerLeases.count>=64) return;
    id controller=RVObject(node,"nodeController");if (!controller) return;
    for (RVDAUIConsumerLease *lease in binding.consumerLeases) if (lease.controller==controller) return;
    if (!RVCan(controller,"setOnNodeDidLoad:forKey:",@"v@:@?@") || !RVCan(controller,"setOnNodeDidLayout:forKey:",@"v@:@?@")) return;
    if (!binding.consumerLeases) binding.consumerLeases=[NSMutableArray array];
    RVDAUIConsumerLease *lease=[RVDAUIConsumerLease new];lease.controller=controller;
    lease.key=[NSString stringWithFormat:@"rvport.dearrow.%llu.%llu.%lu",(unsigned long long)binding.generation,(unsigned long long)binding.consumerEpoch,(unsigned long)binding.consumerLeases.count];
    __weak RVDAUIBinding *weak=binding;uint64_t generation=binding.generation,epoch=binding.consumerEpoch;
    void (^callback)(id,id)=^(id nativeController,id context) {
        // Native invocation has already released its callback-map lock. Do not
        // read/materialize the graph from this callback's construction thread.
        RVDAUIBinding *current=weak;if (!current) return;
        @synchronized(current) { if (current.consumerCallbackPending) return;current.consumerCallbackPending=YES; }
        dispatch_async(dispatch_get_main_queue(),^{
            @synchronized(current) { current.consumerCallbackPending=NO; }
            if (current.generation!=generation || current.consumerEpoch!=epoch) return;
            RVDAUIScheduleConsumers(current,0);
        });
    };
    for (NSString *name in @[@"setOnNodeDidLoad:forKey:",@"setOnNodeDidLayout:forKey:"])
        ((void(*)(id,SEL,id,id))objc_msgSend)(controller,NSSelectorFromString(name),callback,lease.key);
    [binding.consumerLeases addObject:lease];
}
static BOOL RVDAUIConsumerVisible(id node) {
    if (!RVBool(node,"isNodeLoaded") || RVBool(node,"isLayerBacked")) return NO;
    UIView *view=RVObject(node,"view");if (![view isKindOfClass:UIView.class] || !view.window || CGRectIsEmpty(view.bounds)) return NO;
    for (UIView *v=view;v;v=v.superview) if (v.hidden || v.alpha<.05) return NO;
    return !CGRectIsEmpty(CGRectIntersection([view convertRect:view.bounds toView:view.window],view.window.bounds));
}
static void RVDAUIObserveConsumers(RVDAUIBinding *binding,id root,uint64_t generation,NSUInteger attempt) {
    if (!NSThread.isMainThread || !binding.profile.liveCard || !binding.parts || RVDAUIOriginal(binding) || binding.generation!=generation || !RVDAUICurrent(binding)) return;
    if (!binding.nativeRootComponent) RVDAUIBindConsumerRoot(binding,root);
    Class textClass=NSClassFromString(@"ELMTextNode"),imageClass=NSClassFromString(@"ELMImageNode");
    NSArray *urls=binding.profile.imageURLs;
    NSMutableArray *queue=[NSMutableArray arrayWithObject:root];NSUInteger owned=0,texts=0,images=0,titleMatches=0,imageMatches=0,titleApplied=0,imageApplied=0;
    BOOL titleVisible=NO,imageVisible=NO;
    NSString *expected=RVDAUITitle(binding);NSData *expectedBytes=RVDAUIImage(binding) ? binding.imageBytes : nil;
    for (NSUInteger index=0;index<queue.count && index<256;index++) {
        id node=queue[index];BOOL text=textClass && [node isKindOfClass:textClass],image=imageClass && [node isKindOfClass:imageClass];
        if (RVDAUIConsumerOwned(node,binding)) {
            owned++;RVDAUIRegisterConsumer(binding,node);
            if (text && binding.profile.titleRole) {
                texts++;id attributed=RVObject(node,"attributedText");NSString *value=[attributed isKindOfClass:NSAttributedString.class] ? [attributed string] : nil;
                titleMatches+=[value isEqual:binding.originalTitle];
                if (expected && [value isEqual:expected]) { titleApplied++;titleVisible|=RVDAUIConsumerVisible(node); }
            }
            if (image && binding.profile.imageRole) {
                images++;id value=RVObject(node,"URL");NSString *url=[value isKindOfClass:NSURL.class] ? [value absoluteString] : [value isKindOfClass:NSString.class] ? value : nil;
                NSData *input=RVDAPath(RVObject(RVObject(node,"element"),"data"),@[@1,@158796345]);
                NSArray *inputURLs=RVDALiveCardImageURLs(input,binding.video);
                BOOL original=(url && [urls containsObject:url]) || (inputURLs.count && [NSSet setWithArray:inputURLs].count==[NSSet setWithArray:urls].count && [[NSSet setWithArray:inputURLs] isEqual:[NSSet setWithArray:urls]]);
                imageMatches+=original;
                NSArray *sources=RVVoteFields(input);NSUInteger count=0;NSData *bytes=nil;
                for (RVVoteField *source in sources) if (source.number==1) { count++;if (source.wire==2) bytes=RVDAChild(source.value,2); }
                UIImage *decoded=RVObject(node,"image");
                if (expectedBytes && count==1 && [bytes isEqual:expectedBytes] && [decoded isKindOfClass:UIImage.class] && decoded.size.width>0 && decoded.size.height>0) {
                    imageApplied++;imageVisible|=RVDAUIConsumerVisible(node);
                }
            }
        }
        id children=RVObject(node,"subnodes");if ([children isKindOfClass:NSArray.class])
            for (id child in children) if (queue.count<256 && [queue indexOfObjectIdenticalTo:child]==NSNotFound) [queue addObject:child];
    }
    BOOL titleFound=titleMatches==1,imageFound=imageMatches==1;
    BOOL changed=(!binding.titleConsumerVerified && titleFound) || (!binding.imageConsumerVerified && imageFound);
    binding.titleConsumerVerified|=titleFound;binding.imageConsumerVerified|=imageFound;binding.consumerObservations++;
    NSString *blocker=!owned ? @"live_native_owned_descendants_missing" :
        (binding.parts&RVDeArrowPartsTitle) && !binding.titleConsumerVerified ? (titleMatches>1 ? @"live_native_headline_ambiguous" : @"live_native_headline_unconfirmed") :
        (binding.parts&RVDeArrowPartsThumbnail) && !binding.imageConsumerVerified ? (imageMatches>1 ? @"live_native_thumbnail_ambiguous" : @"live_native_thumbnail_unconfirmed") : nil;
    binding.consumerCheckpoint=@{@"attempt":@(attempt+1),@"nodes":@(queue.count),@"factory_root_bound":@(binding.nativeRootComponent!=nil),@"owned_nodes":@(owned),@"text_nodes":@(texts),@"image_nodes":@(images),
        @"title_matches":@(titleMatches),@"image_matches":@(imageMatches),@"title_confirmed":@(binding.titleConsumerVerified),@"thumbnail_confirmed":@(binding.imageConsumerVerified),
        @"title_replacement_consumed":@(titleApplied==1),@"image_replacement_consumed":@(imageApplied==1),@"title_visible":@(titleVisible && titleApplied==1),@"image_visible":@(imageVisible && imageApplied==1),
        @"keyed_leases":@(binding.consumerLeases.count),@"blocker":blocker ?: @"ready"};
    RVDADiagRecord(@"D04_live_native_consumers",binding,blocker,binding.consumerCheckpoint);
    if (titleApplied==1 || imageApplied==1) RVDADiagRecord(@"D09_live_native_replacement_consumed",binding,nil,@{@"title":@(titleApplied==1),@"image":@(imageApplied==1),@"generation_checked":@YES});
    if ((titleVisible && titleApplied==1) || (imageVisible && imageApplied==1)) {
        RVDADiagRecord(@"D10_live_replacement_visible",binding,nil,@{@"title":@(titleVisible && titleApplied==1),@"image":@(imageVisible && imageApplied==1)});
        [binding.request recordCheckpoint:RVDeArrowCheckpointReplacementApplied blocker:RVDeArrowBlockerNone];
    }
    if (changed) RVDAUIRefresh(binding);
    if (attempt<3 && (blocker || (expected && titleApplied!=1) || (expectedBytes && imageApplied!=1))) RVDAUIScheduleConsumers(binding,.15);
}
