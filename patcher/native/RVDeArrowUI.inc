// SPDX-License-Identifier: MIT
// Consumer-owned DeArrow settings, generations and native presentation adapters.
#include "RVDeArrowProjection.inc"
static const void *RVDAUIBindingKey=&RVDAUIBindingKey,*RVDAUIDataKey=&RVDAUIDataKey;
static const void *RVDAUIModelKey=&RVDAUIModelKey,*RVDAUIImageKey=&RVDAUIImageKey;
static const void *RVDAUIAccessibilityKey=&RVDAUIAccessibilityKey;
static NSHashTable *RVDAUIBindings;
static NSMutableArray *RVDAUIHistory;
static NSMutableArray *RVDAUIHooks;
static NSMutableSet<NSString *> *RVDAUIOriginalVideos;
static uint64_t RVDAUIGeneration,RVDAUISettingsRevision=1;
static NSTimeInterval RVDAUIStarted;
static _Thread_local __unsafe_unretained id RVDAUISection;
static _Thread_local NSInteger RVDAUIIndex;
static _Thread_local __unsafe_unretained id RVDAUIConfiguring;

@interface RVDAUIBinding : NSObject
@property(nonatomic,weak) id controller,section,imageView;
@property(nonatomic,strong) id originalEntry;
@property(nonatomic,strong) RVDAProjection *profile;
@property(nonatomic,strong) RVDeArrowRequest *request;
@property(nonatomic,strong) RVDeArrowResult *result;
@property(nonatomic,strong) NSData *imageBytes;
@property(nonatomic,strong) NSData *legacySource;
@property(nonatomic,strong) UIImage *encodedImage,*nativeImage;
@property(nonatomic,copy) NSString *video,*surface,*route,*titleBlocker,*imageBlocker;
@property(nonatomic) NSInteger index;
@property(nonatomic) uint64_t generation,settingsRevision;
@property(nonatomic) RVDeArrowParts parts;
@property(nonatomic) BOOL refreshQueued,modern;
@end
@implementation RVDAUIBinding
- (void)dealloc { [_request cancel]; }
@end
@interface RVDAUIImageLease : NSObject
@property(nonatomic,weak) RVDAUIBinding *binding;
@end
@implementation RVDAUIImageLease @end
static void RVDAUIRestoreAccessibility(id value) {
    if (![value isKindOfClass:UIView.class]) return;
    id baseline=objc_getAssociatedObject(value,RVDAUIAccessibilityKey);
    if (baseline) {
        [(UIView *)value setAccessibilityLabel:baseline==NSNull.null ? nil : baseline];
        objc_setAssociatedObject(value,RVDAUIAccessibilityKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
}
static void RVDAUIApplyAccessibility(id value,NSString *title) {
    if (!title || ![value isKindOfClass:UIView.class]) return;
    objc_setAssociatedObject(value,RVDAUIAccessibilityKey,[(UIView *)value accessibilityLabel] ?: NSNull.null,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [(UIView *)value setAccessibilityLabel:title];
}

static void RVDAUIEvent(NSString *checkpoint,RVDAUIBinding *binding,NSString *blocker) {
    if (!RVEnabled(@"diagnostics")) return;
    @synchronized(RVDAUIBinding.class) {
        if (!RVDAUIHistory) RVDAUIHistory=[NSMutableArray array];
        if (RVDAUIHistory.count>=48) [RVDAUIHistory removeObjectAtIndex:0];
        [RVDAUIHistory addObject:@{@"checkpoint":checkpoint,@"surface":binding.surface ?: @"unknown",
            @"route":binding.route ?: @"unresolved",@"generation":@(binding.generation),
            @"elapsed_ms":@(MAX(0,1000*(NSProcessInfo.processInfo.systemUptime-RVDAUIStarted))),
            @"settings_revision":@(RVDAUISettingsRevision),@"blocker":blocker ?: @"ready"}];
    }
}
static BOOL RVDAUIPolicy(NSString *surface,BOOL title) {
    if (!RVCompatible || !surface) return NO;
    id overrides=RVSetting(title ? @"dearrow_title_modes" : @"dearrow_thumbnail_modes");
    NSString *mode=[overrides isKindOfClass:NSDictionary.class] ? overrides[surface] : nil;
    if ([mode isEqual:@"custom"]) return YES;
    if ([mode isEqual:@"original"]) return NO;
    if (!title) {
        id legacy=RVSetting(@"thumbnail_modes");NSString *old=[legacy isKindOfClass:NSDictionary.class] ? legacy[[surface isEqual:@"related"] ? @"player" : surface] : nil;
        if ([@[@"dearrow",@"dearrow-stills"] containsObject:old]) return YES;
        if ([@[@"original",@"stills"] containsObject:old]) return NO;
    }
    return RVEnabled(title ? @"dearrow_titles" : @"dearrow_thumbnails");
}
static NSString *RVDAUIOwnerSurface(id owner,BOOL watchTitle,NSString **watchVideo) {
    for (NSUInteger depth=0;owner && depth<16;depth++,owner=RVObject(owner,"parentResponder")) {
        NSString *name=NSStringFromClass([owner class]);
        if ([@[@"YTSlimVideoMetadataSectionController",@"YTSlimVideoInformationCellController"] containsObject:name]) {
            NSString *video=RVObject(owner,"videoID");if (RVVideoIDValid(video)) { if (watchVideo) *watchVideo=video;if (watchTitle) return @"player"; }
        }
        if ([name hasPrefix:@"YTReel"] || [name hasPrefix:@"YTShorts"]) return nil;
        if ([name isEqual:@"YTWatchViewController"]) return watchTitle ? nil : @"related";
        if ([@[@"YTSearchViewController",@"YTSearchResultsViewController"] containsObject:name]) return @"search";
        NSString *browse=RVObject(owner,"browseID") ?: RVObject(owner,"browseIdentifier");
        if ([browse isEqual:@"FEwhat_to_watch"]) return @"home";
        if ([browse isEqual:@"FEsubscriptions"]) return @"subscriptions";
        if ([@[@"FElibrary",@"FEyou",@"FEhistory"] containsObject:browse]) return @"library";
    }return nil;
}
static id RVDAUILegacyRenderer(id entry,NSString **unionSelector);
static id RVDAUICurrentController(RVDAUIBinding *binding) {
    id section=binding.section;
    if (!section || binding.index<0 || !RVCan(section,"cellControllerAtIndex:",@"@@:q")) return nil;
    // Guard bounds before a native lookup; never reload an obsolete index.
    id entries=RVObject(RVObject(section,"viewModel"),"entries");
    if (![entries isKindOfClass:NSArray.class] || (NSUInteger)binding.index>=[entries count] || entries[binding.index]!=binding.originalEntry) return nil;
    id controller=((id(*)(id,SEL,NSInteger))objc_msgSend)(section,sel_registerName("cellControllerAtIndex:"),binding.index);
    return controller==binding.controller && RVObject(controller,"entry")==binding.originalEntry ? controller : nil;
}
static BOOL RVDAUICurrent(RVDAUIBinding *binding) {
    if (binding.settingsRevision!=RVDAUISettingsRevision || !binding.controller ||
        objc_getAssociatedObject(binding.controller,RVDAUIBindingKey)!=binding || !RVDAUICurrentController(binding)) return NO;
    NSString *watchVideo=nil;
    NSString *surface=RVDAUIOwnerSurface(binding.controller,binding.modern ? binding.profile.watchTitle : [binding.surface isEqual:@"player"],&watchVideo);
    if (![surface isEqual:binding.surface]) return NO;
    if ([surface isEqual:@"player"] && ![watchVideo isEqual:binding.video]) return NO;
    if (binding.modern) {
        NSData *current=RVObject(RVAdRootElement(binding.originalEntry),"elementData");
        if (![current isEqual:binding.profile.source]) return NO;
        if (binding.profile.watchTitle && ![watchVideo isEqual:binding.video]) return NO;
    } else if (binding.legacySource) {
        NSData *current=RVObject(RVDAUILegacyRenderer(binding.originalEntry,NULL),"data");
        if (![current isEqual:binding.legacySource]) return NO;
    }
    return YES;
}
static BOOL RVDAUIOriginal(RVDAUIBinding *binding) { return [RVDAUIOriginalVideos containsObject:binding.video]; }
static NSString *RVDAUITitle(RVDAUIBinding *binding) {
    return !RVDAUIOriginal(binding) && (binding.parts&RVDeArrowPartsTitle) &&
        binding.result.titleDecision==RVDeArrowDecisionCustom ? binding.result.title : nil;
}
static UIImage *RVDAUIImage(RVDAUIBinding *binding) {
    return !RVDAUIOriginal(binding) && (binding.parts&RVDeArrowPartsThumbnail) &&
        binding.result.imageState==RVDeArrowImageStateReady ? binding.result.thumbnailImage : nil;
}
static BOOL RVDAUIFrameFallback(RVDAUIBinding *binding) {
    RVDeArrowResult *result=binding.result;
    return !RVDAUIOriginal(binding) && (binding.parts&RVDeArrowPartsThumbnail) && result.isFinal &&
        result.thumbnailDecision!=RVDeArrowDecisionOriginal && result.thumbnailDecision!=RVDeArrowDecisionDisabled &&
        result.imageState!=RVDeArrowImageStateReady && [RVSetting(@"dearrow_fallback") isEqual:@"stills"] && RVDAFrameURL(binding.video);
}
static void RVDAUIRefresh(RVDAUIBinding *binding) {
    if (!NSThread.isMainThread || binding.refreshQueued) return;binding.refreshQueued=YES;
    __weak RVDAUIBinding *weak=binding;uint64_t generation=binding.generation;
    dispatch_async(dispatch_get_main_queue(),^{
        RVDAUIBinding *state=weak;if (!state) return;state.refreshQueued=NO;
        if (state.generation!=generation || !RVDAUICurrent(state)) { RVDAUIEvent(@"D11_lifecycle_rebound",state,@"stale_generation");return; }
        id section=state.section,delegate=RVObject(section,"sectionControllerDelegate");
        if (!RVCan(delegate,"indexPathForIndex:inSection:",@"@@:Q@") || !RVCan(delegate,"isValidIndexPath:",@"B@:@") || !RVCan(section,"reloadCellAtIndex:",@"v@:q")) {
            RVDAUIEvent(@"D10_native_layout",state,@"native_refresh_contract_missing");return;
        }
        id path=((id(*)(id,SEL,NSUInteger,id))objc_msgSend)(delegate,sel_registerName("indexPathForIndex:inSection:"),state.index,section);
        if (!path || !((BOOL(*)(id,SEL,id))objc_msgSend)(delegate,sel_registerName("isValidIndexPath:"),path)) { RVDAUIEvent(@"D10_native_layout",state,@"stale_index");return; }
        ((void(*)(id,SEL,NSInteger))objc_msgSend)(section,sel_registerName("reloadCellAtIndex:"),state.index);
        RVDAUIEvent(@"D10_native_layout_requested",state,nil);
        [state.request recordCheckpoint:RVDeArrowCheckpointNativeLayout blocker:RVDeArrowBlockerNone];
    });
}
static void RVDAUIConfigureBackend(void) {
    RVDeArrowOptions *options=RVDeArrowOptions.defaultOptions;
    options.brandingEndpoint=[NSURL URLWithString:RVSetting(@"dearrow_branding_url") ?: @"https://sponsor.ajay.app/api/branding"];
    options.thumbnailEndpoint=[NSURL URLWithString:RVSetting(@"dearrow_url") ?: @"https://dearrow-thumb.ajay.app/api/v1/getThumbnail"];
    options.diagnosticsEnabled=RVEnabled(@"diagnostics");
    if (![RVDeArrowBackend.sharedBackend configure:options]) RVDAUIEvent(@"D01_adapter_ready",nil,@"invalid_options");
}
static void RVDAUIRequest(RVDAUIBinding *binding) {
    [binding.request cancel];binding.request=nil;binding.result=nil;binding.imageBytes=nil;binding.encodedImage=nil;
    binding.generation=++RVDAUIGeneration;binding.settingsRevision=RVDAUISettingsRevision;
    BOOL title=(!binding.modern || binding.profile.titleRole) && RVDAUIPolicy(binding.surface,YES);
    BOOL image=(!binding.modern || binding.profile.imageRole) && RVDAUIPolicy(binding.surface,NO) && ![binding.surface isEqual:@"player"];
    binding.parts=(title ? RVDeArrowPartsTitle : 0)|(image ? RVDeArrowPartsThumbnail : 0);
    binding.titleBlocker=title ? @"awaiting_metadata" : (binding.modern && !binding.profile.titleRole ? binding.profile.blocker ?: @"title_role_unknown" : @"policy_off");
    binding.imageBlocker=image ? @"awaiting_metadata" : (binding.modern && !binding.profile.imageRole ? binding.profile.blocker ?: @"image_role_unknown" : @"policy_off");
    if (RVDAUIOriginal(binding)) { binding.titleBlocker=@"original_selected";binding.imageBlocker=@"original_selected"; }
    if (!binding.parts || RVDAUIOriginal(binding)) return;
    uint64_t generation=binding.generation;__weak RVDAUIBinding *weak=binding;
    binding.request=[RVDeArrowBackend.sharedBackend requestVideoID:binding.video parts:binding.parts clientGeneration:generation completion:^(RVDeArrowResult *result) {
        RVDAUIBinding *state=weak;
        if (!state || !RVDAUICurrent(state) || generation!=state.generation || result.clientGeneration!=generation ||
            result.configurationRevision!=RVDeArrowBackend.sharedBackend.configurationRevision || ![result.videoID isEqual:state.video]) {
            RVDAUIEvent(@"D09_replacement_applied",state,@"stale_generation");return;
        }
        state.result=result;state.titleBlocker=RVDABlockerName(result.titleBlocker);
        state.imageBlocker=RVDABlockerName(result.imageState==RVDeArrowImageStateUnavailable ? result.imageBlocker : result.thumbnailSelectionBlocker);
        RVDAUIRefresh(state); // titles do not wait for image decode/encoding
        if (state.modern && result.imageState==RVDeArrowImageStateReady && result.thumbnailImage && state.encodedImage!=result.thumbnailImage) {
            state.encodedImage=result.thumbnailImage;UIImage *image=result.thumbnailImage;
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
                @autoreleasepool {
                    NSData *bytes=UIImageJPEGRepresentation(image,.9);
                    dispatch_async(dispatch_get_main_queue(),^{
                        RVDAUIBinding *current=weak;
                        if (!current || !RVDAUICurrent(current) || current.generation!=generation || current.result.thumbnailImage!=image) return;
                        if (bytes.length && bytes.length<=1024*1024) { current.imageBytes=bytes;RVDAUIRefresh(current); }
                        else { current.imageBlocker=@"presentation_image_byte_limit";RVDAUIEvent(@"D08_image_received",current,current.imageBlocker); }
                    });
                }
            });
        }
    }];
    [binding.request recordCheckpoint:RVDeArrowCheckpointOwnerBound blocker:RVDeArrowBlockerNone];
    [binding.request recordCheckpoint:RVDeArrowCheckpointVideoBound blocker:RVDeArrowBlockerNone];
    [binding.request recordCheckpoint:RVDeArrowCheckpointRoleBound blocker:RVDeArrowBlockerNone];
}
static id RVDAUILegacyRenderer(id entry,NSString **unionSelector) {
    NSString *name=NSStringFromClass([entry class]);
    if ([@[@"YTIVideoRenderer",@"YTICompactVideoRenderer",@"YTIVideoWithContextRenderer",@"YTIVideoCardRenderer",@"YTISlimVideoInformationRenderer"] containsObject:name]) return entry;
    if (![@[@"YTISectionListSupportedRenderers",@"YTIItemSectionSupportedRenderers",@"YTIGridSupportedRenderers",@"YTIHorizontalListSupportedRenderers",@"YTIShelfSupportedRenderers",@"YTIRenderer"] containsObject:name]) return nil;
    for (NSString *name in @[@"videoRenderer",@"compactVideoRenderer",@"videoWithContextRenderer",@"videoCardRenderer"]) {
        NSString *presence=[NSString stringWithFormat:@"has%@%@",[[name substringToIndex:1] uppercaseString],[name substringFromIndex:1]];
        if (!RVAdResolve(entry,presence.UTF8String,@"B@:") || !RVBool(entry,presence.UTF8String)) continue;
        id renderer=RVAdResolvedObject(entry,name.UTF8String);if (renderer) { if (unionSelector) *unionSelector=name;return renderer; }
    }return nil;
}
static RVDAUIBinding *RVDAUIBind(id controller,id entry,id section,NSInteger index) {
    if (!NSThread.isMainThread || !controller || !entry || !section || index<0) return nil;
    RVDAUIBinding *prior=objc_getAssociatedObject(controller,RVDAUIBindingKey);
    BOOL sameSource=!prior.modern || [RVObject(RVAdRootElement(entry),"elementData") isEqual:prior.profile.source];
    if (prior.legacySource) sameSource=[RVObject(RVDAUILegacyRenderer(entry,NULL),"data") isEqual:prior.legacySource];
    if ([prior.surface isEqual:@"player"]) {
        NSString *watchVideo=nil;RVDAUIOwnerSurface(controller,YES,&watchVideo);
        sameSource=sameSource && [watchVideo isEqual:prior.video];
    }
    if (prior && sameSource && prior.originalEntry==entry && prior.section==section && prior.index==index) {
        NSString *surface=RVDAUIOwnerSurface(controller,prior.modern ? prior.profile.watchTitle : [prior.surface isEqual:@"player"],NULL);
        BOOL changed=![surface isEqual:prior.surface];if (changed) prior.surface=surface;
        if (changed || prior.settingsRevision!=RVDAUISettingsRevision) RVDAUIRequest(prior);return prior;
    }
    [prior.request cancel];
    RVDAUIBinding *state=[RVDAUIBinding new];state.controller=controller;state.section=section;state.index=index;state.originalEntry=entry;
    id element=RVAdRootElement(entry);NSString *watchVideo=nil;
    // Resolve a watch ID through its explicit metadata owner, never current player.
    RVDAUIOwnerSurface(controller,YES,&watchVideo);
    if (element) {
        id options=RVAdResolvedObject(element,"compatibilityOptions");
        if (!RVAdResolve(options,"hasAdLoggingData",@"B@:") || RVBool(options,"hasAdLoggingData")) { RVDAUIEvent(@"D03_video_bound",state,@"ad_status_unknown_or_ad_root");return nil; }
        state.profile=RVDAProfile(RVObject(element,"elementData"),watchVideo);state.modern=YES;
        if (!state.profile) {
            NSData *data=RVObject(element,"elementData");
            BOOL shared=RVDAPath(data,@[@1,@484616221])!=nil;
            RVDAUIEvent(@"D04_role_bound",state,shared ? @"shared_component_input_unavailable" : @"template_or_video_profile_unknown");return nil;
        }
        state.video=state.profile.video;state.route=state.profile.route;
        state.surface=RVDAUIOwnerSurface(controller,state.profile.watchTitle,&watchVideo);
        state.titleBlocker=state.profile.titleRole ? @"awaiting_metadata" : state.profile.blocker ?: @"title_role_unknown";
        state.imageBlocker=state.profile.imageRole ? @"awaiting_metadata" : state.profile.blocker ?: @"image_role_unknown";
    } else {
        id renderer=RVDAUILegacyRenderer(entry,NULL);
        if (!renderer) return nil;state.route=NSStringFromClass([renderer class]);
        BOOL slim=[state.route isEqual:@"YTISlimVideoInformationRenderer"];
        if (slim) {
            if (!RVVideoIDValid(watchVideo) || !RVOrdinaryContent(RVCurrentPlayback) || ![RVObject(RVCurrentPlayback,"contentVideoID") isEqual:watchVideo]) return nil;
            state.video=watchVideo;state.surface=RVDAUIOwnerSurface(controller,YES,NULL);
        } else {
            if (!RVAdResolve(renderer,"isAd",@"B@:") || RVBool(renderer,"isAd")) return nil;
            state.video=RVAdResolvedObject(renderer,"videoId");state.surface=RVDAUIOwnerSurface(controller,NO,NULL);
        }
        // Legacy image leases are initially limited to ordinary full/compact cells.
        if (![@[@"YTIVideoRenderer",@"YTICompactVideoRenderer",@"YTISlimVideoInformationRenderer"] containsObject:state.route]) return nil;
        NSData *source=RVObject(renderer,"data");if ([source isKindOfClass:NSData.class] && source.length<=2*1024*1024) state.legacySource=[source copy];
    }
    if (!RVVideoIDValid(state.video) || !state.surface) { RVDAUIEvent(@"D02_owner_bound",state,@"owner_or_video_unknown");return nil; }
    objc_setAssociatedObject(controller,RVDAUIBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [RVDAUIBindings addObject:state];RVDAUIEvent(@"D04_role_bound",state,state.profile.blocker);RVDAUIRequest(state);return state;
}
static id RVDAUIProjectedEntry(RVDAUIBinding *state) {
    if (!state || state.settingsRevision!=RVDAUISettingsRevision || RVDAUIOriginal(state)) return state.originalEntry;
    NSString *title=RVDAUITitle(state);NSData *image=RVDAUIImage(state) ? state.imageBytes : nil;
    if (state.modern) {
        NSData *next=RVDAProject(state.profile,title,image,RVDAUIFrameFallback(state));
        if (!next || [next isEqual:state.profile.source]) return state.originalEntry;
        id element=RVAdRootElement(state.originalEntry);
        if (![element conformsToProtocol:@protocol(NSCopying)]) return state.originalEntry;
        id copy=[element copy];objc_setAssociatedObject(copy,RVDAUIDataKey,next,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        id result=copy;
        if (element!=state.originalEntry) {
            if (![state.originalEntry conformsToProtocol:@protocol(NSCopying)] || !RVAdResolve(state.originalEntry,"setElementRenderer:",@"v@:@")) return state.originalEntry;
            result=[state.originalEntry copy];RVSetObject(result,"setElementRenderer:",copy);
        }
        RVDAUIEvent(@"presentation_model_projected",state,nil);return result;
    }
    BOOL fallback=RVDAUIFrameFallback(state);if (!title && !fallback) return state.originalEntry;
    NSString *unionSelector=nil;id renderer=RVDAUILegacyRenderer(state.originalEntry,&unionSelector);
    if (![renderer conformsToProtocol:@protocol(NSCopying)]) return state.originalEntry;
    id copy=[renderer copy];
    if (title) {
        if (!RVAdResolve(renderer,"setTitle:",@"v@:@")) return state.originalEntry;
        id value=[NSClassFromString(@"YTIFormattedString") new];
        if (!RVAdResolve(value,"setSimpleText:",@"v@:@")) return state.originalEntry;
        RVSetObject(value,"setSimpleText:",title);RVSetObject(copy,"setTitle:",value);
    }
    if (fallback && RVAdResolve(copy,"setThumbnail:",@"v@:@")) {
        id thumbnail=[NSClassFromString(@"YTIThumbnailDetails") new],source=[NSClassFromString(@"YTIThumbnailDetails_Thumbnail") new];
        if (RVAdResolve(thumbnail,"setThumbnailsArray:",@"v@:@") && RVAdResolve(source,"setURL:",@"v@:@")) {
            RVSetObject(source,"setURL:",RVDAFrameURL(state.video));RVSetObject(thumbnail,"setThumbnailsArray:",[NSMutableArray arrayWithObject:source]);RVSetObject(copy,"setThumbnail:",thumbnail);
        }
    }
    id result=copy;
    if (unionSelector) {
        NSString *setter=[NSString stringWithFormat:@"set%@%@:",[[unionSelector substringToIndex:1] uppercaseString],[unionSelector substringFromIndex:1]];
        if (![state.originalEntry conformsToProtocol:@protocol(NSCopying)] || !RVAdResolve(state.originalEntry,setter.UTF8String,@"v@:@")) return state.originalEntry;
        result=[state.originalEntry copy];RVSetObject(result,setter.UTF8String,copy);
    }
    objc_setAssociatedObject(copy,RVDAUIModelKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);return result;
}
static void RVDAUILegacyView(id view,id renderer,void (^configure)(id)) {
    id titleView=RVObject(view,"titleLabel") ?: RVObject(RVObject(view,"metadataView"),"titleLabel");
    RVDAUIRestoreAccessibility(titleView);
    id thumbnail=RVObject(view,"thumbnailView");
    id imageView=RVObject(thumbnail,"imageView");
    RVDAUIImageLease *old=objc_getAssociatedObject(imageView,RVDAUIImageKey);
    if (old) {
        UIImage *native=old.binding.nativeImage;objc_setAssociatedObject(imageView,RVDAUIImageKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        if (RVCan(imageView,"setImage:animated:",@"v@:@B")) ((void(*)(id,SEL,id,BOOL))objc_msgSend)(imageView,sel_registerName("setImage:animated:"),native,NO);
    }
    configure(renderer); // Native configuration and reuse always run first.
    RVDAUIBinding *state=RVDAUIConfiguring;
    if (!state || state.modern || !RVDAUICurrent(state)) return;
    NSString *title=RVDAUITitle(state);titleView=RVObject(view,"titleLabel") ?: RVObject(RVObject(view,"metadataView"),"titleLabel");
    RVDAUIApplyAccessibility(titleView,title);
    imageView=RVObject(RVObject(view,"thumbnailView"),"imageView");
    if (RVDAUIImage(state) && RVCan(imageView,"setImage:animated:",@"v@:@B")) {
        state.nativeImage=RVObject(imageView,"image");state.imageView=imageView;
        RVDAUIImageLease *lease=[RVDAUIImageLease new];lease.binding=state;
        objc_setAssociatedObject(imageView,RVDAUIImageKey,lease,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        ((void(*)(id,SEL,id,BOOL))objc_msgSend)(imageView,sel_registerName("setImage:animated:"),RVDAUIImage(state),NO);
    }
    RVDAUIEvent(@"D09_replacement_applied",state,nil);
}
static void RVDAUIPreferencesChanged(void) {
    if (!NSThread.isMainThread) { dispatch_async(dispatch_get_main_queue(),^{ RVDAUIPreferencesChanged(); });return; }
    RVDAUISettingsRevision++;RVDAUIConfigureBackend();
    for (RVDAUIBinding *state in RVDAUIBindings.allObjects) {
        RVDAUIRequest(state);RVDAUIEvent(@"D12_original_restore_or_rebind_requested",state,nil);RVDAUIRefresh(state);
    }
}
static NSDictionary *RVDAUIReport(void) {
    NSMutableArray *bindings=[NSMutableArray array];
    for (RVDAUIBinding *state in RVDAUIBindings.allObjects) {
        if (bindings.count>=32) break;
        [bindings addObject:@{@"surface":state.surface ?: @"unknown",@"route":state.route ?: @"unresolved",
            @"generation":@(state.generation),@"current_source":@(RVDAUICurrent(state)),@"requested_parts":@(state.parts),
            @"title_blocker":state.titleBlocker ?: @"policy_off",@"image_blocker":state.imageBlocker ?: @"policy_off",
            @"original_selected":@(RVDAUIOriginal(state)),@"title_role":@(!state.modern || state.profile.titleRole),
            @"image_role":@(state.modern ? state.profile.imageRole : ![state.surface isEqual:@"player"]),@"encoded_image_ready":@(state.imageBytes!=nil)}];
    }
    NSArray *history;@synchronized(RVDAUIBinding.class) { history=RVDAUIHistory.copy ?: @[]; }
    return @{@"schema":@1,@"settings_revision":@(RVDAUISettingsRevision),@"backend":RVDeArrowBackend.sharedBackend.diagnostics,
        @"bindings":bindings,@"history":history,@"hooks":RVDAUIHooks.copy ?: @[],@"device_verified":@NO};
}
static BOOL RVDAUISetCurrentOriginal(BOOL original) {
    // This is an explicit user action on the watched video, not a feed binding rule.
    NSString *video=RVObject(RVCurrentPlayback,"contentVideoID");if (!RVVideoIDValid(video)) return NO;
    if (original) { if (RVDAUIOriginalVideos.count>=128) [RVDAUIOriginalVideos removeAllObjects];[RVDAUIOriginalVideos addObject:video]; }
    else [RVDAUIOriginalVideos removeObject:video];RVDAUIPreferencesChanged();return YES;
}
static void RVDAUIClearCaches(void) { [RVDeArrowBackend.sharedBackend clearCaches];RVDAUIPreferencesChanged(); }
static BOOL RVDAUIHook(NSString *className,NSString *selector,NSString *abi,BOOL classMethod,RVReplacementFactory factory) {
    BOOL installed=RVHook(className,selector,abi,classMethod,factory);
    [RVDAUIHooks addObject:@{@"class":className,@"selector":selector,@"expected_abi":abi,@"installed":@(installed)}];return installed;
}

static void RVInstallDeArrowUI(void) {
    RVDAUIStarted=NSProcessInfo.processInfo.systemUptime;
    RVDAUIBindings=[NSHashTable weakObjectsHashTable];RVDAUIOriginalVideos=[NSMutableSet set];RVDAUIHooks=[NSMutableArray array];RVDAUIConfigureBackend();
    RVDAUIHook(@"YTIElementRenderer",@"elementData",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) { id native=((id(*)(id,SEL))original)(object,sel);return objc_getAssociatedObject(object,RVDAUIDataKey) ?: native; };
    });
    // Only the materialization argument changes; native controller entries and
    // the shared response graph remain the authoritative original identities.
    RVDAUIHook(@"YTSectionController",@"nodeBlockForItemAtIndex:",@"@?@:q",NO,^id(IMP original,SEL sel) {
        return ^id(id section,NSInteger index) {
            id prior=RVDAUISection;NSInteger old=RVDAUIIndex;RVDAUISection=section;RVDAUIIndex=index;
            @try { return ((id(*)(id,SEL,NSInteger))original)(section,sel,index); }
            @finally { RVDAUISection=prior;RVDAUIIndex=old; }
        };
    });
    RVDAUIHook(@"ELMCellNode",@"nodeBlockForEntry:responder:clientDataObservable:blockRegistry:blockRegistryProvider:",@"@?@:@@@@@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id entry,id responder,id observable,id registry,id provider) {
            RVDAUIBinding *state=RVDAUISection ? RVDAUIBind(responder,entry,RVDAUISection,RVDAUIIndex) : nil;
            id next=state ? RVDAUIProjectedEntry(state) : entry;
            id (^native)(void)=((id(*)(id,SEL,id,id,id,id,id))original)(cls,sel,next,responder,observable,registry,provider);
            if (!state || next==entry || !native) return native;
            __weak RVDAUIBinding *weak=state;uint64_t generation=state.generation;
            // Native block descriptor: @8@?0. Native construction may run off
            // main; ownership diagnostics return to main after it completes.
            return [^id(void) {
                id node=native();__weak id weakNode=node;
                dispatch_async(dispatch_get_main_queue(),^{
                    RVDAUIBinding *current=weak;
                    if (current && weakNode && current.generation==generation && RVDAUICurrent(current)) {
                        RVDAUIEvent(@"D09_native_target_materialized",current,nil);
                        [current.request recordCheckpoint:RVDeArrowCheckpointReplacementApplied blocker:RVDeArrowBlockerNone];
                    }
                });return node;
            } copy];
        };
    });
    RVDAUIHook(@"YTSectionController",@"updateCell:forIndex:",@"v@:@q",NO,^id(IMP original,SEL sel) {
        return ^(id section,id cell,NSInteger index) {
            id prior=RVDAUISection;NSInteger old=RVDAUIIndex;RVDAUISection=section;RVDAUIIndex=index;
            @try { ((void(*)(id,SEL,id,NSInteger))original)(section,sel,cell,index); }
            @finally { RVDAUISection=prior;RVDAUIIndex=old; }
        };
    });
    RVDAUIHook(@"YTCellController",@"setCell:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id cell) {
            RVDAUIBinding *state=cell && RVDAUISection ? RVDAUIBind(controller,RVObject(controller,"entry"),RVDAUISection,RVDAUIIndex) : nil;
            id prior=RVDAUIConfiguring;RVDAUIConfiguring=state;
            @try { ((void(*)(id,SEL,id))original)(controller,sel,cell); }
            @finally { RVDAUIConfiguring=prior; }
        };
    });
    for (NSString *cls in @[@"YTVideoCell",@"YTCompactVideoCell",@"YTSlimVideoInformationCell"]) RVDAUIHook(cls,@"setEntry:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id cell,id entry) {
            if ([cls isEqual:@"YTSlimVideoInformationCell"]) RVDAUIRestoreAccessibility(RVObject(cell,"titleLabel"));
            RVDAUIBinding *state=RVDAUIConfiguring;
            id next=state && state.originalEntry==entry ? RVDAUIProjectedEntry(state) : entry;
            ((void(*)(id,SEL,id))original)(cell,sel,next);
            if ([cls isEqual:@"YTSlimVideoInformationCell"] && state && RVDAUITitle(state)) {
                RVDAUIApplyAccessibility(RVObject(cell,"titleLabel"),RVDAUITitle(state));
                RVDAUIEvent(@"D09_replacement_applied",state,nil);
            }
        };
    });
    for (NSArray *route in @[@[@"YTVideoView",@"setModel:"],@[@"YTCompactVideoView",@"setRenderer:"]]) RVDAUIHook(route[0],route[1],@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id view,id renderer) { RVDAUILegacyView(view,renderer,^(id value) { ((void(*)(id,SEL,id))original)(view,sel,value); }); };
    });
    RVDAUIHook(@"YTImageView",@"setImage:animated:",@"v@:@B",NO,^id(IMP original,SEL sel) {
        return ^(id view,id image,BOOL animated) {
            RVDAUIImageLease *lease=objc_getAssociatedObject(view,RVDAUIImageKey);RVDAUIBinding *state=lease.binding;
            UIImage *custom=state && RVDAUICurrent(state) && state.imageView==view ? RVDAUIImage(state) : nil;
            if (state && image!=custom && [image isKindOfClass:UIImage.class]) state.nativeImage=image;
            ((void(*)(id,SEL,id,BOOL))original)(view,sel,custom ?: image,custom ? NO : animated);
        };
    });
    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
        RVDAUIPreferencesChanged();
    }];
    RVDAUIEvent(@"D01_adapter_ready",nil,nil);
}
