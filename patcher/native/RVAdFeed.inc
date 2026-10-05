// SPDX-License-Identifier: MIT
// Explicit, profile-proven GPB renderer fields. No generic 'ad' substring rule.
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
        @"YTIItemSectionSupportedRenderers":@[@"promotedVideoRenderer",@"compactPromotedVideoRenderer",
            @"compactPromotedBannerRenderer",@"compactPromotedItemRenderer",@"promotedAppInstallRenderer",
            @"promotedTextBannerRenderer",@"promoted15ClickTextWatchRenderer",@"promoted15ClickTextCtdWatchRenderer",
            @"promoted15ClickPtTextWatchRenderer",@"promoted15ClickPtTextCtdWatchRenderer",
            @"promotedSparklesTextCtdWatch15ClickRenderer",@"promotedSparklesTextCtdWatchGridFormRenderer",
            @"promotedSparklesTextCtdWatchWideFormRenderer",@"promotedSparklesTextWatch15ClickRenderer",
            @"promotedSparklesTextWatchGridFormRenderer",@"promotedSparklesTextWatchWideFormRenderer",
            @"promotedVideoInlineMutedRenderer",@"promotedSparklesTextProductWatchRenderer",
            @"promotedSparklesTextHomeRenderer",@"promotedSparklesTextSearchRenderer",@"adsWebViewRenderer",
            @"promotedSparklesTextProductHomeRenderer",@"promotedSparklesTextCtdHomeRenderer",
            @"promotedSparklesTextCtdHomeCompactFormRenderer",@"promotedDiscoveryAppPromoCompactFormRenderer",@"adSlotRenderer"]}; });
    return fields;
}
static NSString *RVAdPresence(NSString *field) {
    return [@"has" stringByAppendingString:[[[field substringToIndex:1] uppercaseString] stringByAppendingString:[field substringFromIndex:1]]];
}
#include "RVShortsLayout.inc"
static BOOL RVAdFeedKind(id item,NSString **reason) {
    NSArray *fields=RVAdFieldAccessors[NSStringFromClass([item class])];
    for (RVAdField *field in fields) {
        // Exact receiver class and ABI were checked once at installation.
        if (((BOOL(*)(id,SEL))objc_msgSend)(item,field.selector)) {
            if (reason) *reason=field.name;return YES;
        }
    }
    return NO;
}
static BOOL RVAdBareEmptySection(id section) {
    Class cls=NSClassFromString(@"YTIItemSectionRenderer");
    if (!cls || ![section isKindOfClass:cls]) return NO;
    id contents=RVAdResolvedObject(section,"contentsArray");if (![contents isKindOfClass:NSArray.class] || [contents count]) return NO;
    // Comparing the complete serialization also preserves unknown fields,
    // continuations, headers and metadata; only a genuinely bare shell collapses.
    id data=RVObject(section,"data"),blank=RVObject([cls new],"data");
    return [data isKindOfClass:NSData.class] && [blank isKindOfClass:NSData.class] && [data isEqualToData:blank];
}
static id RVAdFilterModel(id model,NSUInteger depth,NSUInteger *budget,NSHashTable *seen);
static NSArray *RVAdFilterEntries(NSArray *entries,NSUInteger depth,NSUInteger *budget,NSHashTable *seen) {
    if (![entries isKindOfClass:NSArray.class] || entries.count>4096 || depth>8) { RVAdEvent(nil,@"feed_budget_passthrough",1);return entries; }
    NSMutableArray *result=[NSMutableArray arrayWithCapacity:entries.count];BOOL changed=NO;
    for (id item in entries) {
        if (!*budget) { RVAdEvent(nil,@"feed_budget_passthrough",1);[result addObject:item];continue; }
        --*budget;NSString *reason=nil;
        NSString *shorts=RVShortsRowKind(item);
        if (shorts) {
            RVShortsEvent(@"feed_row_removed");RVShortsEvent([@"feed_removed_" stringByAppendingString:shorts]);changed=YES;continue;
        }
        if (RVEnabled(@"feed_ads") && RVAdFeedKind(item,&reason)) {
            RVAdEvent(nil,@"feed_ad_input",1);RVAdEvent(nil,@"feed_removed",1);
            RVAdEvent(nil,[@"feed_removed_" stringByAppendingString:reason],1);changed=YES;continue;
        }
        id updated=item;
        if (RVAdFeedFields()[NSStringFromClass([item class])]) {
            if (RVAdResolve(item,"hasItemSectionRenderer",@"B@:") && RVBool(item,"hasItemSectionRenderer")) {
                id section=RVAdResolvedObject(item,"itemSectionRenderer");
                id filtered=RVAdFilterModel(section,depth+1,budget,seen);
                if (filtered!=section && [item conformsToProtocol:@protocol(NSCopying)] && RVAdResolve(item,"setItemSectionRenderer:",@"v@:@")) {
                    if (RVAdBareEmptySection(filtered)) { RVAdEvent(nil,@"feed_bare_section_collapsed",1);changed=YES;continue; }
                    updated=[item copy];RVSetObject(updated,"setItemSectionRenderer:",filtered);
                }
            }
        } else {
            updated=RVAdFilterModel(item,depth+1,budget,seen);
            if (updated!=item && RVAdBareEmptySection(updated)) { RVAdEvent(nil,@"feed_bare_section_collapsed",1);changed=YES;continue; }
        }
        changed|=updated!=item;[result addObject:updated];
    }
    return changed ? result : entries;
}
static id RVAdFilterModel(id model,NSUInteger depth,NSUInteger *budget,NSHashTable *seen) {
    Class section=NSClassFromString(@"YTISectionListRenderer"),itemSection=NSClassFromString(@"YTIItemSectionRenderer");
    if (!model || !((section && [model isKindOfClass:section]) || (itemSection && [model isKindOfClass:itemSection]))) return model;
    if (depth>8 || !*budget || [seen containsObject:model]) { RVAdEvent(nil,@"feed_budget_passthrough",1);return model; }
    [seen addObject:model];
    id contents=RVAdResolvedObject(model,"contentsArray");
    if (![contents isKindOfClass:NSArray.class] || ![model conformsToProtocol:@protocol(NSCopying)] || !RVAdResolve(model,"setContentsArray:",@"v@:@")) {
        RVAdEvent(nil,@"feed_model_contract_missing",1);return model;
    }
    NSArray *filtered=RVAdFilterEntries(contents,depth,budget,seen);
    if (filtered==contents) return model;
    id copy=[model copy];RVSetObject(copy,"setContentsArray:",filtered);RVAdEvent(nil,@"feed_model_copied",1);return copy;
}
static id RVFilterModel(id model) {
    if (!RVEnabled(@"feed_ads") && !RVEnabled(@"hide_shorts")) return model;
    NSUInteger budget=4096;NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    return RVAdFilterModel(model,0,&budget,seen);
}
static BOOL RVAdElementReject(NSData *data) {
    if (!RVEnabled(@"feed_ads") || !RVDataMatches(data,RVSetting(@"feed_patterns"))) return NO;
    RVAdEvent(nil,@"element_pattern_candidate",1);
    // Conservative compatibility guard: an enclosing ordinary-video/comment
    // payload cannot be rejected because a descendant happens to match an ad.
    if (RVDataMatches(data,@[@"home_video_with_context",@"related_video_with_context",@"comment_thread",@"|comment.",@"library_recent_shelf"])) {
        RVAdEvent(nil,@"element_protected_context",1);return NO;
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
    RVAdFieldAccessors=accessors.copy;
    RVAdHook(@"YTIElementRenderer",@"elementData",@"@@:",NO,^id(IMP original,SEL sel) {
        RVShortsElementDataOriginal=original;
        return ^id(id object) {
            id data=((id(*)(id,SEL))original)(object,sel);
            if (![data isKindOfClass:NSData.class] || [data length]>2*1024*1024) return data;
            static _Thread_local BOOL makingEmpty;if (makingEmpty) return data;
            BOOL ad=RVAdElementReject(data);
            NSString *shorts=RVShortsDataKind(data);
            BOOL reject=ad || shorts || RVExtraReject(data);
            if (!reject) return RVVoteNormalizeElementData(data);
            id result=nil;makingEmpty=YES;
            @try { result=RVObject(RVObject(NSClassFromString(@"YTIElementRenderer"),"emptyCellElementRenderer"),"elementData"); }
            @finally { makingEmpty=NO; }
            if (![result isKindOfClass:NSData.class]) { if (ad) RVAdEvent(nil,@"element_empty_contract_missing",1);return RVVoteNormalizeElementData(data); }
            if (ad) RVAdEvent(nil,@"element_empty_substitution",1);
            if (shorts) RVShortsEvent([@"element_empty_" stringByAppendingString:shorts]);return result;
        };
    });
    for (NSString *cls in @[@"YTSectionListViewController",@"YTInnerTubeCollectionViewController"]) RVAdHook(cls,@"loadWithModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id model) {
            RVAdEvent(nil,@"feed_initial_load",1);RVShortsEvent(@"feed_initial_load_observed");id filtered=RVFilterModel(model);
            ((void(*)(id,SEL,id))original)(controller,sel,filtered);RVAdEvent(nil,@"feed_initial_load_completed",1);
        };
    });
    RVAdHook(@"YTInnerTubeCollectionViewController",@"addSectionsFromArray:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id entries) {
            id filtered=entries;
            if ((RVEnabled(@"feed_ads") || RVEnabled(@"hide_shorts")) && [entries isKindOfClass:NSArray.class]) {
                NSUInteger budget=4096;NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
                RVAdEvent(nil,@"feed_append_input",[entries count]);RVShortsEvent(@"feed_append_observed");filtered=RVAdFilterEntries(entries,0,&budget,seen);
                RVAdEvent(nil,@"feed_append_surviving",[filtered count]);
            }
            ((void(*)(id,SEL,id))original)(controller,sel,filtered);RVAdEvent(nil,@"feed_append_completed",1);
        };
    });
    RVAdHook(@"YTInnerTubeCollectionViewController",@"sectionControllersForSectionRenderers:reloadingSectionControllerByRenderer:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id entries,id reload) {
            id filtered=entries;
            if (RVEnabled(@"hide_shorts") && [entries isKindOfClass:NSArray.class]) {
                NSUInteger budget=4096;NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
                RVShortsEvent(@"feed_section_construction_observed");filtered=RVAdFilterEntries(entries,0,&budget,seen);
            }
            return ((id(*)(id,SEL,id,id))original)(object,sel,filtered,reload);
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
