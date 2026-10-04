// SPDX-License-Identifier: MIT
#include "RVMiniplayerSupport.h"
static NSString *const RVMiniplayerPanScope=@"RVPort.MiniplayerPanScope";
static const void *RVMiniplayerAlphaKey=&RVMiniplayerAlphaKey;
static const void *RVMiniplayerAlphaApplyingKey=&RVMiniplayerAlphaApplyingKey;
static __weak id RVQuickActions;

static BOOL RVMiniplayerHorizontalPan(id recognizer) {
    return RVEnabled(@"miniplayer_disable_horizontal_drag") &&
        NSThread.currentThread.threadDictionary[RVMiniplayerPanScope]==recognizer;
}
static void RVMiniplayerWithPan(id recognizer,void (^action)(void)) {
    if (!RVEnabled(@"miniplayer_disable_horizontal_drag") || ![recognizer isKindOfClass:UIPanGestureRecognizer.class]) { action();return; }
    NSMutableDictionary *dictionary=NSThread.currentThread.threadDictionary;
    id previous=dictionary[RVMiniplayerPanScope];dictionary[RVMiniplayerPanScope]=recognizer;
    @try { action(); }
    @finally { if (previous) dictionary[RVMiniplayerPanScope]=previous;else [dictionary removeObjectForKey:RVMiniplayerPanScope]; }
}
static NSArray *RVShortcutsWithoutShorts(id items) {
    if (![items isKindOfClass:NSArray.class]) return items;
    NSMutableArray *kept=[NSMutableArray array];
    for (id item in items) {
        if ([item isKindOfClass:UIApplicationShortcutItem.class] && [((UIApplicationShortcutItem *)item).type isEqual:@"com.google.ios.youtube.shorts"]) continue;
        [kept addObject:item];
    }
    return kept.count==[items count] ? items : kept;
}
static void RVRefreshShortcuts(void) {
    if (!NSThread.isMainThread) { dispatch_async(dispatch_get_main_queue(),^{ RVRefreshShortcuts(); });return; }
    id controller=RVQuickActions;
    if (!RVCan(controller,"shortcutItems",@"@@:")) return;
    id items=RVObject(controller,"shortcutItems");
    if ([items isKindOfClass:NSArray.class] && ![items filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id item,NSDictionary *bindings) {
        return ![item isKindOfClass:UIApplicationShortcutItem.class];
    }]].count) UIApplication.sharedApplication.shortcutItems=items;
}
static void RVInstallMiniplayer(void) {
    RVHook(@"YTWatchFloatingMiniplayerViewController",@"gestureRecognizerShouldBegin:",@"B@:@",NO,^id(IMP original,SEL selector) {
        return ^BOOL(id object,id recognizer) {
            if (RVEnabled(@"miniplayer_disable_drag") && [recognizer isKindOfClass:UIPanGestureRecognizer.class]) return NO;
            return ((BOOL(*)(id,SEL,id))original)(object,selector,recognizer);
        };
    });
    RVHook(@"YTWatchFloatingMiniplayerViewController",@"didPan:",@"v@:@",NO,^id(IMP original,SEL selector) {
        return ^(id object,id recognizer) {
            if (RVEnabled(@"miniplayer_disable_drag") && [recognizer isKindOfClass:UIPanGestureRecognizer.class]) return;
            RVMiniplayerWithPan(recognizer,^{ ((void(*)(id,SEL,id))original)(object,selector,recognizer); });
        };
    });
    RVHook(@"YTMiniplayerLayerView",@"didPanMiniplayer:",@"v@:@",NO,^id(IMP original,SEL selector) {
        return ^(id object,id recognizer) {
            if (RVEnabled(@"miniplayer_disable_drag") && [recognizer isKindOfClass:UIPanGestureRecognizer.class]) return;
            RVMiniplayerWithPan(recognizer,^{ ((void(*)(id,SEL,id))original)(object,selector,recognizer); });
        };
    });
    RVHook(@"YTWatchFloatingMiniplayerViewController",@"didPanMiniBarWithRecognizer:state:delta:velocity:",@"v@:@q{CGPoint=dd}{CGPoint=dd}",NO,^id(IMP original,SEL selector) {
        return ^(id object,id recognizer,NSInteger state,CGPoint delta,CGPoint velocity) {
            if (RVEnabled(@"miniplayer_disable_drag") && [recognizer isKindOfClass:UIPanGestureRecognizer.class]) return;
            if (RVEnabled(@"miniplayer_disable_horizontal_drag") && [recognizer isKindOfClass:UIPanGestureRecognizer.class]) { delta.x=0;velocity.x=0; }
            ((void(*)(id,SEL,id,NSInteger,CGPoint,CGPoint))original)(object,selector,recognizer,state,delta,velocity);
        };
    });
    // UIKit getters remain native except for the exact recognizer and thread
    // currently inside one of the two verified miniplayer pan handlers.
    for (NSString *getter in @[@"translationInView:",@"velocityInView:"]) {
        RVHook(@"UIPanGestureRecognizer",getter,@"{CGPoint=dd}@:@",NO,^id(IMP original,SEL selector) {
            return ^CGPoint(id object,id view) {
                CGPoint result=((CGPoint(*)(id,SEL,id))original)(object,selector,view);
                if (RVMiniplayerHorizontalPan(object)) result.x=0;
                return result;
            };
        });
    }
    RVHook(@"YTWatchFloatingMiniplayerViewController",@"didDoubleTap:",@"v@:@",NO,^id(IMP original,SEL selector) {
        return ^(id object,id recognizer) {
            if (!RVEnabled(@"miniplayer_disable_double_tap")) ((void(*)(id,SEL,id))original)(object,selector,recognizer);
        };
    });
    RVHook(@"YTWatchFloatingMiniplayerBadgeView",@"setMessagingText:showingPremiumBadge:",@"v@:@B",NO,^id(IMP original,SEL selector) {
        return ^(id object,id text,BOOL premium) {
            BOOL hide=RVEnabled(@"miniplayer_hide_subtext");
            ((void(*)(id,SEL,id,BOOL))original)(object,selector,hide ? nil : text,hide ? NO : premium);
        };
    });
    RVHook(@"YTWatchFloatingMiniplayerWithPersistentControlsView",@"maskMiniplayerView",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^(id object) {
            ((void(*)(id,SEL))original)(object,selector);
            if (!RVEnabled(@"miniplayer_square_corners") || !RVCan(object,"isCollapsingOrExpanding",@"B@:") || RVBool(object,"isCollapsingOrExpanding")) return;
            id content=RVObject(object,"contentView");
            if (![content isKindOfClass:UIView.class]) return;
            UIView *contentView=(UIView *)content;
            CGRect bounds=contentView.bounds;CALayer *mask=contentView.layer.mask;
            if (![mask isKindOfClass:CAShapeLayer.class] || CGRectIsEmpty(bounds) || !isfinite(bounds.origin.x) || !isfinite(bounds.origin.y) || !isfinite(bounds.size.width) || !isfinite(bounds.size.height)) return;
            CGPathRef path=CGPathCreateWithRect(bounds,NULL);
            if (path) { ((CAShapeLayer *)mask).path=path;CGPathRelease(path); }
        };
    });
    RVHook(@"YTWatchMiniplayerConstants",@"minimumDefaultDimension",@"d@:",YES,^id(IMP original,SEL selector) {
        return ^double(id object) {
            double native=((double(*)(id,SEL))original)(object,selector);
            if (!RVCompatible) return native;
            id utils=NSClassFromString(@"YTUIUtils");
            if (!RVCan(utils,"appBounds",@"{CGRect={CGPoint=dd}{CGSize=dd}}@:")) return native;
            // NSInvocation preserves the aggregate return ABI in the host harness too.
            SEL appBounds=sel_registerName("appBounds");
            NSInvocation *invocation=[NSInvocation invocationWithMethodSignature:[utils methodSignatureForSelector:appBounds]];
            invocation.target=utils;invocation.selector=appBounds;[invocation invoke];
            if (!invocation) return native;
            CGRect bounds=CGRectZero;[invocation getReturnValue:&bounds];
            return RVMiniplayerDimension(native,[RVSetting(@"miniplayer_min_dimension_points") doubleValue],bounds.size.width,bounds.size.height);
        };
    });
    for (NSString *className in @[@"YTWatchFloatingMiniplayerCircularBackgroundView",@"YTWatchFloatingMiniplayerFrostedGlassCircularBackgroundView"]) {
        // Class-local inherited UIView override; do not hook UIView globally.
        RVHook(className,@"setAlpha:",@"v@:d",NO,^id(IMP original,SEL selector) {
            return ^(id object,CGFloat alpha) {
                // A subclass may inherit another patched implementation. Apply
                // the factor once while retaining the unmodified native baseline.
                if ([objc_getAssociatedObject(object,RVMiniplayerAlphaApplyingKey) boolValue]) {
                    ((void(*)(id,SEL,CGFloat))original)(object,selector,alpha);return;
                }
                objc_setAssociatedObject(object,RVMiniplayerAlphaKey,@(alpha),OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                id setting=RVSetting(@"miniplayer_overlay_opacity");
                double factor=RVCompatible && [setting isKindOfClass:NSNumber.class] ? [setting doubleValue] : 1;
                objc_setAssociatedObject(object,RVMiniplayerAlphaApplyingKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                @try { ((void(*)(id,SEL,CGFloat))original)(object,selector,RVMiniplayerAlpha(alpha,factor)); }
                @finally { objc_setAssociatedObject(object,RVMiniplayerAlphaApplyingKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
            };
        });
        RVHook(className,@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL selector) {
            return ^(id object) {
                ((void(*)(id,SEL))original)(object,selector);
                if (![object isKindOfClass:UIView.class]) return;
                id baseline=objc_getAssociatedObject(object,RVMiniplayerAlphaKey);
                if (!baseline) baseline=@([(UIView *)object alpha]);
                [(UIView *)object setAlpha:[baseline doubleValue]];
            };
        });
    }
    RVHook(@"YTQuickActionsController",@"shortcutItems",@"@@:",NO,^id(IMP original,SEL selector) {
        return ^id(id object) {
            RVQuickActions=object;id items=((id(*)(id,SEL))original)(object,selector);
            return RVEnabled(@"hide_shorts_shortcut") ? RVShortcutsWithoutShorts(items) : items;
        };
    });
}
