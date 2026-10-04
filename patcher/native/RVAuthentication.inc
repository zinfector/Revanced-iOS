// SPDX-License-Identifier: MIT
#include "RVAuthenticationSupport.h"

static NSMutableArray<NSDictionary *> *RVAuthEvents;
static NSMutableDictionary<NSString *,NSNumber *> *RVAuthCounts;
static NSObject *RVAuthGroupLock;
static NSString *RVAuthGroup;
static RVAuthKeychainAPI RVAuthAPI={SecItemAdd,SecItemCopyMatching,SecItemDelete};

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
static NSString *RVAuthAccessibleGroup(void) {
    @synchronized(RVAuthGroupLock) {
        if (RVAuthGroup) return RVAuthGroup;
        OSStatus status,cleanup;
        NSString *group=RVAuthDiscoverGroup(RVAuthAPI,&status,&cleanup);
        RVAuthRecord(@"keychain_probe",@{@"numeric_status":@(status),@"cleanup_status":@(cleanup),@"group_available":@(group!=nil)});
        // A locked-device or entitlement failure remains retryable.
        if (group) RVAuthGroup=group;
        return group;
    }
}
static NSDictionary *RVAuthenticationReport(void) {
    NSDictionary *counts;NSArray *events;
    @synchronized(RVAuthEvents) { counts=[RVAuthCounts copy] ?: @{};events=[RVAuthEvents copy] ?: @[]; }
    BOOL available;@synchronized(RVAuthGroupLock) { available=RVAuthGroup!=nil; }
    return @{@"schema":@1,@"identity_enabled":@(RVEnabled(@"sideload_auth_identity")),
        @"keychain_enabled":@(RVEnabled(@"sideload_auth_keychain")),@"diagnostics_enabled":@([RVSetting(@"diagnostics") boolValue]),
        @"installed_bundle_matches_original":@([NSBundle.mainBundle.bundleIdentifier isEqual:RVAuthOriginalBundle]),
        @"original_callback_scheme_registered":@(RVAuthSchemeRegistered()),@"accessible_group_discovered":@(available),
        @"counts":counts,@"recent_events":events,@"device_login_verified":@NO};
}
static void RVInstallAuthentication(void) {
    RVAuthEvents=[NSMutableArray array];RVAuthCounts=[NSMutableDictionary dictionary];RVAuthGroupLock=[NSObject new];
    RVHook(@"SSOConfiguration",@"applicationIdentifier",@"@@:",NO,^id(IMP original,SEL selector) {
        return ^id(id object) {
            id native=((id(*)(id,SEL))original)(object,selector);
            BOOL tuple=RVAuthSupportedConfiguration(object),active=RVEnabled(@"sideload_auth_identity") && tuple;
            RVAuthRecord(@"identity",@{@"tuple_matches":@(tuple),@"native_matches_original":@([native isEqual:RVAuthOriginalBundle]),@"adapter_applied":@(active)});
            return active ? RVAuthOriginalBundle : native;
        };
    });
    for (NSString *getter in @[@"accessGroup",@"sharedAccessGroup"]) {
        RVHook(@"SSOKeychainHelper",getter,@"@@:",YES,^id(IMP original,SEL selector) {
            return ^id(id object) {
                if (RVEnabled(@"sideload_auth_keychain") && RVAuthSchemeRegistered()) {
                    NSString *group=RVAuthAccessibleGroup();
                    if (group) { RVAuthRecord(@"keychain_adapter",@{@"adapter_applied":@YES});return group; }
                }
                return ((id(*)(id,SEL))original)(object,selector);
            };
        });
    }
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
    RVHook(@"SSOKeychainCore",@"secItemUpdate:value:",@"i@:@@",YES,^id(IMP original,SEL selector) {
        return ^int(id object,id query,id value) {
            int status=((int(*)(id,SEL,id,id))original)(object,selector,query,value);
            RVAuthRecord(@"keychain_update",@{@"numeric_status":@(status)});return status;
        };
    });
}
