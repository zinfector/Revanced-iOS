// SPDX-License-Identifier: MIT
// Semantic model evidence + native image identity + exact native text match.
// No template URI/revision, font/position heuristic, descendant walk or reload.
static const void *RVDAUITextLeaseKey=&RVDAUITextLeaseKey;
static _Thread_local BOOL RVDAUITextWriting;
static NSMapTable *RVDAUITextContexts,*RVDAUITextImages,*RVDAUITextIndexes,*RVDAUITextImageGenerations;
static NSHashTable *RVDAUITextWaiting;
static NSOperationQueue *RVDAUITextParseQueue;
static NSUInteger RVDAUITextParses;
@interface RVDAUITextLease : NSObject
@property(nonatomic,weak) id context;
@property(nonatomic,strong) id element;
@property(nonatomic,strong) NSAttributedString *nativeText;
@property(nonatomic) BOOL queued,attributesReported;
@property(nonatomic,copy) NSString *lastRejection;
@end
@implementation RVDAUITextLease @end
@interface RVDAUITextIndex : NSObject
@property(nonatomic,strong) id entry;
@property(nonatomic,copy) NSData *source;
@property(nonatomic,copy) NSDictionary *titles;
@property(nonatomic) BOOL pending,complete;
@end
@implementation RVDAUITextIndex @end
static void RVDAUITextQueue(id node);
static NSMapTable *RVDAUITextMap(void) {
    return [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsWeakMemory|NSPointerFunctionsObjectPointerPersonality valueOptions:NSPointerFunctionsStrongMemory];
}
static void RVDAUITextCollect(NSData *data,NSMutableDictionary *titles,NSUInteger depth,NSUInteger *nodes,NSUInteger *bytes,BOOL *complete) {
    if (depth>24) { *complete=NO;return; }
    if (!*nodes || data.length>*bytes) { *complete=NO;return; }
    (*nodes)--;*bytes-=data.length;
    NSArray *fields=RVVoteFields(data);if (!fields) return;
    // Reviewed VideoData semantic fields: ID(7), ThumbnailDetails(1),
    // VideoMetadata(2) and video-title Lockup(21/8178/1). Unknown wrappers
    // are traversed only as bounded immutable protobuf messages, never rewritten.
    if (RVVoteSingularFields(fields,@[@1,@2,@7,@21])) {
        NSString *video=RVDAText(RVVoteOne(fields,7,2).value);
        NSData *thumbnail=RVVoteOne(fields,1,2).value;
        NSString *url=RVVideoIDValid(video) ? RVDAImageURL(RVDAChild(thumbnail,1),video) : nil;
        if (url && [RVDAImageNodeVideo([NSURL URLWithString:url]) isEqual:video]) {
            NSMutableArray *values=[NSMutableArray array];
            NSData *metadata=RVVoteOne(fields,2,2).value;NSArray *mf=RVVoteFields(metadata);
            if (mf && RVVoteSingularFields(mf,@[@1,@27])) {
                NSString *plain=RVDAText(RVVoteOne(mf,1,2).value);
                if (plain.length) [values addObject:plain];
                NSData *rich=RVVoteOne(mf,27,2).value;
                if (rich && !RVDALiveTitleFailure(rich)) [values addObject:RVDAText(RVDAChild(rich,1))];
            }
            NSData *lockup=RVDAPath(data,@[@21,@8178,@1]);
            if (lockup && !RVDALiveTitleFailure(lockup)) [values addObject:RVDAText(RVDAChild(lockup,1))];
            for (NSString *title in values) {
                if (!title.length || title.length>4096 || [title rangeOfCharacterFromSet:NSCharacterSet.controlCharacterSet].location!=NSNotFound) continue;
                NSMutableSet *ids=titles[title];if (!ids) { ids=[NSMutableSet set];titles[title]=ids; }
                [ids addObject:video];
            }
        }
    }
    for (RVVoteField *field in fields) {
        if (!*complete) return;
        if (field.wire==2 && field.value.length>=2) RVDAUITextCollect(field.value,titles,depth+1,nodes,bytes,complete);
    }
}
static void RVDAUITextWake(id context) {
    for (id node in ((NSHashTable *)[RVDAUITextContexts objectForKey:context]).allObjects) RVDAUITextQueue(node);
}
static NSArray *RVDAUITextScopes(id node,id context) {
    NSMutableArray *scopes=[NSMutableArray array],*queue=[NSMutableArray array];
    id start=RVObject(node,"nodeController");if (start) [queue addObject:start];
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    Class component=NSClassFromString(@"ELMComponent"),controller=NSClassFromString(@"ASNodeController");
    for (NSUInteger index=0;index<queue.count && index<16;index++) {
        id object=queue[index];if ([seen containsObject:object]) continue;[seen addObject:object];
        BOOL isComponent=component && [object isKindOfClass:component];
        if (!(isComponent || (controller && [object isKindOfClass:controller]))) continue;
        if (isComponent) [scopes addObject:object];
        for (NSString *edge in @[@"owningComponent",@"parent"]) {
            id next=RVObject(object,edge.UTF8String);
            if (next && ![seen containsObject:next] && ![queue containsObject:next]) [queue addObject:next];
        }
    }
    // A truncated ancestry cannot authorize a new component-scope join.
    if (queue.count>16) [scopes removeAllObjects];
    if (context) [scopes addObject:context];return scopes.copy;
}
static void RVDAUITextObserveImage(RVDAUIBinding *binding) {
    if (!binding.imageOnly || !binding.imageContext || !RVDAUICurrent(binding)) return;
    if (!RVDAUITextImages) RVDAUITextImages=RVDAUITextMap();
    NSNumber *prior=[RVDAUITextImageGenerations objectForKey:binding.controller];
    BOOL changed=prior.unsignedLongLongValue!=binding.generation;
    [RVDAUITextImageGenerations setObject:@(binding.generation) forKey:binding.controller];
    for (id scope in RVDAUITextScopes(binding.controller,binding.imageContext)) {
        NSHashTable *nodes=[RVDAUITextImages objectForKey:scope];
        if (!nodes) { nodes=[NSHashTable weakObjectsHashTable];[RVDAUITextImages setObject:nodes forKey:scope]; }
        if (nodes.count>=96 && ![nodes containsObject:binding.controller]) continue;
        BOOL added=![nodes containsObject:binding.controller];[nodes addObject:binding.controller];
        if (added || changed) RVDAUITextWake(scope);
    }
}
static void RVDAUITextResume(void) {
    for (id scope in RVDAUITextContexts) RVDAUITextWake(scope);
}
static RVDAUITextIndex *RVDAUITextIndexForOwner(id owner) {
    id entry=RVObject(owner,"entry");NSData *source=RVObject(RVAdRootElement(entry),"elementData");
    if (![source isKindOfClass:NSData.class] || !source.length || source.length>2*1024*1024) return nil;
    RVDAUITextIndex *index=[RVDAUITextIndexes objectForKey:owner];
    if (index.entry==entry && [index.source isEqual:source]) return index;
    if (RVDAUITextParses>=4) return nil;
    if (RVDAUITextIndexes.count>=16) {
        id victim=nil;for (id candidate in RVDAUITextIndexes) if (!((RVDAUITextIndex *)[RVDAUITextIndexes objectForKey:candidate]).pending) { victim=candidate;break; }
        if (!victim) return nil;[RVDAUITextIndexes removeObjectForKey:victim];
    }
    index=[RVDAUITextIndex new];index.entry=entry;index.source=source;index.pending=YES;
    [RVDAUITextIndexes setObject:index forKey:owner];__weak id weakOwner=owner;
    RVDAUITextParses++;
    [RVDAUITextParseQueue addOperationWithBlock:^{
        NSMutableDictionary *titles=[NSMutableDictionary dictionary];NSUInteger nodes=1024,bytes=16*1024*1024;BOOL complete=YES;
        RVDAUITextCollect(index.source,titles,0,&nodes,&bytes,&complete);
        NSMutableDictionary *result=[NSMutableDictionary dictionary];
        if (complete) for (NSString *title in titles) result[title]=[titles[title] allObjects];
        dispatch_async(dispatch_get_main_queue(),^{
            RVDAUITextParses--;id target=weakOwner;
            if (target && [RVDAUITextIndexes objectForKey:target]==index) {
                index.pending=NO;index.complete=complete;index.titles=result.copy;
            }
            RVDADiagRecord(@"T01_semantic_title_index",nil,complete ? nil : @"title_parse_budget_exceeded",
                @{@"title_count":@(result.count),@"messages":@(1024-nodes),@"off_main":@YES});
            NSArray *waiting=RVDAUITextWaiting.allObjects;[RVDAUITextWaiting removeAllObjects];
            for (id node in waiting) RVDAUITextQueue(node);
        });
    }];return index;
}
static BOOL RVDAUITextUnique(id node,id context,NSString *title) {
    NSUInteger count=0;NSHashTable *nodes=[RVDAUITextContexts objectForKey:context];
    for (id candidate in nodes.allObjects) {
        RVDAUITextLease *lease=objc_getAssociatedObject(candidate,RVDAUITextLeaseKey);
        if (RVObject(candidate,"element")==lease.element && [lease.nativeText.string isEqual:title]) {
            if (++count>1) return NO;
        }
    }return count==1 && [nodes containsObject:node];
}
static NSString *RVDAUITextImageVideo(id scope,id owner,NSString *video) {
    NSMutableSet *ids=[NSMutableSet set];NSUInteger inspected=0;
    for (id node in ((NSHashTable *)[RVDAUITextImages objectForKey:scope]).allObjects) {
        if (++inspected>16) return nil;
        RVDAUIBinding *image=objc_getAssociatedObject(node,RVDAUIBindingKey);
        if (!image.imageOnly || !RVDAUICurrent(image) || ![RVDAUITextScopes(node,image.imageContext) containsObject:scope]) continue;
        if (image.imageOwner!=owner) return nil;[ids addObject:image.video];
        if (ids.count>1) return nil;
    }
    return ids.count==1 && [ids containsObject:video] ? video : nil;
}
static BOOL RVDAUITextExistingRole(id owner,NSString *video) {
    RVDAUIBinding *binding=objc_getAssociatedObject(owner,RVDAUIBindingKey);
    for (RVDAUIBinding *row in RVDAUIRowBindings(binding)) {
        if (row.imageOnly || row.textOnly || row.settingsRevision!=RVDAUISettingsRevision ||
            row.originalEntry!=RVObject(owner,"entry") || ![row.video isEqual:video]) continue;
        if ((row.parts&RVDeArrowPartsTitle) && (row.profile.titleRole || (!row.modern && row.originalTitle.length))) return YES;
    }return NO;
}
static NSString *RVDAUITextBlocker(RVDAUIBinding *state) {
    RVDAUITextLease *lease=state.textNodeLease;id node=state.controller;
    if (objc_getAssociatedObject(node,RVDAUITextLeaseKey)!=lease || RVObject(node,"element")!=lease.element) return @"text_element_generation_changed";
    if (!state.textContext || !state.textOwner || lease.context!=state.textContext || RVDAImageNodeOwner(state.textContext)!=state.textOwner) return @"text_owner_changed_or_ad";
    if (![RVDAUIOwnerSurface(state.textOwner,NO,NULL) isEqual:state.surface]) return @"text_surface_changed";
    if (RVObject(state.textOwner,"entry")!=state.originalEntry || ![RVObject(RVAdRootElement(state.originalEntry),"elementData") isEqual:state.textNodeSource]) return @"text_model_source_changed";
    if (![lease.nativeText.string isEqual:state.originalTitle]) return @"native_title_changed";
    if (RVDAUITextExistingRole(state.textOwner,state.video)) return @"existing_title_adapter_owns_video";
    if (![RVDAUITextScopes(node,state.textContext) containsObject:state.textScope]) return @"text_component_scope_changed";
    if (!RVDAUITextImageVideo(state.textScope,state.textOwner,state.video)) return @"matching_image_identity_missing";
    if (!RVDAUITextUnique(node,state.textScope,state.originalTitle)) return @"title_role_ambiguous";
    return nil;
}
static void RVDAUITextWrite(id node,NSAttributedString *text) {
    if (!RVCan(node,"setAttributedText:",@"v@:@")) return;
    BOOL prior=RVDAUITextWriting;RVDAUITextWriting=YES;
    @try { RVSetObject(node,"setAttributedText:",text); }
    @finally { RVDAUITextWriting=prior; }
}
static void RVDAUITextRestore(RVDAUIBinding *state) {
    if (!state.textOnly || !state.textNodeApplied) return;
    NSString *blocker=RVDAUICurrentBlocker(state);
    if (blocker && ![@[@"title_role_ambiguous",@"existing_title_adapter_owns_video"] containsObject:blocker]) return;
    NSAttributedString *current=RVObject(state.controller,"attributedText");
    if ([current isKindOfClass:NSAttributedString.class] && [current isEqualToAttributedString:state.textNodeApplied]) RVDAUITextWrite(state.controller,state.textNodeOriginal);
    state.textNodeApplied=nil;RVDADiagCount(@"text_node.original_restored");
}
static BOOL RVDAUITextSafeAttributes(NSAttributedString *text) {
    if (!text.length || text.length>4096) return NO;
    __block BOOL safe=YES;
    [text enumerateAttributesInRange:NSMakeRange(0,text.length) options:0 usingBlock:^(NSDictionary *attributes,NSRange range,BOOL *stop) {
        // Native command/link/attachment ranges must not be carried onto new text.
        static NSSet *allowed;static dispatch_once_t once;
        dispatch_once(&once,^{ allowed=[NSSet setWithArray:@[NSFontAttributeName,NSForegroundColorAttributeName,NSBackgroundColorAttributeName,NSParagraphStyleAttributeName,NSKernAttributeName,NSLigatureAttributeName,NSBaselineOffsetAttributeName,NSStrokeColorAttributeName,NSStrokeWidthAttributeName,NSShadowAttributeName,NSObliquenessAttributeName,NSExpansionAttributeName,NSWritingDirectionAttributeName,NSUnderlineStyleAttributeName,NSUnderlineColorAttributeName,NSStrikethroughStyleAttributeName,NSStrikethroughColorAttributeName]]; });
        for (NSString *key in attributes) if (![allowed containsObject:key]) { safe=NO;*stop=YES;break; }
    }];return safe;
}
static void RVDAUITextPresent(RVDAUIBinding *state) {
    if (!state.textOnly) return;
    NSString *blocker=RVDAUICurrentBlocker(state);
    if (blocker) { RVDAUITextRestore(state);return; }
    NSString *title=RVDAUITitle(state);
    if (!title || [title isEqual:state.originalTitle]) { RVDAUITextRestore(state);return; }
    NSAttributedString *current=RVObject(state.controller,"attributedText"),*baseline=((RVDAUITextLease *)state.textNodeLease).nativeText;
    if (![current isKindOfClass:NSAttributedString.class] || !RVDAUITextSafeAttributes(baseline)) return;
    if (state.textNodeApplied && [current isEqualToAttributedString:state.textNodeApplied]) return;
    if (![current.string isEqual:state.originalTitle]) { RVDAUIEvent(@"T04_title_node_rejected",state,@"native_text_not_original");return; }
    state.textNodeOriginal=baseline;
    NSAttributedString *next=[[NSAttributedString alloc] initWithString:title attributes:[baseline attributesAtIndex:0 effectiveRange:NULL]];
    RVDAUITextWrite(state.controller,next);state.textNodeApplied=next;
    NSAttributedString *confirmed=RVObject(state.controller,"attributedText");
    state.nativeTitleProjected=[confirmed isKindOfClass:NSAttributedString.class] && [confirmed.string isEqual:title];
    RVDADiagRecord(@"T04_title_node_presented",state,state.nativeTitleProjected ? nil : @"native_title_not_confirmed",
        @{@"confirmed":@(state.nativeTitleProjected),@"visible":@(RVDAUIConsumerVisible(state.controller))});
}
static void RVDAUITextQueue(id node) {
    RVDAUITextLease *lease=objc_getAssociatedObject(node,RVDAUITextLeaseKey);if (!lease) return;
    @synchronized(lease) { if (lease.queued) return;lease.queued=YES; }
    __weak id weak=node;dispatch_async(dispatch_get_main_queue(),^{
        @synchronized(lease) { lease.queued=NO; }
        id target=weak,context=lease.context;
        if (!target || !context || objc_getAssociatedObject(target,RVDAUITextLeaseKey)!=lease || RVObject(target,"element")!=lease.element) return;
        NSArray *scopes=RVDAUITextScopes(target,context);
        for (id scope in scopes) {
            NSHashTable *nodes=[RVDAUITextContexts objectForKey:scope];
            if (!nodes) { nodes=[NSHashTable weakObjectsHashTable];[RVDAUITextContexts setObject:nodes forKey:scope]; }
            if (nodes.count>=96 && ![nodes containsObject:target]) { RVDADiagCount(@"text_node.context_budget_exceeded");continue; }
            BOOL added=![nodes containsObject:target];[nodes addObject:target];
            if (added) RVDAUITextWake(scope); // New duplicate roles retire an earlier write.
        }
        NSAttributedString *text=RVObject(target,"attributedText");
        RVDAUIBinding *state=objc_getAssociatedObject(target,RVDAUIBindingKey);
        BOOL held=state.textOnly && state.textNodeLease==lease && state.textNodeApplied &&
            [text isKindOfClass:NSAttributedString.class] && [text.string isEqual:state.textNodeApplied.string];
        if (held && ![text isEqualToAttributedString:state.textNodeApplied] && RVDAUITextSafeAttributes(text)) {
            // Native CSS/dynamic-font updates may restyle our displayed string.
            // Rebase original styling without treating the replacement as native text.
            lease.nativeText=[[NSAttributedString alloc] initWithString:state.originalTitle attributes:[text attributesAtIndex:0 effectiveRange:NULL]];
            state.textNodeOriginal=lease.nativeText;state.textNodeApplied=text.copy;
        } else if (!held) lease.nativeText=[text isKindOfClass:NSAttributedString.class] ? text.copy : nil;
        id owner=RVDAImageNodeOwner(context);NSString *surface=RVDAUIOwnerSurface(owner,NO,NULL);
        if (!owner || !surface || !RVDAUIPolicy(surface,YES)) { RVDAUITextRestore(state);return; }
        BOOL hasImageScope=NO;for (id scope in scopes) if (((NSHashTable *)[RVDAUITextImages objectForKey:scope]).count) { hasImageScope=YES;break; }
        if (!hasImageScope) return; // Image admission will wake this text node later.
        if (!RVDAUITextSafeAttributes(lease.nativeText)) {
            if (!lease.attributesReported && lease.nativeText.length) {
                lease.attributesReported=YES;RVDADiagRecord(@"T00_text_attributes_rejected",nil,@"interactive_or_unreviewed_attributes",
                    @{@"context_class":NSStringFromClass([context class]),@"attribute_keys":[lease.nativeText attributesAtIndex:0 effectiveRange:NULL].allKeys});
            }
            RVDAUITextRestore(state);return;
        }
        RVDAUITextIndex *index=RVDAUITextIndexForOwner(owner);
        if (!index || index.pending) { if (RVDAUITextWaiting.count<512) [RVDAUITextWaiting addObject:target];return; }
        NSArray *ids=index.titles[lease.nativeText.string];NSString *video=ids.count==1 ? ids.firstObject : nil;
        if (video && RVDAUITextExistingRole(owner,video)) {
            RVDAUITextRestore(state);RVDADiagCount(@"text_node.existing_title_adapter_preserved");return;
        }
        id joinedScope=nil;for (id scope in scopes) if (video && RVDAUITextImageVideo(scope,owner,video)) { joinedScope=scope;break; }
        NSString *blocker=!index.complete ? @"title_parse_budget_exceeded" : !video ? @"semantic_title_unmatched_or_ambiguous" : !joinedScope ? @"matching_image_identity_missing" : !RVDAUITextUnique(target,joinedScope,lease.nativeText.string) ? @"title_role_ambiguous" : nil;
        if (blocker) {
            RVDAUITextRestore(state);
            if (![lease.lastRejection isEqual:blocker]) {
                lease.lastRejection=blocker;RVDADiagCount([@"text_node.rejected." stringByAppendingString:blocker]);
                if (video) { RVDAUIBinding *sample=[RVDAUIBinding new];sample.video=video;sample.surface=surface;
                    RVDADiagRecord(@"T03_title_node_rejected",sample,blocker,@{@"context_class":NSStringFromClass([context class]),@"owner_class":NSStringFromClass([owner class]),@"context_image_nodes":@(((NSHashTable *)[RVDAUITextImages objectForKey:context]).count)}); }
            }
            return;
        }
        lease.lastRejection=nil;
        if (!state.textOnly || state.textNodeLease!=lease || state.textOwner!=owner || state.textScope!=joinedScope || state.originalEntry!=index.entry || ![state.textNodeSource isEqual:index.source] || ![state.originalTitle isEqual:lease.nativeText.string]) {
            RVDAUITextRestore(state);[state.request cancel];[state.fallbackTransfer cancel];
            state=[RVDAUIBinding new];state.textOnly=YES;state.modern=YES;state.controller=target;
            state.textScope=joinedScope;state.textContext=context;state.textOwner=owner;state.textNodeLease=lease;state.originalEntry=index.entry;state.textNodeSource=index.source;
            state.originalTitle=lease.nativeText.string;state.textNodeOriginal=lease.nativeText;
            state.video=video;state.surface=surface;state.route=@"semantic_video_model_title";state.index=-1;
            objc_setAssociatedObject(target,RVDAUIBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);[RVDAUIBindings addObject:state];
            RVDADiagRecord(@"T02_title_node_bound",state,nil,@{@"exact_original_match":@YES,@"matching_scope_image_identity":@YES,@"scope_class":NSStringFromClass([joinedScope class]),@"unique_text_role":@YES,@"template_independent":@YES});
            RVDAUIRequest(state);
        }
        RVDAUITextPresent(state);
    });
}
static void RVDAUITextInstall(void) {
    RVDAUITextContexts=RVDAUITextMap();RVDAUITextImages=RVDAUITextMap();RVDAUITextIndexes=RVDAUITextMap();RVDAUITextImageGenerations=RVDAUITextMap();RVDAUITextWaiting=[NSHashTable weakObjectsHashTable];
    RVDAUITextParseQueue=[NSOperationQueue new];RVDAUITextParseQueue.maxConcurrentOperationCount=2;RVDAUITextParseQueue.qualityOfService=NSQualityOfServiceUtility;
    RVDAUIHook(@"ELMTextNode",@"initWithElement:context:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id node,id element,id context) {
            id result=((id(*)(id,SEL,id,id))original)(node,sel,element,context);
            if (result) { RVDAUITextLease *lease=[RVDAUITextLease new];lease.context=context;lease.element=element;
                objc_setAssociatedObject(result,RVDAUITextLeaseKey,lease,OBJC_ASSOCIATION_RETAIN);RVDAUITextQueue(result); }
            return result;
        };
    });
    RVDAUIHook(@"ELMTextNode",@"setElement:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id node,id element) {
            RVDAUITextLease *previous=objc_getAssociatedObject(node,RVDAUITextLeaseKey);
            if (previous && previous.element!=element) { RVDAUITextLease *lease=[RVDAUITextLease new];lease.context=previous.context;lease.element=element;
                objc_setAssociatedObject(node,RVDAUITextLeaseKey,lease,OBJC_ASSOCIATION_RETAIN); }
            ((void(*)(id,SEL,id))original)(node,sel,element);RVDAUITextQueue(node);
        };
    });
    RVDAUIHook(@"ASTextNode",@"setAttributedText:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id node,id text) { ((void(*)(id,SEL,id))original)(node,sel,text);if (!RVDAUITextWriting) RVDAUITextQueue(node); };
    });
    for (NSString *selector in @[@"didLoad",@"controllerDidApplyProperties"]) RVDAUIHook(@"ELMTextNode",selector,@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id node) { ((void(*)(id,SEL))original)(node,sel);RVDAUITextQueue(node); };
    });
}
