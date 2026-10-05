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
static NSString *RVShortsDataKind(NSData *data) {
    if (!RVEnabled(@"hide_shorts") || ![data isKindOfClass:NSData.class] || !data.length || data.length>2*1024*1024) return nil;
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
    if ([@[@"YTISectionListSupportedRenderers",@"YTIItemSectionSupportedRenderers",@"YTIHorizontalListSupportedRenderers",
        @"YTIGridSupportedRenderers",@"YTIVerticalListSupportedRenderers",@"YTIExpandedShelfContentsSupportedRenderers",@"YTIRenderer"] containsObject:name]) {
        for (NSString *field in @[@"elementRenderer",@"shelfRenderer",@"itemSectionRenderer"] ) {
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
    BOOL list=[@[@"YTIHorizontalListRenderer",@"YTIGridRenderer",@"YTIVerticalListRenderer",@"YTIExpandedShelfContentsRenderer"] containsObject:name];
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
    if (!RVEnabled(@"hide_shorts_navigation")) {
        if (!lease) return NO;
        RVSetObject(renderer,"setItemsArray:",[lease.original mutableCopy]);
        objc_setAssociatedObject(renderer,RVShortsPivotLeaseKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        RVShortsEvent(@"pivot_restored");RVShortsValue(@"pivot_checkpoint",@"restored");return YES;
    }
    if (lease) return NO;
    NSMutableArray *filtered=[NSMutableArray array];
    for (id item in current) if (!RVShortsIdentifier(RVShortsPivotID(item))) [filtered addObject:item];
    if (filtered.count==current.count) { RVShortsValue(@"pivot_checkpoint",@"no_shorts_entry");return NO; }
    if (!filtered.count) { RVShortsValue(@"pivot_checkpoint",@"no_surviving_tab");return NO; }
    lease=[RVShortsPivotLease new];lease.original=current.copy;
    RVSetObject(renderer,"setItemsArray:",filtered);
    lease.installed=RVAdResolvedObject(renderer,"itemsArray");lease.snapshot=lease.installed.copy;
    if (![lease.installed isKindOfClass:NSArray.class] || ![lease.installed isEqual:filtered]) {
        RVShortsValue(@"pivot_checkpoint",@"items_write_not_applied");return NO;
    }
    objc_setAssociatedObject(renderer,RVShortsPivotLeaseKey,lease,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    RVShortsEvent(@"pivot_compacted");RVShortsValue(@"pivot_input_count",@(current.count));
    RVShortsValue(@"pivot_output_count",@(filtered.count));RVShortsValue(@"pivot_checkpoint",@"native_model_compacted");return YES;
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
    } else if (RVEnabled(@"hide_shorts_navigation") && RVShortsIdentifier(selected)) {
        NSString *fallback=[identifiers containsObject:@"FEwhat_to_watch"] ? @"FEwhat_to_watch" : identifiers.firstObject;
        if (fallback && !RVShortsIdentifier(fallback) && RVCan(controller,"selectItemWithPivotIdentifier:",@"v@:@")) {
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
    }
}
static void RVShortsInstall(void) {
    RVShortsHooks=[NSMutableDictionary dictionary];RVShortsPivotViews=[NSHashTable weakObjectsHashTable];
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
            if (!NSThread.isMainThread || !RVEnabled(@"diagnostics") || !RVEnabled(@"hide_shorts_navigation")) return;
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
        return @{@"schema":@1,@"diagnostics_revision":@"shorts-layout-1",@"hide_feed_shorts":@(RVEnabled(@"hide_shorts")),
            @"hide_navigation_shorts":@(RVEnabled(@"hide_shorts_navigation")),@"hooks":RVShortsHooks.copy ?: @{},
            @"counts":RVShortsCounts.copy ?: @{},@"state":RVShortsState.copy ?: @{},@"device_validated":@NO};
    }
}
