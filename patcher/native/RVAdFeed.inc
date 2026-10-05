// SPDX-License-Identifier: MIT
// Whitelist derived from the 21.39.4 GPB descriptors; unknown unions pass through.
@interface RVAdField : NSObject
@property(nonatomic) SEL selector;
@property(nonatomic,copy) NSString *name;
@end
@implementation RVAdField @end
static NSDictionary *RVAdFieldAccessors;
static NSDictionary *RVAdFeedFields(void) {
    static NSDictionary *fields;static dispatch_once_t once;
    dispatch_once(&once,^{ fields=@{
        @"YTISectionListSupportedRenderers":@[@"companionAdRenderer",@"adSlotRenderer"],
        @"YTIItemSectionSupportedRenderers":@[@"promotedVideoRenderer",@"compactPromotedVideoRenderer",@"compactPromotedBannerRenderer",@"compactPromotedItemRenderer",@"promotedAppInstallRenderer",@"promotedTextBannerRenderer",@"promoted15ClickTextWatchRenderer",@"promoted15ClickTextCtdWatchRenderer",@"promoted15ClickPtTextWatchRenderer",@"promoted15ClickPtTextCtdWatchRenderer",@"promotedSparklesTextCtdWatch15ClickRenderer",@"promotedSparklesTextCtdWatchGridFormRenderer",@"promotedSparklesTextCtdWatchWideFormRenderer",@"promotedSparklesTextWatch15ClickRenderer",@"promotedSparklesTextWatchGridFormRenderer",@"promotedSparklesTextWatchWideFormRenderer",@"promotedVideoInlineMutedRenderer",@"promotedSparklesTextProductWatchRenderer",@"promotedSparklesTextHomeRenderer",@"promotedSparklesTextSearchRenderer",@"adsWebViewRenderer",@"promotedSparklesTextProductHomeRenderer",@"promotedSparklesTextCtdHomeRenderer",@"promotedSparklesTextCtdHomeCompactFormRenderer",@"promotedDiscoveryAppPromoCompactFormRenderer",@"adSlotRenderer"],
        @"YTIGridSupportedRenderers":@[@"gridPromotedVideoRenderer",@"gridPromotedBannerRenderer",@"promotedSparklesTextCtdWatchGridFormRenderer",@"promotedSparklesTextWatchGridFormRenderer"],
        @"YTIHorizontalListSupportedRenderers":@[@"gridPromotedVideoRenderer",@"gridPromotedBannerRenderer",@"promoted15ClickTextWatchRenderer",@"promoted15ClickTextCtdWatchRenderer",@"promoted15ClickPtTextWatchRenderer",@"promoted15ClickPtTextCtdWatchRenderer",@"promotedSparklesTextCtdWatch15ClickRenderer",@"promotedSparklesTextCtdWatchGridFormRenderer",@"promotedSparklesTextCtdWatchWideFormRenderer",@"promotedSparklesTextWatch15ClickRenderer",@"promotedSparklesTextWatchGridFormRenderer",@"promotedSparklesTextWatchWideFormRenderer",@"promotedSparklesTextProductWatchRenderer",@"promotedSparklesTextHomeRenderer",@"promotedSparklesTextSearchRenderer",@"promotedSparklesTextProductHomeRenderer",@"promotedSparklesTextCtdHomeRenderer",@"promotedSparklesTextCtdHomeCompactFormRenderer",@"promotedDiscoveryAppPromoCompactFormRenderer",@"adSlotRenderer"]}; });return fields;
}
static NSDictionary *RVAdFeedChildren(void) {
    static NSDictionary *fields;static dispatch_once_t once;
    dispatch_once(&once,^{ fields=@{
        @"YTISectionListSupportedRenderers":@[@"itemSectionRenderer",@"horizontalListRenderer",@"gridRenderer",@"shelfRenderer"],
        @"YTIItemSectionSupportedRenderers":@[@"horizontalListRenderer",@"gridRenderer",@"shelfRenderer"],
        @"YTIShelfSupportedRenderers":@[@"horizontalListRenderer",@"gridRenderer"],
        @"YTISectionListRenderer":@[@"contentsArray"],
        @"YTIItemSectionRenderer":@[@"contentsArray"],
        @"YTIGridRenderer":@[@"itemsArray"],
        @"YTIHorizontalListRenderer":@[@"itemsArray"],
        @"YTIShelfRenderer":@[@"content"]}; });return fields;
}
static NSString *RVAdPresence(NSString *field) {
    return [@"has" stringByAppendingString:[[[field substringToIndex:1] uppercaseString] stringByAppendingString:[field substringFromIndex:1]]];
}
static NSString *RVAdSetter(NSString *field) {
    return [NSString stringWithFormat:@"set%@%@:",[[field substringToIndex:1] uppercaseString],[field substringFromIndex:1]];
}
static BOOL RVAdFeedKind(id item,NSString **reason) {
    if (!item) return NO;
    for (RVAdField *field in RVAdFieldAccessors[NSStringFromClass([item class])]) {
        if (((BOOL(*)(id,SEL))objc_msgSend)(item,field.selector)) { if (reason) *reason=field.name;return YES; }
    }
    return NO;
}
@interface RVAdFeedWalk : NSObject
@property(nonatomic) NSUInteger budget;
@property(nonatomic,strong) NSHashTable *visiting;
@property(nonatomic,strong) NSMapTable *copies;
@property(nonatomic,strong) RVAdDisplayState *state;
@end
@implementation RVAdFeedWalk @end
static RVAdFeedWalk *RVAdNewWalk(id owner,BOOL boundary,NSString *route) {
    RVAdFeedWalk *walk=[RVAdFeedWalk new];walk.budget=4096;
    walk.visiting=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    walk.copies=[NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory|NSPointerFunctionsObjectPointerPersonality valueOptions:NSPointerFunctionsStrongMemory];
    walk.state=RVDisplayState(owner,boundary);RVDisplayValue(walk.state,@"route",route);return walk;
}
static BOOL RVAdBareEmptySection(id model) {
    NSString *name=NSStringFromClass([model class]);
    if (![@[@"YTISectionListRenderer",@"YTIItemSectionRenderer",@"YTIGridRenderer",@"YTIHorizontalListRenderer",@"YTIShelfRenderer"] containsObject:name]) return NO;
    id data=RVObject(model,"data"),blank=RVObject([[model class] new],"data");
    return [data isKindOfClass:NSData.class] && [blank isKindOfClass:NSData.class] && [data isEqualToData:blank];
}
static id RVAdFilterModel(id model,NSUInteger depth,RVAdFeedWalk *walk);
static NSArray *RVAdFilterEntries(NSArray *entries,NSUInteger depth,RVAdFeedWalk *walk) {
    if (![entries isKindOfClass:NSArray.class]) return entries;
    if (entries.count>4096 || depth>8) { RVAdEvent(nil,@"feed_budget_passthrough",1);RVDisplayEvent(walk.state,@"budget_passthrough",1);RVDisplayValue(walk.state,@"blocked_checkpoint",@"budget_exhausted");return entries; }
    RVDisplayEvent(walk.state,@"input_entries",entries.count);
    NSMutableArray *result=[NSMutableArray arrayWithCapacity:entries.count];BOOL changed=NO;
    for (id item in entries) {
        if (!walk.budget) { RVDisplayEvent(walk.state,@"budget_passthrough",1);RVDisplayValue(walk.state,@"blocked_checkpoint",@"budget_exhausted");[result addObject:item];continue; }
        walk.budget--;NSString *reason=nil;
        if (RVAdFeedKind(item,&reason)) {
            RVAdEvent(nil,@"feed_ad_input",1);RVAdEvent(nil,@"feed_removed",1);RVAdEvent(nil,[@"feed_removed_" stringByAppendingString:reason],1);
            RVDisplayValue(walk.state,@"last_mutation",@"typed_removal");RVDisplayEvent(walk.state,@"ad_classified",1);RVDisplayEvent(walk.state,@"removed_entries",1);
            RVDisplayValue(walk.state,@"last_rule",reason);changed=YES;continue;
        }
        id updated=RVAdFilterModel(item,depth+1,walk);
        if (updated!=item && RVAdBareEmptySection(updated)) { RVAdEvent(nil,@"feed_bare_section_collapsed",1);changed=YES;continue; }
        changed|=updated!=item;[result addObject:updated];
    }
    RVDisplayEvent(walk.state,@"surviving_entries",result.count);
    return changed ? result : entries;
}
static id RVAdFilterModel(id model,NSUInteger depth,RVAdFeedWalk *walk) {
    if (!model) return model;
    NSString *name=NSStringFromClass([model class]);NSArray *fields=RVAdFeedChildren()[name];
    if (!fields) return model;
    id cached=[walk.copies objectForKey:model];if (cached) return cached;
    if (depth>8 || !walk.budget || [walk.visiting containsObject:model]) {
        RVAdEvent(nil,@"feed_budget_passthrough",1);RVDisplayEvent(walk.state,@"budget_or_cycle_passthrough",1);return model;
    }
    walk.budget--;[walk.visiting addObject:model];id result=model;
    RVDisplayEvent(walk.state,[@"container_" stringByAppendingString:name],1);
    if (![model conformsToProtocol:@protocol(NSCopying)]) { RVDisplayEvent(walk.state,@"copy_contract_missing",1);[walk.visiting removeObject:model];[walk.copies setObject:model forKey:model];return model; }
    for (NSString *field in fields) {
        BOOL repeated=[field hasSuffix:@"Array"];NSString *presence=RVAdPresence(field),*setter=RVAdSetter(field);
        if (!repeated && (!RVAdResolve(model,presence.UTF8String,@"B@:") || !RVBool(model,presence.UTF8String))) continue;
        if (!RVAdResolve(model,field.UTF8String,@"@@:") || !RVAdResolve(model,setter.UTF8String,@"v@:@")) {
            RVAdEvent(nil,@"feed_model_contract_missing",1);RVDisplayValue(walk.state,@"blocked_checkpoint",@"descriptor_missing");continue;
        }
        id child=RVObject(model,field.UTF8String);id filtered=repeated ? RVAdFilterEntries(child,depth+1,walk) : RVAdFilterModel(child,depth+1,walk);
        if (filtered==child) continue;
        if (result==model) result=[model copy];
        // Clear only a serialization-proven bare message; functional headers/tokens survive.
        if (!repeated && RVAdBareEmptySection(filtered)) filtered=nil;
        RVSetObject(result,setter.UTF8String,filtered);
    }
    [walk.visiting removeObject:model];[walk.copies setObject:result forKey:model];
    if (result!=model) { RVAdEvent(nil,@"feed_model_copied",1);RVDisplayEvent(walk.state,@"copied_models",1); }
    return result;
}
static id RVFilterModel(id owner,id model) {
    RVAdFeedWalk *walk=RVAdNewWalk(owner,YES,@"model_load");
    if (!RVEnabled(@"feed_ads")) return model;
    return RVAdFilterModel(model,0,walk);
}
static id RVAdFilterIncoming(id owner,id entries,NSString *route) {
    RVAdFeedWalk *walk=RVAdNewWalk(owner,NO,route);
    if (!RVEnabled(@"feed_ads") || ![entries isKindOfClass:NSArray.class]) return entries;
    return RVAdFilterEntries(entries,0,walk);
}
#include "RVAdDisplay.inc"
static BOOL RVAdElementReject(NSData *data) {
    if (!RVEnabled(@"feed_ads") || !RVDataMatches(data,RVSetting(@"feed_patterns"))) return NO;
    RVAdEvent(nil,@"element_pattern_candidate",1);RVDisplayEvent(nil,@"element_pattern_candidate",1);
    // Conservative compatibility guard: an enclosing ordinary-video/comment
    // payload cannot be rejected because a descendant happens to match an ad.
    if (RVDataMatches(data,@[@"home_video_with_context",@"related_video_with_context",@"comment_thread",@"|comment.",@"library_recent_shelf"])) {
        RVAdEvent(nil,@"element_protected_context",1);RVDisplayEvent(nil,@"element_protected_context",1);return NO;
    }
    return YES;
}
static void RVInstallFeed(void) {
    // Resolve descriptor-generated presence accessors once during installation.
    NSMutableDictionary *accessors=[NSMutableDictionary dictionary];
    for (NSString *name in RVAdFeedFields()) {
        Class cls=NSClassFromString(name);id sample=cls ? [cls new] : nil;
        NSMutableArray *ready=[NSMutableArray array];
        for (NSString *field in RVAdFeedFields()[name]) {
            NSString *presence=RVAdPresence(field);
            if (RVAdResolve(sample,presence.UTF8String,@"B@:")) {
                RVAdField *entry=[RVAdField new];entry.name=field;entry.selector=NSSelectorFromString(presence);[ready addObject:entry];
            }
        }
        accessors[name]=ready.copy;NSUInteger resolved=ready.count;
        RVAdEvent(nil,@"feed_presence_accessors_resolved",resolved);
        @synchronized(RVAdPlaybackState.class) { RVAdHooks[[@"feed_fields " stringByAppendingString:name]]=@{ @"resolved":@(resolved),@"expected":@([RVAdFeedFields()[name] count])}; }
    }
    RVAdFieldAccessors=accessors.copy;RVInstallDisplayAds();
    RVAdHook(@"YTIElementRenderer",@"elementData",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id data=((id(*)(id,SEL))original)(object,sel);
            if (![data isKindOfClass:NSData.class] || [data length]>2*1024*1024) return data;
            static _Thread_local BOOL makingEmpty;if (makingEmpty) return data;
            BOOL ad=RVAdElementReject(data);
            BOOL reject=ad || (RVEnabled(@"hide_shorts") && RVDataMatches(data,RVSetting(@"shorts_patterns"))) || RVExtraReject(data);
            if (!reject) return RVVoteNormalizeElementData(data);
            id result=nil;makingEmpty=YES;
            @try { result=RVObject(RVObject(NSClassFromString(@"YTIElementRenderer"),"emptyCellElementRenderer"),"elementData"); }
            @finally { makingEmpty=NO; }
            if (![result isKindOfClass:NSData.class]) { if (ad) RVAdEvent(nil,@"element_empty_contract_missing",1);return RVVoteNormalizeElementData(data); }
            if (ad) { RVAdEvent(nil,@"element_empty_substitution",1);RVDisplayEvent(nil,@"element_empty_substitution",1); }return result;
        };
    });
    for (NSString *cls in @[@"YTSectionListViewController",@"YTInnerTubeCollectionViewController"]) RVAdHook(cls,@"loadWithModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id model) {
            RVAdEvent(nil,@"feed_initial_load",1);id filtered=RVFilterModel(controller,model);
            ((void(*)(id,SEL,id))original)(controller,sel,filtered);RVAdEvent(nil,@"feed_initial_load_completed",1);RVDisplayEvent(RVDisplayState(controller,NO),@"original_load_forwarded",1);RVDisplayCapture(RVDisplayState(controller,NO),@"model_load");
        };
    });
    RVAdHook(@"YTInnerTubeCollectionViewController",@"addSectionsFromArray:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id entries) {
            id filtered=RVAdFilterIncoming(controller,entries,@"append_sections");
            ((void(*)(id,SEL,id))original)(controller,sel,filtered);RVAdEvent(nil,@"feed_append_completed",1);
            RVDisplayEvent(RVDisplayState(controller,NO),@"original_append_forwarded",1);RVDisplayCapture(RVDisplayState(controller,NO),@"append_sections");
        };
    });
    RVAdHook(@"YTInnerTubeCollectionViewController",@"insertSections:byPosition:error:",@"B@:@i^@",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id controller,id entries,int position,id __autoreleasing *error) {
            id filtered=RVAdFilterIncoming(controller,entries,@"insert_sections");
            BOOL result=((BOOL(*)(id,SEL,id,int,id __autoreleasing *))original)(controller,sel,filtered,position,error);
            RVDisplayEvent(RVDisplayState(controller,NO),@"original_insert_sections_forwarded",1);
            RVDisplayValue(RVDisplayState(controller,NO),@"native_insert_succeeded",@(result));RVDisplayCapture(RVDisplayState(controller,NO),@"insert_sections");return result;
        };
    });
    RVAdHook(@"YTInnerTubeCollectionViewController",@"insertContents:byPosition:inSection:error:",@"@@:@i@^@",NO,^id(IMP original,SEL sel) {
        return ^id(id controller,id entries,int position,id section,id __autoreleasing *error) {
            id filtered=RVAdFilterIncoming(controller,entries,@"insert_contents");
            id result=((id(*)(id,SEL,id,int,id,id __autoreleasing *))original)(controller,sel,filtered,position,section,error);
            RVDisplayEvent(RVDisplayState(controller,NO),@"original_insert_contents_forwarded",1);
            RVDisplayValue(RVDisplayState(controller,NO),@"native_insert_result_present",@(result!=nil));RVDisplayCapture(RVDisplayState(controller,NO),@"insert_contents");return result;
        };
    });
    RVAdHook(@"YTInnerTubeCollectionViewController",@"replaceEntry:withReplacementRenderer:inSectionController:undoKey:operation:",@"@@:@@@@@",NO,^id(IMP original,SEL sel) {
        return ^id(id controller,id entry,id replacement,id section,id undo,id operation) {
            RVAdFeedWalk *walk=RVAdNewWalk(controller,NO,@"replace_entry");NSString *reason=nil;id filtered=replacement;
            if (RVEnabled(@"feed_ads")) {
                if (RVAdFeedKind(replacement,&reason)) { RVDisplayEvent(walk.state,@"unsupported_ad_replacement",1);RVDisplayValue(walk.state,@"blocked_checkpoint",@"unsupported_replacement"); }
                else filtered=RVAdFilterModel(replacement,0,walk);
            }
            // Do not invent native undo/removal results for an advertising root.
            id result=((id(*)(id,SEL,id,id,id,id,id))original)(controller,sel,entry,filtered,section,undo,operation);
            RVDisplayEvent(walk.state,@"original_replace_forwarded",1);RVDisplayCapture(walk.state,@"replace_entry");return result;
        };
    });
    for (NSString *cls in @[@"YTSectionListViewController",@"YTInnerTubeCollectionViewController"]) RVAdHook(cls,@"viewDidAppear:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id controller,BOOL animated) {
            ((void(*)(id,SEL,BOOL))original)(controller,sel,animated);
            RVDisplayCapture(RVDisplayState(controller,NO),@"surface_appeared");
        };
    });
    RVAdHook(@"YTReelContentModel",@"makeContentModelForEntry:",@"@@:@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id entry) {
            id model=((id(*)(id,SEL,id))original)(cls,sel,entry);
            if (!RVEnabled(@"shorts_ads") || !model) return model;
            Class reel=NSClassFromString(@"YTReelModel");RVAdEvent(nil,@"shorts_entry_seen",1);
            if (!reel || ![model isKindOfClass:reel] || !RVCan(model,"isAdVideo",@"B@:")) { RVAdEvent(nil,@"shorts_unknown_entry_passthrough",1);return model; }
            if (!RVBool(model,"isAdVideo")) { RVAdEvent(nil,@"shorts_content_surviving",1);return model; }
            RVAdEvent(nil,@"shorts_ad_input",1);RVAdEvent(nil,@"shorts_ad_skipped",1);return nil;
        };
    });
}
