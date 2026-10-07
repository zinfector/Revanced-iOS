// SPDX-License-Identifier: MIT
// Read-only native resource/configuration capture. This never grants a role,
// mutates a renderer, invokes a C++ aggregate ABI, or exports browsing models.
static NSMutableDictionary *RVDALiveRoots,*RVDALiveConfigurations;
static NSMutableSet *RVDALiveObservedKeys,*RVDALiveRootResources;
static uint64_t RVDALiveEpoch=1;
static NSUInteger RVDALiveExportedBytes;
enum { RVDALiveRootLimit=12,RVDALiveModelLimit=2*1024*1024,
       RVDALiveConfigLimit=192*1024,RVDALiveSessionLimit=768*1024 };

@interface RVDALiveSpan : NSObject
@property(nonatomic) uint32_t field;
@property(nonatomic) unsigned int wire;
@property(nonatomic) NSRange range;
@end
@implementation RVDALiveSpan @end

// Unlike RVVoteFields, inspection does not copy each raw field/payload. Large
// cards can expose their headers within the same production byte limit.
static NSArray<RVDALiveSpan *> *RVDALiveFields(NSData *data,NSString **failure) {
    if (![data isKindOfClass:NSData.class]) { if (failure) *failure=@"data_absent";return nil; }
    if (data.length>RVDALiveModelLimit) { if (failure) *failure=@"inspection_byte_limit";return nil; }
    const uint8_t *bytes=data.bytes;NSUInteger offset=0;NSMutableArray *fields=[NSMutableArray array];
    while (offset<data.length) {
        if (fields.count>=4096) { if (failure) *failure=@"inspection_field_limit";return nil; }
        uint64_t tag=0,length=0,value=0;
        if (!RVVoteReadVarint(bytes,data.length,&offset,&tag) || !(tag>>3) || (tag>>3)>0x1fffffff) {
            if (failure) *failure=@"invalid_protobuf_tag";return nil;
        }
        RVDALiveSpan *span=[RVDALiveSpan new];span.field=(uint32_t)(tag>>3);span.wire=(unsigned int)(tag&7);
        NSUInteger start=offset;
        if (span.wire==0) {
            if (!RVVoteReadVarint(bytes,data.length,&offset,&value)) { if (failure) *failure=@"invalid_varint";return nil; }
        } else if (span.wire==2) {
            if (!RVVoteReadVarint(bytes,data.length,&offset,&length) || length>data.length-offset) {
                if (failure) *failure=@"invalid_field_length";return nil;
            }
            start=offset;offset+=(NSUInteger)length;
        } else if (span.wire==1 || span.wire==5) {
            NSUInteger fixed=span.wire==1 ? 8 : 4;
            if (fixed>data.length-offset) { if (failure) *failure=@"invalid_fixed_field";return nil; }offset+=fixed;
        } else { if (failure) *failure=@"unsupported_wire_type";return nil; }
        span.range=NSMakeRange(start,offset-start);[fields addObject:span];
    }return fields;
}
static NSArray *RVDALiveFieldSummary(NSArray<RVDALiveSpan *> *fields) {
    NSMutableArray *summary=[NSMutableArray array];NSMutableSet *seen=[NSMutableSet set];
    for (RVDALiveSpan *span in fields) {
        NSNumber *number=@(span.field);if ([seen containsObject:number]) continue;[seen addObject:number];
        if (summary.count>=32) break;NSUInteger count=0;NSMutableSet *wires=[NSMutableSet set];
        for (RVDALiveSpan *other in fields) if (other.field==span.field) { count++;[wires addObject:@(other.wire)]; }
        [summary addObject:@{@"field":number,@"wire":@(span.wire),@"wires":[wires.allObjects sortedArrayUsingSelector:@selector(compare:)],
            @"occurrences":@(count),@"byte_length":@(span.range.length)}];
    }return summary;
}
static NSDictionary *RVDALiveShape(NSData *data) {
    NSString *failure=nil;NSArray *fields=RVDALiveFields(data,&failure);
    return @{@"byte_length":@(data.length),@"protobuf_valid":@(fields!=nil),@"blocker":failure ?: @"ready",
        @"field_count":@(fields.count),@"fields":RVDALiveFieldSummary(fields)};
}
static NSDictionary *RVDALivePathProbe(NSData *model,NSArray<NSNumber *> *path) {
    NSData *data=model;NSMutableArray *steps=[NSMutableArray array];NSString *failure=nil;
    for (NSNumber *number in path) {
        NSArray<RVDALiveSpan *> *fields=RVDALiveFields(data,&failure);NSMutableArray *matches=[NSMutableArray array];
        for (RVDALiveSpan *span in fields) if (span.field==number.unsignedIntValue) [matches addObject:span];
        RVDALiveSpan *span=matches.firstObject;
        NSString *blocker=!fields ? failure : !matches.count ? @"field_absent" : matches.count!=1 ? @"field_repeated" : span.wire!=2 ? @"wrong_wire_type" : nil;
        [steps addObject:@{@"field":number,@"parent_bytes":@(data.length),@"observed_occurrences":@(matches.count),
            @"observed_wire":span ? @(span.wire) : @(-1),@"blocker":blocker ?: @"ready",
            @"parent_fields":RVDALiveFieldSummary(fields)}];
        if (blocker) return @{@"candidate_path":path,@"status":blocker,@"failed_step":@(steps.count-1),@"steps":steps};
        data=[data subdataWithRange:span.range];
    }
    NSString *video=RVDAText(RVDAChild(data,7));NSData *thumbnail=RVDAChild(data,1),*metadata=RVDAChild(data,2);
    return @{@"candidate_path":path,@"status":@"path_resolved_unverified",@"steps":steps,@"candidate_bytes":@(data.length),
        @"video_id_valid":@(RVVideoIDValid(video)),@"video_hash":RVDADiagVideo(video),
        @"title_present":@(RVDAChild(metadata,1)!=nil || RVDAPath(metadata,@[@27,@1])!=nil),
        @"thumbnail_present":@(thumbnail!=nil),@"image_video_identity_matches":@(RVDAImageURL(RVDAChild(thumbnail,1),video)!=nil),
        @"presentation_authorized":@NO};
}
static NSDictionary *RVDALiveDirectedCardProbes(NSData *model) {
    // These semantic paths bypass both the 96-node queue and depth-6 limit.
    NSDictionary *paths=@{@"navigation_video_id":RVDALiveCardIdentityPath(),@"presentation":@[@232954548,@18,@1],
        @"attributed_title":@[@232954548,@18,@1,@21,@8178,@1],@"title_content":@[@232954548,@18,@1,@21,@8178,@1,@1],
        @"tap_video_title":@[@232954548,@18,@4,@169495254,@462702848,@4],
        @"thumbnail":@[@232954548,@18,@1,@1],@"image_sources":@[@232954548,@18,@1,@1,@1],
        @"duration_text":@[@232954548,@18,@1,@1,@2],@"title_configuration":@[@232954548,@18,@1,@21,@8178,@50001]};
    NSMutableArray *probes=[NSMutableArray array];
    for (NSString *role in [paths.allKeys sortedArrayUsingSelector:@selector(compare:)]) {
        NSString *failure=nil;NSUInteger failed=0;NSArray *path=paths[role];
        NSData *value=RVDACheckedPath(model,path,&failure,&failed);NSString *text=RVDAText(value);
        NSMutableDictionary *probe=[@{@"role":role,@"path":path,@"status":failure ?: @"path_resolved",
            @"byte_length":@(value.length),@"utf8_valid":@(text!=nil),@"shape":RVDALiveShape(value)} mutableCopy];
        if (failure) { probe[@"failed_step"]=@(failed);probe[@"failed_field"]=path[failed]; }
        if ([role isEqual:@"navigation_video_id"]) { probe[@"video_id_valid"]=@(RVVideoIDValid(text));probe[@"video_hash"]=RVDADiagVideo(text); }
        if ([role isEqual:@"title_content"] || [role isEqual:@"tap_video_title"]) probe[@"text_hash"]=RVDADiagHash(value);
        if ([role isEqual:@"duration_text"]) probe[@"duration_valid"]=@(RVDADurationText(text)>0);
        [probes addObject:probe.copy];
    }
    NSDictionary *contract=RVDALiveCardContract(model);
    return @{@"schema":@1,@"probes":probes,@"blocker":contract[@"blocker"] ?: @"unresolved",
        @"video_id_valid":@(RVVideoIDValid(contract[@"video"])),@"video_hash":RVDADiagVideo(contract[@"video"]),
        @"title_input_valid":contract[@"title_role"] ?: @NO,@"thumbnail_identity_matches":contract[@"image_role"] ?: @NO,
        @"title_blocker":contract[@"title_blocker"] ?: @"identity_unresolved",@"image_blocker":contract[@"image_blocker"] ?: @"identity_unresolved",
        @"native_consumer_confirmation_required":@YES,@"model_values_exported":@NO};
}
static void RVDALiveModelWalk(NSData *data,NSArray *path,NSUInteger depth,NSUInteger *budget,NSUInteger *bytes,
                            NSMutableArray *nodes) {
    if (!data || data.length>*bytes) return;
    // Breadth first: endpoint/image descendants cannot consume the entire node
    // budget before sibling metadata, identity and slot containers are observed.
    NSMutableArray *queue=[NSMutableArray arrayWithObject:@{@"data":data,@"path":path,@"depth":@(depth)}];
    NSUInteger cursor=0,reserved=data.length;
    while (cursor<queue.count && *budget) {
        NSDictionary *item=queue[cursor++];NSData *current=item[@"data"];NSArray *currentPath=item[@"path"];
        NSUInteger currentDepth=[item[@"depth"] unsignedIntegerValue];(*budget)--;*bytes-=current.length;
        NSString *failure=nil;NSArray<RVDALiveSpan *> *fields=RVDALiveFields(current,&failure);
        NSMutableArray *ids=[NSMutableArray array];
        for (RVDALiveSpan *span in fields) if (span.wire==2 && span.range.length==11 && ids.count<8) {
            NSString *text=RVDAText([current subdataWithRange:span.range]);
            if (RVVideoIDValid(text)) [ids addObject:@{@"field":@(span.field),@"video_hash":RVDADiagVideo(text),@"candidate_only":@YES}];
        }
        [nodes addObject:@{@"path":currentPath,@"depth":@(currentDepth),@"byte_length":@(current.length),@"protobuf_valid":@(fields!=nil),
            @"blocker":failure ?: @"ready",@"field_count":@(fields.count),@"fields":RVDALiveFieldSummary(fields),@"id_candidate_fields":ids}];
        if (!fields || currentDepth>=6) continue;
        NSMutableDictionary *occurrences=[NSMutableDictionary dictionary];
        for (RVDALiveSpan *span in fields) {
            NSNumber *number=@(span.field);NSUInteger occurrence=[occurrences[number] unsignedIntegerValue];occurrences[number]=@(occurrence+1);
            if (span.wire!=2 || span.range.length<2 || queue.count>=96 || span.range.length>4*1024*1024-reserved) continue;
            // These paths only describe the model. Production projection still
            // rejects repeated siblings and every unverified candidate role.
            NSArray *childPath=[currentPath arrayByAddingObject:@{@"field":number,@"occurrence":@(occurrence)}];
            [queue addObject:@{@"data":[current subdataWithRange:span.range],@"path":childPath,@"depth":@(currentDepth+1)}];
            reserved+=span.range.length;
        }
    }
}
static NSString *RVDALiveKey(NSString *raw) {
    if (![raw isKindOfClass:NSString.class] || raw.length>160 || [raw containsString:@"/"] || [raw containsString:@"%"])
        return nil;
    return RVDAResourceName(raw);
}
static NSString *RVDALiveDigest(NSData *data) {
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(data.bytes,(CC_LONG)data.length,digest);
    NSMutableString *hash=[NSMutableString string];for (NSUInteger i=0;i<sizeof(digest);i++) [hash appendFormat:@"%02x",digest[i]];return hash;
}
static void RVDALiveLookup(NSString *key,NSString *nativeKey,uint64_t epoch,NSUInteger attempt);
static void RVDALiveSchedule(NSString *key,NSString *nativeKey) {
    if (!key || !nativeKey || !RVDAUIDiagnosticsArmed) return;uint64_t epoch;
    @synchronized(RVDAUIBinding.class) {
        if (![RVDALiveRootResources containsObject:key] || RVDALiveConfigurations[key]) return;
        if (!RVDALiveConfigurations) RVDALiveConfigurations=[NSMutableDictionary dictionary];epoch=RVDALiveEpoch;
        if ([RVDAResourceParts(key)[@"variant"] isEqual:@"fe"]) {
            RVDALiveConfigurations[key]=@{@"resource_key":key,@"state":@"native_consumers_required",@"payload_exported":@NO,
                @"blocker":@"legacy_config_getter_not_authoritative_for_live_fe",@"lookup_attempts":@0,
                @"strategy":@"inspect_owned_materialized_inputs_after_native_factory"};return;
        }
        RVDALiveConfigurations[key]=@{@"resource_key":key,@"state":@"queued",@"payload_exported":@NO};
    }
    // Let the original materialization complete before querying cached native
    // configuration. Never perform lookup from inside the URI getter/lock.
    dispatch_async(dispatch_get_main_queue(),^{ RVDALiveLookup(key,nativeKey,epoch,1); });
}
static void RVDALiveLookup(NSString *key,NSString *nativeKey,uint64_t epoch,NSUInteger attempt) {
    @synchronized(RVDAUIBinding.class) { if (epoch!=RVDALiveEpoch || ![RVDALiveRootResources containsObject:key] || !RVDAUIDiagnosticsArmed) return; }
    Class cls=NSClassFromString(@"ELMTemplateProcessorResolver");id resolver=RVObject(cls,"sharedInstance");
    BOOL abi=RVCan(resolver,"getSerializedTemplateConfig:withError:",@"@@:@^@");
    NSData *data=nil;NSError *error=nil;NSString *blocker=nil;
    if (!resolver) blocker=@"native_resolver_unavailable";
    else if (!abi) blocker=@"native_configuration_abi_mismatch";
    else {
        @try {
            id value=((id(*)(id,SEL,id,NSError *__autoreleasing *))objc_msgSend)(resolver,sel_registerName("getSerializedTemplateConfig:withError:"),nativeKey,&error);
            if ([value isKindOfClass:NSData.class] && [value length]) data=value;
            else blocker=value && ![value isKindOfClass:NSData.class] ? @"native_configuration_wrong_type" : @"native_configuration_not_available";
        } @catch (NSException *exception) { (void)exception;blocker=@"native_configuration_exception"; }
    }
    NSMutableDictionary *record=[@{@"resource_key":key,@"lookup_key":nativeKey,@"lookup_attempts":@(attempt),
        @"resolver_present":@(resolver!=nil),@"abi_matches":@(abi),@"byte_length":@(data.length),@"payload_exported":@NO,
        @"state":data ? @"available" : @"unavailable",@"blocker":blocker ?: @"ready",
        @"error_code":@([error isKindOfClass:NSError.class] ? error.code : 0)} mutableCopy];
    @synchronized(RVDAUIBinding.class) {
        if (epoch!=RVDALiveEpoch || ![RVDALiveRootResources containsObject:key]) return;
        if (data) {
            BOOL bounded=data.length<=RVDALiveConfigLimit && data.length<=RVDALiveSessionLimit-RVDALiveExportedBytes;
            if (data.length<=RVDALiveModelLimit) {
                record[@"sha256"]=RVDALiveDigest(data);record[@"shape"]=RVDALiveShape(data);
                const uint8_t *b=data.bytes;record[@"format"]=data.length>=4 && b[0]==0 && b[1]=='a' && b[2]=='s' && b[3]=='m' ? @"wasm" :
                    [record[@"shape"][@"protobuf_valid"] boolValue] ? @"protobuf" : @"opaque_native_configuration";
            }
            if (bounded) {
                // This is provider-returned template configuration requested by
                // resource key only. No live element/model bytes are exported.
                record[@"template_config_base64"]=[data base64EncodedStringWithOptions:0];record[@"payload_exported"]=@YES;
                RVDALiveExportedBytes+=data.length;
            } else record[@"blocker"]=@"configuration_export_budget_exhausted";
        }
        RVDALiveConfigurations[key]=record.copy;
    }
    NSString *eventBlocker=[record[@"blocker"] isEqual:@"ready"] ? nil : record[@"blocker"];
    RVDADiagRecord(@"D04_live_configuration_captured",nil,eventBlocker,@{@"resource_key":key,@"available":@(data!=nil),
        @"lookup_attempts":@(attempt),@"byte_length":@(data.length),@"payload_exported":record[@"payload_exported"]});
    if ([blocker isEqual:@"native_configuration_not_available"] && attempt<3) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)((attempt==1 ? 0.5 : 2)*NSEC_PER_SEC)),dispatch_get_main_queue(),^{
            RVDALiveLookup(key,nativeKey,epoch,attempt+1);
        });
    }
}
static void RVDALiveNativeURI(NSString *raw) {
    if (!RVDAUIDiagnosticsArmed) return;NSString *key=RVDALiveKey(raw);if (!key) return;
    @synchronized(RVDAUIBinding.class) {
        if (!RVDALiveObservedKeys) RVDALiveObservedKeys=[NSMutableSet set];
        if (RVDALiveObservedKeys.count<64) [RVDALiveObservedKeys addObject:key];
        for (NSString *sample in RVDALiveRoots.allKeys) if ([RVDALiveRoots[sample][@"resource_key"] isEqual:key]) {
            NSMutableDictionary *record=[RVDALiveRoots[sample] mutableCopy];record[@"native_uri_observed"]=@YES;RVDALiveRoots[sample]=record.copy;
        }
    }
    RVDALiveSchedule(key,raw);
}
static NSDictionary *RVDALiveDirectedWatchProbes(NSData *model,NSString *watchVideo) {
    NSArray *path=RVDALiveWatchTitlePath();NSString *failure=nil;NSUInteger failed=0;
    NSData *title=RVDACheckedPath(model,path,&failure,&failed);
    NSString *role=title ? RVDALiveTitleFailure(title) : failure;
    return @{@"schema":@1,@"title_path":path,@"path_status":failure ?: @"path_resolved",@"failed_step":@(failed),
        @"title_shape":RVDALiveShape(title),@"title_text_hash":RVDADiagHash(RVDAChild(title,1)),
        @"title_input_valid":@(!role),@"explicit_watch_id_valid":@(RVVideoIDValid(watchVideo)),@"watch_video_hash":RVDADiagVideo(watchVideo),
        @"blocker":role ?: (RVVideoIDValid(watchVideo) ? @"native_consumer_confirmation_required" : @"explicit_watch_owner_required"),@"presentation_authorized":@NO};
}
static NSString *RVDALiveSampleShape(NSData *model,NSString *key) {
    NSMutableArray *shape=[NSMutableArray array];
    for (NSArray *path in @[@[],@[@232954548,@18],@[@232954548,@18,@1],@[@232954548,@18,@4],@[@232954548,@18,@1,@21,@8178],@[@377581260,@6,@1019]]) {
        NSMutableArray *fields=[NSMutableArray array];
        for (NSDictionary *field in RVDALiveShape(RVDAPath(model,path))[@"fields"])
            [fields addObject:@{@"field":field[@"field"],@"wires":field[@"wires"],@"occurrences":field[@"occurrences"]}];
        [shape addObject:@{@"path":path,@"fields":fields}];
    }
    if (RVDALiveCardFamily(key)) {
        NSDictionary *contract=RVDALiveCardContract(model);
        [shape addObject:@{@"semantic_status":contract[@"blocker"] ?: @"unresolved",@"title_role":contract[@"title_role"] ?: @NO,@"image_role":contract[@"image_role"] ?: @NO}];
    }
    return [RVDALiveDigest([NSJSONSerialization dataWithJSONObject:shape options:NSJSONWritingSortedKeys error:nil]) substringToIndex:24];
}
static void RVDALiveCaptureRoot(NSData *data,NSString *controller,NSString *surface,NSString *watchVideo) {
    if (!RVDAUIDiagnosticsArmed || !NSThread.isMainThread || ![data isKindOfClass:NSData.class] || data.length>RVDALiveModelLimit) return;
    NSData *component=RVDAPath(data,@[@1,@168777401]);NSString *raw=RVDAText(RVDAPath(component,@[@3,@172035250,@1]));
    NSString *key=RVDALiveKey(raw);if (!key) return;
    BOOL card=RVDALiveCardFamily(key),watch=RVDALiveWatchFamily(key);
    if (!card && !watch && !RVVideoIDValid(watchVideo) && ![controller isEqual:@"YTVideoElementCellController"]) return;
    NSString *bucket=watch ? @"watch_title" : RVVideoIDValid(watchVideo) || [surface isEqual:@"player"] ? @"watch_other" : [surface isEqual:@"related"] ? @"related" : @"feed";
    NSData *model=RVDAChild(component,5);NSString *fingerprint=RVDALiveSampleShape(model,key);
    NSString *sample=[NSString stringWithFormat:@"%@/%@/%@",key,surface ?: @"unknown",fingerprint];
    @synchronized(RVDAUIBinding.class) {
        if (!RVDALiveRoots) RVDALiveRoots=[NSMutableDictionary dictionary];
        if (!RVDALiveRootResources) RVDALiveRootResources=[NSMutableSet set];
        NSDictionary *prior=RVDALiveRoots[sample];
        if (prior) {
            NSMutableDictionary *next=[prior mutableCopy];next[@"occurrences"]=@([prior[@"occurrences"] unsignedIntegerValue]+1);RVDALiveRoots[sample]=next.copy;
        } else {
            NSUInteger count=0;for (NSDictionary *record in RVDALiveRoots.allValues) count+=[record[@"capture_bucket"] isEqual:bucket];
            if (RVDALiveRoots.count>=RVDALiveRootLimit || count>=3) return;
            NSArray<RVDALiveSpan *> *fields=RVDALiveFields(model,NULL);
            NSMutableArray *paths=[@[@[@232971250,@25,@1],@[@232903841,@1,@1],RVDALiveWatchTitlePath()] mutableCopy];
            if (fields.count==1 && fields[0].wire==2) [paths addObject:@[@(fields[0].field),@1,@1]];
            NSMutableArray *probes=[NSMutableArray array];for (NSArray *path in paths) [probes addObject:RVDALivePathProbe(model,path)];
            NSMutableArray *nodes=[NSMutableArray array];NSUInteger budget=96,bytes=4*1024*1024;
            RVDALiveModelWalk(model,@[],0,&budget,&bytes,nodes);
            RVDALiveRoots[sample]=@{@"resource_key":key,@"resource_parts":RVDAResourceParts(raw),@"controller_class":controller,
                @"surface":surface ?: @"unknown",@"capture_bucket":bucket,@"structural_fingerprint":fingerprint,@"watch_video_hash":RVDADiagVideo(watchVideo),@"occurrences":@1,@"element_bytes":@(data.length),@"model":RVDALiveShape(model),
                @"path_probes":probes,@"model_nodes":nodes,@"node_budget_remaining":@(budget),@"byte_budget_remaining":@(bytes),
                @"native_uri_observed":@([RVDALiveObservedKeys containsObject:key]),@"presentation_authorized":@NO,
                @"directed_card_contract":card ? RVDALiveDirectedCardProbes(model) : @{},
                @"directed_watch_contract":watch ? RVDALiveDirectedWatchProbes(model,watchVideo) : @{},
                @"sample_policy":@"resource + surface + structural shape; 3 slots per feed/related/watch-title/watch-other bucket"};
            [RVDALiveRootResources addObject:key];
        }
    }
    RVDALiveSchedule(key,raw);
}
static void RVDALiveReset(void) {
    @synchronized(RVDAUIBinding.class) {
        RVDALiveEpoch++;RVDALiveRoots=[NSMutableDictionary dictionary];RVDALiveConfigurations=[NSMutableDictionary dictionary];
        RVDALiveObservedKeys=[NSMutableSet set];RVDALiveRootResources=[NSMutableSet set];RVDALiveExportedBytes=0;
    }
}
static NSDictionary *RVDALiveReport(void) {
    @synchronized(RVDAUIBinding.class) {
        return @{@"schema":@3,@"armed":@(RVDAUIDiagnosticsArmed),@"roots":RVDALiveRoots.allValues.copy ?: @[],@"configurations":RVDALiveConfigurations.allValues.copy ?: @[],
            @"configuration_bytes_exported":@(RVDALiveExportedBytes),@"root_limit":@(RVDALiveRootLimit),
            @"model_byte_limit":@(RVDALiveModelLimit),@"configuration_byte_limit":@(RVDALiveConfigLimit),@"session_byte_limit":@(RVDALiveSessionLimit),
            @"model_values_exported":@NO,@"automatic_role_authorization":@NO};
    }
}
