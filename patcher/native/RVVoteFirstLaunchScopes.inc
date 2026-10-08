// SPDX-License-Identifier: MIT
// Defaults are confined to serialized watch request contexts. Native consumers
// must see account configuration when constructing commands and panel registries.
static BOOL RVInstallVoteFirstLaunchScopes(void) {
    BOOL ready=YES;
    // YTWatchNextServiceImpl supplies service 0xa1 to its native request factory.
    // Preserve service/cache/callback configuration. Nested context factories
    // enter their own compatibility scope; a synchronous response stays native.
    BOOL request=RVHook(@"YTWatchNextServiceImpl",@"makeRequest:refresh:retryEnabled:timeoutInterval:responseBlock:errorBlock:",@"v@:@BBd@?@?",NO,^id(IMP original,SEL sel) {
        return ^(id service,id proto,BOOL refresh,BOOL retry,NSTimeInterval timeout,id response,id error) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_next_service_native",NO);
            @try { ((void(*)(id,SEL,id,BOOL,BOOL,NSTimeInterval,id,id))original)(service,sel,proto,refresh,retry,timeout,response,error); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && request;
    // Context decorators obtain emlTemplateServingContext after clientInfo. Both
    // factories must be covered, and non-watch services must suspend defaults.
    BOOL context=RVHook(@"YTInnerTubeContextFactory",@"contextWithPageID:isIncognitoActive:criticality:clickTrackingInfo:sendDeviceIdentifier:clientScreenNonce:consistencyTokenJars:innertubeTokenJar:attestationResponseData:reauthProofToken:serviceType:isPrefetch:",@"@@:@Bi@B@@@@@qB",NO,^id(IMP original,SEL sel) {
        return ^id(id factory,id page,BOOL incognito,int criticality,id click,BOOL send,id nonce,id jars,id token,id attestation,id reauth,NSInteger service,BOOL prefetch) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"request_context",RVVoteFirstLaunchWatchService(service));
            @try { return ((id(*)(id,SEL,id,BOOL,int,id,BOOL,id,id,id,id,id,NSInteger,BOOL))original)(factory,sel,page,incognito,criticality,click,send,nonce,jars,token,attestation,reauth,service,prefetch); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && context;
    BOOL accountContext=RVHook(@"YTAccountScopedInnerTubeContextFactory",@"contextWithPageID:isIncognitoActive:criticality:clickTrackingInfo:sendDeviceIdentifier:clientScreenNonce:consistencyTokenJars:attestationResponseData:reauthProofToken:serviceType:isPrefetch:",@"@@:@Bi@B@@@@qB",NO,^id(IMP original,SEL sel) {
        return ^id(id factory,id page,BOOL incognito,int criticality,id click,BOOL send,id nonce,id jars,id attestation,id reauth,NSInteger service,BOOL prefetch) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"account_scoped_request_context",RVVoteFirstLaunchWatchService(service));
            @try { return ((id(*)(id,SEL,id,BOOL,int,id,BOOL,id,id,id,id,NSInteger,BOOL))original)(factory,sel,page,incognito,criticality,click,send,nonce,jars,attestation,reauth,service,prefetch); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && accountContext;
    // Explicitly suspend a surrounding request scope at consumer boundaries.
    // Defaulting these constructors/loads broke comments and chapters on-device:
    // their cached routing decisions disagreed with the native command runtime.
    BOOL watchInit=RVHook(@"YTWatchViewController",@"initWithThumbnailProvider:parentResponder:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id thumbnails,id parent) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_controller_init_native",NO);
            @try { return ((id(*)(id,SEL,id,id))original)(object,sel,thumbnails,parent); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && watchInit;
    BOOL nextInit=RVHook(@"YTWatchNextResultsViewController",@"initWithNullableService:parentResponder:format:layout:",@"@@:@@q@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id service,id parent,NSInteger format,id layout) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_results_init_native",NO);
            @try { return ((id(*)(id,SEL,id,id,NSInteger,id))original)(object,sel,service,parent,format,layout); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && nextInit;
    BOOL update=RVHook(@"YTWatchViewController",@"updateWithWatchNextResponse:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id model) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_response_update_native",NO);
            @try { ((void(*)(id,SEL,id))original)(object,sel,model); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && update;
    BOOL load=RVHook(@"YTWatchNextResultsViewController",@"loadWithModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id model) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_results_load_native",NO);
            @try { ((void(*)(id,SEL,id))original)(object,sel,model); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && load;
    RVVoteFirstLaunchCheckpoint(@"scope_hooks_ready",@{@"watch_request_hook":@(request),@"request_context_hook":@(context),
        @"account_request_context_hook":@(accountContext),@"watch_init_hook":@(watchInit),@"watch_results_init_hook":@(nextInit),
        @"watch_update_hook":@(update),@"watch_results_load_hook":@(load),@"scope_hooks_ready":@(ready),
        @"native_watch_consumers_preserved":@YES,@"request_service_callbacks_preserved":@YES});
    return ready;
}
