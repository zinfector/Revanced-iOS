// Observe native ownership after original callbacks; no policy/UI/network effects.
static void RVDebugOnlyBindPlayer(id player) {
    if (!NSThread.isMainThread) {
        __weak id weakPlayer=player;
        dispatch_async(dispatch_get_main_queue(),^{ if (weakPlayer) RVDebugOnlyBindPlayer(weakPlayer); });return;
    }
    id controller=RVPlaybackForPlayer(player);
    if (!controller) return;
    RVCurrentPlayer=player;RVCurrentPlayback=controller;
    RVPlaybackSession *session=RVSession(controller);
    NSString *video=RVObject(controller,"contentVideoID");
    NSString *cpn=RVObject(RVObject(controller,"contentVideoCurrentTime"),"CPN") ?: RVObject(controller,"contentVideoCPN");
    if (![session.videoID isEqual:video] || ![session.cpn isEqual:cpn]) {
        session.videoID=video;session.cpn=cpn;session.generation++;
    }
    RVDBGRecord(@"debug_player_bound",session.generation,@"player",nil);
}
static void RVInstallDebugOnlyObservers(void) {
    RVHook(@"YTPlayerViewController",@"playbackController:didLoadContentPlaybackData:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id player,id controller,id data) {
            ((void(*)(id,SEL,id,id))original)(player,sel,controller,data);
            RVDebugOnlyBindPlayer(player);
        };
    });
    for (NSString *name in @[@"viewDidAppear:",@"viewDidDisappear:"]) {
        BOOL appearing=[name isEqual:@"viewDidAppear:"];
        RVHook(@"YTWatchViewController",name,@"v@:B",NO,^id(IMP original,SEL sel) {
            return ^(id watch,BOOL animated) {
                ((void(*)(id,SEL,BOOL))original)(watch,sel,animated);
                if (appearing) RVCurrentWatch=watch;
                else if (RVCurrentWatch==watch) RVCurrentWatch=nil;
                RVDBGRecord(appearing ? @"debug_watch_appeared" : @"debug_watch_disappeared",0,@"watch",nil);
            };
        });
    }
}
