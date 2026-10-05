// SPDX-License-Identifier: MIT
// Native dislike text for positively identified video models and slim buttons.
static const void *RVNativeDislikeKey=&RVNativeDislikeKey;
static NSHashTable *RVNativeDislikeViews;
static NSMutableDictionary *RVNativeDislikeCounts;
static NSString *RVNativeDislikeRefreshKey;

@interface RVNativeDislikeState : NSObject
@property(nonatomic,strong) id renderer;
@property(nonatomic,weak) UIView *label;
@property(nonatomic) BOOL replaced;
@end
@implementation RVNativeDislikeState
@end

static void RVNativeDislikeEvent(NSString *event) {
    if (!RVEnabled(@"diagnostics") || !NSThread.isMainThread) return;
    if (!RVNativeDislikeCounts) RVNativeDislikeCounts=[NSMutableDictionary dictionary];
    RVNativeDislikeCounts[event]=@([RVNativeDislikeCounts[event] unsignedLongLongValue]+1);
}
static NSNumber *RVNativeDislikeCountForVideo(id video) {
    // All cache writes and native view updates run on the main queue. A getter
    // invoked elsewhere keeps the original native result without blocking.
    if (!NSThread.isMainThread || !RVEnabled(@"return_dislikes") || !RVVideoIDValid(video)) return nil;
    if (![RVDislikeVideo isEqual:video]) return nil;
    return RVDislikeCount;
}
static NSString *RVNativeDislikeVideoForAction(id action) {
    id controller=RVObject(action,"delegate");
    Class cls=NSClassFromString(@"YTSlimVideoScrollableActionBarCellController");
    if (!cls || ![controller isKindOfClass:cls]) return nil;
    id video=RVObject(RVObject(controller,"delegate"),"videoId");
    return RVVideoIDValid(video) ? video : nil;
}
static void RVNativeDislikeApply(id action) {
    if (!NSThread.isMainThread) return;
    RVNativeDislikeState *state=objc_getAssociatedObject(action,RVNativeDislikeKey);
    if (!state) return;
    id label=RVObject(action,"label");
    Class labelClass=NSClassFromString(@"YTFormattedStringLabel");
    // Element-backed action views have no native formatted label. Leave those
    // layouts intact instead of inserting or globally replacing their text.
    if (!labelClass || ![label isKindOfClass:labelClass] || ![label isKindOfClass:UIView.class] ||
        ((UIView *)label).superview!=action || !RVCan(label,"setFormattedString:",@"v@:@")) return;
    if (state.label!=label) { state.label=label;state.replaced=NO; }
    NSNumber *count=RVNativeDislikeCountForVideo(RVNativeDislikeVideoForAction(action));
    if (count) {
        Class formattedClass=NSClassFromString(@"YTIFormattedString");
        if (!RVCan(formattedClass,"formattedStringWithString:",@"@@:@")) return;
        NSString *text=[NSNumberFormatter localizedStringFromNumber:count numberStyle:NSNumberFormatterDecimalStyle];
        id existing=RVObject(label,"text");
        if (![existing isKindOfClass:NSString.class] || ![existing isEqual:text]) {
            id formatted=((id(*)(id,SEL,id))objc_msgSend)(formattedClass,sel_registerName("formattedStringWithString:"),text);
            if (formatted) { RVSetObject(label,"setFormattedString:",formatted);RVNativeDislikeEvent(@"native_text_updated"); }
        }
        state.replaced=YES;
    } else if (state.replaced) {
        const char *getter=RVBool(action,"isToggled") ? "slimButton_toggledText" : "slimButton_text";
        if (RVCan(state.renderer,getter,@"@@:")) {
            RVSetObject(label,"setFormattedString:",RVObject(state.renderer,getter));
            state.replaced=NO;RVNativeDislikeEvent(@"native_text_restored");
        }
    }
}
static void RVNativeDislikeRefresh(void) {
    if (!NSThread.isMainThread) return;
    NSString *key=[NSString stringWithFormat:@"%d/%@/%@",RVEnabled(@"return_dislikes"),RVDislikeVideo ?: @"",RVDislikeCount ?: @""];
    if ([RVNativeDislikeRefreshKey isEqual:key]) return;
    RVNativeDislikeRefreshKey=key;
    RVRefreshDislikeUI();
    for (UIView *view in RVNativeDislikeViews.allObjects) {
        RVNativeDislikeApply(view);[view setNeedsLayout];[view.superview setNeedsLayout];
    }
}
static NSDictionary *RVNativeDislikeReport(void) {
    return @{@"schema":@5,@"layout_compatibility":RVVoteLayoutReport(),@"vote_model":RVVoteModelReport(),@"vote_lifecycle":RVVoteLifecycleReport(),@"enabled":@(RVEnabled(@"return_dislikes")),
        @"request_in_flight":@(RVDislikeTask!=nil),@"count_available":@(RVDislikeCount!=nil),
        @"bound_native_actions":@(RVNativeDislikeViews.count),@"counts":RVNativeDislikeCounts.copy ?: @{},
        @"native_surface":@"slim_and_named_resource_yoga_watch_actions",
        @"element_backed_buttons_supported":@YES,@"inline_accessories":@(RVDislikeActions.count),@"element_layout":RVElementDislikeReport(),@"automatic_service_votes":@NO,@"device_verified":@NO};
}
static void RVInstallNativeDislikes(void) {
    RVNativeDislikeViews=[NSHashTable weakObjectsHashTable];
    RVHook(@"YTSlimVideoDetailsActionView",@"actionViewWithSlimMetadataButtonSupportedRenderer:parentResponder:",@"@@:@@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id renderer,id parent) {
            id action=((id(*)(id,SEL,id,id))original)(cls,sel,renderer,parent);
            Class actionClass=NSClassFromString(@"YTSlimVideoDetailsActionView");
            Class rendererClass=NSClassFromString(@"YTISlimMetadataButtonSupportedRenderers");
            if (NSThread.isMainThread && actionClass && rendererClass && [action isKindOfClass:actionClass] &&
                [action isKindOfClass:UIView.class] && [renderer isKindOfClass:rendererClass] &&
                RVBool(renderer,"slimButton_isDislikeButton")) {
                RVNativeDislikeState *state=[RVNativeDislikeState new];state.renderer=renderer;
                objc_setAssociatedObject(action,RVNativeDislikeKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                if (RVNativeDislikeViews.count<128) [RVNativeDislikeViews addObject:action];
                RVNativeDislikeEvent(@"native_action_bound");
            }
            return action;
        };
    });
    RVHook(@"YTSlimVideoDetailsActionView",@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id action) { RVNativeDislikeApply(action);((void(*)(id,SEL))original)(action,sel); };
    });
    RVHook(@"YTSlimVideoDetailsActionView",@"setToggled:",@"v@:B",NO,^id(IMP original,SEL sel) {
        return ^(id action,BOOL value) { ((void(*)(id,SEL,BOOL))original)(action,sel,value);RVNativeDislikeApply(action); };
    });
    RVHook(@"YTSlimVideoDetailsActionView",@"accessibilityLabel",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id action) {
            id native=((id(*)(id,SEL))original)(action,sel);
            if (!native || !objc_getAssociatedObject(action,RVNativeDislikeKey)) return native;
            NSNumber *count=RVNativeDislikeCountForVideo(RVNativeDislikeVideoForAction(action));
            return count ? [NSString stringWithFormat:@"%@, Return YouTube Dislike estimate: %@",native,
                [NSNumberFormatter localizedStringFromNumber:count numberStyle:NSNumberFormatterDecimalStyle]] : native;
        };
    });
    RVHook(@"YTMainVideoEntityModel",@"dislikeCount",@"Q@:",NO,^id(IMP original,SEL sel) {
        return ^unsigned long long(id model) {
            NSNumber *count=RVNativeDislikeCountForVideo(RVObject(model,"videoID"));
            if (count) { RVNativeDislikeEvent(@"native_model_count");return count.unsignedLongLongValue; }
            return ((unsigned long long(*)(id,SEL))original)(model,sel);
        };
    });
    RVHook(@"YTMainVideoEntityModel",@"dislikeCountString",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id model) {
            NSNumber *count=RVNativeDislikeCountForVideo(RVObject(model,"videoID"));
            return count ? [NSNumberFormatter localizedStringFromNumber:count numberStyle:NSNumberFormatterDecimalStyle] : ((id(*)(id,SEL))original)(model,sel);
        };
    });
    RVHook(@"YTMainVideoEntityModel",@"dislikeAccessibilityText",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id model) {
            NSNumber *count=RVNativeDislikeCountForVideo(RVObject(model,"videoID"));
            return count ? [NSString stringWithFormat:@"Return YouTube Dislike estimate: %@",
                [NSNumberFormatter localizedStringFromNumber:count numberStyle:NSNumberFormatterDecimalStyle]] : ((id(*)(id,SEL))original)(model,sel);
        };
    });
}
