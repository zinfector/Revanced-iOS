// SPDX-License-Identifier: MIT
// 21.39.4 GPB descriptors: presentation copies, never shared response mutation.
static NSMutableDictionary *RVVoteModelCounts,*RVVoteModelState;
static NSMutableArray *RVVoteModelHistory;
static NSMutableDictionary *RVVoteStartupState;
static NSMutableArray *RVVoteStartupHistory;
static _Thread_local unsigned int RVVoteModelDepth;
static const void *RVVoteNormalizedKey=&RVVoteNormalizedKey;
static NSDictionary *RVVoteFirstLaunchReport(void);
static void RVVoteCompositionChanged(id component,NSString *reason);
static BOOL RVVotePairEnabled(void) { return RVEnabled(@"return_dislikes") && RVEnabled(@"paired_vote_buttons"); }
static void RVVoteRecord(NSString *event,NSDictionary *values) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(UIImage.class) {
        if (!RVVoteModelCounts) RVVoteModelCounts=[NSMutableDictionary dictionary];
        if (!RVVoteModelState) RVVoteModelState=[NSMutableDictionary dictionary];
        if (!RVVoteModelHistory) RVVoteModelHistory=[NSMutableArray array];
        RVVoteModelCounts[event]=@([RVVoteModelCounts[event] unsignedLongLongValue]+1);
        [RVVoteModelState addEntriesFromDictionary:values ?: @{}];
        if (RVVoteModelHistory.count>=24) [RVVoteModelHistory removeObjectAtIndex:0];
        [RVVoteModelHistory addObject:@{@"event":event,@"elapsed_ms":@(MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-RVVoteLayoutStarted))),@"values":values ?: @{}}];
    }
}
@interface RVVoteField : NSObject
@property(nonatomic) uint32_t number;
@property(nonatomic) unsigned int wire;
@property(nonatomic) uint64_t scalar;
@property(nonatomic,strong) NSData *value;
@property(nonatomic,strong) NSData *raw;
@end
@implementation RVVoteField @end
static BOOL RVVoteReadVarint(const uint8_t *bytes,NSUInteger length,NSUInteger *offset,uint64_t *value) {
    uint64_t result=0;
    for (unsigned int i=0;i<10;i++) {
        if (*offset>=length) return NO;uint8_t next=bytes[(*offset)++];
        if (i==9 && (next&0xfe)) return NO;
        result|=(uint64_t)(next&0x7f)<<(7*i);
        if (!(next&0x80)) { *value=result;return YES; }
    }return NO;
}
static NSArray<RVVoteField *> *RVVoteFields(NSData *data) {
    if (![data isKindOfClass:NSData.class] || data.length>2*1024*1024) return nil;
    const uint8_t *bytes=data.bytes;NSUInteger offset=0;
    NSMutableArray *fields=[NSMutableArray array];
    while (offset<data.length) {
        if (fields.count>=4096) return nil;
        NSUInteger start=offset;uint64_t tag=0,length=0;
        if (!RVVoteReadVarint(bytes,data.length,&offset,&tag) || !(tag>>3) || (tag>>3)>0x1fffffff) return nil;
        RVVoteField *field=[RVVoteField new];field.number=(uint32_t)(tag>>3);field.wire=(unsigned int)(tag&7);
        if (field.wire==0) { uint64_t scalar=0;if (!RVVoteReadVarint(bytes,data.length,&offset,&scalar)) return nil;field.scalar=scalar; }
        else if (field.wire==2) {
            if (!RVVoteReadVarint(bytes,data.length,&offset,&length) || length>data.length-offset) return nil;
            field.value=[data subdataWithRange:NSMakeRange(offset,(NSUInteger)length)];offset+=(NSUInteger)length;
        } else if (field.wire==1 || field.wire==5) {
            NSUInteger fixed=field.wire==1 ? 8 : 4;if (fixed>data.length-offset) return nil;offset+=fixed;
        } else return nil; // Group fields are not rewritten by this adapter.
        field.raw=[data subdataWithRange:NSMakeRange(start,offset-start)];[fields addObject:field];
    }return fields;
}
static void RVVoteWriteVarint(NSMutableData *data,uint64_t value) {
    do { uint8_t byte=(uint8_t)(value&0x7f);value>>=7;if (value) byte|=0x80;[data appendBytes:&byte length:1]; } while (value);
}
static NSData *RVVoteMessageField(uint32_t number,NSData *value) {
    NSMutableData *data=[NSMutableData data];RVVoteWriteVarint(data,((uint64_t)number<<3)|2);
    RVVoteWriteVarint(data,value.length);[data appendData:value];return data;
}
static NSData *RVVoteScalarField(uint32_t number,uint64_t value) {
    NSMutableData *data=[NSMutableData data];RVVoteWriteVarint(data,(uint64_t)number<<3);RVVoteWriteVarint(data,value);return data;
}
static RVVoteField *RVVoteOne(NSArray<RVVoteField *> *fields,uint32_t number,unsigned int wire) {
    RVVoteField *found=nil;
    for (RVVoteField *field in fields) if (field.number==number) { if (found || field.wire!=wire) return nil;found=field; }
    return found;
}
static BOOL RVVoteSingularFields(NSArray<RVVoteField *> *fields,NSArray<NSNumber *> *numbers) {
    if (!fields) return NO;
    NSMutableSet *seen=[NSMutableSet set];
    for (RVVoteField *field in fields) {
        if ([numbers containsObject:@(field.number)]) {
            if ([seen containsObject:@(field.number)]) return NO;[seen addObject:@(field.number)];
        }
    }
    return YES;
}
static NSData *RVVoteReplace(NSArray<RVVoteField *> *fields,NSDictionary<NSNumber *,NSData *> *replacements,NSSet<NSNumber *> *remove) {
    NSMutableData *data=[NSMutableData data];NSMutableSet *written=[NSMutableSet set];
    for (RVVoteField *field in fields) {
        NSNumber *key=@(field.number);NSData *replacement=replacements[key];
        if (replacement) { if (![written containsObject:key]) [data appendData:replacement];[written addObject:key]; }
        else if (![remove containsObject:key]) [data appendData:field.raw];
    }
    for (NSNumber *key in [replacements.allKeys sortedArrayUsingSelector:@selector(compare:)]) if (![written containsObject:key]) [data appendData:replacements[key]];
    return data;
}
// Confirmed GPBExtensionDescription field numbers; no native memory offsets.
enum { RVVoteLikeExtension=484585818,RVVoteDislikeExtension=484590786,RVVoteSegmentExtension=484586709,
       RVVoteCompactExtension=2185,RVVoteBarExtension=37057,RVVoteElementBarExtension=389155117,
       RVVoteComponentExtension=168777401,RVVoteSharedComponentExtension=484616221 };
static BOOL RVVoteNativeCountPresent(NSData *entity) {
    NSArray *fields=RVVoteFields(entity);if (!fields) return NO;
    for (RVVoteField *field in fields) if ((field.number>=4 && field.number<=9 && field.wire==2 && field.value.length) ||
        (field.number>=12 && field.number<=14 && field.wire==0)) return YES;
    return NO;
}
static NSData *RVVoteCombine(NSData *likeButton,NSData *dislikeButton) {
    NSArray *left=RVVoteFields(likeButton),*right=RVVoteFields(dislikeButton);if (!left || !right) return nil;
    if (!RVVoteSingularFields(left,@[@10,@13,@15]) || !RVVoteSingularFields(right,@[@10,@13,@15])) return nil;
    RVVoteField *likeRenderer=RVVoteOne(left,10,2),*downRenderer=RVVoteOne(right,10,2);
    RVVoteField *leftVisibility=RVVoteOne(left,15,2),*rightVisibility=RVVoteOne(right,15,2);
    if ((leftVisibility || rightVisibility) && ![leftVisibility.value isEqual:rightVisibility.value]) {
        RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"visibility_contract_unsupported"});return nil;
    }
    NSArray *likeEnvelope=RVVoteFields(likeRenderer.value),*downEnvelope=RVVoteFields(downRenderer.value);
    RVVoteField *like=RVVoteOne(likeEnvelope,RVVoteLikeExtension,2),*down=RVVoteOne(downEnvelope,RVVoteDislikeExtension,2);
    if (!like || !down) { RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"native_submodel_missing"});return nil; }
    NSArray *likeFields=RVVoteFields(like.value),*downFields=RVVoteFields(down.value);
    if (!RVVoteSingularFields(likeFields,@[@3,@6,@10,@11,@12,@14,@16]) || !RVVoteSingularFields(downFields,@[@2,@3])) return nil;
    RVVoteField *status=RVVoteOne(likeFields,6,2),*downStatus=RVVoteOne(downFields,3,2),*count=RVVoteOne(likeFields,16,2);
    if (status && downStatus && ![status.value isEqual:downStatus.value]) {
        RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"native_state_contract_mismatch"});return nil;
    }
    if (!RVVoteNativeCountPresent(count.value)) {
        RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"native_like_count_unavailable",@"native_like_entity_resolved":@NO});return nil;
    }
    NSMutableData *segment=[NSMutableData data];
    [segment appendData:RVVoteMessageField(1,likeRenderer.value)];[segment appendData:RVVoteMessageField(2,downRenderer.value)];
    RVVoteField *statusKey=RVVoteOne(likeFields,3,2);
    if (statusKey) [segment appendData:RVVoteMessageField(3,statusKey.value)];
    if (status || downStatus) [segment appendData:RVVoteMessageField(4,(status ?: downStatus).value)];
    [segment appendData:RVVoteMessageField(7,count.value)];
    for (NSArray *mapping in @[@[@14,@10],@[@10,@14],@[@11,@15],@[@12,@16]]) {
        uint32_t source=[mapping[0] unsignedIntValue],target=[mapping[1] unsignedIntValue];
        RVVoteField *value=RVVoteOne(likeFields,source,source==10 ? 0 : 2);
        if (value) [segment appendData:value.wire==0 ? RVVoteScalarField(target,value.scalar) : RVVoteMessageField(target,value.value)];
    }
    NSData *renderer=RVVoteReplace(likeEnvelope,@{@(RVVoteSegmentExtension):RVVoteMessageField(RVVoteSegmentExtension,segment)},[NSSet setWithObject:@(RVVoteLikeExtension)]);
    NSData *button=RVVoteReplace(left,@{@10:RVVoteMessageField(10,renderer),@13:RVVoteScalarField(13,15)},nil);
    RVVoteRecord(@"pair_normalized",@{@"normalization_gate":@"paired_model_ready",@"pair_model_ready":@YES,@"native_like_entity_resolved":@YES,@"vote_commands_preserved":@YES});
    return button;
}
static NSData *RVVoteNormalizeBar(NSData *data,BOOL compact,BOOL *paired,uint32_t buttonField) {
    NSArray *fields=RVVoteFields(data);if (!fields) return data;
    NSMutableArray<RVVoteField *> *buttons=[NSMutableArray array];NSMutableArray *types=[NSMutableArray array];
    for (RVVoteField *field in fields) if (field.number==buttonField && field.wire==2) {
        NSArray *entry=RVVoteFields(field.value);RVVoteField *type=RVVoteOne(entry,13,0);
        [buttons addObject:field];[types addObject:@(type ? type.scalar : UINT64_MAX)];
    }
    if (!buttons.count || buttons.count>64) return data;
    RVVoteRecord(@"action_model_observed",@{@"source_model":compact ? @"compactify_view_model" : @"video_action_bar_view_model",@"button_types":types,@"normalization_gate":@"observed",@"pair_model_ready":@NO,@"native_like_entity_resolved":@NO});
    NSUInteger likes=0,downs=0,segments=0,index=NSNotFound;
    for (NSNumber *type in types) { likes+=type.unsignedLongLongValue==3;downs+=type.unsignedLongLongValue==4;segments+=type.unsignedLongLongValue==15; }
    if (segments==1 && !likes && !downs) {
        for (NSUInteger i=0;i<types.count;i++) if ([types[i] unsignedLongLongValue]==15) {
            RVVoteField *renderer=RVVoteOne(RVVoteFields(buttons[i].value),10,2);
            RVVoteField *segment=RVVoteOne(RVVoteFields(renderer.value),RVVoteSegmentExtension,2);
            NSArray *parts=RVVoteFields(segment.value);
            if (!segment || !RVVoteOne(parts,1,2) || !RVVoteOne(parts,2,2)) {
                RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"existing_pair_contract_unsupported"});return data;
            }
            *paired=YES;
            RVVoteField *count=RVVoteOne(RVVoteFields(segment.value),7,2);
            RVVoteRecord(@"existing_pair",@{@"pair_model_ready":@YES,@"native_like_entity_resolved":@(RVVoteNativeCountPresent(count.value)),@"normalization_gate":@"existing_native_pair"});
        }
        return compact ? RVVoteReplace(fields,@{@3:RVVoteScalarField(3,1)},nil) : data;
    }
    if (likes!=1 || downs!=1 || segments) { RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"ambiguous_or_non_vote_model"});return data; }
    for (NSUInteger i=0;i+1<types.count;i++) if ([types[i] unsignedLongLongValue]==3 && [types[i+1] unsignedLongLongValue]==4) index=i;
    if (index==NSNotFound) { RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"nonadjacent_vote_entries"});return data; }
    NSData *replacement=RVVoteCombine(buttons[index].value,buttons[index+1].value);if (!replacement) return data;
    NSMutableData *output=[NSMutableData data];
    for (RVVoteField *field in fields) {
        if (field==buttons[index]) [output appendData:RVVoteMessageField(buttonField,replacement)];
        else if (field!=buttons[index+1]) [output appendData:field.raw];
    }
    *paired=YES;
    return compact ? RVVoteReplace(RVVoteFields(output),@{@3:RVVoteScalarField(3,1)},nil) : output;
}
static NSData *RVVoteNormalizeRenderer(NSData *data,BOOL *paired) {
    NSArray *fields=RVVoteFields(data);if (!fields) return data;
    if (!RVVoteSingularFields(fields,@[@(RVVoteCompactExtension),@(RVVoteBarExtension)]) ||
        (RVVoteOne(fields,RVVoteCompactExtension,2) && RVVoteOne(fields,RVVoteBarExtension,2))) {
        RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"ambiguous_renderer_contract"});return data;
    }
    NSMutableDictionary *replace=[NSMutableDictionary dictionary];
    for (NSNumber *number in @[@(RVVoteCompactExtension),@(RVVoteBarExtension)]) {
        RVVoteField *field=RVVoteOne(fields,number.unsignedIntValue,2);if (!field) continue;
        NSData *value=RVVoteNormalizeBar(field.value,number.unsignedIntValue==RVVoteCompactExtension,paired,1);
        if (![value isEqual:field.value]) replace[number]=RVVoteMessageField(field.number,value);
    }
    return replace.count ? RVVoteReplace(fields,replace,nil) : data;
}
static NSData *RVVoteNormalizeElementBar(NSData *data,BOOL *paired) {
    NSArray *fields=RVVoteFields(data);RVVoteField *renderer=RVVoteOne(fields,18,2);
    NSData *value=renderer ? RVVoteNormalizeRenderer(renderer.value,paired) : RVVoteNormalizeBar(data,NO,paired,4);if (!*paired) return data;
    RVVoteRecord(@"native_pill_style",@{@"target_button_style":@1,@"target_bar_style":@1});
    // Native enums: Pill=1, SizeButtonsToContent=1 (verified descriptor tables).
    return renderer ? RVVoteReplace(fields,@{@18:RVVoteMessageField(18,value),@5:RVVoteScalarField(5,1),@8:RVVoteScalarField(8,1)},nil) :
        RVVoteReplace(RVVoteFields(value),@{@5:RVVoteScalarField(5,1),@8:RVVoteScalarField(8,1)},nil);
}
static NSData *RVVoteNormalizeModel(NSData *data,BOOL *paired) {
    NSArray *fields=RVVoteFields(data);RVVoteField *bar=RVVoteOne(fields,RVVoteElementBarExtension,2);if (!bar) return data;
    NSData *value=RVVoteNormalizeElementBar(bar.value,paired);
    return [value isEqual:bar.value] ? data : RVVoteReplace(fields,@{@(RVVoteElementBarExtension):RVVoteMessageField(RVVoteElementBarExtension,value)},nil);
}
static BOOL RVVoteWatchTemplate(NSString *name) {
    if (![name isKindOfClass:NSString.class] || name.length>2048) return NO;
    return [name hasPrefix:@"video_action_bar.eml"] || [name hasPrefix:@"compactify_video_action_bar.eml"];
}
static NSData *RVVoteNormalizeElement(NSData *data,NSUInteger depth,NSUInteger *budget,BOOL *paired) {
    if (depth>24 || !*budget) return data;(*budget)--;
    NSArray *fields=RVVoteFields(data);if (!fields) return data;
    NSMutableData *result=[NSMutableData data];BOOL changed=NO;
    for (RVVoteField *field in fields) {
        NSData *value=field.value;
        if (field.wire==2 && field.number==3) value=RVVoteNormalizeElement(value,depth+1,budget,paired);
        if (field.wire==2 && field.number==1) {
            NSArray *type=RVVoteFields(value);RVVoteField *component=RVVoteOne(type,RVVoteComponentExtension,2);
            RVVoteField *shared=RVVoteOne(type,RVVoteSharedComponentExtension,2);
            RVVoteField *sharedName=RVVoteOne(RVVoteFields(shared.value),6,2);
            NSString *name=sharedName ? [[NSString alloc] initWithData:sharedName.value encoding:NSUTF8StringEncoding] : nil;
            if (RVVoteWatchTemplate(name)) RVVoteRecord(@"shared_model_observed",@{@"normalization_gate":@"shared_component_contract_opaque",@"pair_model_ready":@NO});
            NSArray *parts=RVVoteFields(component.value);RVVoteField *model=RVVoteOne(parts,5,2);
            if (model) {
                BOOL localPair=NO;NSData *next=RVVoteNormalizeModel(model.value,&localPair);
                if (localPair) *paired=YES;
                if (![next isEqual:model.value]) {
                    NSData *nextComponent=RVVoteReplace(parts,@{@5:RVVoteMessageField(5,next)},nil);
                    value=RVVoteReplace(type,@{@(RVVoteComponentExtension):RVVoteMessageField(RVVoteComponentExtension,nextComponent)},nil);
                }
            }
        }
        if (field.wire==2 && ![value isEqual:field.value]) { [result appendData:RVVoteMessageField(field.number,value)];changed=YES; }
        else [result appendData:field.raw];
    }return changed ? result : data;
}
static id RVVoteNormalizeNativeElement(id native,NSString *source) {
    if (!RVVotePairEnabled() || RVVoteModelDepth || ![native isKindOfClass:NSClassFromString(@"ELMPBElement")]) return native;
    RVVoteModelDepth++;
    @try {
        NSData *data=RVObject(native,"data");NSUInteger budget=512;BOOL paired=NO;
        NSData *next=RVVoteNormalizeElement(data,0,&budget,&paired);
        RVVoteRecord(@"producer_returned",@{@"producer":source,@"native_element_available":@(data!=nil),@"pair_in_producer":@(paired)});
        Class cls=[native class];
        if (data && next && ![next isEqual:data] && RVCan(cls,"yt_parseFromData:error:",@"@@:@^@")) {
            NSError *error=nil;id parsed=((id(*)(id,SEL,id,NSError **))objc_msgSend)(cls,sel_registerName("yt_parseFromData:error:"),next,&error);
            if (parsed && !error) { objc_setAssociatedObject(parsed,RVVoteNormalizedKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);return parsed; }
            RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"native_parse_rejected",@"pair_model_ready":@NO});
        }
    } @catch (NSException *exception) { RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"native_contract_exception",@"pair_model_ready":@NO}); }
    @finally { RVVoteModelDepth--; }
    return native;
}
static NSData *RVVoteNormalizeElementData(NSData *data) {
    if (!RVVotePairEnabled() || RVVoteModelDepth || ![data isKindOfClass:NSData.class]) return data;
    RVVoteModelDepth++;NSUInteger budget=512;BOOL paired=NO;
    NSData *next=RVVoteNormalizeElement(data,0,&budget,&paired);RVVoteModelDepth--;
    if (![next isEqual:data]) RVVoteRecord(@"serialized_element_normalized",@{@"pair_model_ready":@(paired)});
    return next;
}
static NSString *RVVoteComponentTemplate(id component) {
    id name=RVObject(component,"templateURI");
    if (![name isKindOfClass:NSString.class] || [name length]>2048) return nil;
    NSString *leaf=[name lastPathComponent];
    for (NSString *prefix in @[@"segmented_like_dislike_button",@"like_button",@"dislike_button",@"animated_like_icon",@"video_action_bar",@"compactify_video_action_bar"])
        if ([leaf hasPrefix:[prefix stringByAppendingString:@".eml"]] || [leaf hasPrefix:[prefix stringByAppendingString:@"_"]]) return leaf;
    return nil;
}
static int RVVoteComponentRole(id component) {
    NSString *name=RVVoteComponentTemplate(component);if (!name) return 0;
    if ([name hasPrefix:@"segmented_like_dislike_button"]) return 1;
    if ([name hasPrefix:@"like_button"] || [name hasPrefix:@"animated_like_icon"]) return 2;
    if ([name hasPrefix:@"dislike_button"]) return 3;
    return RVVoteWatchTemplate(name) ? 4 : 0;
}
static NSDictionary *RVVoteModelReport(void) {
    @synchronized(UIImage.class) {
        NSString *blocked=@"none_detected";
        if (![RVVoteModelCounts[@"action_model_observed"] unsignedIntegerValue]) blocked=@"action_model_not_observed";
        else if (![RVVoteModelState[@"pair_model_ready"] boolValue]) blocked=RVVoteModelState[@"normalization_gate"] ?: @"pair_model_not_ready";
        else if (![RVVoteModelCounts[@"paired_template_materialized"] unsignedIntegerValue]) blocked=@"paired_template_not_materialized";
        return @{@"schema":@1,@"enabled":@(RVVotePairEnabled()),@"strategy":@"native_watch_model_pair_normalization",
            @"observation_scope":@"latest_supported_model_in_process",@"first_blocked_model_checkpoint":blocked,
            @"state":RVVoteModelState.copy ?: @{},@"counts":RVVoteModelCounts.copy ?: @{},@"history":RVVoteModelHistory.copy ?: @[],
            @"first_launch_policy":RVVoteFirstLaunchReport(),@"startup":RVVoteStartupState.copy ?: @{},@"startup_history":RVVoteStartupHistory.copy ?: @[],@"device_verified":@NO};
    }
}
static NSData *RVVoteNormalizeSerialized(NSData *data,NSString *modelClass) {
    BOOL paired=NO;
    if ([modelClass isEqual:@"ELMPBElement"]) { NSUInteger budget=512;return RVVoteNormalizeElement(data,0,&budget,&paired); }
    if ([modelClass isEqual:@"ELMPBModel"]) return RVVoteNormalizeModel(data,&paired);
    if ([modelClass isEqual:@"ELMPBVideoActionBarModel"]) return RVVoteNormalizeElementBar(data,&paired);
    return RVVoteNormalizeBar(data,[modelClass isEqual:@"YTICompactifyVideoActionBarViewModel"],&paired,1);
}
static NSString *RVVoteDigest(id value) {
    NSData *data=[value isKindOfClass:NSData.class] ? value : [value isKindOfClass:NSString.class] && [value length]<=2*1024*1024 ? [value dataUsingEncoding:NSUTF8StringEncoding] : nil;
    if (!data || data.length>2*1024*1024) return @"unavailable";
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(data.bytes,(CC_LONG)data.length,digest);
    NSMutableString *result=[NSMutableString string];for (NSUInteger i=0;i<sizeof(digest);i++) [result appendFormat:@"%02x",digest[i]];return result;
}
static void RVVoteStartup(NSString *event,NSDictionary *values) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(UIImage.class) {
        if (!RVVoteStartupState) RVVoteStartupState=[NSMutableDictionary dictionary];
        if (!RVVoteStartupHistory) RVVoteStartupHistory=[NSMutableArray array];
        RVVoteStartupState[event]=values;
        if (RVVoteStartupHistory.count>=16) [RVVoteStartupHistory removeObjectAtIndex:0];
        [RVVoteStartupHistory addObject:@{@"event":event,@"elapsed_ms":@(MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-RVVoteLayoutStarted))),@"values":values}];
    }
}
#include "RVVoteFirstLaunch.inc"
static void RVInstallVoteStartup(void) {
    RVInstallVoteFirstLaunch();
    BOOL loadInstalled=RVHook(@"YTColdConfigStorage",@"loadColdConfigGroupFromBlobStorage:dataKey:",@"@@:@@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id store,id key) {
            id group=((id(*)(id,SEL,id,id))original)(cls,sel,store,key);
            RVSpeedCaptureColdGroup(group,NO);
            if (RVEnabled(@"diagnostics")) {
                NSData *bytes=RVObject(group,"data");
                NSData *effective=RVVoteFirstLaunchReady ? RVObject(RVVoteFirstLaunchGroup,"data") : bytes;
                RVVoteStartup(@"cold_config_loaded",@{@"source_group_fingerprint":RVVoteDigest(bytes),@"source_group_bytes":@(bytes.length),
                    @"group_fingerprint":RVVoteDigest(effective),@"group_empty":@(effective.length==0),@"group_bytes":@(effective.length),@"first_launch_applied":@(RVVoteFirstLaunchReady)});
            }
            if (RVVoteFirstLaunchReady) { RVVoteFirstLaunchCheckpoint(@"loaded_group_replaced",@{@"loaded_group_defaults":@YES});return RVVoteFirstLaunchGroup; }
            return group;
        };
    });
    RVHook(@"YTColdConfigStorage",@"saveToBlobStoreColdConfig:hash:error:",@"B@:@@^@",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id store,id group,id hash,NSError **error) {
            BOOL saved=((BOOL(*)(id,SEL,id,id,NSError **))original)(store,sel,group,hash,error);
            if (saved) RVSpeedCaptureColdGroup(group,YES);
            if (RVEnabled(@"diagnostics")) RVVoteStartup(@"cold_config_saved",@{@"save_succeeded":@(saved),@"group_present":@(group!=nil),@"group_fingerprint":RVVoteDigest(RVObject(group,"data")),@"hash_fingerprint":RVVoteDigest(hash)});
            return saved;
        };
    });
    RVHook(@"YTInnerTubeContextFactory",@"clientInfoWithSendDeviceIdentifier:serviceType:",@"@@:Bq",NO,^id(IMP original,SEL sel) {
        return ^id(id factory,BOOL send,NSInteger service) {
            id info=((id(*)(id,SEL,BOOL,NSInteger))original)(factory,sel,send,service);
            if (RVEnabled(@"diagnostics")) {
                id config=RVObject(info,"configInfo"),cold=RVObject(config,"coldConfigData");
                id hash=RVObject(config,"coldHashData");
                BOOL defaults=(![cold respondsToSelector:@selector(length)] || ![cold length]) &&
                    (![hash respondsToSelector:@selector(length)] || ![hash length]);
                RVVoteFirstLaunchCheckpoint(@"request_checked",@{@"request_cold_defaults":@(defaults),@"request_service_type":@(service)});
                if (RVVoteFirstLaunchReady && !defaults) RVVoteFirstLaunchCheckpoint(@"request_cold_present",nil);
                RVVoteStartup(@"request_configuration",@{@"cold_data_present":@([cold respondsToSelector:@selector(length)] && [cold length]>0),@"cold_data_fingerprint":RVVoteDigest(cold),@"cold_hash_fingerprint":RVVoteDigest(RVObject(config,"coldHashData")),@"service_type":@(service)});
            }return info;
        };
    });
    BOOL servingInstalled=RVHook(@"YTElementUpdateHandler",@"emlTemplateServingContext",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id handler) {
            id context=((id(*)(id,SEL))original)(handler,sel);
            id effective=RVVoteFirstLaunchReady ? nil : context;
            if (RVEnabled(@"diagnostics")) RVVoteStartup(@"elements_serving_context",@{@"source_context_present":@([context respondsToSelector:@selector(length)] && [context length]>0),
                @"context_present":@([effective respondsToSelector:@selector(length)] && [effective length]>0),@"context_fingerprint":RVVoteDigest(effective),@"first_launch_applied":@(RVVoteFirstLaunchReady)});
            RVVoteFirstLaunchCheckpoint(@"serving_context_read",@{@"template_context_defaults":@(RVVoteFirstLaunchReady)});
            return effective;
        };
    });
    RVVoteFirstLaunchReady=RVVoteFirstLaunchReady && loadInstalled && servingInstalled;
    RVVoteFirstLaunchCheckpoint(@"startup_hooks_ready",@{@"load_hook_ready":@(loadInstalled),@"serving_context_hook_ready":@(servingInstalled),@"policy_ready":@(RVVoteFirstLaunchReady)});
}
static void RVInstallVoteModel(void) {
    RVInstallVoteStartup();
    for (NSString *modelClass in @[@"ELMPBElement",@"ELMPBModel",@"ELMPBVideoActionBarModel",@"YTICompactifyVideoActionBarViewModel",@"YTIVideoActionBarViewModel"]) {
        RVHook(modelClass,@"data",@"@@:",NO,^id(IMP original,SEL sel) {
            return ^id(id object) {
                id data=((id(*)(id,SEL))original)(object,sel);
                if (!RVVotePairEnabled() || RVVoteModelDepth || ![data isKindOfClass:NSData.class] || [data length]>2*1024*1024) return data;
                RVVoteRecord(@"native_serialization_entered",@{@"serialized_model_class":modelClass});
                RVVoteModelDepth++;NSData *next=data;
                @try { next=RVVoteNormalizeSerialized(data,modelClass); }
                @catch (NSException *exception) { RVVoteRecord(@"normalization_skipped",@{@"normalization_gate":@"serialized_contract_exception"}); }
                @finally { RVVoteModelDepth--; }
                if (![next isEqual:data]) RVVoteRecord(@"native_serialization_normalized",@{@"serialized_model_class":modelClass});
                return next ?: data;
            };
        });
    }
    for (NSString *selector in @[@"getVideoActionBarElement:error:",@"getCompactifyVideoActionBarElement:error:"]) {
        BOOL installed=RVHook(@"YTBAWatchPageFillerBlock",selector,@"@@:@^@",NO,^id(IMP original,SEL sel) {
            return ^id(id object,id request,NSError **error) {
                id native=((id(*)(id,SEL,id,NSError **))original)(object,sel,request,error);
                return RVVoteNormalizeNativeElement(native,selector);
            };
        });RVVoteRecord(@"producer_hook",@{@"producer":selector,@"hook_installed":@(installed)});
    }
}
