// SPDX-License-Identifier: MIT
// Semantic recipes, independent of a server revision. All writes also require
// per-instance materialized native role confirmation and immutable owner identity.
static NSArray *RVDALiveCardCommandPath(void) { return @[@232954548,@18,@4,@169495254]; }
static NSArray *RVDALiveWatchTitlePath(void) { return @[@377581260,@6,@1019,@1]; }
static NSData *RVDACheckedPath(NSData *data,NSArray *path,NSString **failure,NSUInteger *failedStep) {
    NSUInteger step=0;
    for (id number in path) {
        if ([number isKindOfClass:NSDictionary.class]) {
            data=RVDAPathChild(data,number);if (!data) { if (failure) *failure=@"occurrence_absent";if (failedStep) *failedStep=step;return nil; }step++;continue;
        }
        NSArray<RVVoteField *> *fields=RVVoteFields(data);NSUInteger count=0;RVVoteField *match=nil;
        for (RVVoteField *field in fields) if (field.number==[number unsignedIntValue]) { count++;match=field; }
        NSString *blocker=!fields ? @"invalid_parent_protobuf" : !count ? @"field_absent" : count!=1 ? @"field_repeated" : match.wire!=2 ? @"wrong_wire_type" : nil;
        if (blocker) { if (failure) *failure=blocker;if (failedStep) *failedStep=step;return nil; }
        data=match.value;step++;
    }
    return data;
}
// Reviewed YTICommand extensions, not a search for arbitrary 11-byte strings.
// Only the card's primary tap command is inspected; commands are never executed.
static NSDictionary *RVDALiveCardNavigationAt(NSData *model,NSArray *path) {
    NSString *failure=nil;NSUInteger failed=0;
    NSData *command=RVDACheckedPath(model,path,&failure,&failed);
    NSMutableDictionary *result=[@{@"identity_path":path,@"command_type":@"unresolved"} mutableCopy];
    if (!command) {
        result[@"blocker"]=[@"live_card_identity_" stringByAppendingString:failure ?: @"unresolved"];
        result[@"failed_step"]=@(failed);return result;
    }
    // The registry contains only native descriptor-proven transparent identity
    // wrappers. Their flags and command payloads are never changed or executed.
    NSMutableArray *chain=[NSMutableArray array];result[@"wrapper_chain"]=chain;
    for (NSUInteger depth=0;depth<=6;depth++) {
        NSArray<RVVoteField *> *fields=RVVoteFields(command);NSMutableArray *extensions=[NSMutableArray array];
        for (RVVoteField *field in fields) if (field.number>=1000000) [extensions addObject:field];
        result[@"wrapper_depth"]=@(depth);
        if (!fields || extensions.count!=1 || [extensions[0] wire]!=2) {
            result[@"command_type"]=extensions.count>1 ? @"ambiguous" : @"unsupported";
            result[@"blocker"]=!fields ? @"live_card_command_invalid" : extensions.count>1 ? @"live_card_command_ambiguous" : @"live_card_command_unsupported";
            result[@"identity_path"]=path;result[@"failed_step"]=@(path.count);return result;
        }
        RVVoteField *extension=extensions[0];result[@"command_extension"]=@(extension.number);
        if (extension.number==48687626) {
            result[@"command_type"]=@"browse";result[@"blocker"]=@"live_card_non_video_browse";
            result[@"identity_path"]=path;result[@"failed_step"]=@(path.count);return result;
        }
        // YTIWatchEndpoint and YTIReelWatchEndpoint both declare videoId as
        // singular string field 1. Reel support applies to the feed preview's
        // identity only; its navigation payload and player are untouched.
        if (extension.number==48687757 || extension.number==139608561) {
            NSString *terminal=extension.number==139608561 ? @"reel" : @"watch";
            NSArray *identity=[path arrayByAddingObjectsFromArray:@[@(extension.number),@1]];
            NSData *value=RVDACheckedPath(model,identity,&failure,&failed);NSString *video=RVDAText(value);
            NSMutableArray *names=[NSMutableArray array];for (NSDictionary *wrapper in chain) [names addObject:wrapper[@"type"]];
            [names addObject:terminal];result[@"command_type"]=depth ? [names componentsJoinedByString:@"_"] : [@"direct_" stringByAppendingString:terminal];
            result[@"identity_path"]=identity;result[@"terminal_extension"]=@(extension.number);
            if (!value || !RVVideoIDValid(video)) {
                result[@"blocker"]=value ? @"live_card_identity_invalid" : [@"live_card_identity_" stringByAppendingString:failure ?: @"unresolved"];
                result[@"failed_step"]=@(failed);return result;
            }
            result[@"video"]=video;result[@"blocker"]=@"ready";return result;
        }
        NSString *kind=extension.number==462702848 ? @"cowatch" : extension.number==200453700 ? @"inline_muted" : nil;
        if (!kind || depth==6) {
            result[@"command_type"]=@"unsupported";result[@"blocker"]=depth==6 && kind ? @"live_card_wrapper_depth_limit" : @"live_card_command_unsupported";
            result[@"identity_path"]=path;result[@"failed_step"]=@(path.count);return result;
        }
        if (!result[@"tap_title_path"] && extension.number==462702848)
            result[@"tap_title_path"]=[path arrayByAddingObjectsFromArray:@[@462702848,@4]];
        [chain addObject:@{@"type":kind,@"extension":@(extension.number),@"command_path":path,@"child_field":@1}];
        path=[path arrayByAddingObjectsFromArray:@[@(extension.number),@1]];
        command=RVDACheckedPath(model,path,&failure,&failed);
        if (!command) {
            result[@"command_type"]=[kind stringByAppendingString:@"_unresolved"];result[@"identity_path"]=path;
            result[@"blocker"]=[@"live_card_identity_" stringByAppendingString:failure ?: @"unresolved"];
            result[@"failed_step"]=@(failed);return result;
        }
    }
    return result;
}

static NSDictionary *RVDALiveCardNavigation(NSData *model) { return RVDALiveCardNavigationAt(model,RVDALiveCardCommandPath()); }
static NSArray<NSString *> *RVDALiveCardImageURLs(NSData *image,NSString *video) {
    NSArray<RVVoteField *> *fields=RVVoteFields(image);if (!fields) return nil;
    NSMutableArray *urls=[NSMutableArray array];
    for (RVVoteField *field in fields) if (field.number==1) {
        if (field.wire!=2 || urls.count>=16) return nil;
        NSArray *source=RVVoteFields(field.value);
        if (!source || !RVVoteSingularFields(source,@[@1,@2,@3,@4,@5,@6]) ||
            RVDAChild(field.value,2) || RVDAChild(field.value,3) || RVDAChild(field.value,6)) return nil;
        NSString *url=RVDAImageURL(RVVoteMessageField(1,field.value),video);
        if (!url) return nil;[urls addObject:url];
    }
    return urls.count ? urls.copy : nil;
}
static NSString *RVDALiveTitleFailure(NSData *attributed) {
    NSArray *fields=RVVoteFields(attributed);
    NSString *title=RVDAText(RVVoteOne(fields,1,2).value);
    if (!title.length || !RVVoteSingularFields(fields,@[@1])) return @"title_input_invalid";
    // Interactive text requires range remapping. Never erase its command runs.
    for (RVVoteField *field in fields) if (field.number==5) return @"interactive_title_requires_range_adapter";
    return nil;
}
static NSString *RVDAStructuralFingerprint(NSString *adapter,BOOL title,BOOL image) {
    // Recipe selection cache contains no values, IDs or instance permissions.
    // Unknown optional fields and byte lengths cannot invalidate a compatible recipe.
    static NSCache *cache;static dispatch_once_t once;dispatch_once(&once,^{cache=[NSCache new];cache.countLimit=64;});
    NSString *shape=[NSString stringWithFormat:@"%@/title:%d/image:%d",adapter,title,image],*fingerprint=[cache objectForKey:shape];
    if (!fingerprint) {
        NSData *data=[shape dataUsingEncoding:NSUTF8StringEncoding];unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(data.bytes,(CC_LONG)data.length,digest);
        NSMutableString *value=[NSMutableString string];for (NSUInteger i=0;i<12;i++) [value appendFormat:@"%02x",digest[i]];
        fingerprint=value.copy;[cache setObject:fingerprint forKey:shape];
    }
    return fingerprint;
}
static NSDictionary *RVDALiveCardContract(NSData *model) {
    NSMutableDictionary *result=[RVDALiveCardNavigation(model) mutableCopy];NSString *failure=nil;NSUInteger failed=0;
    if (![result[@"blocker"] isEqual:@"ready"]) return result;
    NSString *video=result[@"video"];
    NSData *data=RVDACheckedPath(model,@[@232954548,@18,@1],&failure,&failed);
    if (!data) { result[@"blocker"]=@"live_card_presentation_input_unresolved";return result; }
    NSString *legacyID=RVDAText(RVDAChild(data,7));
    NSArray *fields=RVVoteFields(data);
    if (!RVVoteSingularFields(fields,@[@7])) { result[@"blocker"]=@"live_card_identity_conflict";return result; }
    for (RVVoteField *field in fields) if (field.number==7 && (field.wire!=2 || ![legacyID isEqual:video])) {
        result[@"blocker"]=@"live_card_identity_conflict";return result;
    }
    NSArray *titlePath=@[@232954548,@18,@1,@21,@8178,@1],*imagePath=@[@232954548,@18,@1,@1];
    NSData *attributed=RVDACheckedPath(model,titlePath,&failure,&failed);
    NSString *titleFailure=RVDALiveTitleFailure(attributed),*title=RVDAText(RVDAChild(attributed,1));
    NSString *tapTitle=result[@"tap_title_path"] ? RVDAText(RVDAPath(model,result[@"tap_title_path"])) : nil;
    if (!titleFailure && tapTitle && ![tapTitle isEqual:title]) titleFailure=@"title_identity_conflict";
    NSArray *entityPath=@[@232954548,@18,@1,@21,@8178,@50001];NSData *config=RVDAPath(model,entityPath);
    NSArray *lockupFields=RVVoteFields(RVDAPath(model,@[@232954548,@18,@1,@21,@8178]));
    if (!RVVoteSingularFields(lockupFields,@[@50001])) titleFailure=@"title_entity_ambiguous";
    for (RVVoteField *field in lockupFields) if (field.number==50001 && field.wire!=2) titleFailure=@"title_entity_invalid";
    if (config && !RVVoteSingularFields(RVVoteFields(config),@[@7])) titleFailure=@"title_entity_ambiguous";
    if (config && !titleFailure) result[@"title_entity_path"]=entityPath;
    NSData *thumbnail=RVDACheckedPath(model,imagePath,&failure,&failed),*image=RVDAChild(thumbnail,1);
    NSArray *thumbnailFields=RVVoteFields(thumbnail),*urls=RVDALiveCardImageURLs(image,video);
    BOOL composite=NO;for (RVVoteField *field in thumbnailFields) composite|=field.number==60;
    BOOL imageRole=thumbnailFields && RVVoteSingularFields(thumbnailFields,@[@1,@2,@14,@60]) && !composite && urls.count;
    result[@"title_role"]=@(!titleFailure);result[@"image_role"]=@(imageRole);
    result[@"title_blocker"]=titleFailure ?: @"ready";result[@"image_blocker"]=imageRole ? @"ready" : @"thumbnail_identity_unverified";
    result[@"title_path"]=titlePath;result[@"image_path"]=imagePath;
    if (!titleFailure) result[@"title"]=title;
    if (imageRole) { result[@"image_url"]=RVDAImageURL(image,video);result[@"image_urls"]=urls; }
    result[@"duration"]=@(RVDADurationText(RVDAText(RVDAChild(thumbnail,2))));
    // YTIVideoThumbnailData descriptor: field 4 timestampStyle; its native
    // enum is Unknown=0, Default=1, Live=2. Never classify by localized text.
    RVVoteField *style=RVVoteOne(thumbnailFields,4,0);
    result[@"livestream"]=@(RVVoteSingularFields(thumbnailFields,@[@4]) && style && style.scalar==2);
    result[@"blocker"]=!titleFailure || imageRole ? @"ready" : @"live_card_no_verified_input_role";return result;
}
static NSString *RVDALiveCardFailure(NSData *model) { return RVDALiveCardContract(model)[@"blocker"] ?: @"live_card_input_unresolved"; }
static RVDAProjection *RVDALiveCardProfile(NSData *model,NSString *key) {
    NSDictionary *contract=RVDALiveCardContract(model);if (![contract[@"blocker"] isEqual:@"ready"]) return nil;
    RVDAProjection *profile=[RVDAProjection new];profile.liveCard=YES;profile.route=key;profile.adapterID=[contract[@"terminal_extension"] unsignedIntValue]==139608561 ? @"live_primary_reel_preview_v1" : @"live_primary_watch_v3";
    profile.video=contract[@"video"];profile.identityPath=contract[@"identity_path"];profile.commandType=contract[@"command_type"];profile.videoDataPath=@[@232954548,@18,@1];
    profile.titlePath=contract[@"title_path"];profile.imagePath=contract[@"image_path"];profile.titleEntityPath=contract[@"title_entity_path"];
    profile.titleRole=[contract[@"title_role"] boolValue];profile.imageRole=[contract[@"image_role"] boolValue];
    profile.titleRoleBlocker=contract[@"title_blocker"];profile.imageRoleBlocker=contract[@"image_blocker"];
    profile.originalTitle=contract[@"title"];profile.originalImageURL=contract[@"image_url"];profile.imageURLs=contract[@"image_urls"];
    profile.livestream=[contract[@"livestream"] boolValue];profile.duration=[contract[@"duration"] doubleValue];profile.structuralFingerprint=RVDAStructuralFingerprint(profile.adapterID,profile.titleRole,profile.imageRole);return profile;
}
static NSString *RVDALiveWatchFailure(NSData *model) {
    NSString *failure=nil;NSUInteger step=0;NSData *title=RVDACheckedPath(model,RVDALiveWatchTitlePath(),&failure,&step);
    NSString *role=title ? RVDALiveTitleFailure(title) : failure;
    return role ? [@"watch_role_" stringByAppendingString:role] : @"explicit_watch_owner_required";
}
static RVDAProjection *RVDALiveWatchProfile(NSData *model,NSString *key,NSString *watchVideo) {
    if (!RVVideoIDValid(watchVideo)) return nil;
    NSData *attributed=RVDACheckedPath(model,RVDALiveWatchTitlePath(),NULL,NULL);
    if (RVDALiveTitleFailure(attributed)) return nil;
    RVDAProjection *profile=[RVDAProjection new];profile.liveCard=YES;profile.watchTitle=YES;profile.titleRole=YES;
    profile.route=key;profile.adapterID=@"watch_metadata_v1";profile.video=watchVideo;profile.titlePath=RVDALiveWatchTitlePath();
    profile.originalTitle=RVDAText(RVDAChild(attributed,1));profile.titleRoleBlocker=@"ready";profile.imageRoleBlocker=@"watch_image_not_requested";
    profile.structuralFingerprint=RVDAStructuralFingerprint(profile.adapterID,YES,NO);return profile;
}
