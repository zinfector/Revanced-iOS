// SPDX-License-Identifier: MIT
// Semantic live contracts: profiles/dearrow-adaptive-contract.json.
// Legacy bundled recipes retain their independently established contracts.
// Reuse the bounded, unknown-field-preserving protobuf helpers used by RYD.
static NSData *RVDAChild(NSData *data,uint32_t number) {
    return RVVoteOne(RVVoteFields(data),number,2).value;
}
static NSData *RVDAPathChild(NSData *data,id step) {
    if ([step isKindOfClass:NSNumber.class]) return RVDAChild(data,[step unsignedIntValue]);
    if (![step isKindOfClass:NSDictionary.class]) return nil;
    NSUInteger wanted=[step[@"occurrence"] unsignedIntegerValue],index=0;uint32_t number=[step[@"field"] unsignedIntValue];
    if (!number || wanted>=16) return nil;
    for (RVVoteField *field in RVVoteFields(data)) if (field.number==number) {
        if (field.wire!=2) return nil;if (index++==wanted) return field.value;
    }return nil;
}
static NSData *RVDAPath(NSData *data,NSArray *path) {
    for (id step in path) { data=RVDAPathChild(data,step);if (!data) return nil; }return data;
}
static NSData *RVDASet(NSData *data,uint32_t number,NSData *value) {
    if (!data || !value) return nil;
    NSArray *fields=RVVoteFields(data);if (!fields || !RVVoteSingularFields(fields,@[@(number)])) return nil;
    return RVVoteReplace(fields,@{@(number):RVVoteMessageField(number,value)},nil);
}
static NSData *RVDASetPath(NSData *data,NSArray *path,NSData *value) {
    if (!data || !value) return nil;
    if (!path.count) return value;
    id step=path[0];NSData *child=RVDAPathChild(data,step);if (!child) return nil;
    NSData *next=RVDASetPath(child,[path subarrayWithRange:NSMakeRange(1,path.count-1)],value);
    if (!next) return nil;
    if ([step isKindOfClass:NSNumber.class]) return RVDASet(data,[step unsignedIntValue],next);
    uint32_t number=[step[@"field"] unsignedIntValue];NSUInteger wanted=[step[@"occurrence"] unsignedIntegerValue],index=0;
    NSArray *fields=RVVoteFields(data);if (!fields) return nil;NSMutableData *result=[NSMutableData data];BOOL replaced=NO;
    for (RVVoteField *field in fields) {
        if (field.number==number && index++==wanted) { [result appendData:RVVoteMessageField(number,next)];replaced=YES; }
        else [result appendData:field.raw];
    }return replaced ? result : nil;
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
    if (RVDAChild(thumbnail,60)) return nil; // composite has no owned single image
    NSData *source=RVVoteMessageField(2,imageBytes); // ImageSource.imageData, native oneof
    NSData *next=RVVoteReplace(fields,@{@1:RVVoteMessageField(1,source)},nil);
    return RVDASet(thumbnail,1,next);
}
static NSString *RVDAFrameURL(NSString *video) {
    if (!RVVideoIDValid(video)) return nil;
    NSString *url=[NSString stringWithFormat:@"https://i.ytimg.com/vi/%@/hq%d.jpg",video,[RVSetting(@"thumbnail_frame") intValue]];
    return RVImageCachedAvailable(url) ? url : nil;
}
static NSData *RVDAVideoData(NSData *data,NSString *video,NSString *title,NSData *image,BOOL fallback,NSInteger lines) {
    NSData *next=data,*metadata=RVDAChild(data,2),*thumbnail=RVDAChild(data,1);
    if (title) {
        NSData *value=metadata ? RVDAMetadataTitle(metadata,title) : nil;if (value) next=RVDASet(next,2,value) ?: next;
        NSData *lockup=RVDAChild(next,21);if (lockup) next=RVDASet(next,21,RVDALockupTitle(lockup,title)) ?: next;
    }
    if (lines>=1 && lines<=6 && metadata) {
        NSData *value=RVVoteReplace(RVVoteFields(RVDAChild(next,2)),@{@7:RVVoteScalarField(7,lines)},nil);
        if (value) next=RVDASet(next,2,value) ?: next;
    }
    if (image && thumbnail) { NSData *value=RVDAStaticImage(thumbnail,image);if (value) next=RVDASet(next,1,value) ?: next; }
    else if (fallback && thumbnail && !RVDAChild(thumbnail,14)) {
        NSString *url=RVDAFrameURL(video);NSData *original=RVDAChild(thumbnail,1);
        if (url && original) {
            NSData *source=RVVoteMessageField(1,[url dataUsingEncoding:NSUTF8StringEncoding]);
            NSData *value=RVVoteReplace(RVVoteFields(original),@{@1:RVVoteMessageField(1,source)},nil);
            next=RVDASet(next,1,RVDASet(thumbnail,1,value)) ?: next;
        }
    }
    return next;
}
static NSData *RVDAHolderAccessibility(NSData *model,NSArray<NSNumber *> *videoPath,NSString *title,NSString *originalTitle) {
    if (!title || videoPath.count<2) return model;
    NSArray *path=[videoPath subarrayWithRange:NSMakeRange(0,videoPath.count-1)];NSData *holder=RVDAPath(model,path);
    NSString *label=RVDAText(RVDAChild(holder,2));if (!label) return model;
    NSData *metadata=RVDAChild(RVDAPath(model,videoPath),2);
    NSString *plain=originalTitle ?: RVDAText(RVDAChild(metadata,1)) ?: RVDAText(RVDAPath(metadata,@[@27,@1]));
    NSRange range=plain.length ? [label rangeOfString:plain] : NSMakeRange(NSNotFound,0);
    NSString *next;
    if (range.location!=NSNotFound) next=[label stringByReplacingCharactersInRange:range withString:title];
    else {
        if (originalTitle) return model; // Preserve the original accessibility details.
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
@property(nonatomic,strong) NSArray<NSNumber *> *identityPath;
@property(nonatomic,strong) NSData *source;
@property(nonatomic,copy) NSString *originalTitle,*untranslatedTitle,*originalImageURL;
@property(nonatomic) double duration;
@property(nonatomic) BOOL watchTitle,titleRole,imageRole;
@property(nonatomic) BOOL contextSlots;
@property(nonatomic) BOOL liveCard,livestream,shelfCard;
@property(nonatomic,copy) NSString *commandType;
@property(nonatomic,copy) NSString *adapterID,*structuralFingerprint,*titleRoleBlocker,*imageRoleBlocker;
@property(nonatomic,strong) NSArray<NSNumber *> *titlePath,*imagePath,*titleEntityPath;
@property(nonatomic,copy) NSArray<NSString *> *imageURLs;
@end
@implementation RVDAProjection @end

static NSDictionary *RVDAParseResourceParts(NSString *uri) {
    if (!uri.length || uri.length>2048) return nil;
    // The service can supply a resource path, while bundled filenames use an
    // underscore revision separator. Keep the revision for diagnostics and native
    // instance matching; a family match alone never authorizes a model rewrite.
    NSString *leaf=[uri.lastPathComponent stringByRemovingPercentEncoding];if (!leaf) return nil;
    static NSRegularExpression *regex;static dispatch_once_t once;
    dispatch_once(&once,^{ regex=[NSRegularExpression regularExpressionWithPattern:@"^([A-Za-z0-9_]+\\.eml(?:-js)?)(-fe)?[_|]([0-9a-fA-F]{1,16})$" options:0 error:nil]; });
    NSTextCheckingResult *match=[regex firstMatchInString:leaf options:0 range:NSMakeRange(0,leaf.length)];
    if (!match) return nil;
    NSString *name=[leaf substringWithRange:[match rangeAtIndex:1]],*revision=[[leaf substringWithRange:[match rangeAtIndex:3]] lowercaseString];
    BOOL fe=[match rangeAtIndex:2].location!=NSNotFound;
    // Native .eml-fe resources are distinct compiled templates. Recognizing a
    // key does not authorize projecting the old .eml/-js field contracts.
    return @{@"basename":name,@"variant":fe ? @"fe" : @"bundled",
        @"revision":revision,@"key":[NSString stringWithFormat:@"%@%@|%@",name,fe ? @"-fe" : @"",revision]};
}
static NSDictionary *RVDAResourceParts(NSString *uri) {
    if (![uri isKindOfClass:NSString.class] || !uri.length || uri.length>2048) return nil;
    static NSCache *cache;static dispatch_once_t once;dispatch_once(&once,^{ cache=[NSCache new];cache.countLimit=128; });
    id result=[cache objectForKey:uri];
    if (!result) { result=RVDAParseResourceParts(uri) ?: NSNull.null;[cache setObject:result forKey:uri.copy]; }
    return result==NSNull.null ? nil : result;
}
static NSString *RVDAResourceName(NSString *uri) {
    return RVDAResourceParts(uri)[@"key"];
}
static BOOL RVDAResourceFamily(NSString *raw,NSString *basename,NSString *variant) {
    NSDictionary *parts=RVDAResourceParts(raw);
    return [parts[@"basename"] isEqual:basename] && [parts[@"variant"] isEqual:variant];
}
static BOOL RVDALiveCardFamily(NSString *raw) { return RVDAResourceFamily(raw,@"video_lockup_with_attachment.eml",@"fe"); }
static BOOL RVDALiveCardCandidate(NSString *raw) {
    if (RVDALiveCardFamily(raw)) return YES;
    NSDictionary *parts=RVDAResourceParts(raw);
    return [parts[@"variant"] isEqual:@"fe"] &&
        [@[@"video_lockup_with_attachment.eml",@"video_card.eml",@"video_card_content.eml"] containsObject:parts[@"basename"] ?: @""];
}
static BOOL RVDALiveWatchFamily(NSString *raw) {
    NSDictionary *parts=RVDAResourceParts(raw);
    return [@[@"video_metadata.eml",@"video_metadata_inner.eml"] containsObject:parts[@"basename"] ?: @""];
}
static NSString *RVDAResourceProfileStatus(NSString *raw) {
    NSDictionary *parts=RVDAResourceParts(raw);if (!parts) return @"template_uri_grammar_rejected";
    if (RVDALiveCardCandidate(raw) || RVDALiveWatchFamily(raw) || RVDAResourceFamily(raw,@"horizontal_shelf.eml",@"fe")) return @"structural_model_validation_required";
    NSArray *pinned=@[@"compact_video.eml-js|73af68f5375dcfd2",@"video_card.eml-js|309e33d369a3db21",@"video_card_content.eml-js|1fe054d511a8304d",
        @"video_with_context_slots.eml-js|7f46bdff852ff164"];
    if ([pinned containsObject:parts[@"key"]]) return @"pinned_uri_model_validation_required";
    for (NSString *key in pinned) if ([RVDAResourceParts(key)[@"basename"] isEqual:parts[@"basename"]])
        return [RVDAResourceParts(key)[@"variant"] isEqual:parts[@"variant"]] ? @"template_revision_unmapped" : @"template_variant_unmapped";
    return @"template_resource_unmapped";
}
static NSString *RVDALiveCardFailure(NSData *model);
static NSString *RVDALiveWatchFailure(NSData *model);
static NSArray<RVDAProjection *> *RVDAShelfProfiles(NSData *model,NSString *key);
static NSString *RVDAProfileFailure(NSData *data) {
    NSData *component=RVDAPath(data,@[@1,@168777401]);
    if (!component) return @"serialized_component_root_unresolved";
    NSString *raw=RVDAText(RVDAPath(component,@[@3,@172035250,@1]));
    if (RVDAResourceFamily(raw,@"horizontal_shelf.eml",@"fe")) return @"shelf_presentation_suspended_pending_child_ownership";
    if (RVDALiveCardCandidate(raw)) return RVDALiveCardFailure(RVDAChild(component,5));
    if (RVDALiveWatchFamily(raw)) return RVDALiveWatchFailure(RVDAChild(component,5));
    NSString *status=RVDAResourceProfileStatus(raw);
    return [status isEqual:@"pinned_uri_model_validation_required"] ? @"video_model_path_unresolved" : status;
}
static NSString *RVDATemplate(NSData *component) {
    return RVDAResourceName(RVDAText(RVDAPath(component,@[@3,@172035250,@1])));
}
static double RVDADurationText(NSString *text) {
    if (!text.length || text.length>16) return 0;NSArray *parts=[text componentsSeparatedByString:@":"];
    if (parts.count<2 || parts.count>3) return 0;double value=0;NSUInteger index=0;
    for (NSString *part in parts) {
        if (!part.length || [part rangeOfCharacterFromSet:NSCharacterSet.decimalDigitCharacterSet.invertedSet].location!=NSNotFound) return 0;
        double next=part.doubleValue;if (next<0 || (index++ && next>=60)) return 0;value=value*60+next;
    }return value>0 && value<=604800 ? value : 0;
}
static NSString *RVDAImageURL(NSData *image,NSString *video) {
    NSString *best=nil;uint64_t width=0;NSUInteger count=0;
    for (RVVoteField *field in RVVoteFields(image)) if (field.number==1 && field.wire==2) {
        if (++count>16) return nil;
        NSString *url=RVDAText(RVDAChild(field.value,1));NSURLComponents *parts=[NSURLComponents componentsWithString:url ?: @""];
        NSArray *path=parts.path.pathComponents;
        if (![parts.scheme isEqual:@"https"] || ![@[@"i.ytimg.com",@"i1.ytimg.com",@"i2.ytimg.com",@"i3.ytimg.com",@"i4.ytimg.com"] containsObject:parts.host.lowercaseString] || parts.user || parts.password || path.count!=4 || ![@[@"vi",@"vi_webp"] containsObject:path[1]] || ![path[2] isEqual:video]) continue;
        RVVoteField *size=RVVoteOne(RVVoteFields(field.value),4,0);
        if (!best || size.scalar>=width) { best=url;width=size.scalar; }
    }return best;
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
#include "RVDeArrowLiveCard.inc"
// Native descriptors: HorizontalShelfModel.shelfData=20,
// HorizontalShelfData.videoCards=9/cards=17, HorizontalShelfCard.video=1,
// VideoCardData.videoData=1/onTap=6. Preserve every unselected sibling byte.
static NSArray<RVDAProjection *> *RVDAShelfProfiles(NSData *model,NSString *key) {
    if (!RVDAResourceFamily(key,@"horizontal_shelf.eml",@"fe")) return @[];
    NSArray *shelfPath=@[@312131490,@20];NSData *shelf=RVDACheckedPath(model,shelfPath,NULL,NULL);
    NSArray *fields=RVVoteFields(shelf);if (!fields) return @[];
    NSMutableArray *found=[NSMutableArray array];NSUInteger direct=0,wrapped=0;
    for (RVVoteField *field in fields) {
        if (field.number!=9 && field.number!=17) continue;
        if (field.wire!=2 || direct+wrapped>=16) return @[];
        NSUInteger occurrence=field.number==9 ? direct++ : wrapped++;
        NSArray *cardPath=[shelfPath arrayByAddingObject:@{@"field":@(field.number),@"occurrence":@(occurrence)}];
        NSData *card=field.value;
        if (field.number==17) {
            NSArray *oneof=RVVoteFields(card);
            if (!RVVoteSingularFields(oneof,@[@1,@2]) || RVDAChild(card,2)) continue;
            card=RVDAChild(card,1);cardPath=[cardPath arrayByAddingObject:@1];
        }
        NSArray *cardFields=RVVoteFields(card);if (!RVVoteSingularFields(cardFields,@[@1,@6])) continue;
        NSArray *videoPath=[cardPath arrayByAddingObject:@1];NSData *data=RVDACheckedPath(model,videoPath,NULL,NULL);
        NSString *video=RVDAText(RVDAChild(data,7));
        NSDictionary *identity=RVDALiveCardNavigationAt(model,[cardPath arrayByAddingObjectsFromArray:@[@6,@169495254]]);
        if (!RVVideoIDValid(video) || ![identity[@"blocker"] isEqual:@"ready"] || ![identity[@"video"] isEqual:video]) continue;
        NSData *thumbnail=RVDAChild(data,1);NSArray *tf=RVVoteFields(thumbnail);
        NSArray *urls=RVDALiveCardImageURLs(RVDAChild(thumbnail,1),video);
        BOOL image=tf && RVVoteSingularFields(tf,@[@1,@2,@4,@14,@60]) && !RVDAChild(thumbnail,60) && urls.count;
        NSArray *titlePath=[videoPath arrayByAddingObjectsFromArray:@[@21,@8178,@1]];
        NSData *attributed=RVDACheckedPath(model,titlePath,NULL,NULL);
        BOOL title=!RVDALiveTitleFailure(attributed);
        if (!title && !image) continue;
        RVDAProjection *profile=[RVDAProjection new];profile.liveCard=YES;profile.shelfCard=YES;
        profile.route=key;profile.adapterID=@"horizontal_shelf_video_card_v1";profile.video=video;
        profile.videoDataPath=videoPath;profile.identityPath=identity[@"identity_path"];profile.commandType=identity[@"command_type"];
        profile.titlePath=titlePath;profile.titleRole=title;profile.imageRole=image;
        profile.titleRoleBlocker=title ? @"ready" : @"shelf_title_input_unverified";profile.imageRoleBlocker=image ? @"ready" : @"shelf_image_identity_unverified";
        profile.imagePath=[videoPath arrayByAddingObject:@1];profile.imageURLs=urls;profile.originalImageURL=RVDAImageURL(RVDAChild(thumbnail,1),video);
        profile.originalTitle=title ? RVDAText(RVDAChild(attributed,1)) : RVDAText(RVDAPath(data,@[@2,@1]));
        // Entity titles need the same narrow override removal as primary cards.
        NSArray *entity=[videoPath arrayByAddingObjectsFromArray:@[@21,@8178,@50001]];NSData *config=RVDAPath(model,entity);
        NSArray *lockup=RVVoteFields(RVDAPath(model,[videoPath arrayByAddingObjectsFromArray:@[@21,@8178]]));
        if (!RVVoteSingularFields(lockup,@[@50001])) { profile.titleRole=NO;profile.titleRoleBlocker=@"shelf_title_entity_ambiguous"; }
        if (config) {
            if (!RVVoteSingularFields(RVVoteFields(config),@[@7])) { profile.titleRole=NO;profile.titleRoleBlocker=@"shelf_title_entity_ambiguous"; }
            else profile.titleEntityPath=entity;
        }
        profile.duration=RVDADurationText(RVDAText(RVDAChild(thumbnail,2)));
        profile.livestream=RVVoteOne(tf,4,0).scalar==2;
        profile.structuralFingerprint=RVDAStructuralFingerprint(profile.adapterID,profile.titleRole,profile.imageRole);[found addObject:profile];
    }return found.copy;
}
// Descriptor-proven alternative: ShelfCard.viewModel (YTIRenderer) ->
// YTIElementViewModel extension 36905 -> ELMPBElement field 1. These bytes
// describe one child, never authorize a write to the shared shelf container.
static NSArray<NSDictionary *> *RVDAShelfViewModels(NSData *model,NSString *key) {
    if (!RVDAResourceFamily(key,@"horizontal_shelf.eml",@"fe") || model.length>2*1024*1024) return @[];
    NSData *shelf=RVDACheckedPath(model,@[@312131490,@20],NULL,NULL);
    NSArray *fields=RVVoteFields(shelf);if (!fields) return @[];
    NSMutableArray *children=[NSMutableArray array];NSUInteger occurrence=0,total=0;
    for (RVVoteField *field in fields) if (field.number==17) {
        NSUInteger index=occurrence++;if (field.wire!=2 || occurrence>64) return @[];
        NSArray *oneof=RVVoteFields(field.value);
        if (!RVVoteSingularFields(oneof,@[@1,@2]) || RVDAChild(field.value,1)) continue;
        NSData *renderer=RVDAChild(field.value,2);
        NSArray *rendererFields=RVVoteFields(renderer);
        if (!RVVoteSingularFields(rendererFields,@[@36905])) continue;
        NSData *viewModel=RVDAChild(renderer,36905);
        if (!RVVoteSingularFields(RVVoteFields(viewModel),@[@1,@997])) continue;
        NSData *element=RVDAChild(viewModel,1);
        if (!element.length || element.length>128*1024 || !RVVoteFields(element)) continue;
        total+=element.length;if (total>2*1024*1024) return @[];
        [children addObject:@{@"element":element,@"occurrence":@(index),
            @"path":@[@312131490,@20,@{@"field":@17,@"occurrence":@(index)},@2,@36905,@1]}];
    }return children.copy;
}
static void RVDADiscover(NSData *element,NSArray *path,NSMutableArray<RVDAProjection *> *found,
                       NSUInteger depth,NSUInteger *budget,NSString *watchVideo) {
    if (depth>16 || !*budget || found.count>16) return;(*budget)--;
    NSArray *fields=RVVoteFields(element);if (!fields) return;
    NSData *component=RVDAPath(element,@[@1,@168777401]);NSString *name=RVDATemplate(component);
    if (RVDAResourceFamily(name,@"horizontal_shelf.eml",@"fe")) {
        // The shelf model describes multiple videos, but its factory proves
        // only the container occurrence, not independent materialized cells.
        // Do not create per-child UI leases or speculative projections from
        // that shared factory. Read-only child probes remain available.
        return;
    }
    if (RVDALiveCardCandidate(name) || RVDALiveWatchFamily(name)) {
        RVDAProjection *profile=RVDALiveCardCandidate(name) ? RVDALiveCardProfile(RVDAChild(component,5),name) : RVDALiveWatchProfile(RVDAChild(component,5),name,watchVideo);
        if (profile) { profile.componentPath=[path arrayByAddingObjectsFromArray:@[@1,@168777401]];[found addObject:profile]; }
        return;
    }
    NSDictionary *roots=@{@"compact_video.eml-js|73af68f5375dcfd2":@[@232971250,@25,@1],
        @"video_card.eml-js|309e33d369a3db21":@[@232903841,@1,@1],
        @"video_card_content.eml-js|1fe054d511a8304d":@[@232903841,@1,@1]};
    NSArray *modelPath=roots[name ?: @""];NSData *model=RVDAChild(component,5);
    if ([name isEqual:@"video_with_context_slots.eml-js|7f46bdff852ff164"]) {
        // This model is C++ only. Its extension number is not guessed: the pinned
        // template establishes $1.1.1 as VideoData; accept one opaque extension.
        NSArray<RVVoteField *> *extensions=RVVoteFields(model);
        if (extensions.count==1 && [extensions[0] wire]==2) modelPath=@[@([extensions[0] number]),@1,@1];
    }
    NSData *videoData=modelPath ? RVDAPath(model,modelPath) : nil;
    NSString *video=RVDAText(RVDAChild(videoData,7));
    if (RVVideoIDValid(video)) {
        RVDAProjection *profile=[RVDAProjection new];profile.video=video;profile.route=name;
        profile.componentPath=[path arrayByAddingObjectsFromArray:@[@1,@168777401]];
        profile.videoDataPath=modelPath;profile.titleRole=RVDAChild(videoData,2) || RVDAPath(videoData,@[@21,@8178,@1]);
        NSData *thumbnail=RVDAChild(videoData,1);
        // sourcesArray is repeated; preview metadata owns a separate preview.
        profile.imageRole=RVDAChild(thumbnail,1) && !RVDAChild(thumbnail,60);
        NSData *metadata=RVDAChild(videoData,2);
        profile.originalTitle=RVDAText(RVDAChild(metadata,1)) ?: RVDAText(RVDAPath(metadata,@[@27,@1])) ?: RVDAText(RVDAPath(videoData,@[@21,@8178,@1,@1]));
        profile.untranslatedTitle=RVDAText(RVDAChild(metadata,30));
        profile.duration=RVDADurationText(RVDAText(RVDAChild(thumbnail,2)));
        profile.originalImageURL=RVDAImageURL(RVDAChild(thumbnail,1),video);
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
    NSUInteger children=0,occurrence=0;for (RVVoteField *field in fields) if (field.number==3) children++;
    if (children>16) { *budget=0;return; }
    for (RVVoteField *field in fields) if (field.number==3) {
        if (field.wire!=2) { *budget=0;return; }
        id step=children==1 ? (id)@3 : @{@"field":@3,@"occurrence":@(occurrence)};occurrence++;
        // Only the reviewed generic Element.children field is traversed. Opaque
        // serialized shelf-model fields do not authorize a descendant rewrite.
        RVDADiscover(field.value,[path arrayByAddingObject:step],found,depth+1,budget,watchVideo);
    }
}
static NSArray<RVDAProjection *> *RVDAProfiles(NSData *data,NSString *watchVideo) {
    NSMutableArray *found=[NSMutableArray array];NSUInteger budget=128;
    RVDADiscover(data,@[],found,0,&budget,watchVideo);
    if (!budget || found.count>16) return @[];
    if (found.count>1) for (RVDAProjection *profile in found) if (!profile.liveCard || profile.watchTitle) return @[];
    for (RVDAProjection *profile in found) profile.source=data;return found.copy;
}
static void RVDAFeedCollect(NSData *element,NSMutableArray *profiles,NSUInteger depth,NSUInteger *budget) {
    if (depth>12 || !*budget || profiles.count>=48 || element.length>2*1024*1024) return;(*budget)--;
    NSData *component=RVDAPath(element,@[@1,@168777401]);NSString *key=RVDATemplate(component);
    if (RVDAResourceFamily(key,@"horizontal_shelf.eml",@"fe")) {
        for (NSDictionary *child in RVDAShelfViewModels(RVDAChild(component,5),key)) {
            for (RVDAProjection *profile in RVDAProfiles(child[@"element"],nil)) {
                if (profile.liveCard && !profile.watchTitle && !profile.shelfCard && profiles.count<48) [profiles addObject:profile];
            }
        }return;
    }
    for (RVVoteField *field in RVVoteFields(element)) if (field.number==3 && field.wire==2)
        RVDAFeedCollect(field.value,profiles,depth+1,budget);
}
static NSArray<RVDAProjection *> *RVDAFeedProfiles(NSData *element) {
    NSMutableArray *profiles=[RVDAProfiles(element,nil) mutableCopy];NSUInteger budget=96;
    RVDAFeedCollect(element,profiles,0,&budget);return profiles.copy;
}

static RVDAProjection *RVDAProfile(NSData *data,NSString *watchVideo) {
    NSArray *found=RVDAProfiles(data,watchVideo);return found.count==1 ? found[0] : nil;
}
static NSData *RVDAProjectOnSource(RVDAProjection *profile,NSData *source,NSString *title,NSData *image,BOOL fallback,NSInteger lines) {
    NSData *component=RVDAPath(source,profile.componentPath),*model=RVDAChild(component,5),*next=model;
    if (profile.watchTitle && title && profile.titleRole) {
        NSArray *path=profile.titlePath ?: @[@377581260,@6,@1019,@1];NSData *original=RVDAPath(model,path);
        next=RVDASetPath(model,path,RVDAAttributed(original,title)) ?: model;
    } else if (profile.videoDataPath) {
        NSData *original=RVDAPath(model,profile.videoDataPath);
        NSData *value=original;
        if (profile.liveCard) {
            if (title && profile.titleRole) {
                NSData *attributed=RVDAAttributed(RVDAPath(model,profile.titlePath),title);
                NSArray *relative=[profile.titlePath subarrayWithRange:NSMakeRange(profile.videoDataPath.count,profile.titlePath.count-profile.videoDataPath.count)];
                if (attributed) value=RVDASetPath(value,relative,attributed) ?: value;
                if (profile.titleEntityPath) {
                    NSArray *entityPath=[profile.titleEntityPath subarrayWithRange:NSMakeRange(profile.videoDataPath.count,profile.titleEntityPath.count-profile.videoDataPath.count)];
                    NSData *config=RVDAPath(value,entityPath);
                    if (config) value=RVDASetPath(value,entityPath,RVVoteReplace(RVVoteFields(config),@{},[NSSet setWithObject:@7])) ?: value;
                }
            }
            if (image && profile.imageRole) value=RVDASet(value,1,RVDAStaticImage(RVDAChild(value,1),image)) ?: value;
        } else value=RVDAVideoData(original,profile.video,profile.titleRole ? title : nil,profile.imageRole ? image : nil,profile.imageRole && fallback,profile.titleRole ? lines : 0);
        next=profile.shelfCard ? model : RVDAHolderAccessibility(model,profile.videoDataPath,profile.titleRole ? title : nil,profile.liveCard ? profile.originalTitle : nil);
        next=RVDASetPath(next,profile.videoDataPath,value) ?: next;
        if (profile.contextSlots) {
            NSArray *path=@[profile.videoDataPath[0],@5];NSData *slots=RVDAPath(next,path);
            if (slots) {
                NSData *filtered=RVDASlots(slots,title && profile.titleRole,(image || fallback) && profile.imageRole);
                if (filtered) next=RVDASetPath(next,path,filtered) ?: next;
            }
        }
    }
    // The entire graph outside explicitly owned presentation roles is invariant.
    if (profile.liveCard) {
        NSMutableArray *paths=[NSMutableArray array];
        if (title && profile.titleRole) { [paths addObject:profile.titlePath];if (profile.titleEntityPath) [paths addObject:profile.titleEntityPath]; }
        if (image && profile.imageRole) [paths addObject:[profile.imagePath arrayByAddingObject:@1]];
        // The card accessibility label has no command ranges; its value may change.
        if (!profile.watchTitle && title && RVDAChild(RVDAPath(model,@[@232954548,@18]),2)) [paths addObject:@[@232954548,@18,@2]];
        NSData *restored=next;
        for (NSArray *path in paths) { NSData *original=RVDAPath(model,path);if (original) restored=RVDASetPath(restored,path,original); }
        if (![restored isEqual:model]) return source;
    }
    if ([next isEqual:model]) return source;
    NSData *projected=RVDASet(component,5,next);
    return projected ? RVDASetPath(source,profile.componentPath,projected) ?: source : source;
}
