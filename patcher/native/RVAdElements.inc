// SPDX-License-Identifier: MIT
// Matches the supplied binary's native ad-cell factory predicate on root entries.
static id RVAdRootElement(id root) {
    if (!root) return nil;
    if (RVAdIsClass(root,@"YTIElementRenderer")) return root;
    NSString *name=NSStringFromClass([root class]);
    if (![@[@"YTISectionListSupportedRenderers",@"YTIItemSectionSupportedRenderers",@"YTIGridSupportedRenderers",
            @"YTIHorizontalListSupportedRenderers",@"YTIShelfSupportedRenderers",@"YTIRenderer"] containsObject:name]) return nil;
    if (!RVAdResolve(root,"hasElementRenderer",@"B@:") || !RVBool(root,"hasElementRenderer")) return nil;
    id element=RVAdResolvedObject(root,"elementRenderer");
    return RVAdIsClass(element,@"YTIElementRenderer") ? element : nil;
}
static BOOL RVAdStructuralElement(id root,RVAdDisplayState *state,NSString *route) {
    if (!RVEnabled(@"feed_ads")) return NO;
    id element=RVAdRootElement(root);if (!element) return NO;
    RVDisplayEvent(state,@"structural_element_seen",1);
    BOOL optionsKnown=RVAdResolve(element,"compatibilityOptions",@"@@:");
    id options=optionsKnown ? RVObject(element,"compatibilityOptions") : nil;
    BOOL presenceKnown=RVAdResolve(options,"hasAdLoggingData",@"B@:");
    BOOL hasAd=presenceKnown && RVBool(options,"hasAdLoggingData");
    if (!hasAd) {
        RVDisplayEvent(state,presenceKnown ? @"structural_ordinary_element" : @"structural_contract_missing",1);
        if (!presenceKnown) RVDisplayValue(state,@"blocked_checkpoint",@"element_ad_logging_contract_missing");
        return NO;
    }
    id logging=RVAdResolvedObject(options,"adLoggingData");
    BOOL slotPresent=RVAdResolve(logging,"hasAdSlotLoggingData",@"B@:") && RVBool(logging,"hasAdSlotLoggingData");
    BOOL layoutPresent=RVAdResolve(logging,"hasAdLayoutLoggingData",@"B@:") && RVBool(logging,"hasAdLayoutLoggingData");
    id slot=slotPresent ? RVAdResolvedObject(logging,"adSlotLoggingData") : nil;
    id data=RVAdResolve(slot,"hasSlotData",@"B@:") && RVBool(slot,"hasSlotData") ? RVAdResolvedObject(slot,"slotData") : nil;
    BOOL typeKnown=RVAdResolve(data,"type",@"i@:");int type=typeKnown ? RVInt(data,"type") : -1;
    // Keep player/Shorts overlay slots outside the display-ad rule.
    BOOL displaySlot=!typeKnown || [@[@2,@6,@9,@12,@13,@25] containsObject:@(type)];
    BOOL reject=displaySlot;
    NSDictionary *checkpoint=@{@"stage":@"classification",@"route":route,@"root_class":NSStringFromClass([root class]),
        @"surface":state.surface ?: @"native_cell",@"context_generation":@(state.generation),
        @"ad_logging_present":@YES,@"slot_logging_present":@(slotPresent),@"layout_logging_present":@(layoutPresent),
        @"slot_type_known":@(typeKnown),@"slot_type":@(type),@"decision":reject ? @"display_ad" : @"player_slot_passthrough",
        @"captured_at_epoch":@(NSDate.date.timeIntervalSince1970)};
    RVDisplayStructuralCheckpoint(checkpoint);
    RVDisplayEvent(state,reject ? @"structural_ad_classified" : @"structural_player_slot_passthrough",1);
    if (reject) { RVDisplayEvent(state,@"ad_classified",1);RVDisplayValue(state,@"last_rule",@"native_element_ad_logging"); }
    return reject;
}
static id RVAdEmptyElementRoot(id root,RVAdDisplayState *state,NSString *route) {
    if (!RVAdStructuralElement(root,state,route)) return root;
    id empty=RVObject(NSClassFromString(@"YTIElementRenderer"),"emptyCellElementRenderer");
    if (!RVAdIsClass(empty,@"YTIElementRenderer")) {
        RVDisplayEvent(state,@"structural_empty_contract_missing",1);RVDisplayValue(state,@"blocked_checkpoint",@"native_empty_element_missing");return root;
    }
    id result=empty;
    if (root!=RVAdRootElement(root)) {
        if (![root conformsToProtocol:@protocol(NSCopying)] || !RVAdResolve(root,"setElementRenderer:",@"v@:@")) {
            RVDisplayEvent(state,@"structural_union_copy_missing",1);RVDisplayValue(state,@"blocked_checkpoint",@"element_union_copy_missing");return root;
        }
        result=[root copy];RVSetObject(result,"setElementRenderer:",empty);
    }
    RVDisplayEvent(state,@"structural_empty_renderer",1);
    RVDisplayStructuralCheckpoint(@{@"stage":@"mutation",@"route":route,@"mutation":@"native_empty_renderer",
        @"root_class":NSStringFromClass([root class]),@"surface":state.surface ?: @"native_cell",
        @"context_generation":@(state.generation),@"captured_at_epoch":@(NSDate.date.timeIntervalSince1970)});
    return result;
}
static const void *RVAdHiddenElementKey=&RVAdHiddenElementKey;
static void RVInstallStructuralAdCells(void) {
    // The ad subclass calls this native superclass initializer before binding its
    // renderer API. Keep a real controller and native layout/disposal semantics.
    RVAdHook(@"YTElementsCellController",@"initWithEntry:parentResponder:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id entry,id parent) {
            RVAdDisplayState *state=RVDisplayState(parent,NO);
            RVDisplayEvent(state,@"native_element_cell_consumed",1);
            id filtered=RVAdEmptyElementRoot(entry,state,@"native_cell_initialization");
            if (filtered!=entry) RVDisplayEvent(state,@"native_cell_ad_fallback",1);
            return ((id(*)(id,SEL,id,id))original)(object,sel,filtered,parent);
        };
    });
    // Retained native controllers may outlive a model refresh. Collapse only an
    // entry with the same root ad provenance; native lifecycle calls still run.
    RVAdHook(@"YTElementsCellController",@"viewSizeThatFits:",@"{CGSize=dd}@:{CGSize=dd}",NO,^id(IMP original,SEL sel) {
        return ^CGSize(id controller,CGSize constraint) {
            CGSize result=((CGSize(*)(id,SEL,CGSize))original)(controller,sel,constraint);
            if (!RVAdStructuralElement(RVObject(controller,"elementEntry"),nil,@"retained_cell_size")) return result;
            RVDisplayEvent(nil,@"retained_ad_cell_collapsed",1);return CGSizeZero;
        };
    });
    RVAdHook(@"YTElementsCellController",@"updateView:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id view) {
            // Restore our temporary mask before native reuse/configuration so
            // an ordinary cell's native hidden decision always takes precedence.
            if (NSThread.isMainThread && [view isKindOfClass:UIView.class]) {
                NSNumber *prior=objc_getAssociatedObject(view,RVAdHiddenElementKey);
                if (prior) {
                    [(UIView *)view setHidden:prior.boolValue];objc_setAssociatedObject(view,RVAdHiddenElementKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                }
            }
            ((void(*)(id,SEL,id))original)(controller,sel,view);
            if (!NSThread.isMainThread || ![view isKindOfClass:UIView.class]) return;
            BOOL ad=RVAdStructuralElement(RVObject(controller,"elementEntry"),nil,@"retained_cell_view");
            if (ad) {
                objc_setAssociatedObject(view,RVAdHiddenElementKey,@([view isHidden]),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                [(UIView *)view setHidden:YES];RVDisplayEvent(nil,@"retained_ad_cell_hidden",1);
            }
        };
    });
}
