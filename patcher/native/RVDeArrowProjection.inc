// SPDX-License-Identifier: MIT
// Pinned serialized presentation contracts: profiles/dearrow-ui-contract.json.
// Reuse the bounded, unknown-field-preserving protobuf helpers used by RYD.
static NSData *RVDAChild(NSData *data,uint32_t number) {
    return RVVoteOne(RVVoteFields(data),number,2).value;
}
static NSData *RVDAPath(NSData *data,NSArray<NSNumber *> *path) {
    for (NSNumber *number in path) { data=RVDAChild(data,number.unsignedIntValue);if (!data) return nil; }return data;
}
static NSData *RVDASet(NSData *data,uint32_t number,NSData *value) {
    if (!data || !value) return nil;
    NSArray *fields=RVVoteFields(data);if (!fields || !RVVoteSingularFields(fields,@[@(number)])) return nil;
    return RVVoteReplace(fields,@{@(number):RVVoteMessageField(number,value)},nil);
}
static NSData *RVDASetPath(NSData *data,NSArray<NSNumber *> *path,NSData *value) {
    if (!data || !value) return nil;
    if (!path.count) return value;
    uint32_t number=[path[0] unsignedIntValue];NSData *child=RVDAChild(data,number);if (!child) return nil;
    NSData *next=RVDASetPath(child,[path subarrayWithRange:NSMakeRange(1,path.count-1)],value);
    return next ? RVDASet(data,number,next) : nil;
}
static NSString *RVDAText(NSData *data) {
    return data.length<=4096 ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
}
static NSData *RVDAAttributed(NSData *original,NSString *title) {
    NSArray *fields=RVVoteFields(original);if (!fields || !RVVoteSingularFields(fields,@[@1])) return nil;
    // Keep paragraph/line/truncation policy. Old command/style/attachment ranges
    // refer to the old text; the native component supplies its font and color.
    return RVVoteReplace(fields,@{@1:RVVoteMessageField(1,[title dataUsingEncoding:NSUTF8StringEncoding])},
        [NSSet setWithArray:@[@5,@6,@8,@10,@13,@14]]);
}
static NSData *RVDAMetadataTitle(NSData *data,NSString *title) {
    NSArray *fields=RVVoteFields(data);if (!fields || !RVVoteSingularFields(fields,@[@1,@27,@30,@31])) return nil;
    RVVoteField *rich=RVVoteOne(fields,27,2);NSData *text=[title dataUsingEncoding:NSUTF8StringEncoding];
    NSData *value=rich ? RVDAAttributed(rich.value,title) : text;if (!value) return nil;
    return RVVoteReplace(fields,@{@(rich ? 27 : 1):RVVoteMessageField(rich ? 27 : 1,value),@30:RVVoteMessageField(30,text)},
        [NSSet setWithArray:@[@(rich ? 1 : 27),@31]]);
}
static NSData *RVDALockupTitle(NSData *renderer,NSString *title) {
    NSData *lockup=RVDAChild(renderer,8178),*original=RVDAChild(lockup,1);if (!original) return renderer;
    NSData *attributed=RVDAAttributed(original,title);if (!attributed) return renderer;
    NSData *next=RVDASet(lockup,1,attributed),*config=RVDAChild(next,50001);
    // The exact template can otherwise read an alternate title from an entity.
    // Remove only that title-entity key, preserving its native style configuration.
    if (config) next=RVDASet(next,50001,RVVoteReplace(RVVoteFields(config),@{},[NSSet setWithObject:@7]));
    return next ? RVDASet(renderer,8178,next) : renderer;
}
static NSData *RVDAStaticImage(NSData *thumbnail,NSData *imageBytes) {
    NSData *image=RVDAChild(thumbnail,1);NSArray *fields=RVVoteFields(image);if (!fields) return nil;
    if (RVDAChild(thumbnail,14) || RVDAChild(thumbnail,60)) return nil; // animation/composite
    NSData *source=RVVoteMessageField(2,imageBytes); // ImageSource.imageData, native oneof
    NSData *next=RVVoteReplace(fields,@{@1:RVVoteMessageField(1,source)},nil);
    return RVDASet(thumbnail,1,next);
}
static NSString *RVDAFrameURL(NSString *video) {
    if (!RVVideoIDValid(video)) return nil;
    NSString *url=[NSString stringWithFormat:@"https://i.ytimg.com/vi/%@/hq%d.jpg",video,[RVSetting(@"thumbnail_frame") intValue]];
    return RVImageCachedAvailable(url) ? url : nil;
}
static NSData *RVDAVideoData(NSData *data,NSString *title,NSData *image,BOOL fallback) {
    NSData *next=data,*metadata=RVDAChild(data,2),*thumbnail=RVDAChild(data,1);
    if (title) {
        NSData *value=metadata ? RVDAMetadataTitle(metadata,title) : nil;if (value) next=RVDASet(next,2,value) ?: next;
        NSData *lockup=RVDAChild(next,21);if (lockup) next=RVDASet(next,21,RVDALockupTitle(lockup,title)) ?: next;
    }
    if (image && thumbnail) { NSData *value=RVDAStaticImage(thumbnail,image);if (value) next=RVDASet(next,1,value) ?: next; }
    else if (fallback && thumbnail && !RVDAChild(thumbnail,14)) {
        NSString *url=RVDAFrameURL(RVDAText(RVDAChild(data,7)));NSData *original=RVDAChild(thumbnail,1);
        if (url && original) {
            NSData *source=RVVoteMessageField(1,[url dataUsingEncoding:NSUTF8StringEncoding]);
            NSData *value=RVVoteReplace(RVVoteFields(original),@{@1:RVVoteMessageField(1,source)},nil);
            next=RVDASet(next,1,RVDASet(thumbnail,1,value)) ?: next;
        }
    }
    return next;
}
static NSData *RVDAHolderAccessibility(NSData *model,NSArray<NSNumber *> *videoPath,NSString *title) {
    if (!title || videoPath.count<2) return model;
    NSArray *path=[videoPath subarrayWithRange:NSMakeRange(0,videoPath.count-1)];NSData *holder=RVDAPath(model,path);
    NSString *label=RVDAText(RVDAChild(holder,2));if (!label) return model;
    NSData *metadata=RVDAChild(RVDAPath(model,videoPath),2);
    NSString *plain=RVDAText(RVDAChild(metadata,1)) ?: RVDAText(RVDAPath(metadata,@[@27,@1]));
    NSRange range=plain.length ? [label rangeOfString:plain] : NSMakeRange(NSNotFound,0);
    NSString *next;
    if (range.location!=NSNotFound) next=[label stringByReplacingCharactersInRange:range withString:title];
    else {
        NSMutableArray *parts=[NSMutableArray arrayWithObject:title];
        for (NSNumber *field in @[@2,@3]) { NSString *value=RVDAText(RVDAChild(metadata,field.unsignedIntValue));if (value.length) [parts addObject:value]; }
        next=[parts componentsJoinedByString:@", "];
    }
    NSData *value=RVDASet(holder,2,[next dataUsingEncoding:NSUTF8StringEncoding]);
    return value ? RVDASetPath(model,path,value) ?: model : model;
}

@interface RVDAProjection : NSObject
@property(nonatomic,copy) NSString *video,*route,*blocker;
@property(nonatomic,strong) NSArray<NSNumber *> *componentPath,*videoDataPath;
@property(nonatomic,strong) NSData *source;
@property(nonatomic) BOOL watchTitle,titleRole,imageRole;
@property(nonatomic) BOOL contextSlots;
@end
@implementation RVDAProjection @end

static NSString *RVDATemplate(NSData *component) {
    return RVDAText(RVDAPath(component,@[@3,@172035250,@1]));
}
static NSData *RVDASlots(NSData *data,BOOL title,BOOL image) {
    NSArray *fields=RVVoteFields(data);if (!fields) return nil;
    NSMutableData *result=[NSMutableData data];NSUInteger count=0;NSMutableSet *names=[NSMutableSet set];
    for (RVVoteField *field in fields) {
        if (field.number==1) {
            if (field.wire!=2 || ++count>64) return nil;
            NSArray *slot=RVVoteFields(field.value);NSString *name=RVDAText(RVVoteOne(slot,1,2).value);
            if (!name.length || [names containsObject:name] || !RVVoteOne(slot,2,2)) return nil;
            [names addObject:name];
            // The pinned template has explicit metadata/thumbnail fallback
            // branches when these optional override slots are absent. Project
            // those verified input models instead of guessing a node in a slot.
            if (title && [name isEqual:@"headline_slot"]) continue;
            if (image && [@[@"thumbnail_slot",@"compact_thumbnail_slot"] containsObject:name]) continue;
        }
        [result appendData:field.raw];
    }return result;
}
static void RVDADiscover(NSData *element,NSArray<NSNumber *> *path,NSMutableArray<RVDAProjection *> *found,
                       NSUInteger depth,NSUInteger *budget,NSString *watchVideo) {
    if (depth>16 || !*budget || found.count>1) return;(*budget)--;
    NSArray *fields=RVVoteFields(element);if (!fields) return;
    NSData *component=RVDAPath(element,@[@1,@168777401]);NSString *name=RVDATemplate(component);
    NSDictionary *roots=@{@"compact_video.eml-js|73af68f5375dcfd2":@[@232971250,@25,@1],
        @"video_card.eml-js|309e33d369a3db21":@[@232903841,@1,@1],
        @"video_card_content.eml-js|1fe054d511a8304d":@[@232903841,@1,@1]};
    NSArray *modelPath=roots[name ?: @""];NSData *model=RVDAChild(component,5);
    if ([name isEqual:@"video_with_context_slots.eml-js|7f46bdff852ff164"]) {
        // This model is C++ only. Its extension number is not guessed: the pinned
        // template establishes $1.1.1 as VideoData; accept one opaque extension.
        NSArray *extensions=RVVoteFields(model);
        if (extensions.count==1 && [extensions[0] wire]==2) modelPath=@[@([extensions[0] number]),@1,@1];
    }
    BOOL watch=[@[@"video_metadata.eml|5eb7b56b3539d373",@"video_metadata_inner.eml|e50f36a914ed35f7"] containsObject:name];
    NSData *videoData=modelPath ? RVDAPath(model,modelPath) : nil;
    NSString *video=RVDAText(RVDAChild(videoData,7));
    if (watch && RVVideoIDValid(watchVideo) && RVDAPath(model,@[@377581260,@6,@1019,@1])) video=watchVideo;
    if (RVVideoIDValid(video)) {
        RVDAProjection *profile=[RVDAProjection new];profile.video=video;profile.route=name;profile.watchTitle=watch;
        profile.componentPath=[path arrayByAddingObjectsFromArray:@[@1,@168777401]];
        profile.videoDataPath=modelPath;profile.titleRole=watch || RVDAChild(videoData,2) || RVDAPath(videoData,@[@21,@8178,@1]);
        NSData *thumbnail=RVDAChild(videoData,1);
        profile.imageRole=!watch && RVDAPath(thumbnail,@[@1,@1]) && !RVDAChild(thumbnail,14) && !RVDAChild(thumbnail,60);
        // Semantic slots can supersede the title/image passed to metadata.
        // A slot without an individually verified element profile passes through.
        if ([name hasPrefix:@"video_with_context_slots."]) {
            profile.contextSlots=YES;RVVoteField *extension=[RVVoteFields(model) firstObject];NSData *slots=RVDAChild(extension.value,5);
            if (slots && !RVDASlots(slots,NO,NO)) {
                profile.titleRole=NO;profile.imageRole=NO;profile.blocker=@"semantic_slot_shape_unknown";
            }
        }
        [found addObject:profile];return;
    }
    for (RVVoteField *field in fields) if (field.number==3 && field.wire==2) {
        // A numeric path cannot distinguish repeated siblings. Requiring a single
        // child prevents replacing an unrelated video in a shelf/multi-video root.
        NSUInteger children=0;for (RVVoteField *candidate in fields) children+=candidate.number==3;
        if (children==1) RVDADiscover(field.value,[path arrayByAddingObject:@3],found,depth+1,budget,watchVideo);
    }
}
static RVDAProjection *RVDAProfile(NSData *data,NSString *watchVideo) {
    NSMutableArray *found=[NSMutableArray array];NSUInteger budget=64;
    RVDADiscover(data,@[],found,0,&budget,watchVideo);
    if (found.count!=1) return nil;RVDAProjection *profile=found[0];profile.source=data;return profile;
}
static NSData *RVDAProject(RVDAProjection *profile,NSString *title,NSData *image,BOOL fallback) {
    NSData *component=RVDAPath(profile.source,profile.componentPath),*model=RVDAChild(component,5),*next=model;
    if (profile.watchTitle && title && profile.titleRole) {
        NSArray *path=@[@377581260,@6,@1019,@1];NSData *original=RVDAPath(model,path);
        next=RVDASetPath(model,path,RVDAAttributed(original,title)) ?: model;
    } else if (profile.videoDataPath) {
        NSData *original=RVDAPath(model,profile.videoDataPath);
        NSData *value=RVDAVideoData(original,profile.titleRole ? title : nil,profile.imageRole ? image : nil,profile.imageRole && fallback);
        next=RVDAHolderAccessibility(model,profile.videoDataPath,profile.titleRole ? title : nil);
        next=RVDASetPath(next,profile.videoDataPath,value) ?: next;
        if (profile.contextSlots) {
            NSArray *path=@[profile.videoDataPath[0],@5];NSData *slots=RVDAPath(next,path);
            if (slots) {
                NSData *filtered=RVDASlots(slots,title && profile.titleRole,(image || fallback) && profile.imageRole);
                if (filtered) next=RVDASetPath(next,path,filtered) ?: next;
            }
        }
    }
    if ([next isEqual:model]) return profile.source;
    NSData *projected=RVDASet(component,5,next);
    return projected ? RVDASetPath(profile.source,profile.componentPath,projected) ?: profile.source : profile.source;
}
