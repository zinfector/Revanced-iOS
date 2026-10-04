// SPDX-License-Identifier: MIT
#include "RVAuthenticationSupport.h"

static NSMutableArray<NSDictionary *> *RVAuthEvents;
static NSMutableDictionary<NSString *,NSNumber *> *RVAuthCounts;

static void RVAuthRecord(NSString *kind,NSDictionary *fields) {
    if (!RVAuthEvents || ![RVSetting(@"diagnostics") boolValue]) return;
    @synchronized(RVAuthEvents) {
        RVAuthCounts[kind]=@([RVAuthCounts[kind] unsignedLongLongValue]+1);
        NSMutableDictionary *event=[fields mutableCopy];event[@"event"]=kind;
        if (RVAuthEvents.count>=64) [RVAuthEvents removeObjectAtIndex:0];
        [RVAuthEvents addObject:event];
    }
}
static BOOL RVAuthSchemeRegistered(void) {
    id types=NSBundle.mainBundle.infoDictionary[@"CFBundleURLTypes"];
    if (![types isKindOfClass:NSArray.class]) return NO;
    for (id type in types) {
        id schemes=[type isKindOfClass:NSDictionary.class] ? type[@"CFBundleURLSchemes"] : nil;
        if ([schemes isKindOfClass:NSArray.class] && [schemes containsObject:RVAuthScheme]) return YES;
    }
    return NO;
}
static BOOL RVAuthSupportedConfiguration(id object) {
    return RVAuthTupleMatches(RVObject(object,"clientID"),RVObject(object,"applicationScheme")) && RVAuthSchemeRegistered();
}
static NSDictionary *RVAuthenticationReport(void) {
    NSDictionary *counts;NSArray *events;
    @synchronized(RVAuthEvents) { counts=[RVAuthCounts copy] ?: @{};events=[RVAuthEvents copy] ?: @[]; }
    return @{@"schema":@2,@"identity_enabled":@(RVEnabled(@"sideload_auth_identity")),
        @"keychain_enabled":@(RVEnabled(@"sideload_auth_keychain")),@"diagnostics_enabled":@([RVSetting(@"diagnostics") boolValue]),
        @"installed_bundle_matches_original":@([NSBundle.mainBundle.bundleIdentifier isEqual:RVAuthOriginalBundle]),
        @"original_callback_scheme_registered":@(RVAuthSchemeRegistered()),@"private_keychain_enabled":@(RVEnabled(@"sideload_auth_keychain") && RVAuthSchemeRegistered()),
        @"storage_strategy":@"native_private_keychain",
        @"counts":counts,@"recent_events":events,@"device_login_verified":@NO};
}
static void RVInstallAuthentication(void) {
    RVAuthEvents=[NSMutableArray array];RVAuthCounts=[NSMutableDictionary dictionary];
    RVHook(@"SSOConfiguration",@"applicationIdentifier",@"@@:",NO,^id(IMP original,SEL selector) {
        return ^id(id object) {
            id native=((id(*)(id,SEL))original)(object,selector);
            BOOL tuple=RVAuthSupportedConfiguration(object),active=RVEnabled(@"sideload_auth_identity") && tuple;
            RVAuthRecord(@"identity",@{@"tuple_matches":@(tuple),@"native_matches_original":@([native isEqual:RVAuthOriginalBundle]),@"adapter_applied":@(active)});
            return active ? RVAuthOriginalBundle : native;
        };
    });
    // The verified native query builders omit an explicit access group when
    // these getters return nil or usePrivateKeychain is enabled. Security then
    // chooses the installed app's authorized default group without a probe.
    for (NSString *getter in @[@"accessGroup",@"sharedAccessGroup"]) {
        RVHook(@"SSOKeychainHelper",getter,@"@@:",YES,^id(IMP original,SEL selector) {
            return ^id(id object) {
                if (RVEnabled(@"sideload_auth_keychain") && RVAuthSchemeRegistered()) {
                    RVAuthRecord(@"keychain_adapter",@{@"adapter_applied":@YES,@"mode":@"default_group"});return nil;
                }
                return ((id(*)(id,SEL))original)(object,selector);
            };
        });
    }
    RVHook(@"SSOKeychainHelper",@"usePrivateKeychain",@"B@:",YES,^id(IMP original,SEL selector) {
        return ^BOOL(id object) {
            return RVEnabled(@"sideload_auth_keychain") && RVAuthSchemeRegistered() ? YES : ((BOOL(*)(id,SEL))original)(object,selector);
        };
    });
    RVHook(@"SSOService",@"fetcherWithRequest:configuration:",@"@@:@@",YES,^id(IMP original,SEL selector) {
        return ^id(id object,id request,id configuration) {
            id fetcher=((id(*)(id,SEL,id,id))original)(object,selector,request,configuration);
            BOOL tuple=RVAuthSupportedConfiguration(configuration);
            NSMutableDictionary *summary=[RVAuthRequestSummary(request) mutableCopy];summary[@"tuple_matches"]=@(tuple);
            BOOL applied=NO;
            if (RVEnabled(@"sideload_auth_identity") && tuple && [request isKindOfClass:NSURLRequest.class]
                && RVAuthProductionURL([request URL]) && RVCan(fetcher,"setRequestValue:forHTTPHeaderField:",@"v@:@@")) {
                id effective=RVObject(fetcher,"request");
                if ([effective isKindOfClass:NSURLRequest.class] && RVAuthProductionURL([effective URL])) {
                    NSString *native=[effective valueForHTTPHeaderField:@"User-Agent"];
                    id adapted=RVAuthUserAgentIdentity(native,NSBundle.mainBundle.bundleIdentifier);
                    if (adapted!=native) {
                        ((void(*)(id,SEL,id,id))objc_msgSend)(fetcher,sel_registerName("setRequestValue:forHTTPHeaderField:"),adapted,@"User-Agent");applied=YES;
                    }
                    summary[@"user_agent_original_identity"]=@([adapted hasPrefix:[RVAuthOriginalBundle stringByAppendingString:@"/"]]);
                }
            }
            summary[@"user_agent_adapter_applied"]=@(applied);RVAuthRecord(@"auth_request",summary);
            return fetcher;
        };
    });
    RVHook(@"SSOSafariSignIn",@"signInWithURL:presentationAnchor:completionHandler:",@"v@:@@@?",NO,^id(IMP original,SEL selector) {
        return ^(id object,id url,id anchor,id completion) {
            RVAuthRecord(@"system_browser",RVAuthURLSummary(url));
            ((void(*)(id,SEL,id,id,id))original)(object,selector,url,anchor,completion);
        };
    });
    RVHook(@"SSOSafariSignIn",@"SSOErrorFromAuthSessionError:",@"@@:@",NO,^id(IMP original,SEL selector) {
        return ^id(id object,id error) {
            RVAuthRecord(@"session_error",RVAuthErrorSummary(error));
            return ((id(*)(id,SEL,id))original)(object,selector,error);
        };
    });
    RVHook(@"SSOOAuth2SignIn",@"displayRequest:",@"v@:@",NO,^id(IMP original,SEL selector) {
        return ^(id object,id request) {
            RVAuthRecord(@"embedded_browser",RVAuthURLSummary([request isKindOfClass:NSURLRequest.class] ? [request URL] : nil));
            ((void(*)(id,SEL,id))original)(object,selector,request);
        };
    });
    RVHook(@"SSOOAuth2SignIn",@"loadFailedWithError:",@"B@:@",NO,^id(IMP original,SEL selector) {
        return ^BOOL(id object,id error) {
            RVAuthRecord(@"embedded_error",RVAuthErrorSummary(error));
            return ((BOOL(*)(id,SEL,id))original)(object,selector,error);
        };
    });
    RVHook(@"SSOService",@"continueAuthenticationForURL:externalAuthState:",@"v@:@@",NO,^id(IMP original,SEL selector) {
        return ^(id object,id url,id state) {
            RVAuthRecord(@"callback",RVAuthURLSummary(url));
            ((void(*)(id,SEL,id,id))original)(object,selector,url,state);
        };
    });
    RVHook(@"SSOService",@"finishExternalSignInWithSceneSessionID:callbackURL:error:",@"v@:@@@",NO,^id(IMP original,SEL selector) {
        return ^(id object,id scene,id url,id error) {
            RVAuthRecord(@"session_end",RVAuthErrorSummary(error));
            RVAuthRecord(@"session_end_url",RVAuthURLSummary(url));
            ((void(*)(id,SEL,id,id,id))original)(object,selector,scene,url,error);
        };
    });
    for (NSString *getter in @[@"secItemAdd:result:",@"secItemCopyMatching:result:"]) {
        RVHook(@"SSOKeychainCore",getter,@"i@:@^@",YES,^id(IMP original,SEL selector) {
            return ^int(id object,id query,id __autoreleasing *result) {
                int status=((int(*)(id,SEL,id,id __autoreleasing *))original)(object,selector,query,result);
                RVAuthRecord([getter isEqual:@"secItemAdd:result:"] ? @"keychain_add" : @"keychain_read",@{@"numeric_status":@(status)});
                return status;
            };
        });
    }
    RVHook(@"SSOKeychainCore",@"secItemDelete:",@"i@:@",YES,^id(IMP original,SEL selector) {
        return ^int(id object,id query) {
            int status=((int(*)(id,SEL,id))original)(object,selector,query);
            RVAuthRecord(@"keychain_delete",@{@"numeric_status":@(status)});return status;
        };
    });
    RVHook(@"SSOKeychainCore",@"secItemUpdate:value:",@"i@:@@",YES,^id(IMP original,SEL selector) {
        return ^int(id object,id query,id value) {
            int status=((int(*)(id,SEL,id,id))original)(object,selector,query,value);
            RVAuthRecord(@"keychain_update",@{@"numeric_status":@(status)});return status;
        };
    });
}
