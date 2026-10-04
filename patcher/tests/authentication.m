// SPDX-License-Identifier: MIT
// Runs the production ABI guards, auth hooks, redaction and keychain probe.
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

static BOOL RVCompatible=YES;
static NSMutableDictionary *Settings;
static NSMutableArray *RVStatus;
static id RVSetting(NSString *key) { return Settings[key]; }
static BOOL RVEnabled(NSString *key) { return RVCompatible && [RVSetting(key) boolValue]; }
static void RVLog(NSString *message) { [RVStatus addObject:message]; }
#include "../native/RVRuntime.inc"
#include "../native/RVAuthentication.inc"

@interface SSOConfiguration : NSObject
@property(copy) NSString *clientID;
@property(copy) NSString *applicationScheme;
@property(copy) NSString *nativeIdentity;
- (NSString *)applicationIdentifier;
@end
@implementation SSOConfiguration
- (NSString *)applicationIdentifier { return self.nativeIdentity; }
@end
@interface SSOKeychainHelper : NSObject
+ (NSString *)accessGroup;
+ (NSString *)sharedAccessGroup;
@end
@implementation SSOKeychainHelper
+ (NSString *)accessGroup { return [self sharedAccessGroup]; }
+ (NSString *)sharedAccessGroup { return @"native.fixture.group"; }
@end
static int CoreStatus=-34018;
static id LastQuery,LastValue,LastURL,LastState,LastAnchor,LastCompletion,LastError;
@interface SSOKeychainCore : NSObject
+ (int)secItemAdd:(id)query result:(id __autoreleasing *)result;
+ (int)secItemCopyMatching:(id)query result:(id __autoreleasing *)result;
+ (int)secItemUpdate:(id)query value:(id)value;
@end
@implementation SSOKeychainCore
+ (int)secItemAdd:(id)query result:(id __autoreleasing *)result { LastQuery=query;if (result) *result=@"RESULT_SECRET";return CoreStatus; }
+ (int)secItemCopyMatching:(id)query result:(id __autoreleasing *)result { return [self secItemAdd:query result:result]; }
+ (int)secItemUpdate:(id)query value:(id)value { LastQuery=query;LastValue=value;return CoreStatus; }
@end
@interface SSOSafariSignIn : NSObject
- (void)signInWithURL:(id)url presentationAnchor:(id)anchor completionHandler:(void (^)(id,id))completion;
- (id)SSOErrorFromAuthSessionError:(id)error;
@end
@implementation SSOSafariSignIn
- (void)signInWithURL:(id)url presentationAnchor:(id)anchor completionHandler:(void (^)(id,id))completion {
    LastURL=url;LastAnchor=anchor;LastCompletion=completion;
}
- (id)SSOErrorFromAuthSessionError:(id)error { LastError=error;return error; }
@end
@interface SSOOAuth2SignIn : NSObject
- (void)displayRequest:(id)request;
- (bool)loadFailedWithError:(id)error;
@end
@implementation SSOOAuth2SignIn
- (void)displayRequest:(id)request { LastURL=[request URL]; }
- (bool)loadFailedWithError:(id)error { LastError=error;return true; }
@end
@interface SSOService : NSObject
- (void)continueAuthenticationForURL:(id)url externalAuthState:(id)state;
- (void)finishExternalSignInWithSceneSessionID:(id)scene callbackURL:(id)url error:(id)error;
@end
@implementation SSOService
- (void)continueAuthenticationForURL:(id)url externalAuthState:(id)state { LastURL=url;LastState=state; }
- (void)finishExternalSignInWithSceneSessionID:(id)scene callbackURL:(id)url error:(id)error { LastState=scene;LastURL=url;LastError=error; }
@end

static OSStatus AddStatus,CopyStatus,RemoveStatus;
static int Adds,Copies,Removes;
static BOOL Malformed;
static NSString *ProbeGroup;
static NSDictionary *LastProbe;
static OSStatus ProbeAdd(CFDictionaryRef raw,CFTypeRef *result) {
    NSDictionary *query=(__bridge NSDictionary *)raw;LastProbe=query;Adds++;
    assert(!query[(__bridge id)kSecAttrAccessGroup]);assert(!query[(__bridge id)kSecReturnData]);
    assert([query[(__bridge id)kSecAttrService] isEqual:@"RVPort.AuthGroupProbe"]);
    assert([query[(__bridge id)kSecValueData] length]==0);
    if (AddStatus==errSecSuccess) *result=CFBridgingRetain(Malformed ? (id)@1 : @{(__bridge id)kSecAttrAccessGroup:ProbeGroup});
    return AddStatus;
}
static OSStatus ProbeCopy(CFDictionaryRef raw,CFTypeRef *result) {
    NSDictionary *query=(__bridge NSDictionary *)raw;Copies++;
    assert(!query[(__bridge id)kSecReturnData]);assert(!query[(__bridge id)kSecValueData]);
    assert([query[(__bridge id)kSecAttrAccount] isEqual:LastProbe[(__bridge id)kSecAttrAccount]]);
    if (CopyStatus==errSecSuccess) *result=CFBridgingRetain(@{(__bridge id)kSecAttrAccessGroup:ProbeGroup});
    return CopyStatus;
}
static OSStatus ProbeDelete(CFDictionaryRef raw) {
    NSDictionary *query=(__bridge NSDictionary *)raw;Removes++;
    assert(query.count==3);assert([query[(__bridge id)kSecAttrAccount] isEqual:LastProbe[(__bridge id)kSecAttrAccount]]);
    return RemoveStatus;
}
static void ResetProbe(void) { AddStatus=CopyStatus=RemoveStatus=0;Adds=Copies=Removes=0;Malformed=NO;ProbeGroup=@"AUTHORIZED_GROUP_SECRET";RVAuthGroup=nil; }
static NSString *JSON(id value) {
    return [[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:value options:0 error:nil] encoding:NSUTF8StringEncoding];
}
static void Private(id value) {
    NSString *text=JSON(value);
    for (NSString *secret in @[@"EMAIL_SECRET",@"CODE_SECRET",@"STATE_SECRET",@"TOKEN_SECRET",@"VERIFIER_SECRET",
            @"COOKIE_SECRET",@"DEVICE_SECRET",@"ERROR_SECRET",@"DOMAIN_SECRET",@"RESULT_SECRET",@"AUTHORIZED_GROUP_SECRET"])
        assert(![text containsString:secret]);
}
int main(void) { @autoreleasepool {
    Settings=[@{@"diagnostics":@YES,@"sideload_auth_identity":@YES,@"sideload_auth_keychain":@YES} mutableCopy];
    RVStatus=[NSMutableArray array];
    // Construct before installation: the getter must adapt existing objects too.
    SSOConfiguration *config=[SSOConfiguration new];config.clientID=RVAuthClientID;config.applicationScheme=RVAuthScheme;
    config.nativeIdentity=@"io.fixture.sideload";
    assert(RVAuthSchemeRegistered());
    assert([config.applicationIdentifier isEqual:config.nativeIdentity]);
    RVInstallAuthentication();RVAuthAPI=(RVAuthKeychainAPI){ProbeAdd,ProbeCopy,ProbeDelete};
    assert([config.applicationIdentifier isEqual:RVAuthOriginalBundle]);
    config.clientID=@"unknown";assert([config.applicationIdentifier isEqual:config.nativeIdentity]);
    config.clientID=RVAuthClientID;config.applicationScheme=@"unknown";assert([config.applicationIdentifier isEqual:config.nativeIdentity]);
    config.applicationScheme=RVAuthScheme;Settings[@"sideload_auth_identity"]=@NO;
    assert([config.applicationIdentifier isEqual:config.nativeIdentity]);Settings[@"sideload_auth_identity"]=@YES;
    RVCompatible=NO;assert([config.applicationIdentifier isEqual:config.nativeIdentity]);RVCompatible=YES;

    OSStatus status,cleanup;ResetProbe();
    assert([RVAuthDiscoverGroup(RVAuthAPI,&status,&cleanup) isEqual:ProbeGroup]);assert(Adds==1 && Removes==1 && !Copies && status==0);
    ResetProbe();AddStatus=errSecDuplicateItem;
    assert([RVAuthDiscoverGroup(RVAuthAPI,&status,&cleanup) isEqual:ProbeGroup]);assert(Copies==1 && Removes==0);
    ResetProbe();AddStatus=errSecDuplicateItem;CopyStatus=errSecItemNotFound;
    assert(!RVAuthDiscoverGroup(RVAuthAPI,&status,&cleanup));assert(Removes==0 && status==errSecItemNotFound);
    ResetProbe();Malformed=YES;assert(!RVAuthDiscoverGroup(RVAuthAPI,&status,&cleanup));assert(status==errSecParam && Removes==1);
    ResetProbe();ProbeGroup=@"*";assert(!RVAuthDiscoverGroup(RVAuthAPI,&status,&cleanup));assert(Removes==1);
    ResetProbe();RemoveStatus=errSecInteractionNotAllowed;
    assert(RVAuthDiscoverGroup(RVAuthAPI,&status,&cleanup));assert(cleanup==errSecInteractionNotAllowed);
    ResetProbe();AddStatus=errSecInteractionNotAllowed;
    assert([[SSOKeychainHelper accessGroup] isEqual:@"native.fixture.group"]);assert(!RVAuthGroup && Removes==0);
    AddStatus=0;assert([[SSOKeychainHelper accessGroup] isEqual:ProbeGroup]);int calls=Adds;
    assert([[SSOKeychainHelper sharedAccessGroup] isEqual:ProbeGroup]);assert(Adds==calls);
    Settings[@"sideload_auth_keychain"]=@NO;assert([[SSOKeychainHelper accessGroup] isEqual:@"native.fixture.group"]);
    Settings[@"sideload_auth_keychain"]=@YES;

    NSURL *url=[NSURL URLWithString:[NSString stringWithFormat:@"https://accounts.google.com/o/oauth2/v2/auth?client_id=%@&package_name=%@&redirect_uri=%@%%3A%%2FauthCallback&state=STATE_SECRET&code_challenge=VERIFIER_SECRET&code_challenge_method=S256&code=CODE_SECRET&login_hint=EMAIL_SECRET&token=TOKEN_SECRET&cookie=COOKIE_SECRET&device_challenge_request=DEVICE_SECRET#error=ERROR_SECRET",RVAuthClientID,RVAuthOriginalBundle,RVAuthScheme]];
    NSDictionary *summary=RVAuthURLSummary(url);assert([summary[@"client_matches"] boolValue] && [summary[@"package_matches"] boolValue]);
    assert([summary[@"redirect_scheme_matches"] boolValue] && [summary[@"challenge_method"] isEqual:@"S256"]);Private(summary);
    NSURL *callback=[NSURL URLWithString:[RVAuthScheme stringByAppendingString:@":/authCallback?authorization_code=CODE_SECRET&state=STATE_SECRET"]];
    assert([RVAuthURLSummary(callback)[@"callback_matches"] boolValue]);Private(RVAuthURLSummary(callback));
    assert(![RVAuthURLSummary([NSURL URLWithString:@"https://accounts.google.com.evil.test/signin?code=CODE_SECRET"])[@"google_endpoint"] boolValue]);
    assert(![RVAuthURLSummary([NSURL URLWithString:@"https://user:COOKIE_SECRET@accounts.google.com/signin"])[@"google_endpoint"] boolValue]);
    NSDictionary *errorURL=RVAuthURLSummary([NSURL URLWithString:@"https://accounts.google.com/signin?error=ERROR_SECRET&error_description=EMAIL_SECRET"]);
    assert([errorURL[@"oauth_error"] isEqual:@"other"]);Private(errorURL);
    assert([RVAuthURLSummary([NSURL URLWithString:@"https://accounts.google.com/signin?client_id=other&client_id=other&client_id=other"])[@"client_matches"] boolValue]==NO);
    assert([RVAuthURLSummary([NSURL URLWithString:@"https://accounts.google.com/signin?error=disallowed_useragent"])[@"oauth_error"] isEqual:@"disallowed_useragent"]);
    NSError *error=[NSError errorWithDomain:@"DOMAIN_SECRET" code:7 userInfo:@{NSLocalizedDescriptionKey:@"EMAIL_SECRET TOKEN_SECRET"}];
    Private(RVAuthErrorSummary(error));assert([RVAuthErrorSummary(error)[@"numeric_code"] intValue]==7);
    SSOSafariSignIn *safari=[SSOSafariSignIn new];id anchor=[NSObject new];void (^completion)(id,id)=^(id a,id b) { (void)a;(void)b; };
    [safari signInWithURL:url presentationAnchor:anchor completionHandler:completion];assert(LastURL==url && LastAnchor==anchor && LastCompletion==completion);
    assert([safari SSOErrorFromAuthSessionError:error]==error && LastError==error);
    SSOOAuth2SignIn *embedded=[SSOOAuth2SignIn new];[embedded displayRequest:[NSURLRequest requestWithURL:url]];
    assert(LastURL==url && [embedded loadFailedWithError:error]);
    SSOService *service=[SSOService new];id state=[NSObject new];[service continueAuthenticationForURL:callback externalAuthState:state];
    assert(LastURL==callback && LastState==state);[service finishExternalSignInWithSceneSessionID:state callbackURL:callback error:error];
    assert(LastURL==callback && LastError==error && LastState==state);
    NSDictionary *query=@{@"TOKEN_SECRET":@"TOKEN_SECRET"};id result=nil;
    assert([SSOKeychainCore secItemAdd:query result:&result]==CoreStatus && LastQuery==query && [result isEqual:@"RESULT_SECRET"]);
    assert([SSOKeychainCore secItemCopyMatching:query result:&result]==CoreStatus);
    assert([SSOKeychainCore secItemUpdate:query value:state]==CoreStatus && LastValue==state);
    Private(RVAuthenticationReport());
    for (int i=0;i<100;i++) (void)config.applicationIdentifier;
    assert([RVAuthenticationReport()[@"recent_events"] count]==64);Private(RVAuthenticationReport());
    NSUInteger records=RVAuthEvents.count;Settings[@"diagnostics"]=@NO;(void)config.applicationIdentifier;
    assert(RVAuthEvents.count==records);
    assert(RVStatus.count==12); // Every production auth hook installed with its actual ABI.
    for (NSString *entry in RVStatus) assert([entry hasPrefix:@"Installed "]);
    puts("Authentication privacy, keychain ownership, fallback and native forwarding checks passed");
} return 0; }
