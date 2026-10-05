// SPDX-License-Identifier: MIT
// Verified 21.39.4 watch request/consumer boundaries; no global UI state switch.
static BOOL RVInstallVoteFirstLaunchScopes(void) {
    BOOL ready=YES;
    // YTWatchNextServiceImpl supplies service 0xa1 to its native request factory.
    // Scope the entire synchronous construction, including cache/decorator work.
    BOOL request=RVHook(@"YTWatchNextServiceImpl",@"makeRequest:refresh:retryEnabled:timeoutInterval:responseBlock:errorBlock:",@"v@:@BBd@?@?",NO,^id(IMP original,SEL sel) {
        return ^(id service,id proto,BOOL refresh,BOOL retry,NSTimeInterval timeout,id response,id error) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_next_request",YES);
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
    // Native watch constructors cache experiment decisions. Retain compatibility
    // for those controllers without replacing the globally loaded cold group.
    BOOL watchInit=RVHook(@"YTWatchViewController",@"initWithThumbnailProvider:parentResponder:",@"@@:@@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id thumbnails,id parent) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_controller_init",YES);
            @try { return ((id(*)(id,SEL,id,id))original)(object,sel,thumbnails,parent); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && watchInit;
    BOOL nextInit=RVHook(@"YTWatchNextResultsViewController",@"initWithNullableService:parentResponder:format:layout:",@"@@:@@q@",NO,^id(IMP original,SEL sel) {
        return ^id(id object,id service,id parent,NSInteger format,id layout) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_results_init",YES);
            @try { return ((id(*)(id,SEL,id,id,NSInteger,id))original)(object,sel,service,parent,format,layout); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && nextInit;
    BOOL update=RVHook(@"YTWatchViewController",@"updateWithWatchNextResponse:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id model) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_response_update",YES);
            @try { ((void(*)(id,SEL,id))original)(object,sel,model); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && update;
    BOOL load=RVHook(@"YTWatchNextResultsViewController",@"loadWithModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id model) {
            NSUInteger previous=RVVoteFirstLaunchEnter(@"watch_results_load",YES);
            @try { ((void(*)(id,SEL,id))original)(object,sel,model); }
            @finally { RVVoteFirstLaunchLeave(previous); }
        };
    });
    ready=ready && load;
    RVVoteFirstLaunchCheckpoint(@"scope_hooks_ready",@{@"watch_request_hook":@(request),@"request_context_hook":@(context),
        @"account_request_context_hook":@(accountContext),@"watch_init_hook":@(watchInit),@"watch_results_init_hook":@(nextInit),
        @"watch_update_hook":@(update),@"watch_results_load_hook":@(load),@"scope_hooks_ready":@(ready)});
    return ready;
}
