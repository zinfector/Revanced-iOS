// SPDX-License-Identifier: MIT
// Bounded structural diagnostics. Never serialize title text, IDs, URLs or views.
static BOOL RVDAUIDiagnosticsArmed;
static NSMutableDictionary *RVDAUICounts,*RVDAUILastStages,*RVDAUISamples;
static NSString *RVDAUISalt;
static BOOL RVDADiagEnabled(void) { return RVEnabled(@"diagnostics") || RVDAUIDiagnosticsArmed; }
static NSString *RVDADiagHash(NSData *data) {
    if (!data) return @"absent";
    NSMutableData *salted=[[RVDAUISalt dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];
    [salted appendData:data];unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(salted.bytes,(CC_LONG)salted.length,digest);
    NSMutableString *hash=[NSMutableString string];for (NSUInteger i=0;i<6;i++) [hash appendFormat:@"%02x",digest[i]];return hash;
}
static NSString *RVDADiagVideo(NSString *video) { return RVVideoIDValid(video) ? RVDADiagHash([video dataUsingEncoding:NSUTF8StringEncoding]) : @"invalid_or_absent"; }
static void RVDADiagCount(NSString *name) {
    if (!RVDADiagEnabled()) return;
    @synchronized(RVDAUIBinding.class) {
        if (!RVDAUICounts) RVDAUICounts=[NSMutableDictionary dictionary];
        RVDAUICounts[name]=@([RVDAUICounts[name] unsignedLongLongValue]+1);
    }
}
static void RVDADiagRecord(NSString *checkpoint,RVDAUIBinding *binding,NSString *blocker,NSDictionary *details) {
    if (!RVDADiagEnabled()) return;
    @synchronized(RVDAUIBinding.class) {
        if (!RVDAUIHistory) RVDAUIHistory=[NSMutableArray array];
        if (!RVDAUILastStages) RVDAUILastStages=[NSMutableDictionary dictionary];
        NSMutableDictionary *event=[@{@"checkpoint":checkpoint,@"surface":binding.surface ?: @"unknown",
            @"route":binding.route ?: @"unresolved",@"generation":@(binding.generation),
            @"elapsed_ms":@(MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-RVDAUIStarted))),
            @"settings_revision":@(RVDAUISettingsRevision),@"blocker":blocker ?: @"ready"} mutableCopy];
        if (binding.video) event[@"video_hash"]=RVDADiagVideo(binding.video);
        if (details) [event addEntriesFromDictionary:details];
        RVDADiagCount(checkpoint);if (blocker) RVDADiagCount([@"blocked." stringByAppendingString:blocker]);
        if (RVDAUIHistory.count>=96) [RVDAUIHistory removeObjectAtIndex:0];
        [RVDAUIHistory addObject:event.copy];RVDAUILastStages[checkpoint]=event.copy;
    }
}
static NSDictionary *RVDADiagShape(NSData *data) {
    if (![data isKindOfClass:NSData.class]) return @{@"present":@NO};
    if (data.length>128*1024) return @{@"present":@YES,@"byte_length":@(data.length),@"blocker":@"diagnostic_wire_byte_limit"};
    NSArray<RVVoteField *> *fields=RVVoteFields(data);NSMutableArray *shape=[NSMutableArray array];
    if (fields) {
        NSMutableSet *seen=[NSMutableSet set];
        for (RVVoteField *field in fields) {
            if (shape.count>=20) break;NSNumber *number=@(field.number);if ([seen containsObject:number]) continue;[seen addObject:number];
            NSUInteger count=0;for (RVVoteField *other in fields) count+=other.number==field.number;
            [shape addObject:@{@"field":number,@"wire":@(field.wire),@"occurrences":@(count),@"byte_length":@(field.value.length)}];
        }
    }
    return @{@"present":@YES,@"byte_length":@(data.length),@"source_hash":RVDADiagHash(data),
        @"protobuf_valid":@(fields!=nil),@"field_count":@(fields.count),@"fields":shape};
}
static NSString *RVDADiagTemplateName(NSString *uri) {
    // Admit only the conventional template filename + hex revision, not a URL.
    if (!uri.length || uri.length>160) return @"absent_or_unrecognized";
    NSRange range=[uri rangeOfString:@"^[A-Za-z0-9_]+\\.eml(-js)?\\|[0-9a-fA-F]{16}$" options:NSRegularExpressionSearch];
    return range.location==0 && range.length==uri.length ? uri : @"unrecognized_template_hash";
}
static void RVDADiagWalk(NSData *data,NSUInteger depth,NSUInteger *budget,NSMutableArray *components) {
    if (depth>12 || !*budget || components.count>=8) return;(*budget)--;
    NSArray<RVVoteField *> *fields=RVVoteFields(data);if (!fields) return;
    NSData *component=RVDAPath(data,@[@1,@168777401]),*shared=RVDAPath(data,@[@1,@484616221]);
    if (component || shared) {
        NSString *uri=RVDATemplate(component);NSData *model=RVDAChild(component,5);
        RVDAProjection *profile=RVDAProfile(data,nil);
        [components addObject:@{@"depth":@(depth),@"kind":component ? @"serialized_component" : @"shared_component",
            @"template":RVDADiagTemplateName(uri),@"template_hash":uri ? RVDADiagHash([uri dataUsingEncoding:NSUTF8StringEncoding]) : @"absent",
            @"model":RVDADiagShape(model),@"single_video_profile_found":@(profile!=nil),
            @"title_role":@(profile.titleRole),@"image_role":@(profile.imageRole),
            @"serialized_role_blocker":profile.blocker ?: (profile ? @"ready" : @"not_resolved_without_owner"),
            @"shared_data":RVDADiagShape(shared)}];
    }
    // Read-only observation can inventory repeated siblings; production numeric
    // projection still refuses to address them without an individual role.
    for (RVVoteField *field in fields) if (field.number==3 && field.wire==2) RVDADiagWalk(field.value,depth+1,budget,components);
}
static NSArray *RVDADiagOwners(id owner) {
    if (!NSThread.isMainThread) return @[];NSMutableArray *trail=[NSMutableArray array];
    for (NSUInteger depth=0;owner && depth<16;depth++) {
        BOOL parentKnown=RVCan(owner,"parentResponder",@"@@:");
        NSString *browse=RVObject(owner,"browseID") ?: RVObject(owner,"browseIdentifier");
        BOOL known=[@[@"FEwhat_to_watch",@"FEsubscriptions",@"FElibrary",@"FEyou",@"FEhistory"] containsObject:browse ?: @""];
        [trail addObject:@{@"class":NSStringFromClass([owner class]),@"parent_contract":@(parentKnown),
            @"browse_contract":@(RVCan(owner,"browseID",@"@@:") || RVCan(owner,"browseIdentifier",@"@@:")),
            @"browse_route":known ? browse : @"unknown_or_absent",@"watch_id_valid":@(RVVideoIDValid(RVObject(owner,"videoID")))}];
        if (!parentKnown) break;id next=RVObject(owner,"parentResponder");if (next==owner) break;owner=next;
    }return trail;
}
static void RVDADiagAttempt(id controller,id entry,id section,NSInteger index,NSString *blocker) {
    if (!RVDADiagEnabled()) return;
    NSString *entryClass=entry ? NSStringFromClass([entry class]) : @"absent",*ownerClass=controller ? NSStringFromClass([controller class]) : @"absent";
    NSString *key=[NSString stringWithFormat:@"%@/%@/%@",ownerClass,entryClass,blocker];
    @synchronized(RVDAUIBinding.class) {
        if (!RVDAUISamples) RVDAUISamples=[NSMutableDictionary dictionary];
        if (RVDAUISamples[key]) { NSMutableDictionary *prior=[RVDAUISamples[key] mutableCopy];prior[@"occurrences"]=@([prior[@"occurrences"] unsignedLongLongValue]+1);RVDAUISamples[key]=prior;return; }
        if (RVDAUISamples.count>=32) return;
        id element=NSThread.isMainThread ? RVAdRootElement(entry) : nil;
        NSData *data=RVObject(element,"elementData");NSMutableArray *components=[NSMutableArray array];NSUInteger budget=32;
        if ([data isKindOfClass:NSData.class] && data.length<=128*1024) RVDADiagWalk(data,0,&budget,components);
        id viewModel=NSThread.isMainThread ? RVObject(section,"viewModel") : nil;
        RVDAUISamples[key]=@{@"blocker":blocker,@"entry_class":entryClass,@"controller_class":ownerClass,
            @"section_class":section ? NSStringFromClass([section class]) : @"absent",@"index":@(index),
            @"main_thread":@(NSThread.isMainThread),@"section_scope_present":@(section!=nil),@"occurrences":@1,
            @"view_model_class":viewModel ? NSStringFromClass([viewModel class]) : @"absent",
            @"entries_contract":@(RVCan(viewModel,"entries",@"@@:")),@"controller_entry_matches":@(NSThread.isMainThread && RVObject(controller,"entry")==entry),
            @"owners":RVDADiagOwners(controller),@"element_data":RVDADiagShape(data),@"components":components};
    }
}
static NSDictionary *RVDADiagSettings(void) {
    NSMutableArray *policies=[NSMutableArray array];
    for (NSString *surface in @[@"home",@"search",@"subscriptions",@"library",@"player",@"related"]) {
        [policies addObject:@{@"surface":surface,@"title_requested":@(RVDAUIPolicy(surface,YES)),
            @"image_requested":@(RVDAUIPolicy(surface,NO) && ![surface isEqual:@"player"])}];
    }
    id titles=RVSetting(@"dearrow_title_modes"),images=RVSetting(@"dearrow_thumbnail_modes"),legacy=RVSetting(@"thumbnail_modes");
    return @{@"titles_enabled":@(RVEnabled(@"dearrow_titles")),@"thumbnails_enabled":@(RVEnabled(@"dearrow_thumbnails")),
        @"profile_accepted":@(RVCompatible),@"title_overrides":titles ?: @{},@"image_overrides":images ?: @{},
        @"legacy_thumbnail_modes":legacy ?: @{},@"fallback":RVSetting(@"dearrow_fallback") ?: @"original",@"surface_policies":policies};
}
