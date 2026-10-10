// SPDX-License-Identifier: MIT
// Executes production hooks with UIKit-shaped test objects on macOS.
// This verifies argument/lifecycle contracts, not the actual YouTube UI.
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

static BOOL RVCompatible=YES,ThrowAlpha,ThrowPan,ThrowHidden;
static NSMutableDictionary *Settings;
static NSMutableArray *RVStatus;
static id RVSetting(NSString *key) { return Settings[key]; }
static BOOL RVEnabled(NSString *key) { return RVCompatible && [RVSetting(key) boolValue]; }
static void RVLog(NSString *message) { [RVStatus addObject:message]; }

@interface UIView : NSObject
@property(nonatomic) CGRect bounds;
@property(nonatomic) CGFloat alpha;
@property(nonatomic,strong) CALayer *layer;
@property(nonatomic,weak) UIView *superview;
@property(nonatomic,getter=isHidden) bool hidden;
- (void)layoutSubviews;
@end
@implementation UIView
- (instancetype)init { if ((self=[super init])) { _alpha=1;_bounds=CGRectMake(0,0,192,108);_layer=[CALayer layer]; }return self; }
- (void)setAlpha:(CGFloat)alpha { if (ThrowAlpha) @throw [NSException exceptionWithName:@"AlphaTest" reason:nil userInfo:nil];_alpha=alpha; }
- (void)setHidden:(bool)hidden { if (ThrowHidden) @throw [NSException exceptionWithName:@"HiddenTest" reason:nil userInfo:nil];_hidden=hidden; }
- (void)layoutSubviews {}
@end
@interface UIPanGestureRecognizer : NSObject
@property(nonatomic) CGPoint translation,velocity;
@property(nonatomic) NSInteger state;
- (CGPoint)translationInView:(id)view;
- (CGPoint)velocityInView:(id)view;
@end
static id TranslationView,VelocityView;
@implementation UIPanGestureRecognizer
- (CGPoint)translationInView:(id)view { TranslationView=view;return self.translation; }
- (CGPoint)velocityInView:(id)view { VelocityView=view;return self.velocity; }
@end
@interface UITapGestureRecognizer : NSObject @end
@implementation UITapGestureRecognizer @end
@interface UIApplicationShortcutItem : NSObject
@property(nonatomic,copy) NSString *type;
@end
@implementation UIApplicationShortcutItem @end
@interface UIApplication : NSObject
@property(nonatomic,copy) NSArray *shortcutItems;
+ (instancetype)sharedApplication;
@end
@implementation UIApplication
+ (instancetype)sharedApplication { static UIApplication *app;static dispatch_once_t once;dispatch_once(&once,^{ app=[self new]; });return app; }
@end

@interface YTMiniplayerLayerView : UIView
@property(nonatomic) CGPoint observedTranslation,observedVelocity;
@property(nonatomic) NSInteger panCalls;
- (void)didPanMiniplayer:(id)recognizer;
@end
@implementation YTMiniplayerLayerView
- (void)didPanMiniplayer:(id)recognizer {
    self.panCalls++;
    if ([recognizer isKindOfClass:UIPanGestureRecognizer.class]) {
        self.observedTranslation=[recognizer translationInView:self];self.observedVelocity=[recognizer velocityInView:self];
    }
}
@end
static BOOL NativeBegin=YES;
static NSInteger BeginCalls,PanCalls,ComplexCalls,DoubleCalls,LastState;
static id LastRecognizer;
static CGPoint LastDelta,LastVelocity,AfterNested;
static UIPanGestureRecognizer *NestedRecognizer;
static YTMiniplayerLayerView *NestedLayer;
@interface YTWatchFloatingMiniplayerViewController : NSObject
- (bool)gestureRecognizerShouldBegin:(id)recognizer;
- (void)didPan:(id)recognizer;
- (void)didPanMiniBarWithRecognizer:(id)recognizer state:(NSInteger)state delta:(CGPoint)delta velocity:(CGPoint)velocity;
- (void)didDoubleTap:(id)recognizer;
@end
@implementation YTWatchFloatingMiniplayerViewController
- (bool)gestureRecognizerShouldBegin:(id)recognizer { BeginCalls++;LastRecognizer=recognizer;return NativeBegin; }
- (void)didPan:(id)recognizer {
    PanCalls++;
    if (ThrowPan) @throw [NSException exceptionWithName:@"PanTest" reason:nil userInfo:nil];
    if (NestedRecognizer) { [NestedLayer didPanMiniplayer:NestedRecognizer];AfterNested=[recognizer translationInView:self]; }
    if ([recognizer isKindOfClass:UIPanGestureRecognizer.class])
        [self didPanMiniBarWithRecognizer:recognizer state:[recognizer state] delta:[recognizer translationInView:self] velocity:[recognizer velocityInView:self]];
}
- (void)didPanMiniBarWithRecognizer:(id)recognizer state:(NSInteger)state delta:(CGPoint)delta velocity:(CGPoint)velocity {
    ComplexCalls++;LastRecognizer=recognizer;LastState=state;LastDelta=delta;LastVelocity=velocity;
}
- (void)didDoubleTap:(id)recognizer { DoubleCalls++;LastRecognizer=recognizer; }
@end
@interface YTWatchFloatingMiniplayerBadgeView : UIView
@property(nonatomic,strong) id text;
@property(nonatomic) bool premium,adPlaying,adBadgeVisible;
- (void)setMessagingText:(id)text showingPremiumBadge:(bool)premium;
@end
@implementation YTWatchFloatingMiniplayerBadgeView
- (void)setMessagingText:(id)text showingPremiumBadge:(bool)premium { self.text=text;self.premium=premium;self.adBadgeVisible=self.adPlaying; }
@end
static NSInteger MaskCalls;
static BOOL OtherMask;
@interface YTWatchMiniBarButtonView : UIView @end
@implementation YTWatchMiniBarButtonView @end
// Deliberately inherit a patched setter to test its reentrancy guard.
@interface YTWatchFloatingMiniplayerActionButtonView : YTWatchMiniBarButtonView @end
@implementation YTWatchFloatingMiniplayerActionButtonView @end
@interface YTWatchFloatingMiniplayerWithPersistentControlsView : UIView
{
@public
    YTWatchMiniBarButtonView *_closeButton;
    UIView *_closeButtonCircularBackgroundView,*_actionButtonCircularBackgroundView;
}
@property(nonatomic,strong) UIView *contentView;
@property(nonatomic,strong) UIView *controlsView,*skipAdButton;
@property(nonatomic) bool isCollapsingOrExpanding;
- (void)maskMiniplayerView;
@end
@implementation YTWatchFloatingMiniplayerWithPersistentControlsView
- (void)maskMiniplayerView {
    MaskCalls++;
    if (OtherMask) { self.contentView.layer.mask=[CALayer layer];return; }
    CAShapeLayer *mask=[CAShapeLayer layer];CGPathRef path=CGPathCreateWithRoundedRect(self.contentView.bounds,12,12,NULL);
    mask.path=path;CGPathRelease(path);self.contentView.layer.mask=mask;
}
@end
@interface YTWatchMiniplayerConstants : NSObject
+ (double)minimumDefaultDimension;
@end
@implementation YTWatchMiniplayerConstants
+ (double)minimumDefaultDimension { return 192; }
@end
static CGRect AppBounds;
@interface YTUIUtils : NSObject
+ (CGRect)appBounds;
@end
@implementation YTUIUtils
+ (CGRect)appBounds { return AppBounds; }
@end
@interface YTWatchFloatingMiniplayerCircularBackgroundView : UIView @end
@implementation YTWatchFloatingMiniplayerCircularBackgroundView
- (void)layoutSubviews { [super layoutSubviews];[self setAlpha:.8]; }
@end
// Deliberately inherit already patched methods to exercise nested overrides.
@interface YTWatchFloatingMiniplayerFrostedGlassCircularBackgroundView : YTWatchFloatingMiniplayerCircularBackgroundView @end
@implementation YTWatchFloatingMiniplayerFrostedGlassCircularBackgroundView @end
@interface YTQuickActionsController : NSObject
@property(nonatomic,strong) id items;
- (id)shortcutItems;
@end
@implementation YTQuickActionsController
- (id)shortcutItems { return self.items; }
@end

#include "../native/RVRuntime.inc"
#include "adaptive_fixture.h"
#include "../native/RVMiniplayer.inc"

static BOOL Near(double a,double b) { return fabs(a-b)<1e-8; }
static void RVCheckPoint(CGPoint p,double x,double y) { assert(Near(p.x,x) && Near(p.y,y)); }
static UIApplicationShortcutItem *Shortcut(NSString *type) { UIApplicationShortcutItem *item=[UIApplicationShortcutItem new];item.type=type;return item; }
static BOOL RectMask(UIView *view) {
    CGPathRef rect=CGPathCreateWithRect(view.bounds,NULL);
    BOOL same=CGPathEqualToPath(rect,((CAShapeLayer *)view.layer.mask).path);CGPathRelease(rect);return same;
}
int main(void) { @autoreleasepool {
    FixtureAdaptivePreflight();
    Settings=[@{@"miniplayer_min_dimension_points":@0,@"miniplayer_overlay_opacity":@1} mutableCopy];RVStatus=[NSMutableArray array];
    AppBounds=CGRectMake(0,0,430,932);RVInstallMiniplayer();
    assert(RVStatus.count==18);
    for (NSString *status in RVStatus) assert([status hasPrefix:@"Installed "]);
    YTWatchFloatingMiniplayerViewController *controller=[YTWatchFloatingMiniplayerViewController new];
    UIPanGestureRecognizer *pan=[UIPanGestureRecognizer new];pan.translation=CGPointMake(25,40);pan.velocity=CGPointMake(100,200);pan.state=3;
    UITapGestureRecognizer *tap=[UITapGestureRecognizer new];id view=[UIView new];
    assert([controller gestureRecognizerShouldBegin:pan]);assert(LastRecognizer==pan);
    NativeBegin=NO;assert(![controller gestureRecognizerShouldBegin:pan]);NativeBegin=YES;
    [controller didPan:pan];RVCheckPoint(LastDelta,25,40);RVCheckPoint(LastVelocity,100,200);assert(LastState==3 && LastRecognizer==pan);
    [pan translationInView:view];assert(TranslationView==view);[pan velocityInView:view];assert(VelocityView==view);
    Settings[@"miniplayer_disable_horizontal_drag"]=@YES;
    [controller didPan:pan];RVCheckPoint(LastDelta,0,40);RVCheckPoint(LastVelocity,0,200);
    RVCheckPoint([pan translationInView:view],25,40);RVCheckPoint([pan velocityInView:view],100,200);
    [controller didPanMiniBarWithRecognizer:pan state:4 delta:CGPointMake(12,13) velocity:CGPointMake(14,15)];
    assert(LastState==4);RVCheckPoint(LastDelta,0,13);RVCheckPoint(LastVelocity,0,15);
    [controller didPanMiniBarWithRecognizer:tap state:5 delta:CGPointMake(12,13) velocity:CGPointMake(14,15)];
    RVCheckPoint(LastDelta,12,13);RVCheckPoint(LastVelocity,14,15);
    YTMiniplayerLayerView *layer=[YTMiniplayerLayerView new];[layer didPanMiniplayer:pan];RVCheckPoint(layer.observedTranslation,0,40);RVCheckPoint(layer.observedVelocity,0,200);
    assert(TranslationView==layer && VelocityView==layer);
    NestedRecognizer=[UIPanGestureRecognizer new];NestedRecognizer.translation=CGPointMake(50,60);NestedRecognizer.velocity=CGPointMake(70,80);NestedLayer=layer;
    [controller didPan:pan];RVCheckPoint(layer.observedTranslation,0,60);RVCheckPoint(AfterNested,0,40);NestedRecognizer=nil;
    ThrowPan=YES;@try { [controller didPan:pan];assert(false); } @catch (NSException *exception) { assert([exception.name isEqual:@"PanTest"]); }
    ThrowPan=NO;assert(!NSThread.currentThread.threadDictionary[RVMiniplayerPanScope]);RVCheckPoint([pan translationInView:nil],25,40);
    UIPanGestureRecognizer *other=[UIPanGestureRecognizer new];other.translation=CGPointMake(80,90);
    RVMiniplayerWithPan(pan,^{
        RVCheckPoint([other translationInView:nil],80,90);
        dispatch_semaphore_t done=dispatch_semaphore_create(0);
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT,0),^{ @autoreleasepool { RVCheckPoint([pan translationInView:nil],25,40);dispatch_semaphore_signal(done); } });
        assert(!dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC)));
    });
    Settings[@"miniplayer_disable_drag"]=@YES;NSInteger begin=BeginCalls,pans=PanCalls,complex=ComplexCalls,layerPans=layer.panCalls;
    assert(![controller gestureRecognizerShouldBegin:pan] && BeginCalls==begin);
    assert([controller gestureRecognizerShouldBegin:tap] && BeginCalls==begin+1);
    [controller didPan:pan];[layer didPanMiniplayer:pan];[controller didPanMiniBarWithRecognizer:pan state:3 delta:CGPointZero velocity:CGPointZero];
    assert(PanCalls==pans && ComplexCalls==complex && layer.panCalls==layerPans);
    [controller didPan:tap];assert(PanCalls==pans+1);
    RVCompatible=NO;[controller didPan:pan];RVCheckPoint(LastDelta,25,40);assert([controller gestureRecognizerShouldBegin:pan]);RVCompatible=YES;
    Settings[@"miniplayer_disable_double_tap"]=@YES;[controller didDoubleTap:tap];assert(!DoubleCalls);
    Settings[@"miniplayer_disable_double_tap"]=@NO;[controller didDoubleTap:tap];assert(DoubleCalls==1 && LastRecognizer==tap);
    YTWatchFloatingMiniplayerBadgeView *badge=[YTWatchFloatingMiniplayerBadgeView new];id text=@"native message";
    [badge setMessagingText:text showingPremiumBadge:true];assert(badge.text==text && badge.premium);
    Settings[@"miniplayer_hide_subtext"]=@YES;badge.adPlaying=true;[badge setMessagingText:text showingPremiumBadge:true];
    assert(!badge.text && !badge.premium && badge.adBadgeVisible);
    RVCompatible=NO;[badge setMessagingText:text showingPremiumBadge:true];assert(badge.text==text && badge.premium);RVCompatible=YES;
    YTWatchFloatingMiniplayerWithPersistentControlsView *persistent=[YTWatchFloatingMiniplayerWithPersistentControlsView new];persistent.contentView=[UIView new];
    [persistent maskMiniplayerView];assert(!RectMask(persistent.contentView));
    Settings[@"miniplayer_square_corners"]=@YES;[persistent maskMiniplayerView];assert(RectMask(persistent.contentView));
    persistent.isCollapsingOrExpanding=true;[persistent maskMiniplayerView];assert(!RectMask(persistent.contentView));
    persistent.isCollapsingOrExpanding=false;RVCompatible=NO;[persistent maskMiniplayerView];assert(!RectMask(persistent.contentView));RVCompatible=YES;
    OtherMask=YES;[persistent maskMiniplayerView];assert(![persistent.contentView.layer.mask isKindOfClass:CAShapeLayer.class]);OtherMask=NO;
    assert(MaskCalls==5);
    persistent.contentView.superview=persistent;
    persistent->_closeButton=[YTWatchMiniBarButtonView new];persistent->_closeButton.superview=persistent.contentView;
    persistent.controlsView=[YTWatchFloatingMiniplayerActionButtonView new];persistent.controlsView.superview=persistent.contentView;
    persistent->_closeButtonCircularBackgroundView=[YTWatchFloatingMiniplayerCircularBackgroundView new];persistent->_closeButtonCircularBackgroundView.superview=persistent.contentView;
    persistent->_actionButtonCircularBackgroundView=[YTWatchFloatingMiniplayerFrostedGlassCircularBackgroundView new];persistent->_actionButtonCircularBackgroundView.superview=persistent.contentView;
    persistent.skipAdButton=[UIView new];persistent.skipAdButton.superview=persistent.contentView;
    UIView *close=persistent->_closeButton,*controls=persistent.controlsView,*closeBG=persistent->_closeButtonCircularBackgroundView,*actionBG=persistent->_actionButtonCircularBackgroundView;
    [persistent layoutSubviews];assert(!objc_getAssociatedObject(close,RVMiniplayerControlsKey));
    assert(!close.isHidden && !controls.isHidden && !persistent.skipAdButton.isHidden);
    Settings[@"miniplayer_hide_overlay_buttons"]=@YES;[persistent layoutSubviews];
    assert(close.isHidden && controls.isHidden && !persistent.skipAdButton.isHidden);
    assert(!persistent.contentView.isHidden && Near(closeBG.alpha,0) && Near(actionBG.alpha,0));
    close.hidden=false;controls.hidden=false;assert(close.isHidden && controls.isHidden);
    controls.hidden=true;[closeBG setAlpha:.6];[actionBG setAlpha:.4];
    assert(Near(closeBG.alpha,0) && Near(actionBG.alpha,0));
    Settings[@"miniplayer_hide_overlay_buttons"]=@NO;[persistent layoutSubviews];
    assert(!close.isHidden && controls.isHidden && Near(closeBG.alpha,.6) && Near(actionBG.alpha,.4));
    controls.hidden=false;Settings[@"miniplayer_hide_overlay_buttons"]=@YES;[persistent layoutSubviews];
    Settings[@"miniplayer_overlay_opacity"]=@.5;RVCompatible=NO;[persistent layoutSubviews];
    assert(!close.isHidden && !controls.isHidden && Near(closeBG.alpha,.6));RVCompatible=YES;
    [persistent layoutSubviews];assert(close.isHidden && Near(closeBG.alpha,0));
    UIView *detachedParent=[UIView new];close.superview=detachedParent;close.hidden=false;assert(!close.isHidden);
    close.superview=persistent.contentView;[persistent layoutSubviews];assert(close.isHidden);
    ThrowHidden=YES;@try { close.hidden=false;assert(false); } @catch (NSException *exception) { assert([exception.name isEqual:@"HiddenTest"]); }
    ThrowHidden=NO;assert(![(RVMiniplayerControlsState *)objc_getAssociatedObject(close,RVMiniplayerControlsKey) applying]);
    close.hidden=false;assert(close.isHidden);
    assert(!RVMiniplayerIvar(persistent,"_missing","@\"UIView\"") && !RVMiniplayerIvar(persistent,"_closeButton","@\"UIView\""));
    UIView *unknownControl=[UIView new];unknownControl.superview=persistent.contentView;RVMiniplayerBindControl(persistent,unknownControl,NO);assert(!unknownControl.isHidden);
    YTWatchMiniBarButtonView *unowned=[YTWatchMiniBarButtonView new];unowned.hidden=false;assert(!unowned.isHidden);
    persistent.controlsView=[UIView new];persistent.controlsView.superview=persistent.contentView;[persistent layoutSubviews];assert(!persistent.controlsView.isHidden);
    __weak id weakMini;
    YTWatchMiniBarButtonView *orphan;
    @autoreleasepool { YTWatchFloatingMiniplayerWithPersistentControlsView *temp=[YTWatchFloatingMiniplayerWithPersistentControlsView new];weakMini=temp;temp.contentView=[UIView new];temp->_closeButton=[YTWatchMiniBarButtonView new];orphan=temp->_closeButton;orphan.superview=temp.contentView;[temp layoutSubviews];assert(orphan.isHidden); }
    assert(!weakMini);orphan.hidden=false;assert(!orphan.isHidden);
    Settings[@"miniplayer_hide_overlay_buttons"]=@NO;Settings[@"miniplayer_overlay_opacity"]=@1;
    assert([YTWatchMiniplayerConstants minimumDefaultDimension]==192);
    Settings[@"miniplayer_min_dimension_points"]=@300;assert([YTWatchMiniplayerConstants minimumDefaultDimension]==300);
    AppBounds=CGRectMake(0,0,932,430);assert([YTWatchMiniplayerConstants minimumDefaultDimension]==300);
    Settings[@"miniplayer_min_dimension_points"]=@480;assert([YTWatchMiniplayerConstants minimumDefaultDimension]==398);
    AppBounds=CGRectMake(0,0,180,300);assert([YTWatchMiniplayerConstants minimumDefaultDimension]==192);
    AppBounds=CGRectMake(0,0,NAN,932);assert([YTWatchMiniplayerConstants minimumDefaultDimension]==192);
    AppBounds=CGRectMake(0,0,430,932);RVCompatible=NO;assert([YTWatchMiniplayerConstants minimumDefaultDimension]==192);RVCompatible=YES;
    UIView *plain=[UIView new];[plain setAlpha:.8];Settings[@"miniplayer_overlay_opacity"]=@.5;assert(Near(plain.alpha,.8));
    for (Class cls in @[YTWatchFloatingMiniplayerCircularBackgroundView.class,YTWatchFloatingMiniplayerFrostedGlassCircularBackgroundView.class]) {
        UIView *background=[cls new];[background setAlpha:.8];assert(Near(background.alpha,.4));
        [background layoutSubviews];[background layoutSubviews];assert(Near(background.alpha,.4));
        Settings[@"miniplayer_overlay_opacity"]=@.25;[background layoutSubviews];assert(Near(background.alpha,.2));
        Settings[@"miniplayer_overlay_opacity"]=@1;[background layoutSubviews];assert(Near(background.alpha,.8));
        Settings[@"miniplayer_overlay_opacity"]=@.5;RVCompatible=NO;[background setAlpha:.6];assert(Near(background.alpha,.6));RVCompatible=YES;
        ThrowAlpha=YES;@try { [background setAlpha:.7];assert(false); } @catch (NSException *exception) { assert([exception.name isEqual:@"AlphaTest"]); }
        ThrowAlpha=NO;assert(!objc_getAssociatedObject(background,RVMiniplayerAlphaApplyingKey));[background setAlpha:.6];assert(Near(background.alpha,.3));
    }
    YTQuickActionsController *quick=[YTQuickActionsController new];UIApplicationShortcutItem *shorts=Shortcut(@"com.google.ios.youtube.shorts"),*search=Shortcut(@"com.google.ios.youtube.search"),*unknown=Shortcut(@"com.google.ios.youtube.shorts.other");
    NSArray *items=@[shorts,search,unknown];quick.items=items;assert([quick shortcutItems]==items);
    Settings[@"hide_shorts_shortcut"]=@YES;NSArray *filtered=[quick shortcutItems];assert(filtered.count==2 && filtered[0]==search && filtered[1]==unknown);
    RVRefreshShortcuts();assert([UIApplication.sharedApplication.shortcutItems isEqual:filtered]);
    Settings[@"hide_shorts_shortcut"]=@NO;RVRefreshShortcuts();assert([UIApplication.sharedApplication.shortcutItems isEqual:items]);
    quick.items=@[search,unknown];Settings[@"hide_shorts_shortcut"]=@YES;assert([quick shortcutItems]==quick.items);
    quick.items=@[shorts,@"unknown item"];filtered=[quick shortcutItems];assert(filtered.count==1 && [filtered[0] isEqual:@"unknown item"]);
    NSArray *before=UIApplication.sharedApplication.shortcutItems;RVRefreshShortcuts();assert(UIApplication.sharedApplication.shortcutItems==before);
    quick.items=nil;assert(![quick shortcutItems]);RVRefreshShortcuts();assert(UIApplication.sharedApplication.shortcutItems==before);
    quick.items=items;RVCompatible=NO;assert([quick shortcutItems]==items);
    puts("Native miniplayer and quick-action contract checks passed.");
}return 0; }
