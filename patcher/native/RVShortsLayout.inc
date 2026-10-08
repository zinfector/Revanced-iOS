// SPDX-License-Identifier: MIT
// Shorts row ownership and native pivot model compaction.
static IMP RVShortsElementDataOriginal;
static NSMutableDictionary *RVShortsCounts,*RVShortsState,*RVShortsHooks;
static NSHashTable<UIView *> *RVShortsPivotViews;
static void RVShortsEvent(NSString *name) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(NSClassFromString(@"YTIPivotBarRenderer")) {
        if (!RVShortsCounts) RVShortsCounts=[NSMutableDictionary dictionary];
        RVShortsCounts[name]=@([RVShortsCounts[name] unsignedLongLongValue]+1);
    }
}
static void RVShortsValue(NSString *key,id value) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(NSClassFromString(@"YTIPivotBarRenderer")) {
        if (!RVShortsState) RVShortsState=[NSMutableDictionary dictionary];
        RVShortsState[key]=value;
    }
}
static BOOL RVShortsIdentifier(id value) {
    return [value isKindOfClass:NSString.class] && [@[@"FEshorts",@"FEshorts_tab"] containsObject:value];
}
static BOOL RVShortsVarint(const uint8_t *bytes,NSUInteger length,NSUInteger *offset,uint64_t *value) {
    uint64_t v=0;
    for (unsigned int i=0;i<10 && *offset<length;i++) {
        uint8_t b=bytes[(*offset)++];if (i==9 && b>1) return NO;
        v|=(uint64_t)(b&127)<<(7*i);if (!(b&128)) { *value=v;return YES; }
    }return NO;
}
static BOOL RVShortsIdentityText(const uint8_t *bytes,NSUInteger length,NSUInteger depth,NSUInteger *fields,NSUInteger *byteBudget) {
    if (depth>12 || !length || length>*byteBudget) return NO;*byteBudget-=length;
    NSUInteger offset=0;
    while (offset<length && *fields) {
        --*fields;uint64_t tag=0,value=0;
        if (!RVShortsVarint(bytes,length,&offset,&tag) || !(tag>>3)) return NO;
        switch (tag&7) {
            case 0: if (!RVShortsVarint(bytes,length,&offset,&value)) return NO;break;
            case 1: if (length-offset<8) return NO;offset+=8;break;
            case 5: if (length-offset<4) return NO;offset+=4;break;
            case 2: {
                if (!RVShortsVarint(bytes,length,&offset,&value) || value>length-offset) return NO;
                const uint8_t *p=bytes+offset;NSUInteger n=(NSUInteger)value;
                if ((n==6 && !memcmp(p,"Shorts",6)) || (n==8 && !memcmp(p,"FEshorts",8)) ||
                    (n==12 && !memcmp(p,"FEshorts_tab",12))) return YES;
                if (RVShortsIdentityText(p,n,depth+1,fields,byteBudget)) return YES;
                offset+=n;break;
            }
            default:return NO;
        }
    }return NO;
}
// Descriptor-proven primary command only. Descendant menus are never searched.
static NSData *RVShortsWirePath(NSData *data,NSArray<NSNumber *> *path) {
    for (NSNumber *number in path) {
        RVVoteField *field=RVVoteOne(RVVoteFields(data),number.unsignedIntValue,2);
        if (!field) return nil;data=field.value;
    }return data;
}
static NSString *RVShortsWireText(NSData *data) {
    return data.length && data.length<=512 ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
}
static BOOL RVShortsReelCommand(NSData *command,NSString *expectedVideo) {
    for (NSUInteger depth=0;depth<=6;depth++) {
        NSArray *fields=RVVoteFields(command);RVVoteField *extension=nil;
        if (!fields) return NO;
        for (RVVoteField *field in fields) if (field.number>=1000000) {
            if (extension || field.wire!=2) return NO;extension=field;
        }
        if (!extension) return NO;
        if (extension.number==139608561) {
            NSString *video=RVShortsWireText(RVShortsWirePath(extension.value,@[@1]));
            return RVVideoIDValid(video) && (!expectedVideo || [expectedVideo isEqual:video]);
        }
        if (depth==6 || (extension.number!=462702848 && extension.number!=200453700)) return NO;
        command=RVShortsWirePath(extension.value,@[@1]);if (!command) return NO;
    }return NO;
}
static BOOL RVShortsPrimaryReelCard(NSData *data) {
    static NSCache *cache;static dispatch_once_t once;dispatch_once(&once,^{
        cache=[NSCache new];cache.countLimit=128;cache.totalCostLimit=4*1024*1024;
    });
    NSNumber *cached=[cache objectForKey:data];if (cached) return cached.boolValue;
    NSData *component=RVShortsWirePath(data,@[@1,@168777401]);
    NSString *uri=RVShortsWireText(RVShortsWirePath(component,@[@3,@172035250,@1]));
    // Exact resource basename, independent of path and revision separator.
    static NSRegularExpression *regex;static dispatch_once_t patternOnce;
    dispatch_once(&patternOnce,^{regex=[NSRegularExpression regularExpressionWithPattern:@"^(video_lockup_with_attachment|video_card|video_card_content)\\.eml-fe[_|][0-9a-fA-F]{1,16}$" options:0 error:nil];});
    NSString *leaf=[uri.lastPathComponent stringByRemovingPercentEncoding];
    BOOL family=leaf && [regex firstMatchInString:leaf options:0 range:NSMakeRange(0,leaf.length)]!=nil;
    NSData *model=family ? RVShortsWirePath(component,@[@5]) : nil;
    NSData *command=RVShortsWirePath(model,@[@232954548,@18,@4,@169495254]);
    NSArray *presentation=RVVoteFields(RVShortsWirePath(model,@[@232954548,@18,@1]));
    RVVoteField *identity=RVVoteOne(presentation,7,2);
    BOOL identityValid=RVVoteSingularFields(presentation,@[@7]);
    for (RVVoteField *field in presentation) if (field.number==7 && field.wire!=2) identityValid=NO;
    NSString *video=RVShortsWireText(identity.value);
    BOOL reel=command && identityValid && (!identity || RVVideoIDValid(video)) && RVShortsReelCommand(command,video);
    [cache setObject:@(reel) forKey:data.copy cost:data.length];return reel;
}
static NSString *RVShortsDataKind(NSData *data) {
    if (!RVEnabled(@"hide_shorts") || ![data isKindOfClass:NSData.class] || !data.length || data.length>2*1024*1024) return nil;
    if (RVShortsPrimaryReelCard(data)) return @"shorts_primary_reel_card";
    // Never drop a mixed parent because one descendant happens to be Shorts.
    if (RVDataMatches(data,@[@"home_video_with_context",@"related_video_with_context",@"comment_thread",@"|comment.",@"library_recent_shelf"])) {
        RVShortsEvent(@"protected_parent_preserved");return nil;
    }
    if (RVDataMatches(data,RVSetting(@"shorts_patterns"))) return @"shorts_element";
    if (RVDataMatches(data,@[@"shelf_header.e",@"shelf_header.eml",@"modern_type_shelf_header_content.e"])) {
        NSUInteger fields=1024,bytes=4*1024*1024;
        if (RVShortsIdentityText(data.bytes,data.length,0,&fields,&bytes)) return @"shorts_header";
        RVShortsEvent(@"generic_header_preserved");
    }
    return nil;
}
static id RVShortsChild(id object,NSString *field) {
    NSString *presence=RVAdPresence(field);
    if (!RVAdResolve(object,presence.UTF8String,@"B@:") || !RVBool(object,presence.UTF8String)) return nil;
    return RVAdResolvedObject(object,field.UTF8String);
}
static NSString *RVShortsRowKindAt(id row,NSUInteger depth,NSUInteger *budget) {
    if (!RVEnabled(@"hide_shorts") || !row || depth>8 || !*budget) return nil;--*budget;
    NSString *name=NSStringFromClass([row class]);
    if ([name isEqual:@"YTIElementRenderer"]) {
        id data=RVShortsElementDataOriginal && RVCan(row,"elementData",@"@@:") ?
            ((id(*)(id,SEL))RVShortsElementDataOriginal)(row,sel_registerName("elementData")) : nil;
        return RVShortsDataKind(data);
    }
    if ([name isEqual:@"YTIReelItemRenderer"]) return @"shorts_reel_item";
    if ([@[@"YTIVideoRenderer",@"YTICompactVideoRenderer"] containsObject:name]) {
        id command=RVShortsChild(row,@"navigationEndpoint");
        NSString *video=RVAdResolvedObject(row,"videoId");
        NSData *data=RVAdResolvedObject(command,"data");
        return RVVideoIDValid(video) && RVShortsReelCommand(data,video) ? @"shorts_typed_video" : nil;
    }
    if ([@[@"YTISectionListSupportedRenderers",@"YTIItemSectionSupportedRenderers",@"YTIHorizontalListSupportedRenderers",
        @"YTIGridSupportedRenderers",@"YTIVerticalListSupportedRenderers",@"YTIExpandedShelfContentsSupportedRenderers",@"YTIRenderer"] containsObject:name]) {
        for (NSString *field in @[@"elementRenderer",@"reelItemRenderer",@"reelGridRenderer",@"videoRenderer",@"compactVideoRenderer",@"shelfRenderer",@"itemSectionRenderer"] ) {
            id child=RVShortsChild(row,field);if (!child) continue;
            return RVShortsRowKindAt(child,depth+1,budget);
        }return nil;
    }
    if ([name isEqual:@"YTIShelfRenderer"]) {
        id content=RVShortsChild(row,@"content");
        return RVShortsRowKindAt(content,depth+1,budget) ? @"shorts_shelf" : nil;
    }
    if ([name isEqual:@"YTIShelfSupportedRenderers"]) {
        for (NSString *field in @[@"horizontalListRenderer",@"gridRenderer",@"expandedShelfContentsRenderer",@"verticalListRenderer"]) {
            id child=RVShortsChild(row,field);if (child) return RVShortsRowKindAt(child,depth+1,budget);
        }return nil;
    }
    BOOL section=[@[@"YTIItemSectionRenderer",@"YTISectionListRenderer"] containsObject:name];
    BOOL list=[@[@"YTIHorizontalListRenderer",@"YTIGridRenderer",@"YTIVerticalListRenderer",@"YTIExpandedShelfContentsRenderer",@"YTIReelGridRenderer"] containsObject:name];
    if (!section && !list) return nil;
    NSArray *entries=RVAdResolvedObject(row,section ? "contentsArray" : "itemsArray");
    if (![entries isKindOfClass:NSArray.class] || !entries.count || entries.count>256) return nil;
    BOOL body=NO;
    for (id child in entries) {
        NSString *kind=RVShortsRowKindAt(child,depth+1,budget);if (!kind) return nil;
        if (![kind isEqual:@"shorts_header"]) body=YES;
    }
    // A header alone is insufficient proof to remove enclosing continuations.
    return body ? @"shorts_section" : nil;
}
static NSString *RVShortsRowKind(id row) { NSUInteger budget=512;return RVShortsRowKindAt(row,0,&budget); }

static const void *RVShortsPivotLeaseKey=&RVShortsPivotLeaseKey;
@interface RVShortsPivotLease : NSObject
@property(nonatomic,strong) NSArray *original;
@property(nonatomic,strong) NSArray *installed;
@property(nonatomic,strong) NSArray *snapshot;
@end
@implementation RVShortsPivotLease @end
static NSString *RVShortsPivotID(id item) {
    id native=RVAdResolvedObject(item,"yt_pivotBarItem");
    id identifier=RVObject(native,"pivotIdentifier");
    return [identifier isKindOfClass:NSString.class] ? identifier : nil;
}
#include "RVNavigation.inc"
static BOOL RVShortsPreparePivot(id renderer) {
    if (!NSThread.isMainThread || ![renderer isKindOfClass:NSClassFromString(@"YTIPivotBarRenderer")]) return NO;
    NSArray *current=RVAdResolvedObject(renderer,"itemsArray");
    if (![current isKindOfClass:NSArray.class] || current.count>32 || !RVAdResolve(renderer,"setItemsArray:",@"v@:@")) {
        RVShortsValue(@"pivot_checkpoint",@"items_contract_missing");return NO;
    }
    RVShortsPivotLease *lease=objc_getAssociatedObject(renderer,RVShortsPivotLeaseKey);
    if (lease && (current!=lease.installed || ![current isEqual:lease.snapshot])) {
        objc_setAssociatedObject(renderer,RVShortsPivotLeaseKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        lease=nil;RVShortsEvent(@"pivot_lease_conflict");
    }
    RVShortsValue(@"observed_pivots",RVNavigationIdentifiers(current));
    RVShortsValue(@"navigation_filter_enabled",@(RVNavigationFilteringEnabled()));
    if (!RVNavigationFilteringEnabled()) {
        if (!lease) { RVShortsValue(@"pivot_checkpoint",@"navigation_filter_disabled");return NO; }
        RVSetObject(renderer,"setItemsArray:",[lease.original mutableCopy]);
        objc_setAssociatedObject(renderer,RVShortsPivotLeaseKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        RVShortsEvent(@"pivot_restored");RVShortsValue(@"pivot_checkpoint",@"restored");return YES;
    }
    // Rebuild from the owned original when settings change; never filter an
    // already-compacted array and lose tabs needed for later restoration.
    NSArray *source=lease ? lease.original : current;
    NSMutableArray *filtered=[NSMutableArray array];
    for (id item in source) if (!RVNavigationIdentifierHidden(RVShortsPivotID(item))) [filtered addObject:item];
    // Icon-only creation entries are actions, not navigable content tabs.
    if (!RVNavigationContentIdentifiers(filtered).count && RVNavigationContentIdentifiers(source).count) {
        NSString *fallback=RVNavigationFallback(RVNavigationContentIdentifiers(source));
        [filtered removeAllObjects];
        for (id item in source) if (!RVNavigationIdentifierHidden(RVShortsPivotID(item)) || [RVShortsPivotID(item) isEqual:fallback]) [filtered addObject:item];
        RVShortsEvent(@"last_content_tab_retained");RVShortsValue(@"safety_retained_pivot",fallback);
    }
    if ([filtered isEqual:current]) { RVShortsValue(@"pivot_checkpoint",lease ? @"native_model_already_compacted" : @"no_matching_hidden_tab");return NO; }
    if (!filtered.count) { RVShortsValue(@"pivot_checkpoint",@"no_surviving_tab");return NO; }
    lease=[RVShortsPivotLease new];lease.original=source.copy;
    RVSetObject(renderer,"setItemsArray:",filtered);
    lease.installed=RVAdResolvedObject(renderer,"itemsArray");lease.snapshot=lease.installed.copy;
    if (![lease.installed isKindOfClass:NSArray.class] || ![lease.installed isEqual:filtered]) {
        RVShortsValue(@"pivot_checkpoint",@"items_write_not_applied");return NO;
    }
    objc_setAssociatedObject(renderer,RVShortsPivotLeaseKey,lease,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    RVShortsEvent(@"pivot_compacted");RVShortsValue(@"pivot_input_count",@(current.count));
    RVShortsValue(@"pivot_output_count",@(filtered.count));RVShortsValue(@"retained_pivots",RVNavigationIdentifiers(filtered));
    RVShortsValue(@"pivot_checkpoint",@"native_model_compacted");return YES;
}
static void RVShortsRestoreSelection(UIView *view) {
    id controller=RVObject(view,"delegate");
    if (![controller isKindOfClass:NSClassFromString(@"YTPivotBarViewController")]) return;
    NSString *selected=RVObject(controller,"selectedPivotIdentifier");
    NSArray *entries=RVAdResolvedObject(RVObject(view,"renderer"),"itemsArray");
    if (![entries isKindOfClass:NSArray.class] || entries.count>32) return;
    NSMutableArray *identifiers=[NSMutableArray array];
    for (id item in entries) { NSString *value=RVShortsPivotID(item);if (value.length) [identifiers addObject:value]; }
    if (selected.length && [identifiers containsObject:selected]) {
        RVSetObject(view,"selectItemWithPivotIdentifier:",selected);RVShortsEvent(@"pivot_selection_preserved");
    } else if (selected.length && (RVNavigationFilteringEnabled() || [RVSetting(@"start_page") length])) {
        NSString *fallback=RVNavigationFallback(identifiers);
        if (fallback && RVCan(controller,"selectItemWithPivotIdentifier:",@"v@:@")) {
            RVSetObject(controller,"selectItemWithPivotIdentifier:",fallback);RVShortsEvent(@"pivot_removed_selection_recovered");
        }
    }
}
static void RVShortsRefreshNavigation(void) {
    if (!NSThread.isMainThread) { dispatch_async(dispatch_get_main_queue(),^{RVShortsRefreshNavigation();});return; }
    NSArray *views=RVShortsPivotViews.allObjects;
    NSHashTable *changed=[NSHashTable hashTableWithOptions:NSPointerFunctionsStrongMemory|NSPointerFunctionsObjectPointerPersonality];
    for (UIView *view in views) {
        id renderer=RVObject(view,"renderer");
        if (RVShortsPreparePivot(renderer)) [changed addObject:renderer];
    }
    // Cached views can share a renderer. Rebuild every consumer of a changed
    // model, even after the first consumer has already installed/restored it.
    for (UIView *view in views) {
        id renderer=RVObject(view,"renderer");
        if ([changed containsObject:renderer]) RVSetObject(view,"setRenderer:",renderer);
        RVShortsRestoreSelection(view);
    }
    RVNavigationRefreshLabels();
}
static const void *RVShortsPivotRepairKey=&RVShortsPivotRepairKey;
static void RVShortsObservePivotLayout(UIView *view) {
    if (!NSThread.isMainThread) return;
    [RVShortsPivotViews addObject:view];
    if (!RVNavigationFilteringEnabled() || objc_getAssociatedObject(view,RVShortsPivotRepairKey)) return;
    NSArray *items=RVAdResolvedObject(RVObject(view,"renderer"),"itemsArray");
    NSArray *views=RVObject(view,"itemViews");BOOL stale=NO;
    if ([items isKindOfClass:NSArray.class] && items.count<=32)
        for (id item in items) stale|=RVNavigationIdentifierHidden(RVShortsPivotID(item));
    if ([views isKindOfClass:NSArray.class] && views.count<=6)
        for (UIView *item in views) if ([item isKindOfClass:UIView.class] && !item.hidden)
            stale|=RVNavigationIdentifierHidden(RVObject(item,"pivotIdentifier"));
    if (!stale) return;
    objc_setAssociatedObject(view,RVShortsPivotRepairKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    __weak UIView *weakView=view;
    dispatch_async(dispatch_get_main_queue(),^{
        UIView *current=weakView;if (!current) return;
        objc_setAssociatedObject(current,RVShortsPivotRepairKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        if (!RVNavigationFilteringEnabled()) return;
        id renderer=RVObject(current,"renderer");
        BOOL changed=RVShortsPreparePivot(renderer);
        // Native setRenderer hides unused views and redistributes their widths.
        // Unknown/unfilterable models must not cause an endless layout loop.
        NSArray *entries=RVAdResolvedObject(renderer,"itemsArray");BOOL hiddenRemains=NO;
        if (![entries isKindOfClass:NSArray.class] || entries.count>32) return;
        for (id item in entries) hiddenRemains|=RVNavigationIdentifierHidden(RVShortsPivotID(item));
        if (renderer && (changed || !hiddenRemains)) {
            RVSetObject(current,"setRenderer:",renderer);RVShortsEvent(@"pivot_layout_repaired");
        }
    });
}
static void RVShortsInstall(void) {
    RVShortsHooks=[NSMutableDictionary dictionary];RVShortsPivotViews=[NSHashTable weakObjectsHashTable];
    RVNavigationInstallLabels();
    RVShortsHooks[@"pivot_renderer"]=@(RVHook(@"YTPivotBarView",@"setRenderer:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id renderer) {
            if (NSThread.isMainThread && [object isKindOfClass:UIView.class]) RVShortsPreparePivot(renderer);
            ((void(*)(id,SEL,id))original)(object,sel,renderer);
            if (NSThread.isMainThread && [object isKindOfClass:UIView.class]) {
                [RVShortsPivotViews addObject:object];RVShortsRestoreSelection(object);
            }
        };
    }));
    RVShortsHooks[@"pivot_appearance"]=@(RVHook(@"YTPivotBarViewController",@"viewDidAppear:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id object,BOOL animated) {
            ((void(*)(id,SEL,BOOL))original)(object,sel,animated);
            UIView *view=RVObject(object,"pivotBarView");
            if (NSThread.isMainThread && [view isKindOfClass:UIView.class]) {
                [RVShortsPivotViews addObject:view];RVShortsRefreshNavigation();
            }
        };
    }));
    RVShortsHooks[@"pivot_layout_observation"]=@(RVHook(@"YTPivotBarView",@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) {
            ((void(*)(id,SEL))original)(object,sel);
            RVShortsObservePivotLayout(object);
            if (!NSThread.isMainThread || !RVEnabled(@"diagnostics")) return;
            NSArray *views=RVObject(object,"itemViews");if (![views isKindOfClass:NSArray.class] || views.count>6) return;
            NSMutableArray *visible=[NSMutableArray array];
            for (UIView *view in views) if ([view isKindOfClass:UIView.class] && !view.hidden) {
                CGRect r=view.frame;if (!isfinite(r.origin.x) || !isfinite(r.origin.y) || !isfinite(r.size.width) || !isfinite(r.size.height)) continue;
                NSString *identifier=RVObject(view,"pivotIdentifier");
                [visible addObject:@{@"pivot":identifier ?: @"icon_only",@"x":@(r.origin.x),@"y":@(r.origin.y),@"width":@(r.size.width),@"height":@(r.size.height)}];
            }
            RVShortsValue(@"visible_native_tabs",visible.copy);RVShortsEvent(@"native_layout_observed");
        };
    }));
}
static NSDictionary *RVShortsReport(void) {
    @synchronized(NSClassFromString(@"YTIPivotBarRenderer")) {
        return @{@"schema":@3,@"diagnostics_revision":@"shorts-primary-command-3",@"hide_feed_shorts":@(RVEnabled(@"hide_shorts")),
            @"bundle_identifier":NSBundle.mainBundle.bundleIdentifier ?: @"",
            @"shorts_preference_source":[NSUserDefaults.standardUserDefaults objectForKey:@"RVPort.hide_shorts_navigation"] ? @"saved_override" : @"packaged_default",
            @"shorts_saved_value":[NSUserDefaults.standardUserDefaults objectForKey:@"RVPort.hide_shorts_navigation"] ?: NSNull.null,
            @"shorts_packaged_value":RVConfig[@"hide_shorts_navigation"] ?: NSNull.null,
            @"hide_navigation_home":@(RVEnabled(@"hide_home_navigation")),@"hide_navigation_subscriptions":@(RVEnabled(@"hide_subscriptions_navigation")),
            @"hide_navigation_library":@(RVEnabled(@"hide_library_navigation")),@"hide_navigation_labels":@(RVEnabled(@"hide_navigation_labels")),
            @"configured_start_page":RVSetting(@"start_page") ?: @"",
            @"hide_navigation_shorts":@(RVEnabled(@"hide_shorts_navigation")),@"hooks":RVShortsHooks.copy ?: @{},
            @"counts":RVShortsCounts.copy ?: @{},@"state":RVShortsState.copy ?: @{},@"device_validated":@NO};
    }
}
