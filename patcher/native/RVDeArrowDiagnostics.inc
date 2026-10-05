// SPDX-License-Identifier: MIT
// Bounded structural diagnostics. Never serialize title text, IDs, URLs or views.
static BOOL RVDAUIDiagnosticsArmed;
static NSMutableDictionary *RVDAUICounts,*RVDAUILastStages,*RVDAUISamples;
static NSMutableDictionary *RVDAUITemplateSamples;
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
#include "RVDeArrowTemplateNames.inc"
static NSDictionary *RVDADiagURI(NSString *raw) {
    // Independent of the production matcher. Report structural tokens even
    // when normalization rejects them; arbitrary URI contents remain hashed.
    if (![raw isKindOfClass:NSString.class] || !raw.length || raw.length>2048) return @{@"present":@NO,@"reason":@"missing_or_oversized"};
    static NSRegularExpression *filenameRegex,*hexRegex;static dispatch_once_t once;
    dispatch_once(&once,^{
        filenameRegex=[NSRegularExpression regularExpressionWithPattern:@"[A-Za-z0-9_]+\\.eml(?:-js)?" options:0 error:nil];
        hexRegex=[NSRegularExpression regularExpressionWithPattern:@"^[0-9a-fA-F]{1,64}$" options:0 error:nil];
    });
    NSTextCheckingResult *match=[filenameRegex firstMatchInString:raw options:0 range:NSMakeRange(0,raw.length)];
    NSString *filename=match ? [raw substringWithRange:match.range] : nil;
    BOOL known=filename && [RVDADiagnosticTemplateNames() containsObject:filename];
    NSString *normalized=RVDAResourceName(raw);
    // Both callers observe native template identifiers, never title strings.
    // Strict resource syntax permits new names without admitting URLs/paths.
    BOOL nativeIdentifier=normalized && raw.length<=160 && ![raw containsString:@"/"] &&
        ![raw containsString:@"%"] && ([raw isEqual:normalized] || [raw isEqual:[normalized stringByReplacingOccurrencesOfString:@"|" withString:@"_"]]);
    BOOL disclose=known || nativeIdentifier;
    NSString *prefix=match ? [raw substringToIndex:match.range.location] : @"";
    NSString *suffix=match ? [raw substringFromIndex:NSMaxRange(match.range)] : @"";
    NSMutableArray *delimiters=[NSMutableArray array],*tokens=[NSMutableArray array];
    NSUInteger start=0;BOOL controls=NO,nonASCII=NO;
    for (NSUInteger i=0;i<=suffix.length;i++) {
        unichar c=i<suffix.length ? [suffix characterAtIndex:i] : 0;
        controls|=i<suffix.length && (c<32 || c==127);nonASCII|=c>127;
        BOOL separator=i<suffix.length && [@"|_:./#?&=-" rangeOfString:[NSString stringWithCharacters:&c length:1]].location!=NSNotFound;
        if (separator || i==suffix.length) {
            if (separator && delimiters.count<24) [delimiters addObject:@{@"offset":@(i),@"delimiter":[NSString stringWithCharacters:&c length:1]}];
            if (i>start && tokens.count<8) {
                NSString *token=[suffix substringWithRange:NSMakeRange(start,i-start)];
                BOOL hex=[hexRegex firstMatchInString:token options:0 range:NSMakeRange(0,token.length)]!=nil;
                BOOL decimal=hex && [token rangeOfCharacterFromSet:NSCharacterSet.decimalDigitCharacterSet.invertedSet].location==NSNotFound;
                [tokens addObject:@{@"length":@(token.length),@"kind":decimal ? @"decimal_digits" : hex ? @"hex_digits" : @"other",
                    @"value":disclose && hex ? token : @"redacted",@"hash":RVDADiagHash([token dataUsingEncoding:NSUTF8StringEncoding])}];
            }start=i+1;
        }
    }
    NSString *profileStatus=RVDAResourceProfileStatus(raw);
    NSString *reason=normalized ? @"normalized" : !match ? @"filename_not_found" : !known ? @"basename_not_allowlisted" :
        controls ? @"control_character" : nonASCII ? @"non_ascii_suffix" : prefix.length ? @"prefix_or_path_present" :
        tokens.count==1 && [tokens[0][@"length"] unsignedIntegerValue]>16 ? @"revision_exceeds_16_characters" : @"suffix_structure_unrecognized";
    NSString *namespace=[@[@"eko:",@"eml:",@"js:",@"wasm:"] containsObject:prefix] ? prefix : prefix.length ? @"redacted" : @"none";
    return @{@"present":@YES,@"raw_length":@(raw.length),@"raw_hash":RVDADiagHash([raw dataUsingEncoding:NSUTF8StringEncoding]),
        @"filename":disclose ? filename : @"redacted_or_absent",@"filename_allowlisted":@(known),@"native_identifier_valid":@(nativeIdentifier),
        @"resource_parts":nativeIdentifier ? RVDAResourceParts(raw) : @{},@"filename_offset":match ? @(match.range.location) : @(-1),
        @"prefix_length":@(prefix.length),@"prefix_namespace":namespace,@"prefix_hash":RVDADiagHash([prefix dataUsingEncoding:NSUTF8StringEncoding]),
        @"suffix_length":@(suffix.length),@"suffix_delimiters":delimiters,@"suffix_tokens":tokens,@"suffix_has_control":@(controls),@"suffix_has_non_ascii":@(nonASCII),
        @"production_normalized":@(normalized!=nil),@"normalization_reason":reason,@"resource_profile_status":profileStatus};
}
static NSArray *RVDADiagVideoCandidates(NSData *model) {
    if (!model.length || model.length>128*1024) return @[];
    NSMutableArray *paths=[@[@[@232971250,@25,@1],@[@232903841,@1,@1]] mutableCopy];
    NSArray<RVVoteField *> *extensions=RVVoteFields(model);
    if (extensions.count==1 && extensions[0].wire==2) [paths addObject:@[@(extensions[0].number),@1,@1]];
    NSMutableArray *result=[NSMutableArray array];
    for (NSArray *path in paths) {
        NSData *data=RVDAPath(model,path);if (!data) continue;
        NSData *thumbnail=RVDAChild(data,1),*metadata=RVDAChild(data,2);
        NSString *video=RVDAText(RVDAChild(data,7));
        [result addObject:@{@"candidate_path":path,@"byte_length":@(data.length),@"protobuf_valid":@(RVVoteFields(data)!=nil),
            @"video_id_valid":@(RVVideoIDValid(video)),@"thumbnail_present":@(thumbnail!=nil),@"metadata_present":@(metadata!=nil),
            @"static_image_present":@(RVDAChild(thumbnail,1)!=nil),@"composite_thumbnail_present":@(RVDAChild(thumbnail,60)!=nil),
            @"timestamp_duration_valid":@(RVDADurationText(RVDAText(RVDAChild(thumbnail,2)))>0),
            @"title_present":@(RVDAChild(metadata,1)!=nil || RVDAPath(metadata,@[@27,@1])!=nil),@"presentation_verified":@NO}];
    }return result;
}
#include "RVDeArrowLiveCapture.inc"
static NSString *RVDADiagTemplateName(NSString *uri) {
    // Admit only the conventional template filename + hex revision, not a URL.
    if (!uri.length || uri.length>160) return @"absent_or_unrecognized";
    NSRange range=[uri rangeOfString:@"^[A-Za-z0-9_]+\\.eml(-js)?(-fe)?\\|[0-9a-fA-F]{1,16}$" options:NSRegularExpressionSearch];
    return range.location==0 && range.length==uri.length ? uri : @"unrecognized_template_hash";
}
static void RVDADiagNativeTemplate(NSString *raw) {
    if (!RVDADiagEnabled() || ![raw isKindOfClass:NSString.class] || raw.length>2048) return;
    RVDALiveNativeURI(raw);
    RVDADiagCount(@"native.template_uri_entered");NSString *name=RVDAResourceName(raw),*hash=RVDADiagHash([raw dataUsingEncoding:NSUTF8StringEncoding]);
    @synchronized(RVDAUIBinding.class) {
        if (!RVDAUITemplateSamples) RVDAUITemplateSamples=[NSMutableDictionary dictionary];
        NSDictionary *prior=RVDAUITemplateSamples[hash];if (!prior && RVDAUITemplateSamples.count>=64) return;
        RVDAUITemplateSamples[hash]=@{@"template":RVDADiagTemplateName(name),@"raw_uri_hash":hash,@"raw_uri_length":@(raw.length),@"normalized":@(name!=nil),@"uri_structure":prior[@"uri_structure"] ?: RVDADiagURI(raw),@"occurrences":@([prior[@"occurrences"] unsignedLongLongValue]+1)};
    }
}
static void RVDADiagWalk(NSData *data,NSUInteger depth,NSUInteger *budget,NSMutableArray *components) {
    if (depth>12 || !*budget || components.count>=8) return;(*budget)--;
    NSArray<RVVoteField *> *fields=RVVoteFields(data);if (!fields) return;
    NSData *component=RVDAPath(data,@[@1,@168777401]),*shared=RVDAPath(data,@[@1,@484616221]);
    if (component || shared) {
        NSString *rawURI=RVDAText(RVDAPath(component,@[@3,@172035250,@1]));
        NSString *uri=RVDATemplate(component);NSData *model=RVDAChild(component,5);
        RVDAProjection *profile=RVDAProfile(data,nil);
        [components addObject:@{@"depth":@(depth),@"kind":component ? @"serialized_component" : @"shared_component",
            @"template":RVDADiagTemplateName(uri),@"template_hash":uri ? RVDADiagHash([uri dataUsingEncoding:NSUTF8StringEncoding]) : @"absent",
            @"raw_uri_length":@(rawURI.length),@"uri_field":RVDADiagShape(RVDAPath(component,@[@3,@172035250,@1])),
            @"uri_structure":RVDADiagURI(rawURI),@"video_data_candidates":RVDADiagVideoCandidates(model),
            @"watch_metadata_title_present":@(RVDAPath(model,@[@377581260,@6,@1019,@1,@1])!=nil),
            @"uri_has_path":@([rawURI containsString:@"/"]),@"uri_has_pipe":@([rawURI containsString:@"|"]),@"uri_normalized":@(uri!=nil),
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
    id element=NSThread.isMainThread ? RVAdRootElement(entry) : nil;
    NSData *data=RVObject(element,"elementData");
    if (RVDAUIDiagnosticsArmed && NSThread.isMainThread && section && index>=0 && RVObject(controller,"entry")==entry) {
        NSString *watchVideo=nil,*surface=RVDAUIOwnerSurface(controller,YES,&watchVideo);
        BOOL ordinary=[ownerClass isEqual:@"YTVideoElementCellController"] || ([surface isEqual:@"player"] && RVVideoIDValid(watchVideo));
        id options=RVAdResolvedObject(element,"compatibilityOptions");
        if (ordinary && RVAdResolve(options,"hasAdLoggingData",@"B@:") && !RVBool(options,"hasAdLoggingData"))
            RVDALiveCaptureRoot(data,ownerClass,surface ?: RVDAUIOwnerSurface(controller,NO,NULL),watchVideo);
    }
    NSData *uri=[data isKindOfClass:NSData.class] && data.length<=128*1024 ? RVDAPath(data,@[@1,@168777401,@3,@172035250,@1]) : nil;
    NSString *key=[NSString stringWithFormat:@"%@/%@/%@/%@",ownerClass,entryClass,blocker,RVDADiagHash(uri)];
    @synchronized(RVDAUIBinding.class) {
        if (!RVDAUISamples) RVDAUISamples=[NSMutableDictionary dictionary];
        if (RVDAUISamples[key]) { NSMutableDictionary *prior=[RVDAUISamples[key] mutableCopy];prior[@"occurrences"]=@([prior[@"occurrences"] unsignedLongLongValue]+1);RVDAUISamples[key]=prior;return; }
        if (RVDAUISamples.count>=64) return;
        NSMutableArray *components=[NSMutableArray array];NSUInteger budget=32;
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
    NSMutableDictionary *options=[NSMutableDictionary dictionary];
    for (NSString *key in @[@"dearrow_mode",@"dearrow_casual_categories",@"dearrow_use_crowdsourced_titles",@"dearrow_default_original",@"dearrow_title_format",@"dearrow_format_original_titles",@"dearrow_format_custom_titles",@"dearrow_remove_emojis",@"dearrow_capitalize_non_english",@"dearrow_upcoming_avatar",@"dearrow_ignore_ab_thumbnails",@"dearrow_hide_while_fetching",@"dearrow_ignore_translated_titles",@"dearrow_title_max_lines",@"dearrow_thumbnail_saturation",@"dearrow_store_selected_votes",@"dearrow_always_show_original",@"dearrow_peek_interaction",@"dearrow_show_original_for_formatted",@"dearrow_guideline_help",@"dearrow_settings_theme"]) if (RVSetting(key)) options[key]=RVSetting(key);
    return @{@"options":options,@"titles_enabled":@(RVEnabled(@"dearrow_titles")),@"thumbnails_enabled":@(RVEnabled(@"dearrow_thumbnails")),
        @"profile_accepted":@(RVCompatible),@"title_overrides":titles ?: @{},@"image_overrides":images ?: @{},
        @"legacy_thumbnail_modes":legacy ?: @{},@"fallback":RVSetting(@"dearrow_fallback") ?: @"original",@"surface_policies":policies};
}
