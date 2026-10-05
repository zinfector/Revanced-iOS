// SPDX-License-Identifier: MIT
// Report 620 root + shared APK extension; original iOS GPB command descriptors.
// Native consumer verification in RVDeArrowConsumers.inc gates every write.
static NSString *RVDALiveCardKey(void) { return @"video_lockup_with_attachment.eml-fe|72cb15f170efbde1"; }
static NSArray *RVDALiveCardIdentityPath(void) { return @[@232954548,@18,@4,@169495254,@462702848,@1,@48687757,@1]; }
static NSData *RVDACheckedPath(NSData *data,NSArray *path,NSString **failure,NSUInteger *failedStep) {
    NSUInteger step=0;
    for (NSNumber *number in path) {
        NSArray<RVVoteField *> *fields=RVVoteFields(data);NSUInteger count=0;RVVoteField *match=nil;
        for (RVVoteField *field in fields) if (field.number==number.unsignedIntValue) { count++;match=field; }
        NSString *blocker=!fields ? @"invalid_parent_protobuf" : !count ? @"field_absent" : count!=1 ? @"field_repeated" : match.wire!=2 ? @"wrong_wire_type" : nil;
        if (blocker) { if (failure) *failure=blocker;if (failedStep) *failedStep=step;return nil; }
        data=match.value;step++;
    }return data;
}
static NSArray<NSString *> *RVDALiveCardImageURLs(NSData *image,NSString *video) {
    NSArray<RVVoteField *> *fields=RVVoteFields(image);if (!fields) return nil;
    NSMutableArray *urls=[NSMutableArray array];
    for (RVVoteField *field in fields) if (field.number==1) {
        if (field.wire!=2 || urls.count>=16) return nil;
        // Validate every source, not just the largest matching source. A mixed
        // video image or an ambiguous source oneof cannot own this card.
        NSArray *source=RVVoteFields(field.value);
        if (!source || !RVVoteSingularFields(source,@[@1,@2,@3,@4,@5,@6]) ||
            RVDAChild(field.value,2) || RVDAChild(field.value,3) || RVDAChild(field.value,6)) return nil;
        NSString *url=RVDAImageURL(RVVoteMessageField(1,field.value),video);
        if (!url) return nil;[urls addObject:url];
    }return urls.count ? urls.copy : nil;
}
static NSDictionary *RVDALiveCardContract(NSData *model) {
    NSMutableDictionary *result=[NSMutableDictionary dictionary];NSString *failure=nil;NSUInteger failed=0;
    NSArray *identity=RVDALiveCardIdentityPath();NSData *value=RVDACheckedPath(model,identity,&failure,&failed);
    result[@"identity_path"]=identity;
    if (!value) { result[@"blocker"]=[@"live_card_identity_" stringByAppendingString:failure ?: @"unresolved"];
        result[@"failed_step"]=@(failed);return result; }
    NSString *video=RVDAText(value);if (!RVVideoIDValid(video)) { result[@"blocker"]=@"live_card_identity_invalid";return result; }
    result[@"video"]=video;
    NSData *data=RVDACheckedPath(model,@[@232954548,@18,@1],&failure,&failed);
    if (!data) { result[@"blocker"]=@"live_card_presentation_input_unresolved";return result; }
    NSArray *fields=RVVoteFields(data);
    if (!RVVoteSingularFields(fields,@[@1,@2,@7,@21])) { result[@"blocker"]=@"live_card_presentation_input_ambiguous";return result; }
    NSString *legacyID=RVDAText(RVDAChild(data,7));
    if (RVDAChild(data,7) && ![legacyID isEqual:video]) { result[@"blocker"]=@"live_card_identity_conflict";return result; }
    NSData *attributed=RVDACheckedPath(data,@[@21,@8178,@1],&failure,&failed);
    NSArray *titleFields=RVVoteFields(attributed);
    NSString *title=RVDAText(RVVoteOne(titleFields,1,2).value);
    if (!title.length || !RVVoteSingularFields(titleFields,@[@1])) { result[@"blocker"]=@"live_card_title_input_invalid";return result; }
    // The CoWatch wrapper's videoTitle is independently named in the iOS
    // descriptor. Require agreement when populated; do not replace a byline.
    NSString *tapTitle=RVDAText(RVDAPath(model,@[@232954548,@18,@4,@169495254,@462702848,@4]));
    if (tapTitle && ![tapTitle isEqual:title]) { result[@"blocker"]=@"live_card_title_identity_conflict";return result; }
    NSData *thumbnail=RVDAChild(data,1),*image=RVDAChild(thumbnail,1);
    NSArray *thumbnailFields=RVVoteFields(thumbnail);
    NSArray *urls=RVDALiveCardImageURLs(image,video);
    if (!thumbnailFields || !RVVoteSingularFields(thumbnailFields,@[@1,@2,@14,@60]) || RVDAChild(thumbnail,60) || !urls) {
        result[@"blocker"]=@"live_card_thumbnail_identity_unverified";return result;
    }
    NSData *config=RVDAPath(data,@[@21,@8178,@50001]);
    if (config && !RVVoteSingularFields(RVVoteFields(config),@[@7])) { result[@"blocker"]=@"live_card_title_entity_ambiguous";return result; }
    result[@"title"]=title;result[@"image_url"]=RVDAImageURL(image,video);result[@"image_urls"]=urls;
    result[@"duration"]=@(RVDADurationText(RVDAText(RVDAChild(thumbnail,2))));result[@"blocker"]=@"ready";return result;
}
static NSString *RVDALiveCardFailure(NSData *model) { return RVDALiveCardContract(model)[@"blocker"] ?: @"live_card_input_unresolved"; }
static RVDAProjection *RVDALiveCardProfile(NSData *model) {
    NSDictionary *contract=RVDALiveCardContract(model);if (![contract[@"blocker"] isEqual:@"ready"]) return nil;
    RVDAProjection *profile=[RVDAProjection new];profile.liveCard=YES;profile.route=RVDALiveCardKey();
    profile.video=contract[@"video"];profile.identityPath=RVDALiveCardIdentityPath();profile.videoDataPath=@[@232954548,@18,@1];
    profile.originalTitle=contract[@"title"];profile.originalImageURL=contract[@"image_url"];profile.duration=[contract[@"duration"] doubleValue];
    profile.titleRole=YES;profile.imageRole=YES;return profile;
}
