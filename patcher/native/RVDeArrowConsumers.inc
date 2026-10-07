// SPDX-License-Identifier: MIT
// Ownership comes from one accepted row's native factory, never guessed C++ data.
@interface RVDAUIFactoryCapture : NSObject
@property(nonatomic,strong) NSData *input;
@property(nonatomic,strong) id element,controller;
@property(nonatomic) NSUInteger callbacks;
@property(nonatomic) BOOL entered,inputVerified;
@end
@implementation RVDAUIFactoryCapture @end
static _Thread_local __unsafe_unretained RVDAUIFactoryCapture *RVDAUIFactoryScope;

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
    binding.nativeOwnerCheckpoint=nil;binding.consumerCheckpoint=nil;binding.nativeFactoryElement=nil;binding.nativeExpectedElement=nil;
    binding.nativeFactoryController=nil;binding.nativeFactoryInput=nil;binding.factoryProvenanceVerified=NO;
    binding.imageWitnesses=nil;binding.imageWitnessSource=nil;binding.nativeTitleProjected=NO;binding.nativeImageProjected=NO;
}
// Only reviewed presentation paths can differ in the immutable factory argument.
static BOOL RVDAUINativeModelMatches(RVDAUIBinding *binding,NSData *model) {
    RVDAProjection *profile=binding.profile;
    NSData *original=RVDAPath(profile.source,[profile.componentPath arrayByAddingObject:@5]);
    if (!original || !model) return NO;
    if (!profile.watchTitle) {
        NSDictionary *identity=RVDALiveCardNavigation(model);
        if (![identity[@"blocker"] isEqual:@"ready"] || ![identity[@"video"] isEqual:binding.video] ||
            ![identity[@"identity_path"] isEqual:profile.identityPath] || ![identity[@"command_type"] isEqual:profile.commandType]) return NO;
    }
    // ID equality alone cannot distinguish two occurrences of the same video.
    // Restore only permitted presentation inputs, then compare the complete model.
    NSMutableArray *paths=[NSMutableArray array];
    if (profile.titleRole) { [paths addObject:profile.titlePath];if (profile.titleEntityPath) [paths addObject:profile.titleEntityPath]; }
    if (profile.imageRole) [paths addObject:[profile.imagePath arrayByAddingObject:@1]];
    if (!profile.watchTitle && profile.titleRole && RVDAChild(RVDAPath(original,@[@232954548,@18]),2)) [paths addObject:@[@232954548,@18,@2]];
    NSData *restored=model;
    for (NSArray *path in paths) {
        NSData *value=RVDAPath(original,path);if (value) restored=RVDASetPath(restored,path,value);
        if (!restored) return NO;
    }return [restored isEqual:original];
}

static BOOL RVDAUIFactoryInputAccepted(NSArray<RVDAUIBinding *> *bindings,NSData *input) {
    if (!NSThread.isMainThread || !input || !bindings.count) return NO;
    NSData *source=bindings[0].profile.source,*restored=input;
    for (RVDAUIBinding *binding in bindings) {
        if (!binding.profile.liveCard || !RVDAUICurrent(binding) || ![binding.profile.source isEqual:source]) return NO;
        NSArray *path=[binding.profile.componentPath arrayByAddingObject:@5];
        if (!RVDAUINativeModelMatches(binding,RVDAPath(input,path))) return NO;
        NSData *original=RVDAPath(source,path);restored=RVDASetPath(restored,path,original);
        if (!restored) return NO;
    }
    return [restored isEqual:source];
}
static const void *RVDAUIElementIdentity(id element) {
    if (!RVCan(element,"instance",@"r^v@:")) return NULL;
    // Compare the opaque identity while its ELMElement keeps the arena alive.
    // Never dereference it or call a C++ aggregate-returning method.
    return ((const void *(*)(id,SEL))objc_msgSend)(element,sel_registerName("instance"));
}
static id RVDAUIExpectedElement(RVDAUIBinding *binding,id element) {
    NSArray *path=binding.profile.componentPath;
    if (path.count<2 || ![path[path.count-2] isEqual:@1] || ![path.lastObject isEqual:@168777401]) return nil;
    NSData *input=binding.nativeFactoryInput;
    for (NSUInteger index=0;index+2<path.count;index++) {
        id step=path[index];NSUInteger occurrence=0;
        if ([step isKindOfClass:NSNumber.class]) { if (![step isEqual:@3]) return nil; }
        else if ([step isKindOfClass:NSDictionary.class] && [step[@"field"] isEqual:@3]) occurrence=[step[@"occurrence"] unsignedIntegerValue];
        else return nil;
        NSUInteger count=0;for (RVVoteField *field in RVVoteFields(input)) if (field.number==3) { if (field.wire!=2) return nil;count++; }
        NSArray *children=RVObject(element,"childElements");
        if (![children isKindOfClass:NSArray.class] || children.count!=count || count>16 || occurrence>=count) return nil;
        input=RVDAPathChild(input,step);element=children[occurrence];
    }
    return RVDAUIElementIdentity(element) ? element : nil;
}
static void RVDAUIGraphAppend(NSMutableArray *queue,NSHashTable *seen,id object) {
    if (!object || queue.count>=256 || [seen containsObject:object]) return;
    [seen addObject:object];[queue addObject:object];
}
static NSArray *RVDAUINativeGraph(RVDAUIBinding *binding,id root) {
    NSMutableArray *queue=[NSMutableArray array];NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    RVDAUIGraphAppend(queue,seen,root);RVDAUIGraphAppend(queue,seen,binding.nativeFactoryController);
    Class componentClass=NSClassFromString(@"ELMComponent"),controllerClass=NSClassFromString(@"ASNodeController"),nodeClass=NSClassFromString(@"ASDisplayNode");
    for (NSUInteger index=0;index<queue.count;index++) {
        id object=queue[index];
        if (componentClass && [object isKindOfClass:componentClass]) {
            RVDAUIGraphAppend(queue,seen,RVObject(object,"materializedInstance"));
            // ELMComponent.children is always nil; node can force lazy creation.
            continue;
        }
        if (controllerClass && [object isKindOfClass:controllerClass]) {
            RVDAUIGraphAppend(queue,seen,RVObject(object,"owningComponent"));
            NSArray *children=RVObject(object,"children");if ([children isKindOfClass:NSArray.class]) for (id child in children) RVDAUIGraphAppend(queue,seen,child);
            if (RVBool(object,"isNodeOnControllerLoaded") || RVBool(object,"isNodeLoaded")) RVDAUIGraphAppend(queue,seen,RVObject(object,"node"));
        }
        if (nodeClass && [object isKindOfClass:nodeClass]) {
            RVDAUIGraphAppend(queue,seen,RVObject(object,"nodeController"));
            for (NSString *name in @[@"subnodes",@"yogaChildren"]) {
                NSArray *children=RVObject(object,name.UTF8String);if ([children isKindOfClass:NSArray.class]) for (id child in children) RVDAUIGraphAppend(queue,seen,child);
            }
        }
    }return queue;
}
static BOOL RVDAUIComponentMatches(RVDAUIBinding *binding,id component) {
    const void *expected=RVDAUIElementIdentity(binding.nativeExpectedElement);
    return binding.factoryProvenanceVerified && expected && expected==RVDAUIElementIdentity(RVObject(component,"element")) &&
        [RVDAResourceName(RVObject(component,"templateURI")) isEqual:binding.profile.route];
}
static id RVDAUIConsumerRoot(id node,RVDAUIBinding *binding) {
    id component=RVObject(RVObject(node,"nodeController"),"owningComponent");
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    for (NSUInteger depth=0;component && depth<24;depth++) {
        if ([seen containsObject:component]) return nil;[seen addObject:component];
        if (RVDAUIComponentMatches(binding,component)) return component;
        component=RVObject(component,"parent") ?: RVObject(component,"owningComponent");
    }return nil;
}
static BOOL RVDAUIConsumerOwned(id node,RVDAUIBinding *binding) {
    return binding.nativeRootComponent && RVDAUIConsumerRoot(node,binding)==binding.nativeRootComponent;
}
static void RVDAUIBindConsumerRoot(RVDAUIBinding *binding,id root) {
    if (!root || !RVDAUICurrent(binding)) return;
    NSArray *graph=RVDAUINativeGraph(binding,root);NSHashTable *matches=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    NSHashTable *inspected=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];NSMutableArray *samples=[NSMutableArray array];
    Class cls=NSClassFromString(@"ELMComponent");
    for (id object in graph) {
        id component=cls && [object isKindOfClass:cls] ? object : RVDAUIConsumerRoot(object,binding);
        if (!component || [inspected containsObject:component]) continue;[inspected addObject:component];
        BOOL accepted=RVDAUIComponentMatches(binding,component);if (accepted) [matches addObject:component];
        if (RVDADiagEnabled() && samples.count<4) {
            id element=RVObject(component,"element");[samples addObject:@{@"component_class":NSStringFromClass([component class]),
                @"element_class":element ? NSStringFromClass([element class]) : @"absent",@"opaque_instance_contract":@(RVCan(element,"instance",@"r^v@:")),
                @"factory_element_identity_matches":@(RVDAUIElementIdentity(element) && RVDAUIElementIdentity(element)==RVDAUIElementIdentity(binding.nativeExpectedElement)),
                @"factory_input_verified":@(binding.factoryProvenanceVerified),@"resource_matches":@([RVDAResourceName(RVObject(component,"templateURI")) isEqual:binding.profile.route]),@"component_accepted":@(accepted)}];
        }
    }
    binding.nativeRootComponent=matches.count==1 ? matches.anyObject : nil;
    binding.nativeOwnerCheckpoint=@{@"inspected_components":@(inspected.count),@"matching_components":@(matches.count),@"graph_objects":@(graph.count),@"candidates":samples,
        @"factory_callback_count":@(binding.factoryCallbackCount),@"factory_provenance_verified":@(binding.factoryProvenanceVerified),@"expected_element_present":@(binding.nativeExpectedElement!=nil)};
    if (matches.count>1) RVDAUIEvent(@"D04_native_component_rejected",binding,@"live_native_component_ambiguous");
}
static void RVDAUIObserveConsumers(RVDAUIBinding *binding,id root,uint64_t generation,NSUInteger attempt);
static void RVDAUIScheduleConsumers(RVDAUIBinding *binding,NSTimeInterval delay) {
    if (!NSThread.isMainThread || !binding.profile.liveCard || !binding.parts || RVDAUIOriginal(binding) || binding.consumerQueued || !binding.nativeFactoryRoot || !RVDAUICurrent(binding)) return;
    binding.consumerQueued=YES;uint64_t generation=binding.generation,epoch=binding.consumerEpoch;__weak RVDAUIBinding *weak=binding;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(delay*NSEC_PER_SEC)),dispatch_get_main_queue(),^{
        RVDAUIBinding *current=weak;if (!current || current.generation!=generation || current.consumerEpoch!=epoch) return;
        current.consumerQueued=NO;
        if (!current.parts || RVDAUIOriginal(current) || !RVDAUICurrent(current)) { RVDAUIRetireConsumers(current);return; }
        id node=current.nativeFactoryRoot;if (!node) { RVDAUIRetireConsumers(current);return; }
        current.consumerPasses++;RVDAUIObserveConsumers(current,node,generation,current.consumerPasses-1);
    });
}
static void RVDAUIRegisterController(RVDAUIBinding *binding,id controller) {
    if (!controller || !binding.factoryProvenanceVerified) return;
    if (!RVCan(controller,"setOnNodeDidLoad:forKey:",@"v@:@?@")) return;
    RVDAUIConsumerLease *lease=nil;for (RVDAUIConsumerLease *prior in binding.consumerLeases) if (prior.controller==controller) { lease=prior;break; }
    if (!binding.consumerLeases) binding.consumerLeases=[NSMutableArray array];
    if (!lease) {
        if (binding.consumerLeases.count>=64) return;
        lease=[RVDAUIConsumerLease new];lease.controller=controller;
        lease.key=[NSString stringWithFormat:@"rvport.dearrow.%llu.%llu.%lu",(unsigned long long)binding.generation,(unsigned long long)binding.consumerEpoch,(unsigned long)binding.consumerLeases.count];
        __weak RVDAUIBinding *weak=binding;uint64_t generation=binding.generation,epoch=binding.consumerEpoch;
        lease.callback=^(id nativeController,id context) {
            RVDAUIBinding *current=weak;if (!current) return;
            @synchronized(current) { if (current.consumerCallbackPending) return;current.consumerCallbackPending=YES; }
            dispatch_async(dispatch_get_main_queue(),^{
                @synchronized(current) { current.consumerCallbackPending=NO; }
                if (current.generation!=generation || current.consumerEpoch!=epoch) return;
                RVDAUIScheduleConsumers(current,.05);
            });
        };
        ((void(*)(id,SEL,id,id))objc_msgSend)(controller,sel_registerName("setOnNodeDidLoad:forKey:"),lease.callback,lease.key);
        [binding.consumerLeases addObject:lease];
    }
    Class component=NSClassFromString(@"ELMComponent");BOOL ready=!(component && [controller isKindOfClass:component]) || RVObject(controller,"materializedInstance")!=nil;
    // ELMComponent only retains pre-materialization load callbacks. Register
    // layout after the underlying controller exists, keeping the original key.
    if (!lease.layoutRegistered && ready && RVCan(controller,"setOnNodeDidLayout:forKey:",@"v@:@?@")) {
        ((void(*)(id,SEL,id,id))objc_msgSend)(controller,sel_registerName("setOnNodeDidLayout:forKey:"),lease.callback,lease.key);lease.layoutRegistered=YES;
    }
}
static void RVDAUIAcceptFactory(RVDAUIBinding *binding,id node,RVDAUIFactoryCapture *capture) {
    RVDAUIRetireConsumers(binding);binding.nativeFactoryRoot=node;binding.nativeFactoryController=capture.controller;
    binding.nativeFactoryElement=capture.element;binding.nativeFactoryInput=capture.input;binding.factoryCallbackCount=capture.callbacks;
    binding.factoryProvenanceVerified=capture.inputVerified && capture.callbacks==1 && capture.element && capture.controller;
    binding.nativeExpectedElement=binding.factoryProvenanceVerified ? RVDAUIExpectedElement(binding,capture.element) : nil;
    binding.factoryProvenanceVerified&=binding.nativeExpectedElement!=nil;
    NSData *model=RVDAPath(capture.input,[binding.profile.componentPath arrayByAddingObject:@5]);
    NSString *title=RVDAText(RVDAChild(RVDAPath(model,binding.profile.titlePath),1));
    binding.nativeTitleProjected=binding.factoryProvenanceVerified && binding.profile.titleRole && title && ![title isEqual:binding.originalTitle];
    NSData *image=binding.profile.imageRole ? RVDACheckedPath(model,[binding.profile.imagePath arrayByAddingObjectsFromArray:@[@1,@1,@2]],NULL,NULL) : nil;
    binding.nativeImageProjected=binding.factoryProvenanceVerified && image && [image isEqual:binding.imageBytes];
    RVDADiagRecord(@"D03_native_factory_provenance",binding,binding.factoryProvenanceVerified ? nil : @"native_factory_provenance_missing",
        @{@"callback_count":@(capture.callbacks),@"input_verified":@(capture.inputVerified),@"element_present":@(capture.element!=nil),
          @"controller_class":capture.controller ? NSStringFromClass([capture.controller class]) : @"absent",@"expected_element_present":@(binding.nativeExpectedElement!=nil)});
    // Bootstrap does not wait for title/image confirmation. It only observes
    // the native controller captured within this validated row factory.
    RVDAUIRegisterController(binding,binding.nativeFactoryController);
    RVDAUIBindConsumerRoot(binding,node);RVDAUIScheduleConsumers(binding,0);
}
static BOOL RVDAUIConsumerVisible(id node) {
    if (!RVBool(node,"isNodeLoaded")) return NO;
    if (RVBool(node,"isLayerBacked")) {
        CALayer *layer=RVObject(node,"layer");if (![layer isKindOfClass:CALayer.class] || CGRectIsEmpty(layer.bounds)) return NO;
        for (CALayer *parent=layer;parent;parent=parent.superlayer) {
            if (parent.hidden || parent.opacity<.05) return NO;
            id delegate=parent.delegate;
            if ([delegate isKindOfClass:UIView.class]) {
                UIView *view=delegate;if (!view.window) return NO;
                CGRect rect=[layer convertRect:layer.bounds toLayer:view.window.layer];
                return !CGRectIsEmpty(CGRectIntersection(rect,view.window.bounds));
            }
        }return NO;
    }
    UIView *view=RVObject(node,"view");if (![view isKindOfClass:UIView.class] || !view.window || CGRectIsEmpty(view.bounds)) return NO;
    for (UIView *v=view;v;v=v.superview) if (v.hidden || v.alpha<.05) return NO;
    return !CGRectIsEmpty(CGRectIntersection([view convertRect:view.bounds toView:view.window],view.window.bounds));
}
static NSData *RVDAUIPixelDigest(UIImage *image) {
    CGImageRef cg=image.CGImage;if (!cg) return nil;size_t width=CGImageGetWidth(cg),height=CGImageGetHeight(cg);
    if (!width || !height || width>4096 || height>4096 || width*height>4*1024*1024) return nil;
    NSMutableData *pixels=[NSMutableData dataWithLength:width*height*4];CGColorSpaceRef space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context=CGBitmapContextCreate(pixels.mutableBytes,width,height,8,width*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(space);
    if (!context) return nil;CGContextSetBlendMode(context,kCGBlendModeCopy);CGContextDrawImage(context,CGRectMake(0,0,width,height),cg);CGContextRelease(context);
    CC_SHA256_CTX hash;CC_SHA256_Init(&hash);uint32_t shape[3]={(uint32_t)width,(uint32_t)height,(uint32_t)image.imageOrientation};
    CC_SHA256_Update(&hash,shape,sizeof(shape));CC_SHA256_Update(&hash,pixels.bytes,(CC_LONG)pixels.length);unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256_Final(digest,&hash);
    return [NSData dataWithBytes:digest length:sizeof(digest)];
}
static void RVDAUIImageWitnesses(RVDAUIBinding *binding,NSArray<UIImage *> *images,NSData *bytes) {
    if (!images.count || !bytes) return;
    if (![binding.imageWitnessSource isEqual:bytes]) { binding.imageWitnessSource=bytes;binding.imageWitnesses=[NSMapTable weakToStrongObjectsMapTable]; }
    NSMutableArray *pending=[NSMutableArray array];
    for (UIImage *image in images) if (![binding.imageWitnesses objectForKey:image] && pending.count<8) {
        [binding.imageWitnesses setObject:NSNull.null forKey:image];[pending addObject:image];
    }
    if (!pending.count) return;
    __weak RVDAUIBinding *weak=binding;uint64_t generation=binding.generation,epoch=binding.consumerEpoch;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        @autoreleasepool {
            NSData *expected=RVDAUIPixelDigest([UIImage imageWithData:bytes]);NSMutableArray *matches=[NSMutableArray array];
            for (UIImage *image in pending) { NSData *actual=RVDAUIPixelDigest(image);[matches addObject:@(actual && expected && [actual isEqual:expected])]; }
            dispatch_async(dispatch_get_main_queue(),^{
                RVDAUIBinding *current=weak;if (!current || current.generation!=generation || current.consumerEpoch!=epoch || ![current.imageWitnessSource isEqual:bytes] || !RVDAUICurrent(current)) return;
                for (NSUInteger index=0;index<pending.count;index++) [current.imageWitnesses setObject:matches[index] forKey:pending[index]];
                RVDAUIScheduleConsumers(current,0);
            });
        }
    });
}
static void RVDAUIObserveConsumers(RVDAUIBinding *binding,id root,uint64_t generation,NSUInteger attempt) {
    if (!NSThread.isMainThread || !binding.profile.liveCard || !binding.parts || RVDAUIOriginal(binding) || binding.generation!=generation || !RVDAUICurrent(binding)) return;
    RVDAUIRegisterController(binding,binding.nativeFactoryController);
    if (!binding.nativeRootComponent) RVDAUIBindConsumerRoot(binding,root);
    Class textClass=NSClassFromString(@"ELMTextNode"),imageClass=NSClassFromString(@"ELMImageNode");NSArray *urls=binding.profile.imageURLs;
    NSArray *graph=RVDAUINativeGraph(binding,root);NSUInteger owned=0,texts=0,images=0,titleMatches=0,imageMatches=0,titleApplied=0,imageApplied=0;
    BOOL titleVisible=NO,imageVisible=NO;NSString *expected=RVDAUITitle(binding);
    NSData *model=RVDAPath(binding.nativeFactoryInput,[binding.profile.componentPath arrayByAddingObject:@5]);
    NSData *expectedBytes=RVDAUIImage(binding) ? binding.imageBytes : nil;
    NSData *projectedBytes=binding.profile.imageRole ? RVDACheckedPath(model,[binding.profile.imagePath arrayByAddingObjectsFromArray:@[@1,@1,@2]],NULL,NULL) : nil;
    if (![expectedBytes isEqual:projectedBytes]) expectedBytes=nil;
    NSMutableArray *decodedImages=[NSMutableArray array];
    for (id node in graph) if (RVDAUIConsumerOwned(node,binding)) {
        owned++;RVDAUIRegisterController(binding,RVObject(node,"nodeController"));
        if (textClass && [node isKindOfClass:textClass] && binding.profile.titleRole) {
            texts++;id attributed=RVObject(node,"attributedText");NSString *value=[attributed isKindOfClass:NSAttributedString.class] ? [attributed string] : nil;
            titleMatches+=[value isEqual:binding.originalTitle];
            if (expected && [value isEqual:expected]) { titleApplied++;titleVisible|=RVDAUIConsumerVisible(node); }
        }
        if (imageClass && [node isKindOfClass:imageClass] && binding.profile.imageRole) {
            images++;id value=RVObject(node,"URL");NSString *url=[value isKindOfClass:NSURL.class] ? [value absoluteString] : [value isKindOfClass:NSString.class] ? value : nil;
            imageMatches+=url && [urls containsObject:url];
            UIImage *decoded=RVObject(node,"image");
            if (expectedBytes && [decoded isKindOfClass:UIImage.class] && decoded.size.width>0 && decoded.size.height>0) {
                [decodedImages addObject:decoded];id witness=[binding.imageWitnesses objectForKey:decoded];
                if ([binding.imageWitnessSource isEqual:expectedBytes] && [witness isKindOfClass:NSNumber.class] && [witness boolValue]) { imageApplied++;imageVisible|=RVDAUIConsumerVisible(node); }
            }
        }
    }
    RVDAUIImageWitnesses(binding,decodedImages,expectedBytes);
    BOOL titleFound=titleMatches==1,imageFound=imageMatches==1;
    BOOL changed=(!binding.titleConsumerVerified && titleFound) || (!binding.imageConsumerVerified && imageFound);
    binding.titleConsumerVerified|=titleFound;binding.imageConsumerVerified|=imageFound;binding.consumerObservations++;
    NSString *blocker=!binding.factoryProvenanceVerified ? @"native_factory_provenance_missing" : !binding.nativeRootComponent ? @"live_native_component_unconfirmed" : !owned ? @"live_native_owned_descendants_missing" :
        (binding.parts&RVDeArrowPartsTitle) && !binding.titleConsumerVerified ? (titleMatches>1 ? @"live_native_headline_ambiguous" : @"live_native_headline_unconfirmed") :
        (binding.parts&RVDeArrowPartsThumbnail) && !binding.imageConsumerVerified ? (imageMatches>1 ? @"live_native_thumbnail_ambiguous" : @"live_native_thumbnail_unconfirmed") : nil;
    id controller=binding.nativeFactoryController;NSArray *controllerChildren=RVObject(RVObject(controller,"materializedInstance") ?: controller,"children");
    NSArray *attached=RVObject(root,"subnodes"),*yoga=RVObject(root,"yogaChildren");
    binding.consumerCheckpoint=@{@"attempt":@(attempt+1),@"nodes":@(graph.count),@"factory_root_bound":@(binding.nativeRootComponent!=nil),@"native_owner":binding.nativeOwnerCheckpoint ?: @{},
        @"factory_root_class":NSStringFromClass([root class]),@"factory_controller_class":controller ? NSStringFromClass([controller class]) : @"absent",
        @"root_loaded":@(RVBool(root,"isNodeLoaded")),@"controller_loaded":@(RVBool(controller,"isNodeOnControllerLoaded")),
        @"controller_children":@([controllerChildren isKindOfClass:NSArray.class] ? controllerChildren.count : 0),@"attached_subnodes":@([attached isKindOfClass:NSArray.class] ? attached.count : 0),@"yoga_children":@([yoga isKindOfClass:NSArray.class] ? yoga.count : 0),
        @"owned_nodes":@(owned),@"text_nodes":@(texts),@"image_nodes":@(images),@"title_matches":@(titleMatches),@"image_matches":@(imageMatches),
        @"title_confirmed":@(binding.titleConsumerVerified),@"thumbnail_confirmed":@(binding.imageConsumerVerified),@"image_factory_bytes_match":@(expectedBytes!=nil),
        @"title_replacement_matches":@(titleApplied),@"image_replacement_matches":@(imageApplied),@"image_witness_candidates":@(decodedImages.count),
        @"title_replacement_consumed":@(titleApplied==1),@"image_replacement_consumed":@(imageApplied==1),@"title_visible":@(titleVisible && titleApplied==1),@"image_visible":@(imageVisible && imageApplied==1),
        @"keyed_leases":@(binding.consumerLeases.count),@"blocker":blocker ?: @"ready"};
    RVDADiagRecord(@"D04_live_native_consumers",binding,blocker,binding.consumerCheckpoint);
    if (titleApplied==1 || imageApplied==1) RVDADiagRecord(@"D09_live_native_replacement_consumed",binding,nil,@{@"title":@(titleApplied==1),@"image":@(imageApplied==1),@"generation_checked":@YES});
    if ((titleVisible && titleApplied==1) || (imageVisible && imageApplied==1)) {
        RVDADiagRecord(@"D10_live_replacement_visible",binding,nil,@{@"title":@(titleVisible && titleApplied==1),@"image":@(imageVisible && imageApplied==1)});
        [binding.request recordCheckpoint:RVDeArrowCheckpointReplacementApplied blocker:RVDeArrowBlockerNone];
    }
    if (changed) RVDAUIRefresh(binding);
    // Only bootstrap uses finite polling. Native load/layout events remain live.
    if (attempt<3 && (blocker || (expected && titleApplied!=1) || (expectedBytes && imageApplied!=1))) RVDAUIScheduleConsumers(binding,.15);
}
