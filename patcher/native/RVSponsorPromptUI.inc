// SPDX-License-Identifier: MIT
// A private native skip-view instance with content-seek actions only.
static const void *RVSPHostKey=&RVSPHostKey;
static __weak UIView *RVSPOverlay;
static BOOL RVSPRefreshing;
static BOOL RVSPVisible(UIView *view) {
    if (![view isKindOfClass:UIView.class] || !view.window || CGRectIsEmpty(view.bounds)) return NO;
    for (UIView *v=view;v;v=v.superview) if (v.hidden || v.alpha<.05) return NO;
    return YES;
}
static void RVSPSaveForeground(RVSponsorPromptState *state,NSString *event) {
    // Settings/background refreshes must not erase the preceding player failure.
    if (RVEnabled(@"diagnostics") && RVSPBound(state) && RVAdWatchVisible() &&
        UIApplication.sharedApplication.applicationState==UIApplicationStateActive)
        RVSPLastVisible=RVSPSnapshot(state,event);
}
static BOOL RVSPIsScrubbing(void) {
    return RVBool(RVCurrentPlayer,"userScrubbing");
}
static NSString *RVSPPresentationGate(void) {
    if (!RVEnabled(@"sponsorblock")) return @"feature_disabled";
    if (UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return @"background";
    if (!RVAdWatchVisible()) return @"watch_covered_or_offscreen";
    if (RVPlaybackForPlayer(RVCurrentPlayer)!=RVCurrentPlayback) return @"playback_binding_missing";
    NSString *content=RVContentGate(RVCurrentPlayback);if (content) return content;
    for (NSString *selector in @[@"isPictureInPictureActive",@"isMDXActive",@"isExternalPlaybackActive",@"userScrubbing"])
        if (!RVCan(RVCurrentPlayer,selector.UTF8String,@"B@:")) return @"presentation_contract_missing";
    if (RVBool(RVCurrentPlayer,"isPictureInPictureActive")) return @"picture_in_picture";
    if (RVBool(RVCurrentPlayer,"isMDXActive") || RVBool(RVCurrentPlayer,"isExternalPlaybackActive")) return @"external_playback";
    if (RVSPIsScrubbing()) return @"scrubbing";
    if (!RVCan(RVCurrentPlayer,"playerViewLayout",@"i@:")) return @"layout_contract_missing";
    if (RVInt(RVCurrentPlayer,"playerViewLayout")==1) return @"miniplayer";
    return nil;
}
static NSString *RVSPTitle(RVSponsorPromptState *state) {
    if ([state.mode isEqual:@"undo"]) return NSLocalizedString(@"Undo skip",nil);
    if ([state.mode isEqual:@"highlight"]) return NSLocalizedString(@"Jump to highlight",nil);
    NSDictionary *titles=@{@"sponsor":@"Skip sponsor",@"selfpromo":@"Skip promotion",@"interaction":@"Skip reminder",
        @"intro":@"Skip intro",@"outro":@"Skip outro",@"preview":@"Skip preview",@"music_offtopic":@"Skip non-music",
        @"filler":@"Skip filler",@"hook":@"Skip hook"};
    return NSLocalizedString(titles[state.range[@"category"]] ?: @"Skip segment",nil);
}
@interface RVSponsorPromptHost : UIView
@property(nonatomic,weak) RVSponsorPromptState *state;
@property(nonatomic,strong) UIView *nativeView;
@property(nonatomic,strong) UIButton *nativeButton;
@property(nonatomic,strong) UIImage *forwardImage;
@property(nonatomic) CGRect hitRect;
@property(nonatomic) NSUInteger serial;
@property(nonatomic) BOOL timerScheduled;
- (void)pressed:(id)sender;
@end
@implementation RVSponsorPromptHost
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
    if (self.hidden || self.alpha<.05 || !self.nativeButton.enabled || !CGRectContainsPoint(self.hitRect,point)) return nil;
    return self.nativeButton;
}
- (void)pressed:(id)sender { RVSPAct(self.state,self.serial,NO); }
@end
static void RVSPDetach(RVPlaybackSession *session,NSString *reason) {
    RVSponsorPromptState *state=session.promptState;
    RVSponsorPromptHost *host=(id)state.host;
    if (host) {
        UIView *parent=host.superview;
        [host.nativeButton removeTarget:host action:@selector(pressed:) forControlEvents:UIControlEventTouchUpInside];
        [host removeFromSuperview];
        if (objc_getAssociatedObject(parent,RVSPHostKey)==host) objc_setAssociatedObject(parent,RVSPHostKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        state.host=nil;state.visibleWall=0;state.values[@"host_attached"]=@NO;state.values[@"cleanup_reason"]=reason;
    }
}
static RVSponsorPromptHost *RVSPCreate(RVSponsorPromptState *state) {
    Class wrapper=NSClassFromString(@"YTAdSkipView"),button=NSClassFromString(@"YTQTMAdSkipButton");
    Method init=class_getInstanceMethod(wrapper,@selector(initWithFrame:));
    Ivar field=class_getInstanceVariable(wrapper,"_skipAdButton");
    const char *encoding=field ? ivar_getTypeEncoding(field) : NULL;
    if (!wrapper || !button || ![wrapper isSubclassOfClass:UIView.class] || ![button isSubclassOfClass:UIButton.class] ||
        ![RVSignature(init) isEqual:@"@@:{CGRect={CGPoint=dd}{CGSize=dd}}"] || !field ||
        !encoding || strcmp(encoding,"@\"YTQTMAdSkipButton\"")!=0) {
        state.gate=@"native_wrapper_contract_missing";RVSPEvent(state,@"native_contract_missing");return nil;
    }
    // Use the declared init family so ARC consumes the allocated receiver.
    UIView *native=[(UIView *)[wrapper alloc] initWithFrame:CGRectZero];
    if (![native isKindOfClass:UIView.class]) { state.gate=@"native_wrapper_initialization_failed";return nil; }
    id child=object_getIvar(native,field);
    if (![native isKindOfClass:UIView.class] || ![child isKindOfClass:button] || ![child isKindOfClass:UIButton.class] ||
        !RVCan(native,"setSkipViewForAdAdIndex:totalAdCount:",@"v@:qq") ||
        !RVCan(native,"setFullscreen:",@"v@:B") ||
        !RVCan(native,"addDidPressSkipAdTarget:action:",@"v@:@:") ||
        !RVCan(native,"sizeThatFits:",@"{CGSize=dd}@:{CGSize=dd}")) {
        state.gate=@"button_ivar_contract_mismatch";RVSPEvent(state,@"native_contract_missing");return nil;
    }
    UIButton *control=child;
    for (id target in control.allTargets) {
        // QTM's superclass installs these two self-targets for ink feedback.
        NSArray *actions=[control actionsForTarget:target forControlEvent:control.allControlEvents];
        for (NSString *action in actions) if (target!=control || ![@[@"touchDragEnter:forEvent:",@"touchDragExit:forEvent:"] containsObject:action]) {
            state.gate=@"unexpected_native_targets";RVSPEvent(state,@"target_audit_failed");return nil;
        }
    }
    RVSponsorPromptHost *host=[[RVSponsorPromptHost alloc] initWithFrame:CGRectZero];host.backgroundColor=UIColor.clearColor;
    host.state=state;host.nativeView=native;host.nativeButton=control;host.forwardImage=[control imageForState:UIControlStateNormal];
    host.accessibilityElements=@[control];
    ((void(*)(id,SEL,long long,long long))objc_msgSend)(native,sel_registerName("setSkipViewForAdAdIndex:totalAdCount:"),0,0);
    ((void(*)(id,SEL,id,SEL))objc_msgSend)(native,sel_registerName("addDidPressSkipAdTarget:action:"),host,@selector(pressed:));
    [host addSubview:native];
    state.values[@"native_wrapper_class"]=NSStringFromClass(native.class);
    state.values[@"native_button_class"]=NSStringFromClass(control.class);state.values[@"native_target_audit_passed"]=@YES;
    state.values[@"native_style"]=@"QTM native font/padding/theme/shape; frosted provider unmapped";
    RVSPEvent(state,@"native_control_created");return host;
}
static CGRect RVSPRect(id view,const char *selector) {
    if (!RVCan(view,selector,@"{CGRect={CGPoint=dd}{CGSize=dd}}@:")) return CGRectNull;
    return ((CGRect(*)(id,SEL))objc_msgSend)(view,sel_registerName(selector));
}
static BOOL RVSPValidRect(CGRect r) {
    return !CGRectIsNull(r) && !CGRectIsEmpty(r) && isfinite(r.origin.x) && isfinite(r.origin.y) && isfinite(r.size.width) && isfinite(r.size.height);
}
static CGRect RVSPVideoRect(UIView *view,RVSponsorPromptState *state) {
    id playerView=RVObject(RVCurrentPlayer,"playerView");
    CGRect r=RVSPRect(playerView,"renderingViewFrame");
    NSString *source=@"player_rendering_frame";
    if ([playerView isKindOfClass:UIView.class] && RVSPValidRect(r)) r=[view convertRect:r fromView:playerView];
    else { r=RVSPRect(view,"videoRect");source=@"ordinary_video_rect"; }
    if (RVBool(view,"isPlayerRenderingFrameAdjustedForEngagementPanel")) {
        CGRect adjusted=RVSPRect(view,"playerRenderingFrameForEngagementPanel");
        if (RVSPValidRect(adjusted)) { r=adjusted;source=@"engagement_render_frame"; }
    }
    CGRect elevated=RVSPRect(view,"playerRenderingFrameForElevatedPlayer");
    // Elevated frame is an alternative when the rendering view actually matches it.
    if (RVSPValidRect(elevated) && RVSPValidRect(r) && fabs(CGRectGetMinY(elevated)-CGRectGetMinY(r))<1 &&
        fabs(CGRectGetWidth(elevated)-CGRectGetWidth(r))<1) { r=elevated;source=@"elevated_render_frame"; }
    if (!RVSPValidRect(r)) return CGRectNull;
    r=CGRectIntersection(view.bounds,r);state.values[@"coordinate_source"]=source;return r;
}
static CGFloat RVSPAvoidCaptions(CGRect rect,UIView *overlay) {
    UIView *caption=RVObject(overlay,"captionOverlayView");
    if (!RVSPVisible(caption)) return CGRectGetMinY(rect);
    NSMutableArray *pending=[NSMutableArray arrayWithObject:caption];CGFloat top=CGRectGetMinY(rect);
    for (NSUInteger i=0;i<pending.count && i<64;i++) {
        UIView *v=pending[i];
        if ([v isKindOfClass:UILabel.class] && [(UILabel *)v text].length && RVSPVisible(v)) {
            CGRect r=[overlay convertRect:v.bounds fromView:v];
            if (CGRectIntersectsRect(rect,r)) top=MIN(top,CGRectGetMinY(r)-CGRectGetHeight(rect)-8);
        }
        for (UIView *child in v.subviews) if (pending.count<64) [pending addObject:child];
    }
    return top;
}
static void RVSPLayout(UIView *view) {
    if (!NSThread.isMainThread || RVSPRefreshing || ![view isKindOfClass:NSClassFromString(@"YTMainAppVideoPlayerOverlayView")]) return;
    if (!RVOwnedPlayerView(view)) {
        RVSponsorPromptHost *old=objc_getAssociatedObject(view,RVSPHostKey);
        if (old && old.state.host==old) RVSPDetach(old.state.session,@"host_no_longer_owned");return;
    }
    RVSPOverlay=view;
    RVPlaybackSession *session=RVSession(RVCurrentPlayback);
    if (!session.promptState && !RVEnabled(@"sponsorblock")) return;
    RVSponsorPromptState *state=RVSPState(session);
    RVSPChoose(state,NO);
    NSString *gate=RVSPPresentationGate();
    if (!gate && !RVSPVisible(view)) gate=@"ordinary_overlay_not_visible";
    if (!gate && [state.mode isEqual:@"hidden"]) gate=state.gate;
    if (!gate && [state.mode isEqual:@"pending"]) gate=@"seek_not_confirmed";
    if (!gate) for (NSString *selector in @[@"autonavEndscreenView",@"autonavPreviewView",@"splashScreenView",@"fineScrubberOverlayView"])
        if (RVSPVisible(RVObject(view,selector.UTF8String))) { gate=@"foreground_player_surface";break; }
    if (gate) {
        if (state.host) { state.host.hidden=YES;state.visibleWall=0; }
        state.values[@"presentation_gate"]=gate;state.gate=gate;
        RVSPSaveForeground(state,@"blocked_layout");return;
    }
    RVSPRefreshing=YES;
    @try {
        if (state.host && state.host.superview!=view) RVSPDetach(session,@"overlay_rehosted");
        RVSponsorPromptHost *host=(id)state.host;
        if (!host) {
            host=RVSPCreate(state);if (!host) { RVSPSaveForeground(state,@"native_control_blocked");return; }
            RVSponsorPromptHost *old=objc_getAssociatedObject(view,RVSPHostKey);[old removeFromSuperview];
            [view addSubview:host];objc_setAssociatedObject(view,RVSPHostKey,host,OBJC_ASSOCIATION_RETAIN_NONATOMIC);state.host=host;
        }
        host.frame=view.bounds;host.serial=state.serial;host.nativeButton.enabled=YES;
        BOOL fullscreen=RVBool(view,"isFullscreen"),undo=[state.mode isEqual:@"undo"];
        ((void(*)(id,SEL,BOOL))objc_msgSend)(host.nativeView,sel_registerName("setFullscreen:"),fullscreen);
        NSString *title=RVSPTitle(state);
        [host.nativeButton setTitle:title forState:UIControlStateNormal];
        [host.nativeButton setImage:undo ? [UIImage systemImageNamed:@"arrow.uturn.backward"] : host.forwardImage forState:UIControlStateNormal];
        host.nativeButton.accessibilityLabel=title;
        host.nativeButton.accessibilityHint=undo ? @"Return before the skipped segment and play it without automatic skipping." : @"Seek past this content segment.";
        host.nativeButton.accessibilityIdentifier=undo ? @"rv_sponsor_undo" : @"rv_sponsor_skip";
        [host.nativeButton sizeToFit];[host.nativeView sizeToFit];
        CGSize size=[host.nativeView sizeThatFits:CGSizeMake(CGRectGetWidth(view.bounds),CGFLOAT_MAX)];
        CGRect video=RVSPVideoRect(view,state);
        if (!RVSPValidRect(video) || !isfinite(size.width) || !isfinite(size.height) || size.width<=0 || size.height<=0) {
            host.hidden=YES;state.gate=@"video_rectangle_missing";state.visibleWall=0;
            RVSPSaveForeground(state,@"blocked_layout");return;
        }
        BOOL portrait=CGRectGetHeight(view.bounds)>CGRectGetWidth(view.bounds);
        // Native 21.39.4 skip offsets: main-app trailing 6, bottom 56 portrait/44 landscape.
        // Add only the safe-area contribution not already excluded by the rendering rectangle.
        UIEdgeInsets safe=view.safeAreaInsets;
        CGFloat trailing=6+MAX(0,safe.right-(CGRectGetMaxX(view.bounds)-CGRectGetMaxX(video)));
        CGFloat bottom=(portrait ? 56 : 44)+MAX(0,safe.bottom-(CGRectGetMaxY(view.bounds)-CGRectGetMaxY(video)));
        BOOL rtl=view.effectiveUserInterfaceLayoutDirection==UIUserInterfaceLayoutDirectionRightToLeft;
        CGFloat x=rtl ? CGRectGetMinX(video)+6+MAX(0,safe.left-CGRectGetMinX(video)) : CGRectGetMaxX(video)-trailing-size.width;
        CGRect slot=CGRectMake(x,CGRectGetMaxY(video)-bottom-size.height,size.width,size.height);
        slot.origin.y=RVSPAvoidCaptions(slot,view);
        UIView *bar=RVObject(view,"playerBar");
        if (RVSPVisible(bar)) {
            CGRect obstacle=[view convertRect:bar.bounds fromView:bar];
            if (CGRectIntersectsRect(slot,obstacle)) slot.origin.y=CGRectGetMinY(obstacle)-size.height-8;
        }
        CGRect hit=CGRectInset(slot,-MAX(0,(48-size.width)/2),-MAX(0,(48-size.height)/2));
        if (!CGRectContainsRect(video,hit)) {
            host.hidden=YES;state.gate=@"layout_collision";state.visibleWall=0;state.values[@"hit_target_contained"]=@NO;
            RVSPSaveForeground(state,@"blocked_layout");return;
        }
        host.nativeView.frame=slot;[host.nativeView layoutIfNeeded];host.hitRect=hit;host.hidden=NO;
        [view bringSubviewToFront:host];state.gate=@"ready";
        state.values[@"presentation_gate"]=@"ready";state.values[@"host_attached"]=@YES;
        state.values[@"fullscreen"]=@(fullscreen);state.values[@"hit_target_contained"]=@YES;
        state.values[@"button_width_points"]=@(size.width);state.values[@"button_height_points"]=@(size.height);
        state.values[@"effective_control_alpha"]=@(host.nativeButton.alpha);state.values[@"rtl"]=@(rtl);
        NSTimeInterval wall=NSDate.timeIntervalSinceReferenceDate;
        if (undo) {
            if (state.visibleWall) state.undoRemaining-=MIN(1.5,MAX(0,wall-state.visibleWall));
            state.visibleWall=wall;
            if (state.undoRemaining<=0) {
                state.undo=nil;session.hasLastSkip=NO;state.visibleWall=0;host.hidden=YES;
                RVSPMode(state,@"hidden",nil,@"undo_expired");RVSPEvent(state,@"undo_expired");
                RVSPSaveForeground(state,@"undo_expired");return;
            }
            if (!host.timerScheduled) {
                host.timerScheduled=YES;__weak RVSponsorPromptHost *weakHost=host;
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),dispatch_get_main_queue(),^{
                    RVSponsorPromptHost *live=weakHost;if (!live) return;live.timerScheduled=NO;
                    if (live.state.host==live && RVSPBound(live.state)) RVSPRefreshUI();
                });
            }
        } else state.visibleWall=0;
        RVSPSaveForeground(state,@"visible_layout");
    } @finally { RVSPRefreshing=NO; }
}
static void RVSPRefreshUI(void) {
    if (!NSThread.isMainThread) { dispatch_async(dispatch_get_main_queue(),^{RVSPRefreshUI();});return; }
    RVSponsorPromptState *state=RVSession(RVCurrentPlayback).promptState;
    UIView *view=RVSPOverlay;
    if (view && RVOwnedPlayerView(view)) { RVSPLayout(view);return; }
    UIView *root=RVPlayerOverlay();
    if (root) {
        NSMutableArray *pending=[NSMutableArray arrayWithObject:root];Class expected=NSClassFromString(@"YTMainAppVideoPlayerOverlayView");
        for (NSUInteger i=0;i<pending.count && i<128;i++) {
            UIView *v=pending[i];if ([v isKindOfClass:expected]) { RVSPLayout(v);return; }
            for (UIView *child in v.subviews) if (pending.count<128) [pending addObject:child];
        }
    }
    if (state) { state.gate=@"ordinary_overlay_missing";RVSPDetach(state.session,state.gate);
        RVSPSaveForeground(state,@"overlay_missing"); }
}
static void RVSPInstall(void) {
    RVSPHooks=[NSMutableDictionary dictionary];
    RVSPHooks[@"content_overlay_layout"]=@(RVHook(@"YTMainAppVideoPlayerOverlayView",@"layoutSubviews",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) { ((void(*)(id,SEL))original)(object,sel);if ([object isKindOfClass:UIView.class]) RVSPLayout(object); };
    }));
    RVSPHooks[@"content_overlay_hit_test"]=@(RVHook(@"YTMainAppVideoPlayerOverlayView",@"hitTest:withEvent:",@"@@:{CGPoint=dd}@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,CGPoint point,id event) {
            id result=((id(*)(id,SEL,CGPoint,id))original)(object,sel,point,event);
            RVSponsorPromptHost *host=objc_getAssociatedObject(object,RVSPHostKey);
            if (host && !host.hidden && RVSPVisible(host) && [(UIView *)object isUserInteractionEnabled] &&
                host.serial==host.state.serial && RVSPBound(host.state) && RVOwnedPlayerView(object) && !RVSPPresentationGate()) {
                UIView *hit=[host hitTest:[host convertPoint:point fromView:object] withEvent:event];
                if (hit) { host.state.values[@"owned_hit_test_forwarded"]=@YES;return hit; }
            }
            return result;
        };
    }));
    RVSPHooks[@"seek_supersession"]=@(RVHook(@"YTLocalPlaybackController",@"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi",NO,^id(IMP original,SEL sel) {
        return ^(id object,id time,double before,double after,int source) {
            if (object==RVCurrentPlayback) RVSPUserSeek(@"external_seek");
            ((void(*)(id,SEL,id,double,double,int))original)(object,sel,time,before,after,source);
        };
    }));
    RVSPHooks[@"player_scrubbing"]=@(RVHook(@"YTPlayerViewController",@"notifyDidStartScrubbingAtTime:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id time) { if (object==RVCurrentPlayer) RVSPUserSeek(@"scrubbing");((void(*)(id,SEL,id))original)(object,sel,time); };
    }));
    RVSPHooks[@"player_scrubbing_end"]=@(RVHook(@"YTPlayerViewController",@"notifyDidEndScrubbingAtTime:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id time) { ((void(*)(id,SEL,id))original)(object,sel,time);if (object==RVCurrentPlayer) RVSPRefreshUI(); };
    }));
    RVSPHooks[@"content_reset"]=@(RVHook(@"YTLocalPlaybackController",@"resetWithCurrentVideoSequencer",@"v@:",NO,^id(IMP original,SEL sel) {
        return ^(id object) {
            if (object==RVCurrentPlayback) RVSPReset(RVSession(object),@"content_reset");
            ((void(*)(id,SEL))original)(object,sel);
        };
    }));
    static NSMutableArray *observers;observers=[NSMutableArray array];
    for (NSString *name in @[UIApplicationWillResignActiveNotification,UIApplicationDidBecomeActiveNotification])
        [observers addObject:[NSNotificationCenter.defaultCenter addObserverForName:name object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
            if ([note.name isEqual:UIApplicationWillResignActiveNotification]) {
                RVPlaybackSession *s=RVSession(RVCurrentPlayback);s.promptState.visibleWall=0;RVSPDetach(s,@"background");
            } else RVSPRefreshUI();
        }]];
}
