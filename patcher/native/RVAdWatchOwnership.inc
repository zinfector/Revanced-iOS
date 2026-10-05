// SPDX-License-Identifier: MIT
// Native responder ancestry and reverse-checked Watch/player bindings.
@interface RVAdWatchOwnership : NSObject
@property(nonatomic,copy) NSString *gate;
@property(nonatomic,copy) NSString *route;
@property(nonatomic,strong) id player;
@property(nonatomic,strong) id watch;
@property(nonatomic,strong) NSMutableDictionary *checkpoints;
@end
@implementation RVAdWatchOwnership @end
static id RVAdTypedIvar(id object,NSString *className,const char *name,const char *encoding) {
    Class cls=NSClassFromString(className);
    if (!cls || ![object isKindOfClass:cls]) return nil;
    Ivar field=class_getInstanceVariable(cls,name);
    const char *type=field ? ivar_getTypeEncoding(field) : NULL;
    return type && strcmp(type,encoding)==0 ? object_getIvar(object,field) : nil;
}
static BOOL RVAdIsClass(id object,NSString *name) {
    Class cls=NSClassFromString(name);return object && cls && [object isKindOfClass:cls];
}
static BOOL RVAdWatchOwnsPlayer(id owner,id player) {
    if (RVAdIsClass(owner,@"YTWatchViewController")) return RVObject(owner,"playerViewController")==player;
    if (RVAdIsClass(owner,@"YTWatchPlaybackController"))
        return RVAdTypedIvar(owner,@"YTWatchPlaybackController","_playerViewController","@\"YTPlayerViewController\"")==player;
    if (RVAdIsClass(owner,@"YTWatchController")) {
        id watchPlayback=RVObject(owner,"watchPlaybackController");
        return RVAdTypedIvar(watchPlayback,@"YTWatchPlaybackController","_playerViewController","@\"YTPlayerViewController\"")==player;
    }
    return NO;
}
static RVAdWatchOwnership *RVAdResolveWatchOwnership(id controller,id parent) {
    RVAdWatchOwnership *result=[RVAdWatchOwnership new];result.gate=@"watch_owner_unknown";result.route=@"unresolved";
    result.checkpoints=[NSMutableDictionary dictionary];NSMutableArray *chain=[NSMutableArray array];
    NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    BOOL responderWatch=NO;
    id node=parent ?: controller;
    for (NSUInteger n=0;node && n<16;n++) {
        if ([seen containsObject:node]) { result.checkpoints[@"responder_cycle"]=@YES;break; }
        [seen addObject:node];NSString *name=NSStringFromClass([node class]);[chain addObject:name];
        if ([name hasPrefix:@"YTReel"]) { result.gate=@"shorts_passthrough";break; }
        if (RVAdIsClass(node,@"YTWatchViewController") || RVAdIsClass(node,@"YTWatchController")) {
            responderWatch=YES;
            if (RVAdIsClass(node,@"YTWatchViewController")) result.watch=node;
        }
        node=RVObject(node,"parentResponder");
    }
    result.checkpoints[@"responder_classes"]=chain.copy;
    if ([result.gate isEqual:@"shorts_passthrough"]) return result;
    // The local controller's parentResponder is not its player ownership edge.
    // _delegate and the player's _playbackController form the exact native pair.
    id player=RVAdTypedIvar(controller,@"YTLocalPlaybackController","_delegate","@\"<YTCorePlaybackControllerDelegate>\"");
    BOOL paired=RVAdIsClass(player,@"YTPlayerViewController") && RVPlaybackForPlayer(player)==controller;
    result.checkpoints[@"core_delegate_class"]=player ? NSStringFromClass([player class]) : @"nil";
    result.checkpoints[@"player_core_binding_matches"]=@(paired);
    if (paired) {
        result.player=player;
        id events=RVObject(player,"eventsDelegate"),ui=RVObject(player,"UIDelegate"),viewParent=RVObject(player,"parentViewController");
        result.checkpoints[@"events_delegate_class"]=events ? NSStringFromClass([events class]) : @"nil";
        result.checkpoints[@"ui_delegate_class"]=ui ? NSStringFromClass([ui class]) : @"nil";
        result.checkpoints[@"view_parent_class"]=viewParent ? NSStringFromClass([viewParent class]) : @"nil";
        // A shared/reused player can belong to Shorts. Do not use a stale global Watch.
        for (id candidate in @[events ?: NSNull.null,ui ?: NSNull.null,viewParent ?: NSNull.null]) {
            if ([NSStringFromClass([candidate class]) hasPrefix:@"YTReel"]) { result.gate=@"shorts_passthrough";return result; }
        }
        NSArray *candidates=@[viewParent ?: NSNull.null,events ?: NSNull.null,ui ?: NSNull.null];
        NSArray *routes=@[@"player_view_parent",@"player_events_delegate",@"player_ui_delegate"];
        for (NSUInteger n=0;n<candidates.count;n++) {
            id candidate=candidates[n];
            if (!RVAdWatchOwnsPlayer(candidate,player)) continue;
            result.gate=@"ready";result.route=routes[n];result.checkpoints[@"watch_reverse_binding_matches"]=@YES;
            if (RVAdIsClass(candidate,@"YTWatchViewController")) result.watch=candidate;
            else {
                id watchController=RVAdIsClass(candidate,@"YTWatchController") ? candidate : RVObject(candidate,"delegate");
                id watch=RVAdIsClass(watchController,@"YTWatchController") ? RVObject(watchController,"watchViewController") : nil;
                if (RVAdWatchOwnsPlayer(watch,player)) result.watch=watch;
            }
            return result;
        }
    }
    result.checkpoints[@"watch_reverse_binding_matches"]=@NO;
    if (responderWatch) { result.gate=@"ready";result.route=@"native_responder_chain"; }
    return result;
}
