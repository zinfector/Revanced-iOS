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
    RVDBGRecord(@"native_consumers_retired",binding.generation,binding.surface,nil);
    for (RVDAUIConsumerLease *lease in binding.consumerLeases) [lease retire];
    [binding.consumerLeases removeAllObjects];binding.consumerEpoch++;
    binding.consumerQueued=NO;binding.consumerPasses=0;binding.nativeRootComponent=nil;binding.nativeThumbnailNode=nil;binding.nativeTitleNode=nil;
    binding.nativeOwnerCheckpoint=nil;binding.consumerCheckpoint=nil;binding.nativeFactoryElement=nil;binding.nativeExpectedElement=nil;
    binding.nativeFactoryController=nil;binding.nativeFactoryInput=nil;binding.factoryProvenanceVerified=NO;
    binding.imageWitnesses=nil;binding.imageWitnessSource=nil;binding.nativeTitleProjected=NO;binding.nativeImageProjected=NO;
}
// Only reviewed presentation paths can differ in the immutable factory argument.
static BOOL RVDAUINativeModelMatches(RVDAUIBinding *binding,NSData *model) {
    RVDAProjection *profile=binding.profile;
    NSData *original=RVDAPath(profile.source,[profile.componentPath arrayByAddingObject:@5]);
    if (!original || !model) return NO;
    NSData *restored=model;
    for (RVDAUIBinding *current in RVDAUIRowBindings(binding)) {
        RVDAProjection *role=current.profile;
        if (![role.componentPath isEqual:profile.componentPath]) continue;
        if (!role.watchTitle) {
            NSArray *commandPath=role.shelfCard ? [role.videoDataPath subarrayWithRange:NSMakeRange(0,role.videoDataPath.count-1)] : nil;
            NSDictionary *identity=role.shelfCard ? RVDALiveCardNavigationAt(model,[commandPath arrayByAddingObjectsFromArray:@[@6,@169495254]]) : RVDALiveCardNavigation(model);
            if (![identity[@"blocker"] isEqual:@"ready"] || ![identity[@"video"] isEqual:current.video] ||
                ![identity[@"identity_path"] isEqual:role.identityPath] || ![identity[@"command_type"] isEqual:role.commandType]) return NO;
        }
        NSMutableArray *paths=[NSMutableArray array];
        if (role.titleRole) { [paths addObject:role.titlePath];if (role.titleEntityPath) [paths addObject:role.titleEntityPath]; }
        if (role.imageRole) [paths addObject:[role.imagePath arrayByAddingObject:@1]];
        if (!role.watchTitle && !role.shelfCard && role.titleRole && RVDAChild(RVDAPath(original,@[@232954548,@18]),2)) [paths addObject:@[@232954548,@18,@2]];
        for (NSArray *path in paths) {
            NSData *value=RVDAPath(original,path);if (value) restored=RVDASetPath(restored,path,value);
            if (!restored) return NO;
        }
    }return [restored isEqual:original];
}

static BOOL RVDAUIFactoryInputAccepted(NSArray<RVDAUIBinding *> *bindings,NSData *input) {
    if (!NSThread.isMainThread || !input || !bindings.count) return NO;
    NSData *source=bindings[0].profile.source,*restored=input;NSMutableSet *validated=[NSMutableSet set];
    for (RVDAUIBinding *binding in bindings) {
        if (!binding.profile.liveCard || !RVDAUICurrent(binding) || ![binding.profile.source isEqual:source]) return NO;
        NSArray *path=[binding.profile.componentPath arrayByAddingObject:@5];
        if ([validated containsObject:path]) continue;[validated addObject:path];
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
    RVDBGRecord(@"consumer_graph_begin",binding.generation,binding.surface,nil);
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
            // ASCollectionNode.visibleNodes calls ASCollectionElement.node,
            // which may run a lazy cell factory under a native mutex. A loaded
            // collection does not prove that every child node is materialized.
            // Traverse only existing native controller/subnode edges here.
            RVDAUIGraphAppend(queue,seen,RVObject(object,"nodeController"));
            for (NSString *name in @[@"subnodes",@"yogaChildren"]) {
                NSArray *children=RVObject(object,name.UTF8String);if ([children isKindOfClass:NSArray.class]) for (id child in children) RVDAUIGraphAppend(queue,seen,child);
            }
        }
    }
    RVDBGRecord(@"consumer_graph_end",binding.generation,binding.surface,@{@"nodes":@(queue.count)});
    return queue;
}
static BOOL RVDAUIComponentMatches(RVDAUIBinding *binding,id component) {
    const void *expected=RVDAUIElementIdentity(binding.nativeExpectedElement);
    return binding.factoryProvenanceVerified && expected && expected==RVDAUIElementIdentity(RVObject(component,"element")) &&
        [RVDAResourceName(RVObject(component,"templateURI")) isEqual:binding.profile.route];
}
// Controller.parent and owningComponent are independent native edges. A
// primitive controller can have no direct owningComponent and still belong to
// the proven row through its structural parent. Never force lazy node creation.
static id RVDAUIResolveConsumerRoot(id object,RVDAUIBinding *binding,NSDictionary **details) {
    Class componentClass=NSClassFromString(@"ELMComponent"),controllerClass=NSClassFromString(@"ASNodeController"),nodeClass=NSClassFromString(@"ASDisplayNode");
    id start=nodeClass && [object isKindOfClass:nodeClass] ? RVObject(object,"nodeController") : object;
    BOOL typed=start && ((componentClass && [start isKindOfClass:componentClass]) || (controllerClass && [start isKindOfClass:controllerClass]));
    NSMutableArray *queue=[NSMutableArray array],*paths=details ? [NSMutableArray array] : nil;
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    NSHashTable *matches=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    BOOL foreign=NO,truncated=NO;NSUInteger duplicates=0;NSArray *resolvedPath=nil;
    if (typed) { [queue addObject:@{@"object":start,@"depth":@0}];[seen addObject:start];if (paths) [paths addObject:@[NSStringFromClass([start class])]]; }
    NSArray *siblings=RVDAUIRowBindings(binding);
    for (NSUInteger index=0;index<queue.count;index++) {
        id current=queue[index][@"object"];NSUInteger depth=[queue[index][@"depth"] unsignedIntegerValue];
        BOOL component=componentClass && [current isKindOfClass:componentClass];
        if (component && RVDAUIComponentMatches(binding,current)) {
            [matches addObject:current];if (!resolvedPath && paths) resolvedPath=paths[index];
            // This is the exact occurrence boundary, not a route-name guess.
            continue;
        }
        BOOL other=NO;
        // Competing factory identities reject ownership conservatively; no
        // repeated native row lookup or serialization is needed per ancestor.
        if (component) for (RVDAUIBinding *sibling in siblings) {
            if (sibling!=binding && sibling.factoryProvenanceVerified && RVDAUIComponentMatches(sibling,current)) { other=YES;break; }
        }
        if (other) { foreign=YES;continue; }
        for (NSString *edge in @[@"parent",@"owningComponent"]) {
            id next=RVObject(current,edge.UTF8String);
            if (!next || !((componentClass && [next isKindOfClass:componentClass]) || (controllerClass && [next isKindOfClass:controllerClass]))) continue;
            if ([seen containsObject:next]) { duplicates++;continue; }
            if (depth>=31 || queue.count>=96) { truncated=YES;continue; }
            [seen addObject:next];[queue addObject:@{@"object":next,@"depth":@(depth+1)}];
            if (paths) [paths addObject:[paths[index] arrayByAddingObject:[NSString stringWithFormat:@"%@:%@",edge,NSStringFromClass([next class])]]];
        }
    }
    id result=!foreign && !truncated && matches.count==1 ? matches.anyObject : nil;
    if (details) {
        NSString *reason=!typed ? @"native_controller_absent_or_untyped" : foreign ? @"competing_occurrence_ancestor" : truncated ? @"ancestry_budget_exceeded" : matches.count>1 ? @"ancestry_ambiguous" : !matches.count ? @"proven_occurrence_not_reached" : @"owned";
        *details=@{@"reason":reason,@"controller_class":start ? NSStringFromClass([start class]) : @"absent",
            @"direct_owner_present":@(typed && RVObject(start,"owningComponent")!=nil),@"parent_present":@(typed && RVObject(start,"parent")!=nil),
            @"ancestor_objects":@(queue.count),@"duplicate_edges":@(duplicates),@"matching_roots":@(matches.count),
            @"competing_occurrence":@(foreign),@"truncated":@(truncated),@"resolved_path":resolvedPath ?: @[]};
    }return result;
}
static id RVDAUIConsumerRoot(id object,RVDAUIBinding *binding) { return RVDAUIResolveConsumerRoot(object,binding,NULL); }
static NSString *RVDAUIConsumerURL(id node) {
    id value=RVObject(node,"URL");return [value isKindOfClass:NSURL.class] ? [value absoluteString] : [value isKindOfClass:NSString.class] ? value : nil;
}
static void RVDAUIBindConsumerRoot(RVDAUIBinding *binding,id root) {
    if (!root || !RVDAUICurrent(binding)) return;
    NSTimeInterval started=NSProcessInfo.processInfo.systemUptime;
    NSArray *graph=RVDAUINativeGraph(binding,root);NSHashTable *matches=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    NSHashTable *inspected=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];NSMutableArray *samples=[NSMutableArray array];
    Class cls=NSClassFromString(@"ELMComponent");BOOL truncated=NO;
    for (id object in graph) {
        // Graph traversal already collects existing owningComponent edges.
        // Resolve the row from those components once, not by performing a
        // complete ancestor walk for every primitive in the graph.
        if (NSProcessInfo.processInfo.systemUptime-started>.004) { truncated=YES;break; }
        id component=cls && [object isKindOfClass:cls] ? object : nil;
        if (!component || [inspected containsObject:component]) continue;[inspected addObject:component];
        BOOL accepted=RVDAUIComponentMatches(binding,component);if (accepted) [matches addObject:component];
        if (RVDADiagEnabled() && samples.count<4) {
            id element=RVObject(component,"element");[samples addObject:@{@"component_class":NSStringFromClass([component class]),
                @"element_class":element ? NSStringFromClass([element class]) : @"absent",@"opaque_instance_contract":@(RVCan(element,"instance",@"r^v@:")),
                @"factory_element_identity_matches":@(RVDAUIElementIdentity(element) && RVDAUIElementIdentity(element)==RVDAUIElementIdentity(binding.nativeExpectedElement)),
                @"factory_input_verified":@(binding.factoryProvenanceVerified),@"resource_matches":@([RVDAResourceName(RVObject(component,"templateURI")) isEqual:binding.profile.route]),@"component_accepted":@(accepted)}];
        }
    }
    binding.nativeRootComponent=!truncated && matches.count==1 ? matches.anyObject : nil;
    binding.nativeOwnerCheckpoint=@{@"inspected_components":@(inspected.count),@"matching_components":@(matches.count),@"graph_objects":@(graph.count),@"candidates":samples,
        @"owner_probe_budget_exceeded":@(truncated),@"factory_callback_count":@(binding.factoryCallbackCount),@"factory_provenance_verified":@(binding.factoryProvenanceVerified),@"expected_element_present":@(binding.nativeExpectedElement!=nil)};
    RVDBGRecord(@"consumer_root_bound",binding.generation,binding.surface,@{@"nodes":@(graph.count),@"owned_nodes":@(matches.count),@"blocker":truncated ? @"owner_probe_budget_exceeded" : binding.nativeRootComponent ? @"ready" : @"native_root_unconfirmed"});
    RVDADiagTiming(@"consumer_root_probe",started);
    if (matches.count>1) RVDAUIEvent(@"D04_native_component_rejected",binding,@"live_native_component_ambiguous");
}
static void RVDAUIObserveConsumers(RVDAUIBinding *binding,id root,uint64_t generation,NSUInteger attempt);
static NSHashTable *RVDAUIConsumerPending;
static BOOL RVDAUIConsumerPumpScheduled;
static void RVDAUIDrainConsumers(void);
static void RVDAUIQueueConsumerPump(void) {
    if (RVDAUIConsumerPumpScheduled || !RVDAUIConsumerPending.count) return;
    RVDAUIConsumerPumpScheduled=YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC/60),dispatch_get_main_queue(),^{ RVDAUIConsumerPumpScheduled=NO;RVDAUIDrainConsumers(); });
}
static void RVDAUIScheduleConsumers(RVDAUIBinding *binding,NSTimeInterval delay) {
    if (!NSThread.isMainThread || !binding.profile.liveCard || !binding.parts || RVDAUIOriginal(binding) || !binding.nativeFactoryRoot || !RVDAUICurrent(binding)) return;
    if (binding.consumerQueued) { RVDADiagCount(@"performance.consumer_callback_coalesced");return; }
    binding.consumerQueued=YES;binding.consumerQueuedGeneration=binding.generation;binding.consumerQueuedEpoch=binding.consumerEpoch;
    binding.consumerDue=MAX(NSProcessInfo.processInfo.systemUptime+delay,binding.consumerLastScan+.12);
    if (!RVDAUIConsumerPending) RVDAUIConsumerPending=[NSHashTable weakObjectsHashTable];
    [RVDAUIConsumerPending addObject:binding];RVDAUIQueueConsumerPump();
}
static void RVDAUIDrainConsumers(void) {
    NSTimeInterval now=NSProcessInfo.processInfo.systemUptime;RVDAUIBinding *selected=nil;
    for (RVDAUIBinding *current in RVDAUIConsumerPending.allObjects) {
        if (!current.consumerQueued || current.generation!=current.consumerQueuedGeneration || current.consumerEpoch!=current.consumerQueuedEpoch) { [RVDAUIConsumerPending removeObject:current];continue; }
        if (current.consumerDue>now) continue;
        if (!selected || RVDAUIFallbackVisible(current)) selected=current;
        if (selected && RVDAUIFallbackVisible(selected)) break;
    }
    if (selected) {
        [RVDAUIConsumerPending removeObject:selected];selected.consumerQueued=NO;
        if (!selected.parts || RVDAUIOriginal(selected) || !RVDAUICurrent(selected) || !selected.nativeFactoryRoot) RVDAUIRetireConsumers(selected);
        else {
            selected.consumerLastScan=now;selected.consumerPasses++;
            RVDAUIObserveConsumers(selected,selected.nativeFactoryRoot,selected.generation,selected.consumerPasses-1);
        }
    }
    RVDAUIQueueConsumerPump();
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
    RVDAUIScheduleConsumers(binding,0);
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
    [RVDAImageWorkQueue() addOperationWithBlock:^{
        @autoreleasepool {
            static NSCache *digests;static dispatch_once_t once;dispatch_once(&once,^{ digests=[NSCache new];digests.countLimit=64;digests.totalCostLimit=8*1024*1024; });
            NSData *expected=[digests objectForKey:bytes];
            if (!expected) { expected=RVDAUIPixelDigest([UIImage imageWithData:bytes]);if (expected) [digests setObject:expected forKey:bytes cost:bytes.length]; }
            NSMutableArray *matches=[NSMutableArray array];
            for (UIImage *image in pending) { NSData *actual=RVDAUIPixelDigest(image);[matches addObject:@(actual && expected && [actual isEqual:expected])]; }
            dispatch_async(dispatch_get_main_queue(),^{
                RVDAUIBinding *current=weak;if (!current || current.generation!=generation || current.consumerEpoch!=epoch || ![current.imageWitnessSource isEqual:bytes] || !RVDAUICurrent(current)) return;
                for (NSUInteger index=0;index<pending.count;index++) [current.imageWitnesses setObject:matches[index] forKey:pending[index]];
                RVDAUIScheduleConsumers(current,0);
            });
        }
    }];
}

// A fast path only observes roles already proven by a full unique-role scan.
// It never grants permission, changes a native node, or loads a lazy view.
static BOOL RVDAUIObserveKnownConsumers(RVDAUIBinding *binding,NSUInteger attempt) {
    if (RVDADiagDetailed() || !binding.factoryProvenanceVerified || !binding.nativeRootComponent || ![binding.consumerCheckpoint[@"blocker"] isEqual:@"ready"]) return NO;
    if (![binding.nativeFactoryInput isEqual:RVDAUIProjectedData(binding)]) return NO;
    BOOL titleNeeded=(binding.parts&RVDeArrowPartsTitle)!=0,imageNeeded=(binding.parts&RVDeArrowPartsThumbnail)!=0;
    NSString *expected=RVDAUITitle(binding);NSData *bytes=RVDAUIImage(binding) ? binding.imageBytes : nil;
    id titleNode=binding.nativeTitleNode,imageNode=binding.nativeThumbnailNode;
    if (titleNeeded) {
        if (!binding.titleConsumerVerified || !RVBool(titleNode,"isNodeLoaded") || RVDAUIResolveConsumerRoot(titleNode,binding,NULL)!=binding.nativeRootComponent) return NO;
        id text=RVObject(titleNode,"attributedText");
        if (![text isKindOfClass:NSAttributedString.class] || ![[text string] isEqual:expected ?: binding.originalTitle]) return NO;
    }
    if (imageNeeded) {
        if (!binding.imageConsumerVerified || !RVBool(imageNode,"isNodeLoaded") || RVDAUIResolveConsumerRoot(imageNode,binding,NULL)!=binding.nativeRootComponent) return NO;
        if (bytes) {
            id witness=[binding.imageWitnesses objectForKey:RVObject(imageNode,"image")];
            if (![binding.imageWitnessSource isEqual:bytes] || ![witness isKindOfClass:NSNumber.class] || ![witness boolValue]) return NO;
        } else if (![binding.profile.imageURLs containsObject:RVDAUIConsumerURL(imageNode) ?: @""]) return NO;
    }
    NSMutableDictionary *checkpoint=[binding.consumerCheckpoint mutableCopy];
    checkpoint[@"attempt"]=@(attempt+1);checkpoint[@"observation_mode"]=@"verified_roles";
    checkpoint[@"title_visible"]=@(titleNeeded && expected && RVDAUIConsumerVisible(titleNode));
    checkpoint[@"image_visible"]=@(imageNeeded && bytes && RVDAUIConsumerVisible(imageNode));
    checkpoint[@"image_replacement_matches"]=@(imageNeeded && bytes ? 1 : 0);
    checkpoint[@"image_replacement_consumed"]=@(imageNeeded && bytes!=nil);
    checkpoint[@"title_replacement_matches"]=@(titleNeeded && expected ? 1 : 0);
    checkpoint[@"title_replacement_consumed"]=@(titleNeeded && expected!=nil);
    binding.consumerCheckpoint=checkpoint.copy;binding.consumerObservations++;
    RVDADiagCount(@"performance.consumer_verified_roles_reused");
    RVDAUIFallbackWake(binding);
    if (binding.refreshDeferred && RVDAUIFallbackVisible(binding)) RVDAUIRefresh(binding);
    return YES;
}
static void RVDAUIObserveConsumers(RVDAUIBinding *binding,id root,uint64_t generation,NSUInteger attempt) {
    if (!NSThread.isMainThread || !binding.profile.liveCard || !binding.parts || RVDAUIOriginal(binding) || binding.generation!=generation || !RVDAUICurrent(binding)) return;
    RVDAUIRegisterController(binding,binding.nativeFactoryController);
    NSTimeInterval scanStarted=NSProcessInfo.processInfo.systemUptime;
    if (RVDAUIObserveKnownConsumers(binding,attempt)) { RVDADiagTiming(@"consumer_fast_observation",scanStarted);return; }
    if (!binding.nativeRootComponent) RVDAUIBindConsumerRoot(binding,root);
    if (!binding.nativeRootComponent) {
        // An unproven container cannot authorize any primitive. Do not walk
        // all of its descendants' ancestry just to reject the same root again.
        binding.consumerCheckpoint=@{@"attempt":@(attempt+1),@"factory_root_bound":@NO,
            @"native_owner":binding.nativeOwnerCheckpoint ?: @{},@"blocker":@"live_native_component_unconfirmed"};
        RVDADiagRecord(@"D04_live_native_consumers",binding,@"live_native_component_unconfirmed",binding.consumerCheckpoint);
        RVDADiagTiming(@"consumer_scan",scanStarted);
        if (attempt<3) RVDAUIScheduleConsumers(binding,.15);
        return;
    }
    Class textClass=NSClassFromString(@"ELMTextNode"),imageClass=NSClassFromString(@"ELMImageNode"),allTextClass=NSClassFromString(@"ASTextNode");NSArray *urls=binding.profile.imageURLs;
    Class expandableClass=NSClassFromString(@"ELMExpandableTextNode"),canaryClass=NSClassFromString(@"ELMExpandableTextNodeCanary");
    NSArray *graph=RVDAUINativeGraph(binding,root);NSUInteger owned=0,texts=0,images=0,titleMatches=0,imageMatches=0,titleApplied=0,imageApplied=0;
    id matchedTitleNode=nil;
    BOOL titleVisible=NO,imageVisible=NO;NSString *expected=RVDAUITitle(binding);
    NSData *model=RVDAPath(binding.nativeFactoryInput,[binding.profile.componentPath arrayByAddingObject:@5]);
    NSData *expectedBytes=RVDAUIImage(binding) ? binding.imageBytes : nil;
    NSData *projectedBytes=binding.profile.imageRole ? RVDACheckedPath(model,[binding.profile.imagePath arrayByAddingObjectsFromArray:@[@1,@1,@2]],NULL,NULL) : nil;
    if (![expectedBytes isEqual:projectedBytes]) expectedBytes=nil;
    NSMutableArray *decodedImages=[NSMutableArray array],*candidateSamples=[NSMutableArray array];
    NSMutableDictionary *classCounts=[NSMutableDictionary dictionary],*rejections=[NSMutableDictionary dictionary];
    NSMutableSet *sampleReasons=[NSMutableSet set];BOOL diagnostic=RVDADiagDetailed();
    NSUInteger allTexts=0,allImages=0,allExpandable=0,allTitleMatches=0,allImageMatches=0,sampleOmitted=0;
    for (id node in graph) {
        BOOL text=textClass && [node isKindOfClass:textClass],image=imageClass && [node isKindOfClass:imageClass];
        BOOL anyText=allTextClass && [node isKindOfClass:allTextClass];
        BOOL expandable=(expandableClass && [node isKindOfClass:expandableClass]) || (canaryClass && [node isKindOfClass:canaryClass]);
        allTexts+=anyText;allImages+=image;allExpandable+=expandable;
        id attributed=anyText ? RVObject(node,"attributedText") : nil;
        NSString *originalText=[attributed isKindOfClass:NSAttributedString.class] ? [attributed string] : nil;
        NSString *originalURL=image ? RVDAUIConsumerURL(node) : nil;
        BOOL titleMatch=originalText && [originalText isEqual:binding.originalTitle],imageMatch=originalURL && [urls containsObject:originalURL];
        allTitleMatches+=titleMatch;allImageMatches+=imageMatch;
        NSDictionary *ancestry=nil;id resolved=RVDAUIResolveConsumerRoot(node,binding,diagnostic && (anyText || image || expandable) ? &ancestry : NULL);
        BOOL isOwned=binding.nativeRootComponent && resolved==binding.nativeRootComponent;
        if (diagnostic) {
            NSString *name=NSStringFromClass([node class]);classCounts[name]=@([classCounts[name] unsignedIntegerValue]+1);
            if (anyText || image || expandable) {
                NSString *reason=!isOwned ? ancestry[@"reason"] ?: @"native_root_unconfirmed" : expandable || (anyText && !text) ? @"text_adapter_not_reviewed" : (titleMatch || imageMatch) ? @"original_role_match" : @"original_role_mismatch";
                NSString *sampleKey=[NSString stringWithFormat:@"%@/%@",image ? @"image" : expandable ? @"expandable" : @"text",reason];
                rejections[sampleKey]=@([rejections[sampleKey] unsignedIntegerValue]+1);
                if (candidateSamples.count<8 && ![sampleReasons containsObject:sampleKey]) {
                    [sampleReasons addObject:sampleKey];
                    [candidateSamples addObject:@{@"node_class":name,@"role_kind":image ? @"image" : expandable ? @"expandable_text" : @"text",
                        @"owned":@(isOwned),@"loaded":@(RVBool(node,"isNodeLoaded")),@"content_present":@(image ? originalURL!=nil : originalText!=nil),
                        @"original_title_matches":@(titleMatch),@"original_thumbnail_matches":@(imageMatch),@"reviewed_adapter":@(text || image),
                        @"reason":reason,@"ancestry":ancestry ?: @{}}];
                }else sampleOmitted++;
            }
        }
        if (!isOwned) continue;
        owned++;RVDAUIRegisterController(binding,RVObject(node,"nodeController"));
        if ([node isKindOfClass:NSClassFromString(@"ASNodeController")]) RVDAUIRegisterController(binding,node);
        if (textClass && [node isKindOfClass:textClass] && binding.profile.titleRole) {
            texts++;id attributed=RVObject(node,"attributedText");NSString *value=[attributed isKindOfClass:NSAttributedString.class] ? [attributed string] : nil;
            titleMatches+=[value isEqual:binding.originalTitle];
            if ([value isEqual:binding.originalTitle] || (expected && [value isEqual:expected])) matchedTitleNode=node;
            if (expected && [value isEqual:expected]) { titleApplied++;titleVisible|=RVDAUIConsumerVisible(node); }
        }
        if (imageClass && [node isKindOfClass:imageClass] && binding.profile.imageRole) {
            images++;NSString *url=RVDAUIConsumerURL(node);
            imageMatches+=url && [urls containsObject:url];
            if (url && [urls containsObject:url]) binding.nativeThumbnailNode=node;
            UIImage *decoded=RVObject(node,"image");
            if (expectedBytes && [decoded isKindOfClass:UIImage.class] && decoded.size.width>0 && decoded.size.height>0) {
                [decodedImages addObject:decoded];id witness=[binding.imageWitnesses objectForKey:decoded];
                if ([binding.imageWitnessSource isEqual:expectedBytes] && [witness isKindOfClass:NSNumber.class] && [witness boolValue]) { binding.nativeThumbnailNode=node;imageApplied++;imageVisible|=RVDAUIConsumerVisible(node); }
            }
        }
    }
    RVDAUIImageWitnesses(binding,decodedImages,expectedBytes);
    if (titleMatches==1 || titleApplied==1) binding.nativeTitleNode=matchedTitleNode;
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
        @"primitive_census_schema":@1,@"all_text_nodes":@(allTexts),@"all_image_nodes":@(allImages),@"all_expandable_nodes":@(allExpandable),
        @"all_original_title_matches":@(allTitleMatches),@"all_original_thumbnail_matches":@(allImageMatches),
        @"graph_limit_reached":@(graph.count>=256),@"graph_class_counts":classCounts,@"candidate_reasons":rejections,
        @"candidate_samples":candidateSamples,@"candidate_samples_omitted":@(sampleOmitted),
        @"title_confirmed":@(binding.titleConsumerVerified),@"thumbnail_confirmed":@(binding.imageConsumerVerified),@"image_factory_bytes_match":@(expectedBytes!=nil),
        @"title_factory_content_match":@(binding.nativeTitleProjected),@"thumbnail_factory_content_match":@(binding.nativeImageProjected),
        @"title_replacement_matches":@(titleApplied),@"image_replacement_matches":@(imageApplied),@"image_witness_candidates":@(decodedImages.count),
        @"title_replacement_consumed":@(titleApplied==1),@"image_replacement_consumed":@(imageApplied==1),@"title_visible":@(titleVisible && titleApplied==1),@"image_visible":@(imageVisible && imageApplied==1),
        @"keyed_leases":@(binding.consumerLeases.count),@"blocker":blocker ?: @"ready"};
    RVDADiagRecord(@"D04_live_native_consumers",binding,blocker,binding.consumerCheckpoint);
    if (titleApplied==1 || imageApplied==1) RVDADiagRecord(@"D09_live_native_replacement_consumed",binding,nil,@{@"title":@(titleApplied==1),@"image":@(imageApplied==1),@"generation_checked":@YES});
    if ((titleVisible && titleApplied==1) || (imageVisible && imageApplied==1)) {
        RVDADiagRecord(@"D10_live_replacement_visible",binding,nil,@{@"title":@(titleVisible && titleApplied==1),@"image":@(imageVisible && imageApplied==1)});
        [binding.request recordCheckpoint:RVDeArrowCheckpointReplacementApplied blocker:RVDeArrowBlockerNone];
    }
    RVDAUIFallbackWake(binding);
    if (changed || (binding.refreshDeferred && RVDAUIFallbackVisible(binding))) RVDAUIRefresh(binding);
    RVDADiagTiming(@"consumer_scan",scanStarted);
    // Only bootstrap uses finite polling. Native load/layout events remain live.
    if (attempt<3 && (blocker || (expected && titleApplied!=1) || (expectedBytes && imageApplied!=1))) RVDAUIScheduleConsumers(binding,.15);
}
