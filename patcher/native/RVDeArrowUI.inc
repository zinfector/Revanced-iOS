// SPDX-License-Identifier: MIT
// Consumer-owned DeArrow settings, generations and native presentation adapters.
#include "RVDeArrowProjection.inc"
#include "RVDeArrowOptions.inc"
static const void *RVDAUIBindingKey=&RVDAUIBindingKey,*RVDAUIDataKey=&RVDAUIDataKey;
static const void *RVDAUIModelKey=&RVDAUIModelKey,*RVDAUIImageKey=&RVDAUIImageKey;
static const void *RVDAUIAccessibilityKey=&RVDAUIAccessibilityKey;
static NSHashTable *RVDAUIBindings;
static NSMutableArray *RVDAUIHistory;
static NSMutableArray *RVDAUIHooks;
static NSMutableSet<NSString *> *RVDAUIOriginalVideos;
static NSMutableSet<NSString *> *RVDAUICustomVideos;
static uint64_t RVDAUIGeneration,RVDAUISettingsRevision=1;
static NSTimeInterval RVDAUIStarted;
static RVDeArrowRequest *RVDAUIProbeRequest;
static NSDictionary *RVDAUIProbeResult;
static uint64_t RVDAUIProbeGeneration;
static _Thread_local __unsafe_unretained id RVDAUISection;
static _Thread_local NSInteger RVDAUIIndex;
static _Thread_local __unsafe_unretained id RVDAUIConfiguring;

@interface RVDAUIBinding : NSObject
@property(nonatomic,weak) id controller,section,imageView,nativeRootComponent;
@property(nonatomic,strong) id originalEntry;
@property(nonatomic,strong) RVDAProjection *profile;
@property(nonatomic,strong) RVDeArrowRequest *request;
@property(nonatomic,strong) RVDeArrowResult *result;
@property(nonatomic,strong) NSData *imageBytes;
@property(nonatomic,strong) NSData *legacySource;
@property(nonatomic,strong) UIImage *encodedImage,*nativeImage;
@property(nonatomic,strong) UIImage *presentationImage;
@property(nonatomic,strong) RVDAHTTP *fallbackTransfer;
@property(nonatomic,copy) NSString *originalTitle,*untranslatedTitle,*originalImageURL,*avatarURL,*computedTitle,*imageOrigin;
@property(nonatomic) double duration;
@property(nonatomic) BOOL upcoming,titleComputed,fallbackStarted,originalFetchStarted,pendingExpired;
@property(nonatomic) uint64_t imageRevision;
@property(nonatomic) NSTimeInterval fallbackDeadline;
@property(nonatomic) NSUInteger fallbackRetries;
@property(nonatomic) BOOL fallbackRetryScheduled;
@property(nonatomic,copy) NSString *fallbackPhase,*durationSource;
@property(nonatomic,copy) NSString *video,*surface,*route,*titleBlocker,*imageBlocker;
@property(nonatomic) NSInteger index;
@property(nonatomic) uint64_t generation,settingsRevision;
@property(nonatomic) RVDeArrowParts parts;
@property(nonatomic) RVDeArrowParts serviceParts;
@property(nonatomic) BOOL refreshQueued,modern;
@property(nonatomic,copy) NSString *lastCheckpoint,*lastBlocker;
@property(nonatomic) NSUInteger refreshRequests,materializations;
@property(nonatomic) BOOL titleConsumerVerified,imageConsumerVerified;
@property(nonatomic) NSUInteger consumerObservations;
@property(nonatomic,strong) NSDictionary *consumerCheckpoint;
@property(nonatomic,strong) NSDictionary *ownerSummary;
@end
@implementation RVDAUIBinding
- (void)dealloc { [_request cancel];[_fallbackTransfer cancel]; }
@end
@interface RVDAUIImageLease : NSObject
@property(nonatomic,weak) RVDAUIBinding *binding;
@end
@implementation RVDAUIImageLease @end
static BOOL RVDAUIPolicy(NSString *surface,BOOL title);
static NSString *RVDAUIOwnerSurface(id owner,BOOL watchTitle,NSString **watchVideo);
#include "RVDeArrowDiagnostics.inc"
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
    if (NSThread.isMainThread && binding) { binding.lastCheckpoint=checkpoint;binding.lastBlocker=blocker ?: @"ready"; }
    RVDADiagRecord(checkpoint,binding,blocker,nil);
}
static RVDAUIBinding *RVDAUIReject(RVDAUIBinding *binding,NSString *checkpoint,NSString *blocker) {
    if ([checkpoint isEqual:@"D04_role_bound"]) checkpoint=@"D04_role_rejected";
    RVDADiagCount([NSString stringWithFormat:@"rejected.surface.%@",binding.surface ?: @"unknown"]);
    RVDAUIEvent(checkpoint,binding,blocker);RVDADiagAttempt(binding.controller,binding.originalEntry,binding.section,binding.index,blocker);return nil;
}
static BOOL RVDAUIPolicy(NSString *surface,BOOL title) {
    if (!RVCompatible || !surface) return NO;
    id overrides=RVSetting(title ? @"dearrow_title_modes" : @"dearrow_thumbnail_modes");
    NSString *mode=[overrides isKindOfClass:NSDictionary.class] ? overrides[surface] : nil;
    if ([mode isEqual:@"custom"]) return YES;
    if ([mode isEqual:@"original"]) return NO;
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
static NSString *RVDAUICurrentBlocker(RVDAUIBinding *binding) {
    if (!binding) return @"binding_released";
    if (!NSThread.isMainThread) return @"ownership_check_off_main";
    if (binding.settingsRevision!=RVDAUISettingsRevision) return @"settings_revision_changed";
    if (!binding.controller) return @"controller_released";
    if (objc_getAssociatedObject(binding.controller,RVDAUIBindingKey)!=binding) return @"binding_replaced";
    id section=binding.section;
    if (!section) return @"section_released";
    if (binding.index<0 || !RVCan(section,"cellControllerAtIndex:",@"@@:q")) return @"controller_lookup_contract_missing";
    // Guard bounds before a native lookup; never reload an obsolete index.
    id entries=RVObject(RVObject(section,"viewModel"),"entries");
    if (![entries isKindOfClass:NSArray.class]) return @"view_model_entries_contract_missing";
    if ((NSUInteger)binding.index>=[entries count]) return @"row_out_of_bounds";
    if (entries[binding.index]!=binding.originalEntry) return @"section_entry_identity_changed";
    id controller=((id(*)(id,SEL,NSInteger))objc_msgSend)(section,sel_registerName("cellControllerAtIndex:"),binding.index);
    if (controller!=binding.controller) return @"row_controller_identity_changed";
    if (RVObject(controller,"entry")!=binding.originalEntry) return @"controller_entry_identity_changed";
    NSString *watchVideo=nil;
    NSString *surface=RVDAUIOwnerSurface(binding.controller,binding.modern ? binding.profile.watchTitle : [binding.surface isEqual:@"player"],&watchVideo);
    if (![surface isEqual:binding.surface]) return @"owner_surface_changed_or_unknown";
    if ([surface isEqual:@"player"] && ![watchVideo isEqual:binding.video]) return @"watch_video_identity_changed";
    if (binding.modern) {
        NSData *current=RVObject(RVAdRootElement(binding.originalEntry),"elementData");
        if (![current isEqual:binding.profile.source]) return @"serialized_source_changed";
        if (binding.profile.watchTitle && ![watchVideo isEqual:binding.video]) return @"watch_video_identity_changed";
    } else if (binding.legacySource) {
        NSData *current=RVObject(RVDAUILegacyRenderer(binding.originalEntry,NULL),"data");
        if (![current isEqual:binding.legacySource]) return @"legacy_source_changed";
    }
    return nil;
}
static BOOL RVDAUICurrent(RVDAUIBinding *binding) { return RVDAUICurrentBlocker(binding)==nil; }
static BOOL RVDAUIOriginal(RVDAUIBinding *binding) { return [RVDAUIOriginalVideos containsObject:binding.video] || (RVEnabled(@"dearrow_default_original") && ![RVDAUICustomVideos containsObject:binding.video]); }
static BOOL RVDAUICasual(RVDAUIBinding *binding) {
    if (![RVSetting(@"dearrow_mode") isEqual:@"casual"] || !binding.originalTitle.length) return NO;
    NSString *original=[[binding.originalTitle stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] lowercaseString];
    for (NSDictionary *vote in binding.result.casualVotes) {
        NSString *title=vote[@"title"];
        if ([RVSetting(@"dearrow_casual_categories") containsObject:vote[@"id"]] && [vote[@"count"] doubleValue]>=1 && (!title.length || [title.lowercaseString isEqual:original])) return YES;
    }return NO;
}
static NSString *RVDAUITitle(RVDAUIBinding *binding) {
    if (binding.profile.liveCard && !binding.titleConsumerVerified) return nil;
    if (RVDAUIOriginal(binding) || !(binding.parts&RVDeArrowPartsTitle)) return nil;
    if (!binding.result && (binding.serviceParts&RVDeArrowPartsTitle) && RVEnabled(@"dearrow_hide_while_fetching") && !binding.pendingExpired && binding.originalTitle.length) return @"\u200b";
    if (!binding.titleComputed) {
        BOOL custom=RVEnabled(@"dearrow_use_crowdsourced_titles") && binding.result.titleDecision==RVDeArrowDecisionCustom && !RVDAUICasual(binding);
        NSString *source=custom ? binding.result.title : RVEnabled(@"dearrow_ignore_translated_titles") && binding.untranslatedTitle.length ? binding.untranslatedTitle : binding.originalTitle;
        NSString *formatted=RVDAFormatTitle(source,custom);
        binding.computedTitle=[formatted isEqual:binding.originalTitle] ? nil : formatted;binding.titleComputed=YES;
    }return binding.computedTitle;
}
static UIImage *RVDAUIImage(RVDAUIBinding *binding) {
    if (binding.profile.liveCard && !binding.imageConsumerVerified) return nil;
    if (RVDAUIOriginal(binding) || !(binding.parts&RVDeArrowPartsThumbnail)) return nil;
    return binding.presentationImage;
}
static BOOL RVDAUIFrameFallback(RVDAUIBinding *binding) {
    // Fallbacks now deliver fully decoded images through the same presentation
    // lease as community thumbnails; a cached URL prefix is not image readiness.
    return NO;
}
static void RVDAUIRefresh(RVDAUIBinding *binding) {
    if (!NSThread.isMainThread || binding.refreshQueued) return;binding.refreshQueued=YES;
    __weak RVDAUIBinding *weak=binding;uint64_t generation=binding.generation;
    dispatch_async(dispatch_get_main_queue(),^{
        RVDAUIBinding *state=weak;if (!state) return;state.refreshQueued=NO;
        NSString *blocker=RVDAUICurrentBlocker(state);
        if (state.generation!=generation || blocker) { RVDAUIEvent(@"D11_lifecycle_rebound",state,blocker ?: @"client_generation_changed");return; }
        id section=state.section,delegate=RVObject(section,"sectionControllerDelegate");
        if (!RVCan(delegate,"indexPathForIndex:inSection:",@"@@:Q@") || !RVCan(delegate,"isValidIndexPath:",@"B@:@") || !RVCan(section,"reloadCellAtIndex:",@"v@:q")) {
            RVDAUIEvent(@"D10_native_layout",state,@"native_refresh_contract_missing");return;
        }
        id path=((id(*)(id,SEL,NSUInteger,id))objc_msgSend)(delegate,sel_registerName("indexPathForIndex:inSection:"),state.index,section);
        if (!path || !((BOOL(*)(id,SEL,id))objc_msgSend)(delegate,sel_registerName("isValidIndexPath:"),path)) { RVDAUIEvent(@"D10_native_layout",state,@"stale_index");return; }
        ((void(*)(id,SEL,NSInteger))objc_msgSend)(section,sel_registerName("reloadCellAtIndex:"),state.index);
        state.refreshRequests++;
        RVDAUIEvent(@"D10_native_layout_requested",state,nil);
        [state.request recordCheckpoint:RVDeArrowCheckpointNativeLayout blocker:RVDeArrowBlockerNone];
    });
}
#include "RVDeArrowConsumers.inc"
static void RVDAUIConfigureBackend(void) {
    RVDeArrowOptions *options=RVDeArrowOptions.defaultOptions;
    options.brandingEndpoint=[NSURL URLWithString:RVSetting(@"dearrow_branding_url") ?: @"https://sponsor.ajay.app/api/branding"];
    options.thumbnailEndpoint=[NSURL URLWithString:RVSetting(@"dearrow_url") ?: @"https://dearrow-thumb.ajay.app/api/v1/getThumbnail"];
    options.diagnosticsEnabled=RVDADiagEnabled();
    if (![RVDeArrowBackend.sharedBackend configure:options]) RVDAUIEvent(@"D01_adapter_ready",nil,@"invalid_options");
}
#include "RVDeArrowImages.inc"
static void RVDAUIRequest(RVDAUIBinding *binding) {
    [binding.request cancel];binding.request=nil;binding.result=nil;binding.imageBytes=nil;binding.encodedImage=nil;
    [binding.fallbackTransfer cancel];binding.fallbackTransfer=nil;binding.presentationImage=nil;binding.imageRevision++;
    binding.computedTitle=nil;binding.titleComputed=NO;binding.fallbackStarted=NO;binding.originalFetchStarted=NO;binding.pendingExpired=NO;binding.imageOrigin=nil;
    binding.fallbackDeadline=0;binding.fallbackRetries=0;binding.fallbackRetryScheduled=NO;binding.fallbackPhase=@"idle";
    binding.generation=++RVDAUIGeneration;binding.settingsRevision=RVDAUISettingsRevision;
    BOOL title=(!binding.modern || binding.profile.titleRole) && RVDAUIPolicy(binding.surface,YES);
    BOOL image=(!binding.modern || binding.profile.imageRole) && RVDAUIPolicy(binding.surface,NO) && ![binding.surface isEqual:@"player"];
    binding.parts=(title ? RVDeArrowPartsTitle : 0)|(image ? RVDeArrowPartsThumbnail : 0);
    binding.serviceParts=binding.parts;
    if (!RVEnabled(@"dearrow_use_crowdsourced_titles") && ![RVSetting(@"dearrow_mode") isEqual:@"casual"]) binding.serviceParts&=~RVDeArrowPartsTitle;
    binding.titleBlocker=title ? @"awaiting_metadata" : (binding.modern && !binding.profile.titleRole ? binding.profile.blocker ?: @"title_role_unknown" : @"policy_off");
    binding.imageBlocker=image ? @"awaiting_metadata" : (binding.modern && !binding.profile.imageRole ? binding.profile.blocker ?: @"image_role_unknown" : @"policy_off");
    if (RVDAUIOriginal(binding)) { binding.titleBlocker=@"original_selected";binding.imageBlocker=@"original_selected"; }
    RVDADiagRecord(@"D05_policy_evaluated",binding,!binding.parts ? @"policy_or_role_off" : RVDAUIOriginal(binding) ? @"original_selected" : nil,
        @{@"title_policy":@(RVDAUIPolicy(binding.surface,YES)),@"image_policy":@(RVDAUIPolicy(binding.surface,NO)),@"parts":@(binding.parts),@"service_parts":@(binding.serviceParts)});
    if (!binding.serviceParts) { binding.pendingExpired=YES;RVDAUIRefresh(binding);return; }
    uint64_t generation=binding.generation;__weak RVDAUIBinding *weak=binding;
    if (!RVDAUIOriginal(binding) && RVEnabled(@"dearrow_hide_while_fetching")) {
        if (image) RVDAUIApplyImage(binding,RVDABlankImage(),@"pending",generation);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,15*NSEC_PER_SEC),dispatch_get_main_queue(),^{
            RVDAUIBinding *state=weak;if (!state || state.generation!=generation || !RVDAUICurrent(state)) return;
            state.pendingExpired=YES;if ([state.imageOrigin isEqual:@"pending"]) { state.presentationImage=nil;state.imageBytes=nil;state.imageRevision++; }
            RVDAUIEvent(@"D08_pending_deadline",state,nil);RVDAUIRefresh(state);
        });
    }
    binding.request=[RVDeArrowBackend.sharedBackend requestVideoID:binding.video parts:binding.serviceParts clientGeneration:generation completion:^(RVDeArrowResult *result) {
        RVDAUIBinding *state=weak;
        NSString *blocker=RVDAUICurrentBlocker(state);
        if (!blocker && (generation!=state.generation || result.clientGeneration!=generation)) blocker=@"client_generation_changed";
        if (!blocker && result.configurationRevision!=RVDeArrowBackend.sharedBackend.configurationRevision) blocker=@"backend_revision_changed";
        if (!blocker && ![result.videoID isEqual:state.video]) blocker=@"result_video_identity_changed";
        if (blocker) { RVDAUIEvent(@"D08_result_rejected",state,blocker);return; }
        RVDADiagRecord(@"D08_result_accepted",state,nil,@{@"title_decision":RVDADecisionName(result.titleDecision),
            @"image_decision":RVDADecisionName(result.thumbnailDecision),@"image_state":@(result.imageState),@"final":@(result.isFinal),
            @"metadata_http":@(result.metadataHTTPStatus),@"image_http":@(result.imageHTTPStatus),@"backend_revision":@(result.configurationRevision)});
        state.result=result;state.titleComputed=NO;state.computedTitle=nil;state.titleBlocker=RVDABlockerName(result.titleBlocker);
        state.imageBlocker=RVDABlockerName(result.imageState==RVDeArrowImageStateUnavailable ? result.imageBlocker : result.thumbnailSelectionBlocker);
        RVDAUIRefresh(state); // titles do not wait for image decode/encoding
        if (image && result.imageState==RVDeArrowImageStateReady && result.thumbnailImage) RVDAUIApplyImage(state,result.thumbnailImage,@"community",generation);
        else if (image && result.isFinal) RVDAUIResolveFallback(state,generation);
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
static BOOL RVDAUIFindSection(id controller,id entry,id *section,NSInteger *index) {
    if (!NSThread.isMainThread || RVObject(controller,"entry")!=entry) return NO;
    id owner=RVObject(controller,"parentResponder");
    for (NSUInteger depth=0;owner && depth<16;depth++,owner=RVObject(owner,"parentResponder")) {
        if (![owner isKindOfClass:NSClassFromString(@"YTSectionController")] || !RVCan(owner,"cellControllerAtIndex:",@"@@:q")) continue;
        id entries=RVObject(RVObject(owner,"viewModel"),"entries");
        if (![entries isKindOfClass:NSArray.class] || [entries count]>1024) return NO;
        NSUInteger row=[entries indexOfObjectIdenticalTo:entry];if (row==NSNotFound) return NO;
        if (((id(*)(id,SEL,NSInteger))objc_msgSend)(owner,sel_registerName("cellControllerAtIndex:"),row)!=controller) return NO;
        *section=owner;*index=(NSInteger)row;return YES;
    }return NO;
}
static RVDAUIBinding *RVDAUIBind(id controller,id entry,id section,NSInteger index) {
    RVDADiagCount(@"binding_attempt");
    if ((!section || index<0) && RVDAUIFindSection(controller,entry,&section,&index)) RVDADiagCount(@"section_scope_recovered");
    if (!NSThread.isMainThread || !controller || !entry || !section || index<0) {
        NSString *blocker=!NSThread.isMainThread ? @"binding_off_main" : !controller ? @"controller_absent" : !entry ? @"entry_absent" : !section ? @"section_scope_absent" : @"invalid_row_index";
        RVDADiagCount([@"blocked." stringByAppendingString:blocker]);RVDADiagAttempt(controller,entry,section,index,blocker);return nil;
    }
    RVDAUIBinding *prior=objc_getAssociatedObject(controller,RVDAUIBindingKey);
    BOOL sameSource=!prior.modern || [RVObject(RVAdRootElement(entry),"elementData") isEqual:prior.profile.source];
    if (prior.legacySource) sameSource=[RVObject(RVDAUILegacyRenderer(entry,NULL),"data") isEqual:prior.legacySource];
    if ([prior.surface isEqual:@"player"]) {
        NSString *watchVideo=nil;RVDAUIOwnerSurface(controller,YES,&watchVideo);
        sameSource=sameSource && [watchVideo isEqual:prior.video];
    }
    if (prior && sameSource && prior.originalEntry==entry && prior.section==section && prior.index==index) {
        RVDADiagCount(@"binding_reused");
        NSString *surface=RVDAUIOwnerSurface(controller,prior.modern ? prior.profile.watchTitle : [prior.surface isEqual:@"player"],NULL);
        BOOL changed=![surface isEqual:prior.surface];if (changed) prior.surface=surface;
        if (changed || prior.settingsRevision!=RVDAUISettingsRevision) RVDAUIRequest(prior);return prior;
    }
    [prior.request cancel];
    RVDAUIBinding *state=[RVDAUIBinding new];state.controller=controller;state.section=section;state.index=index;state.originalEntry=entry;
    state.route=NSStringFromClass([entry class]);state.surface=RVDAUIOwnerSurface(controller,NO,NULL);
    if (RVDADiagEnabled()) state.ownerSummary=@{@"controller":NSStringFromClass([controller class]),@"section":NSStringFromClass([section class]),@"chain":RVDADiagOwners(controller)};
    id element=RVAdRootElement(entry);NSString *watchVideo=nil;
    // Resolve a watch ID through its explicit metadata owner, never current player.
    RVDAUIOwnerSurface(controller,YES,&watchVideo);
    if (element) {
        id options=RVAdResolvedObject(element,"compatibilityOptions");
        if (!RVAdResolve(options,"hasAdLoggingData",@"B@:")) return RVDAUIReject(state,@"D03_video_bound",@"ad_presence_contract_missing");
        if (RVBool(options,"hasAdLoggingData")) return RVDAUIReject(state,@"D03_video_bound",@"ad_root");
        NSData *data=RVAdResolvedObject(element,"elementData");
        if (![data isKindOfClass:NSData.class] || !data.length) return RVDAUIReject(state,@"D04_role_bound",@"element_data_missing_or_wrong_type");
        if (RVDAUIDiagnosticsArmed && RVObject(controller,"entry")==entry &&
            [RVDATemplate(RVDAPath(data,@[@1,@168777401])) isEqual:RVDALiveCardKey()])
            RVDALiveCaptureRoot(data,NSStringFromClass([controller class]),state.surface,watchVideo);
        state.profile=RVDAProfile(data,watchVideo);state.modern=YES;
        if (!state.profile) {
            BOOL shared=RVDAPath(data,@[@1,@484616221])!=nil;
            return RVDAUIReject(state,@"D04_role_bound",shared ? @"shared_component_input_unavailable" : RVDAProfileFailure(data));
        }
        state.video=state.profile.video;state.route=state.profile.route;
        state.originalTitle=state.profile.originalTitle;state.untranslatedTitle=state.profile.untranslatedTitle;
        state.originalImageURL=state.profile.originalImageURL;state.duration=state.profile.duration;
        if (state.profile.liveCard) RVDADiagRecord(@"D04_live_card_input_verified",state,nil,
            @{@"identity_path":state.profile.identityPath,@"presentation_path":state.profile.videoDataPath,
              @"title_hash":RVDADiagHash([state.originalTitle dataUsingEncoding:NSUTF8StringEncoding]),
              @"thumbnail_identity_matches":@YES,@"native_consumer_confirmation_pending":@YES});
        state.surface=RVDAUIOwnerSurface(controller,state.profile.watchTitle,&watchVideo);
        state.titleBlocker=state.profile.titleRole ? @"awaiting_metadata" : state.profile.blocker ?: @"title_role_unknown";
        state.imageBlocker=state.profile.imageRole ? @"awaiting_metadata" : state.profile.blocker ?: @"image_role_unknown";
    } else {
        id renderer=RVDAUILegacyRenderer(entry,NULL);
        if (!renderer) return RVDAUIReject(state,@"D04_role_bound",@"renderer_union_or_type_unsupported");state.route=NSStringFromClass([renderer class]);
        BOOL slim=[state.route isEqual:@"YTISlimVideoInformationRenderer"];
        if (slim) {
            if (!RVVideoIDValid(watchVideo)) return RVDAUIReject(state,@"D03_video_bound",@"explicit_watch_video_missing");
            if (!RVOrdinaryContent(RVCurrentPlayback) || ![RVObject(RVCurrentPlayback,"contentVideoID") isEqual:watchVideo]) return RVDAUIReject(state,@"D03_video_bound",@"legacy_watch_content_mismatch");
            state.video=watchVideo;state.surface=RVDAUIOwnerSurface(controller,YES,NULL);
        } else {
            if (!RVAdResolve(renderer,"isAd",@"B@:")) return RVDAUIReject(state,@"D03_video_bound",@"legacy_ad_contract_missing");
            if (RVBool(renderer,"isAd")) return RVDAUIReject(state,@"D03_video_bound",@"ad_root");
            state.video=RVAdResolvedObject(renderer,"videoId");state.surface=RVDAUIOwnerSurface(controller,NO,NULL);
        }
        // Legacy image leases are initially limited to ordinary full/compact cells.
        if (![@[@"YTIVideoRenderer",@"YTICompactVideoRenderer",@"YTISlimVideoInformationRenderer"] containsObject:state.route]) return RVDAUIReject(state,@"D04_role_bound",@"legacy_presentation_contract_unsupported");
        NSData *source=RVObject(renderer,"data");if ([source isKindOfClass:NSData.class] && source.length<=2*1024*1024) state.legacySource=[source copy];
        state.originalTitle=RVPlainText(RVAdResolvedObject(renderer,"title"));
        state.untranslatedTitle=RVPlainText(RVAdResolvedObject(renderer,"untranslatedTitle"));
        state.duration=RVDADurationText(RVPlainText(RVAdResolvedObject(renderer,"lengthText")));
        id thumbnails=RVObject(RVAdResolvedObject(renderer,"thumbnail"),"thumbnailsArray");
        if ([thumbnails isKindOfClass:NSArray.class] && [thumbnails count]<=16) {
            for (id thumbnail in thumbnails) {
                NSString *url=RVAdResolvedObject(thumbnail,"URL");NSURLComponents *parts=[NSURLComponents componentsWithString:url ?: @""];NSArray *path=parts.path.pathComponents;
                if ([parts.scheme isEqual:@"https"] && [@[@"i.ytimg.com",@"i1.ytimg.com",@"i2.ytimg.com",@"i3.ytimg.com",@"i4.ytimg.com"] containsObject:parts.host.lowercaseString] && path.count==4 && [path[2] isEqual:state.video] && !parts.user && !parts.password) state.originalImageURL=url;
            }
        }
        state.upcoming=RVAdResolve(renderer,"hasUpcomingEventData",@"B@:") && RVBool(renderer,"hasUpcomingEventData");
        id avatars=RVObject(RVAdResolvedObject(renderer,"channelThumbnail"),"thumbnailsArray");
        if (state.upcoming && [avatars isKindOfClass:NSArray.class] && [avatars count]<=16) for (id avatar in avatars) {
            NSString *url=RVAdResolvedObject(avatar,"URL");NSURLComponents *parts=[NSURLComponents componentsWithString:url ?: @""];
            if ([parts.scheme isEqual:@"https"] && [@[@"yt3.ggpht.com",@"yt3.googleusercontent.com"] containsObject:parts.host.lowercaseString] && !parts.user && !parts.password) state.avatarURL=url;
        }
    }
    if (!RVVideoIDValid(state.video)) return RVDAUIReject(state,@"D03_video_bound",@"video_id_missing_or_invalid");
    if (!state.surface) return RVDAUIReject(state,@"D02_owner_bound",@"owner_surface_unknown");
    objc_setAssociatedObject(controller,RVDAUIBindingKey,state,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [RVDAUIBindings addObject:state];RVDAUIEvent(@"D02_owner_bound",state,nil);RVDAUIEvent(@"D03_video_bound",state,nil);
    RVDADiagCount([NSString stringWithFormat:@"accepted.surface.%@",state.surface]);
    RVDAUIEvent(@"D04_role_bound",state,state.profile.blocker);RVDAUIRequest(state);return state;
}
static id RVDAUIProjectedEntry(RVDAUIBinding *state) {
    if (!state || state.settingsRevision!=RVDAUISettingsRevision || RVDAUIOriginal(state)) return state.originalEntry;
    NSString *title=RVDAUITitle(state);NSData *image=RVDAUIImage(state) ? state.imageBytes : nil;
    if (state.modern) {
        NSInteger lines=(state.parts&RVDeArrowPartsTitle) && !state.profile.watchTitle ? [RVSetting(@"dearrow_title_max_lines") integerValue] : 0;
        NSData *next=RVDAProject(state.profile,title,image,RVDAUIFrameFallback(state),lines);
        if (!next || [next isEqual:state.profile.source]) {
            if (title || image || RVDAUIFrameFallback(state)) RVDAUIEvent(@"D09_projection_blocked",state,@"projection_made_no_change");
            else RVDADiagCount(@"presentation_waiting_for_replacement");return state.originalEntry;
        }
        id element=RVAdRootElement(state.originalEntry);
        if (![element conformsToProtocol:@protocol(NSCopying)]) { RVDAUIEvent(@"D09_projection_blocked",state,@"element_copy_contract_missing");return state.originalEntry; }
        id copy=[element copy];objc_setAssociatedObject(copy,RVDAUIDataKey,next,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        id result=copy;
        if (element!=state.originalEntry) {
            if (![state.originalEntry conformsToProtocol:@protocol(NSCopying)] || !RVAdResolve(state.originalEntry,"setElementRenderer:",@"v@:@")) { RVDAUIEvent(@"D09_projection_blocked",state,@"element_union_copy_contract_missing");return state.originalEntry; }
            result=[state.originalEntry copy];RVSetObject(result,"setElementRenderer:",copy);
        }
        RVDAUIEvent(@"presentation_model_projected",state,nil);return result;
    }
    BOOL fallback=RVDAUIFrameFallback(state);
    BOOL lines=(state.parts&RVDeArrowPartsTitle) && ![state.surface isEqual:@"player"];
    if (!title && !fallback && !lines && !RVDAUIImage(state)) return state.originalEntry;
    NSString *unionSelector=nil;id renderer=RVDAUILegacyRenderer(state.originalEntry,&unionSelector);
    if (![renderer conformsToProtocol:@protocol(NSCopying)]) return state.originalEntry;
    id copy=[renderer copy];
    if (lines && RVAdResolve(copy,"setMaxTitleLines:",@"v@:i")) ((void(*)(id,SEL,int))objc_msgSend)(copy,sel_registerName("setMaxTitleLines:"),[RVSetting(@"dearrow_title_max_lines") intValue]);
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
    if (!state || state.modern) return;
    NSString *blocker=RVDAUICurrentBlocker(state);if (blocker) { RVDAUIEvent(@"D09_legacy_target_rejected",state,blocker);return; }
    NSString *title=RVDAUITitle(state);titleView=RVObject(view,"titleLabel") ?: RVObject(RVObject(view,"metadataView"),"titleLabel");
    RVDAUIApplyAccessibility(titleView,title);
    imageView=RVObject(RVObject(view,"thumbnailView"),"imageView");
    if (RVDAUIImage(state) && RVCan(imageView,"setImage:animated:",@"v@:@B")) {
        state.nativeImage=RVObject(imageView,"image");state.imageView=imageView;
        RVDAUIImageLease *lease=[RVDAUIImageLease new];lease.binding=state;
        objc_setAssociatedObject(imageView,RVDAUIImageKey,lease,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        ((void(*)(id,SEL,id,BOOL))objc_msgSend)(imageView,sel_registerName("setImage:animated:"),RVDAUIImage(state),NO);
    }
    if (title || RVDAUIImage(state)) { state.materializations++;RVDAUIEvent(@"D09_legacy_target_configured",state,nil); }
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
        RVDeArrowResult *result=state.result;
        [bindings addObject:@{@"surface":state.surface ?: @"unknown",@"route":state.route ?: @"unresolved",
            @"video_hash":RVDADiagVideo(state.video),@"ownership_blocker":RVDAUICurrentBlocker(state) ?: @"ready",
            @"generation":@(state.generation),@"current_source":@(RVDAUICurrent(state)),@"requested_parts":@(state.parts),
            @"service_parts":@(state.serviceParts),
            @"title_blocker":state.titleBlocker ?: @"policy_off",@"image_blocker":state.imageBlocker ?: @"policy_off",
            @"original_selected":@(RVDAUIOriginal(state)),@"title_role":@(!state.modern || state.profile.titleRole),
            @"original_title_bound":@(state.originalTitle.length>0),@"untranslated_title_bound":@(state.untranslatedTitle.length>0),
            @"local_title_changed":@(RVDAUITitle(state)!=nil),@"casual_original_selected":@(RVDAUICasual(state)),
            @"image_origin":state.imageOrigin ?: @"native_original",@"duration_available":@(state.duration>0 || result.videoDuration.doubleValue>0),@"upcoming_contract":@(state.upcoming),
            @"fallback_phase":state.fallbackPhase ?: @"idle",@"fallback_retries":@(state.fallbackRetries),@"duration_source":state.durationSource ?: @"unresolved",@"fallback_retry_scheduled":@(state.fallbackRetryScheduled),
            @"image_role":@(state.modern ? state.profile.imageRole : ![state.surface isEqual:@"player"]),@"encoded_image_ready":@(state.imageBytes!=nil),
            @"refresh_requests":@(state.refreshRequests),@"native_materializations":@(state.materializations),
            @"live_card_contract":@(state.profile.liveCard),@"identity_path":state.profile.identityPath ?: @[],
            @"title_consumer_verified":@(state.titleConsumerVerified),@"image_consumer_verified":@(state.imageConsumerVerified),
            @"consumer_observations":@(state.consumerObservations),@"consumer_checkpoint":state.consumerCheckpoint ?: @{},
            @"last_checkpoint":state.lastCheckpoint ?: @"none",@"last_blocker":state.lastBlocker ?: @"ready",@"owners":state.ownerSummary ?: @{},
            @"result_present":@(result!=nil),@"title_decision":RVDADecisionName(result.titleDecision),
            @"image_decision":RVDADecisionName(result.thumbnailDecision),@"image_state":@(result.imageState),
            @"image_decode_ready":@(result.thumbnailImage!=nil),@"metadata_http":@(result.metadataHTTPStatus),@"image_http":@(result.imageHTTPStatus),
            @"configuration_revision":@(result.configurationRevision),@"result_generation":@(result.clientGeneration)}];
    }
    NSArray *history,*attempts,*templates;NSDictionary *counts,*stages;
    @synchronized(RVDAUIBinding.class) {
        history=RVDAUIHistory.copy ?: @[];counts=RVDAUICounts.copy ?: @{};stages=RVDAUILastStages.copy ?: @{};attempts=RVDAUISamples.allValues.copy ?: @[];templates=RVDAUITemplateSamples.allValues.copy ?: @[];
    }
    return @{@"schema":@2,@"patcher_version":RV_PORT_VERSION,@"build_source_sha256":RV_BUILD_SOURCE_SHA256,
        @"capture_enabled":@(RVDADiagEnabled()),@"capture_armed":@(RVDAUIDiagnosticsArmed),@"settings":RVDADiagSettings(),@"uri_structure_schema":@2,
        @"settings_revision":@(RVDAUISettingsRevision),@"backend":RVDeArrowBackend.sharedBackend.diagnostics,
        @"bindings":bindings,@"history":history,@"counts":counts,@"last_stages":stages,@"attempt_samples":attempts,@"native_templates":templates,
        @"hooks":RVDAUIHooks.copy ?: @[],@"live_contract_capture":RVDALiveReport(),@"service_probe":RVDAUIProbeResult ?: @{@"state":@"not_requested"},@"device_verified":@NO,
        @"capture_instructions":@"Start diagnostic capture, close settings, scroll Home, open an ordinary video and related rows, then copy checkpoints. Unknown roots retain originals. No submission is an intentional original, not a rendering failure."};
}
static void RVDAUIStartCapture(void) {
    [RVDAUIProbeRequest cancel];RVDAUIProbeRequest=nil;RVDAUIProbeResult=nil;RVDAUIProbeGeneration++;
    RVDAUIDiagnosticsArmed=YES;
    RVDALiveReset();
    @synchronized(RVDAUIBinding.class) { RVDAUICounts=[NSMutableDictionary dictionary];RVDAUILastStages=[NSMutableDictionary dictionary];RVDAUISamples=[NSMutableDictionary dictionary];RVDAUITemplateSamples=[NSMutableDictionary dictionary];RVDAUIHistory=[NSMutableArray array]; }
    RVDAUIStarted=NSProcessInfo.processInfo.systemUptime;RVDAUIPreferencesChanged();RVDAUIEvent(@"D00_capture_started",nil,nil);
}
static BOOL RVDAUIProbeCurrentService(void) {
    NSString *video=RVObject(RVCurrentPlayback,"contentVideoID");
    if (!RVVideoIDValid(video) || !RVOrdinaryContent(RVCurrentPlayback) || !RVCompatible) return NO;
    BOOL enabled=RVDADiagEnabled();RVDAUIDiagnosticsArmed=YES;
    if (enabled) RVDAUIConfigureBackend();else RVDAUIPreferencesChanged();
    [RVDAUIProbeRequest cancel];
    uint64_t generation=++RVDAUIProbeGeneration;
    RVDAUIProbeResult=@{@"state":@"pending",@"video_hash":RVDADiagVideo(video),@"service_only":@YES};
    RVDAUIProbeRequest=[RVDeArrowBackend.sharedBackend requestVideoID:video parts:RVDeArrowPartsAll clientGeneration:generation completion:^(RVDeArrowResult *result) {
        if (generation!=RVDAUIProbeGeneration || result.configurationRevision!=RVDeArrowBackend.sharedBackend.configurationRevision) return;
        RVDAUIProbeResult=@{@"state":result.isFinal ? @"complete" : @"metadata_ready_image_pending",@"service_only":@YES,
            @"video_hash":RVDADiagVideo(result.videoID),@"title_decision":RVDADecisionName(result.titleDecision),@"image_decision":RVDADecisionName(result.thumbnailDecision),
            @"title_blocker":RVDABlockerName(result.titleBlocker),@"image_selection_blocker":RVDABlockerName(result.thumbnailSelectionBlocker),
            @"image_blocker":RVDABlockerName(result.imageBlocker),@"metadata_http":@(result.metadataHTTPStatus),@"image_http":@(result.imageHTTPStatus),
            @"image_state":@(result.imageState),@"decoded_image_ready":@(result.thumbnailImage!=nil),@"configuration_revision":@(result.configurationRevision)};
        RVDADiagRecord(@"P01_service_probe",nil,nil,RVDAUIProbeResult);
        if (result.isFinal) RVDAUIProbeRequest=nil;
    }];return YES;
}
static BOOL RVDAUISetCurrentOriginal(BOOL original) {
    // This is an explicit user action on the watched video, not a feed binding rule.
    NSString *video=RVObject(RVCurrentPlayback,"contentVideoID");if (!RVVideoIDValid(video)) return NO;
    if (original) { if (RVDAUIOriginalVideos.count>=128) [RVDAUIOriginalVideos removeAllObjects];[RVDAUIOriginalVideos addObject:video];[RVDAUICustomVideos removeObject:video]; }
    else { [RVDAUIOriginalVideos removeObject:video];if (RVDAUICustomVideos.count>=128) [RVDAUICustomVideos removeAllObjects];[RVDAUICustomVideos addObject:video]; }
    RVDAUIPreferencesChanged();return YES;
}
static RVDAUIBinding *RVDAUICurrentVideoBinding(void) {
    NSString *video=RVObject(RVCurrentPlayback,"contentVideoID");if (!RVVideoIDValid(video) || !RVOrdinaryContent(RVCurrentPlayback)) return nil;
    RVDAUIBinding *found=nil;
    for (RVDAUIBinding *state in RVDAUIBindings.allObjects) if ([state.video isEqual:video] && RVDAUICurrent(state)) { found=state;if ([state.surface isEqual:@"player"]) break; }
    return found;
}
static BOOL RVDAUIOriginalActionsAvailable(void) {
    if (RVEnabled(@"dearrow_always_show_original") || RVEnabled(@"dearrow_default_original")) return YES;
    RVDAUIBinding *state=RVDAUICurrentVideoBinding();
    if (state.result.titleDecision==RVDeArrowDecisionCustom || state.result.thumbnailDecision==RVDeArrowDecisionCustom || state.presentationImage) return YES;
    return state.originalTitle.length && RVEnabled(@"dearrow_show_original_for_formatted") && ![RVDAFormatTitle(state.originalTitle,NO) isEqual:state.originalTitle];
}
static NSDictionary *RVDAUIPreviewSnapshot(void) {
    RVDAUIBinding *state=RVDAUICurrentVideoBinding();if (!state) return nil;
    BOOL custom=RVEnabled(@"dearrow_use_crowdsourced_titles") && state.result.titleDecision==RVDeArrowDecisionCustom && !RVDAUICasual(state);
    NSString *source=custom ? state.result.title : RVEnabled(@"dearrow_ignore_translated_titles") && state.untranslatedTitle.length ? state.untranslatedTitle : state.originalTitle;
    return @{@"original_title":state.originalTitle ?: @"Unavailable",@"custom_title":RVDAFormatTitle(source,custom) ?: @"Unavailable",
        @"original_image":state.nativeImage ?: NSNull.null,@"custom_image":state.presentationImage ?: state.result.thumbnailImage ?: NSNull.null,
        @"decision":RVDADecisionName(state.result.titleDecision)};
}
static void RVDAUIClearCaches(void) { [RVDeArrowBackend.sharedBackend clearCaches];RVDAUIPreferencesChanged(); }
static BOOL RVDAUIHook(NSString *className,NSString *selector,NSString *abi,BOOL classMethod,RVReplacementFactory factory) {
    BOOL installed=RVHook(className,selector,abi,classMethod,factory);
    Class cls=NSClassFromString(className);Class target=classMethod ? object_getClass(cls) : cls;
    NSString *actual=RVSignature(class_getInstanceMethod(target,NSSelectorFromString(selector)));
    [RVDAUIHooks addObject:@{@"class":className,@"selector":selector,@"expected_abi":abi,@"actual_abi":actual ?: @"absent",
        @"class_present":@(cls!=Nil),@"installed":@(installed)}];return installed;
}

static void RVInstallDeArrowUI(void) {
    RVDAUIStarted=NSProcessInfo.processInfo.systemUptime;
    RVDAUISalt=NSUUID.UUID.UUIDString;
    RVDAUIBindings=[NSHashTable weakObjectsHashTable];RVDAUIOriginalVideos=[NSMutableSet set];RVDAUICustomVideos=[NSMutableSet set];RVDAUIHooks=[NSMutableArray array];RVDAUIConfigureBackend();
    RVDAUIHook(@"ELMComponent",@"templateURI",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id component) { id uri=((id(*)(id,SEL))original)(component,sel);RVDADiagNativeTemplate(uri);return uri; };
    });
    RVDAUIHook(@"YTIElementRenderer",@"elementData",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id native=((id(*)(id,SEL))original)(object,sel),next=objc_getAssociatedObject(object,RVDAUIDataKey);
            if (next) RVDADiagCount(@"projected_element_data_consumed");return next ?: native;
        };
    });
    // Lazy descendants may appear after the factory block has returned. Observe
    // already-created nodes later on main; never force ASNodeController.node.
    for (NSString *name in @[@"ELMTextNode",@"ELMImageNode"]) RVDAUIHook(name,@"setElement:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id element) {
            ((void(*)(id,SEL,id))original)(object,sel,element);__weak id weak=object;
            dispatch_async(dispatch_get_main_queue(),^{
                id current=weak;if (!current) return;
                id owner=RVDAUIConsumerRoot(current);
                for (RVDAUIBinding *state in RVDAUIBindings.allObjects) if (state.profile.liveCard && owner && owner==state.nativeRootComponent && RVDAUICurrent(state))
                    RVDAUIObserveConsumers(state,current,state.generation,0);
            });
        };
    });
    // Only the materialization argument changes; native controller entries and
    // the shared response graph remain the authoritative original identities.
    RVDAUIHook(@"YTSectionController",@"nodeBlockForItemAtIndex:",@"@?@:q",NO,^id(IMP original,SEL sel) {
        return ^id(id section,NSInteger index) {
            RVDADiagCount(@"native.section_node_factory_entered");
            if (!NSThread.isMainThread) RVDADiagCount(@"native.section_node_factory_off_main");
            id prior=RVDAUISection;NSInteger old=RVDAUIIndex;RVDAUISection=section;RVDAUIIndex=index;
            @try { return ((id(*)(id,SEL,NSInteger))original)(section,sel,index); }
            @finally { RVDAUISection=prior;RVDAUIIndex=old; }
        };
    });
    RVDAUIHook(@"ELMCellNode",@"nodeBlockForEntry:responder:clientDataObservable:blockRegistry:blockRegistryProvider:",@"@?@:@@@@@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id entry,id responder,id observable,id registry,id provider) {
            RVDADiagCount(@"native.elements_factory_entered");
            RVDAUIBinding *state=RVDAUIBind(responder,entry,RVDAUISection,RVDAUIIndex);
            id next=state ? RVDAUIProjectedEntry(state) : entry;
            id (^native)(void)=((id(*)(id,SEL,id,id,id,id,id))original)(cls,sel,next,responder,observable,registry,provider);
            if (!state || (!state.profile.liveCard && next==entry) || !native) { if (state && !native) RVDAUIEvent(@"D09_native_factory_failed",state,@"native_factory_returned_nil");return native; }
            __weak RVDAUIBinding *weak=state;uint64_t generation=state.generation;
            // Native block descriptor: @8@?0. Native construction may run off
            // main; ownership diagnostics return to main after it completes.
            return [^id(void) {
                id node=native();__weak id weakNode=node;
                dispatch_async(dispatch_get_main_queue(),^{
                    RVDAUIBinding *current=weak;
                    if (current && weakNode && current.generation==generation && RVDAUICurrent(current)) {
                        current.materializations++;
                        if (current.profile.liveCard) { RVDAUIBindConsumerRoot(current,weakNode);RVDAUIObserveConsumers(current,weakNode,generation,0); }
                        if (next!=entry) {
                            RVDAUIEvent(@"D09_native_target_materialized",current,nil);
                            if (!current.profile.liveCard) [current.request recordCheckpoint:RVDeArrowCheckpointReplacementApplied blocker:RVDeArrowBlockerNone];
                        }
                    }
                });return node;
            } copy];
        };
    });
    RVDAUIHook(@"YTSectionController",@"updateCell:forIndex:",@"v@:@q",NO,^id(IMP original,SEL sel) {
        return ^(id section,id cell,NSInteger index) {
            RVDADiagCount(@"native.section_cell_update_entered");
            id prior=RVDAUISection;NSInteger old=RVDAUIIndex;RVDAUISection=section;RVDAUIIndex=index;
            @try { ((void(*)(id,SEL,id,NSInteger))original)(section,sel,cell,index); }
            @finally { RVDAUISection=prior;RVDAUIIndex=old; }
        };
    });
    RVDAUIHook(@"YTCellController",@"setCell:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id cell) {
            if (cell) RVDADiagCount(@"native.controller_cell_bind_entered");
            RVDAUIBinding *state=cell ? RVDAUIBind(controller,RVObject(controller,"entry"),RVDAUISection,RVDAUISection ? RVDAUIIndex : -1) : nil;
            id prior=RVDAUIConfiguring;RVDAUIConfiguring=state;
            @try { ((void(*)(id,SEL,id))original)(controller,sel,cell); }
            @finally { RVDAUIConfiguring=prior; }
        };
    });
    for (NSString *cls in @[@"YTVideoCell",@"YTCompactVideoCell",@"YTSlimVideoInformationCell"]) RVDAUIHook(cls,@"setEntry:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id cell,id entry) {
            RVDADiagCount([@"native.cell_entry." stringByAppendingString:cls]);
            if ([cls isEqual:@"YTSlimVideoInformationCell"]) RVDAUIRestoreAccessibility(RVObject(cell,"titleLabel"));
            RVDAUIBinding *state=RVDAUIConfiguring;
            id next=state && state.originalEntry==entry ? RVDAUIProjectedEntry(state) : entry;
            ((void(*)(id,SEL,id))original)(cell,sel,next);
            if ([cls isEqual:@"YTSlimVideoInformationCell"] && state && RVDAUITitle(state)) {
                RVDAUIApplyAccessibility(RVObject(cell,"titleLabel"),RVDAUITitle(state));
                state.materializations++;RVDAUIEvent(@"D09_legacy_watch_target_configured",state,nil);
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
