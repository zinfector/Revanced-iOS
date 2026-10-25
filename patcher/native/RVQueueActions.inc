// SPDX-License-Identifier: MIT
// Bind a visible queue action to its source video before any acquisition command
// is dispatched. Hidden MDX alternatives are no longer an admission requirement.
static id RVQueueMenuSourceEntry;
static NSUInteger RVQueueElementsSheetDepth;
static const void *RVQueueElementsSourceKey=&RVQueueElementsSourceKey;
static atomic_uint RVQueueSourceActions,RVQueueElementsActions,RVQueueSourceMissing;

static NSString *RVQueueVideoFromEntry(id entry,NSUInteger depth,NSUInteger *budget,NSHashTable *seen) {
    if (!entry || !depth || !*budget || [seen containsObject:entry]) return nil;
    --*budget;[seen addObject:entry];
    for (NSString *field in @[@"bindingVideoID",@"videoId",@"videoID"]) {
        id video=RVObject(entry,field.UTF8String);if (RVQueueValidVideo(video)) return video;
    }
    for (NSString *field in @[@"watchEndpoint",@"reelWatchEndpoint",@"navigationEndpoint",@"descriptorNavigationEndpoint",
        @"videoRenderer",@"compactVideoRenderer",@"playlistPanelVideoRenderer",@"inlinePlaybackRenderer",
        @"videoWithContextRenderer",@"lockupViewModel"]) {
        if (!RVQueuePresent(entry,field)) continue;
        NSString *video=RVQueueVideoFromEntry(RVObject(entry,field.UTF8String),depth-1,budget,seen);
        if (video) return video;
    }
    return nil;
}
static NSString *RVQueueEntryVideo(id entry) {
    NSUInteger budget=64;NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    return RVQueueVideoFromEntry(entry,6,&budget,seen);
}
static NSString *RVQueueSourceVideo(id entry) {
    return RVQueueEntryVideo(RVQueueMenuSourceEntry) ?: RVQueueEntryVideo(entry);
}
static NSDictionary *RVQueueActionOperation(id renderer,id entry) {
    NSNumber *role=RVQueueRole(renderer);if (!role) return nil;
    NSString *video=RVQueueSourceVideo(entry);
    if (video) return @{@"video_id":video,@"play_next":@([role isEqual:@1])};
    id command=RVObject(renderer,"serviceEndpoint") ?: RVObject(renderer,"navigationEndpoint");
    NSDictionary *operation=RVQueueMDXOperation(command);
    return operation && [operation[@"play_next"] boolValue]==[role isEqual:@1] ? operation : nil;
}
static void RVQueueRunAction(NSDictionary *operation,id entry) {
    void (^perform)(void)=^{
        atomic_fetch_add(&RVQueueMenuActionTaps,1);
        if (!RVQueueNativePerform(operation,entry)) {
            atomic_fetch_add(&RVQueueMenuActionFailures,1);
            // The replacement owns this user action. Replaying the original
            // endpoint here would reopen acquisition instead of enqueueing.
            Class events=NSClassFromString(@"YTToastResponderEvent");
            if (RVCan(events,"eventWithMessage:firstResponder:",@"@@:@@")) {
                NSString *message=RVQueueValidVideo(operation[@"video_id"]) ? @"Queue unavailable. Open a video and try again." : @"Unable to identify this video for the queue.";
                id event=((id(*)(id,SEL,id,id))objc_msgSend)(events,sel_registerName("eventWithMessage:firstResponder:"),message,RVQueueLocalWatch ?: RVCurrentPlayer);
                RVQueueVoid(event,"send");
            }
        }
    };
    if (NSThread.isMainThread) perform();else dispatch_async(dispatch_get_main_queue(),perform);
}
static NSNumber *RVQueueLabelRole(NSString *label) {
    if (![label isKindOfClass:NSString.class]) return nil;
    NSString *text=[[label stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] lowercaseString];
    // Supplement the binary icon route for Elements buttons. These are actual
    // accessibility labels of a single menu row, never an opaque payload search.
    if ([text isEqual:@"play next in queue"] || [text isEqual:@"play next"]) return @1;
    if ([text isEqual:@"play last in queue"] || [text isEqual:@"play last"] || [text isEqual:@"add to queue"]) return @2;
    return nil;
}
static NSNumber *RVQueueButtonRole(id button,NSUInteger depth,NSUInteger *budget,NSString **title) {
    if (!button || !depth || !*budget) return nil;--*budget;
    NSString *label=RVObject(button,"accessibilityLabel");NSNumber *role=RVQueueLabelRole(label);
    if (role) { if (title) *title=label;return role; }
    if (![button isKindOfClass:UIView.class]) return nil;
    id elements=[(UIView *)button accessibilityElements];
    if ([elements isKindOfClass:NSArray.class] && [elements count]<=48)
        for (id element in elements) {
            role=RVQueueButtonRole(element,depth-1,budget,title);if (role) return role;
        }
    for (UIView *child in [(UIView *)button subviews]) {
        role=RVQueueButtonRole(child,depth-1,budget,title);if (role) return role;
    }
    return nil;
}
static id RVQueueElementsAction(id action,id entry) {
    if (!RVCompatible || !NSThread.isMainThread || RVBool(RVQueueLocalController,"isMDXRemoteQueue")) return action;
    NSUInteger budget=48;NSString *title=nil;
    NSNumber *role=RVQueueButtonRole(RVObject(action,"button"),5,&budget,&title);
    if (!role) { title=RVObject(action,"title");role=RVQueueLabelRole(title); }
    NSString *video=role ? RVQueueSourceVideo(entry) : nil;
    if (!role) return action;
    if (!video) atomic_fetch_add(&RVQueueSourceMissing,1);
    Class cls=NSClassFromString(@"YTActionSheetAction");
    const char *factory="actionWithTitle:subtitle:iconImage:handler:";
    if (!RVCan(cls,factory,@"@@:@@@@?")) return action;
    NSDictionary *operation=video ? @{@"video_id":video,@"play_next":@([role isEqual:@1])} : @{};
    id source=RVQueueMenuSourceEntry ?: entry;
    void (^handler)(void)=^{ RVQueueRunAction(operation,source); };
    // The original Elements button dispatches its embedded command, even though
    // the outer action callback is a no-op. Replace the whole native action.
    id replacement=((id(*)(id,SEL,id,id,id,id))objc_msgSend)(cls,sel_registerName(factory),title,nil,RVObject(action,"iconImage"),handler);
    if (replacement) atomic_fetch_add(&RVQueueElementsActions,1);
    return replacement ?: action;
}
static id RVQueueElementsContextEntry(id commandContext) {
    id context=RVObject(commandContext,"context");
    id captured=objc_getAssociatedObject(context,RVQueueElementsSourceKey);
    if (captured) return RVQueueEntryVideo(captured) ? captured : nil;
    id command=RVObject(context,"command");
    if (RVQueueEntryVideo(command)) return command;
    id parent=RVObject(context,"parentResponder");
    for (NSUInteger depth=0;parent && depth<6;depth++) {
        if (RVQueueClass(parent,@"YTWatchController") || RVQueueClass(parent,@"YTWatchViewController")
            || RVQueueClass(parent,@"YTPlayerViewController")) break;
        if (RVQueueClass(parent,@"YTCellController") || RVQueueClass(parent,@"YTElementsCellController")) {
            for (NSString *field in @[@"entry",@"elementEntry"]) {
                id entry=RVObject(parent,field.UTF8String);
                if (entry) return RVQueueEntryVideo(entry) ? entry : nil;
            }
        }
        id next=RVObject(parent,"parentResponder");if (next==parent) break;parent=next;
    }
    id sender=RVObject(commandContext,"sender");
    for (NSString *field in @[@"entry",@"elementEntry"]) {
        id entry=RVObject(sender,field.UTF8String);if (RVQueueEntryVideo(entry)) return entry;
    }
    return nil;
}
static id RVQueueElementsRowAction(id view) {
    if (!RVCompatible || !NSThread.isMainThread || !RVQueueElementsSheetDepth
        || RVBool(RVQueueLocalController,"isMDXRemoteQueue")) return nil;
    NSUInteger budget=48;NSString *title=nil;NSNumber *role=RVQueueButtonRole(view,5,&budget,&title);
    NSString *video=role ? RVQueueSourceVideo(RVQueueMenuSourceEntry) : nil;
    if (!role) return nil;
    if (!video) atomic_fetch_add(&RVQueueSourceMissing,1);
    Class cls=NSClassFromString(@"YTActionSheetAction");const char *factory="actionWithTitle:subtitle:iconImage:handler:";
    if (!RVCan(cls,factory,@"@@:@@@@?")) return nil;
    NSDictionary *operation=video ? @{@"video_id":video,@"play_next":@([role isEqual:@1])} : @{};id source=RVQueueMenuSourceEntry;
    void (^handler)(void)=^{ RVQueueRunAction(operation,source); };
    id action=((id(*)(id,SEL,id,id,id,id))objc_msgSend)(cls,sel_registerName(factory),title,nil,nil,handler);
    if (action) atomic_fetch_add(&RVQueueElementsActions,1);return action;
}
static void RVInstallQueueElementsRows(void) {
    // Native Elements command construction drops the YTI source entry while
    // retaining its command and responder. Carry that entry with the context,
    // so later sheet construction can still identify the selected video.
    RVHook(@"YTElementsCommandHandlerImpl",@"executeWithCommand:entry:fromView:sender:completionBlock:",@"v@:@@@@@?",NO,^id(IMP original,SEL selector) {
        return ^(id handler,id command,id entry,id view,id sender,id completion) {
            if (!NSThread.isMainThread) { ((void(*)(id,SEL,id,id,id,id,id))original)(handler,selector,command,entry,view,sender,completion);return; }
            id previous=RVQueueMenuSourceEntry;RVQueueMenuSourceEntry=entry;
            @try { ((void(*)(id,SEL,id,id,id,id,id))original)(handler,selector,command,entry,view,sender,completion); }
            @finally { RVQueueMenuSourceEntry=previous; }
        };
    });
    RVHook(@"YTELMContextImpl",@"setCommand:",@"v@:@",NO,^id(IMP original,SEL selector) {
        return ^(id context,id command) {
            ((void(*)(id,SEL,id))original)(context,selector,command);
            id entry=NSThread.isMainThread && command ? RVQueueMenuSourceEntry : nil;
            objc_setAssociatedObject(context,RVQueueElementsSourceKey,entry,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        };
    });
    RVHook(@"YTElementsActionSheetController",@"showActionSheetWithCommand:commandContext:handler:",@"v@:@@@?",YES,^id(IMP original,SEL selector) {
        return ^(id controller,id command,id context,id handler) {
            if (!NSThread.isMainThread) { ((void(*)(id,SEL,id,id,id))original)(controller,selector,command,context,handler);return; }
            id previous=RVQueueMenuSourceEntry;NSUInteger previousDepth=RVQueueElementsSheetDepth;
            RVQueueMenuSourceEntry=RVQueueElementsContextEntry(context);
            // singleContentElement can contain an entire menu. A row-label
            // match must never replace that whole container with one action.
            id rows=RVObject(command,"listOptionArray");
            RVQueueElementsSheetDepth=!RVBool(command,"hasSingleContentElement")
                && [rows isKindOfClass:NSArray.class] && [rows count]>0 ? 1 : 0;
            @try { ((void(*)(id,SEL,id,id,id))original)(controller,selector,command,context,handler); }
            @finally { RVQueueElementsSheetDepth=previousDepth;RVQueueMenuSourceEntry=previous; }
        };
    });
    // 21.39.4 FUN_10183147c adds each listOption ELMView through this method.
    RVHook(@"YTActionSheetController",@"addActionWithView:size:delegateAccessibility:",@"@@:@{CGSize=dd}B",NO,^id(IMP original,SEL selector) {
        return ^id(id sheet,id view,CGSize size,BOOL delegateAccessibility) {
            id action=RVQueueElementsRowAction(view);
            if (action && RVCan(sheet,"addAction:",@"v@:@")) {
                ((void(*)(id,SEL,id))objc_msgSend)(sheet,sel_registerName("addAction:"),action);return action;
            }
            return ((id(*)(id,SEL,id,CGSize,BOOL))original)(sheet,selector,view,size,delegateAccessibility);
        };
    });
}
