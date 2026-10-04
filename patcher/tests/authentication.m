// SPDX-License-Identifier: MIT
// Runs production ABI guards, native-private-keychain hooks, request identity and redaction.
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
+ (bool)usePrivateKeychain;
+ (NSMutableDictionary *)queryMatchingID:(id)account serviceName:(id)service;
+ (NSMutableDictionary *)deleteQueryMatchingID:(id)account serviceName:(id)service;
+ (NSMutableDictionary *)queryForUpdatingKeychainItem:(id)item;
@end
@implementation SSOKeychainHelper
+ (NSString *)accessGroup { return [self sharedAccessGroup]; }
+ (NSString *)sharedAccessGroup { return @"native.fixture.group"; }
+ (bool)usePrivateKeychain { return false; }
+ (NSMutableDictionary *)queryMatchingID:(id)account serviceName:(id)service {
    if (!account || !service) return nil;
    NSMutableDictionary *query=[@{(__bridge id)kSecClass:(__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrAccount:account,(__bridge id)kSecAttrService:service,
        (__bridge id)kSecReturnAttributes:@YES,(__bridge id)kSecReturnData:@YES,
        (__bridge id)kSecMatchLimit:(__bridge id)kSecMatchLimitOne} mutableCopy];
    NSString *group=[self accessGroup];if (group && ![self usePrivateKeychain]) query[(__bridge id)kSecAttrAccessGroup]=group;
    return query;
}
+ (NSMutableDictionary *)deleteQueryMatchingID:(id)account serviceName:(id)service {
    NSMutableDictionary *query=[self queryMatchingID:account serviceName:service];
    [query removeObjectsForKeys:@[(__bridge id)kSecReturnData,(__bridge id)kSecReturnAttributes,(__bridge id)kSecMatchLimit]];return query;
}
+ (NSMutableDictionary *)queryForUpdatingKeychainItem:(id)item {
    NSMutableDictionary *query=[self deleteQueryMatchingID:item[(__bridge id)kSecAttrAccount] serviceName:item[(__bridge id)kSecAttrService]];
    if (item[(__bridge id)kSecAttrAccessGroup]) query[(__bridge id)kSecAttrAccessGroup]=item[(__bridge id)kSecAttrAccessGroup];return query;
}
@end
static int CoreStatus=-34018;
static id LastQuery,LastValue,LastURL,LastState,LastAnchor,LastCompletion,LastError;
@interface SSOKeychainCore : NSObject
+ (int)secItemAdd:(id)query result:(id __autoreleasing *)result;
+ (int)secItemCopyMatching:(id)query result:(id __autoreleasing *)result;
+ (int)secItemUpdate:(id)query value:(id)value;
+ (int)secItemDelete:(id)query;
@end
@implementation SSOKeychainCore
+ (int)secItemAdd:(id)query result:(id __autoreleasing *)result { LastQuery=query;if (result) *result=@"RESULT_SECRET";return CoreStatus; }
+ (int)secItemCopyMatching:(id)query result:(id __autoreleasing *)result { return [self secItemAdd:query result:result]; }
+ (int)secItemUpdate:(id)query value:(id)value { LastQuery=query;LastValue=value;return CoreStatus; }
+ (int)secItemDelete:(id)query { LastQuery=query;return CoreStatus; }
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
@interface GTMSessionFetcher : NSObject
@property(nonatomic,strong) NSMutableURLRequest *request;
- (void)setRequestValue:(id)value forHTTPHeaderField:(id)field;
@end
@implementation GTMSessionFetcher
- (void)setRequestValue:(id)value forHTTPHeaderField:(id)field { [self.request setValue:value forHTTPHeaderField:field]; }
@end
static BOOL NilFetcher;
static id LastRequest,LastConfiguration;
@interface SSOService : NSObject
+ (id)fetcherWithRequest:(id)request configuration:(id)configuration;
- (void)continueAuthenticationForURL:(id)url externalAuthState:(id)state;
- (void)finishExternalSignInWithSceneSessionID:(id)scene callbackURL:(id)url error:(id)error;
@end
@implementation SSOService
+ (id)fetcherWithRequest:(id)request configuration:(id)configuration {
    LastRequest=request;LastConfiguration=configuration;if (NilFetcher) return nil;
    GTMSessionFetcher *fetcher=[GTMSessionFetcher new];fetcher.request=[request mutableCopy];
    [fetcher.request setValue:[NSBundle.mainBundle.bundleIdentifier stringByAppendingString:@"/21.39.4 iSL/3.5 platform (gzip)"] forHTTPHeaderField:@"User-Agent"];
    return fetcher;
}
- (void)continueAuthenticationForURL:(id)url externalAuthState:(id)state { LastURL=url;LastState=state; }
- (void)finishExternalSignInWithSceneSessionID:(id)scene callbackURL:(id)url error:(id)error { LastState=scene;LastURL=url;LastError=error; }
@end

static NSString *JSON(id value) {
    return [[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:value options:0 error:nil] encoding:NSUTF8StringEncoding];
}
static void Private(id value) {
    NSString *text=JSON(value);
    for (NSString *secret in @[@"EMAIL_SECRET",@"CODE_SECRET",@"STATE_SECRET",@"TOKEN_SECRET",@"VERIFIER_SECRET",
            @"COOKIE_SECRET",@"DEVICE_SECRET",@"ERROR_SECRET",@"DOMAIN_SECRET",@"RESULT_SECRET",@"AUTHORIZED_GROUP_SECRET",@"ACCOUNT_SECRET",@"SERVICE_SECRET"])
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
    RVInstallAuthentication();
    assert([config.applicationIdentifier isEqual:RVAuthOriginalBundle]);
    config.clientID=@"unknown";assert([config.applicationIdentifier isEqual:config.nativeIdentity]);
    config.clientID=RVAuthClientID;config.applicationScheme=@"unknown";assert([config.applicationIdentifier isEqual:config.nativeIdentity]);
    config.applicationScheme=RVAuthScheme;Settings[@"sideload_auth_identity"]=@NO;
    assert([config.applicationIdentifier isEqual:config.nativeIdentity]);Settings[@"sideload_auth_identity"]=@YES;
    RVCompatible=NO;assert([config.applicationIdentifier isEqual:config.nativeIdentity]);RVCompatible=YES;

    // The native builders select Security's default group with no probe or
    // shared-group entitlement. Adapter-off and incompatible profiles stay native.
    assert(![SSOKeychainHelper accessGroup] && ![SSOKeychainHelper sharedAccessGroup]);
    assert([SSOKeychainHelper usePrivateKeychain]);
    id account=@"ACCOUNT_SECRET",serviceName=@"SERVICE_SECRET";
    NSDictionary *read=[SSOKeychainHelper queryMatchingID:account serviceName:serviceName];
    assert(!read[(__bridge id)kSecAttrAccessGroup] && [read[(__bridge id)kSecAttrAccount] isEqual:account]);
    assert([read[(__bridge id)kSecReturnData] boolValue] && [read[(__bridge id)kSecReturnAttributes] boolValue]);
    NSDictionary *remove=[SSOKeychainHelper deleteQueryMatchingID:account serviceName:serviceName];
    assert(!remove[(__bridge id)kSecAttrAccessGroup] && !remove[(__bridge id)kSecReturnData]);
    NSDictionary *update=[SSOKeychainHelper queryForUpdatingKeychainItem:read];assert(!update[(__bridge id)kSecAttrAccessGroup]);
    assert(![SSOKeychainHelper queryMatchingID:nil serviceName:serviceName]);
    Settings[@"sideload_auth_keychain"]=@NO;
    assert([[SSOKeychainHelper accessGroup] isEqual:@"native.fixture.group"] && ![SSOKeychainHelper usePrivateKeychain]);
    read=[SSOKeychainHelper queryMatchingID:account serviceName:serviceName];assert([read[(__bridge id)kSecAttrAccessGroup] isEqual:@"native.fixture.group"]);
    Settings[@"sideload_auth_keychain"]=@YES;RVCompatible=NO;
    assert([[SSOKeychainHelper sharedAccessGroup] isEqual:@"native.fixture.group"] && ![SSOKeychainHelper usePrivateKeychain]);RVCompatible=YES;

    NSString *nativeAgent=[NSBundle.mainBundle.bundleIdentifier stringByAppendingString:@"/21.39.4 iSL/3.5 platform (gzip)"];
    NSString *adaptedAgent=[RVAuthOriginalBundle stringByAppendingString:@"/21.39.4 iSL/3.5 platform (gzip)"];
    assert([RVAuthUserAgentIdentity(nativeAgent,NSBundle.mainBundle.bundleIdentifier) isEqual:adaptedAgent]);
    NSString *otherAgent=@"other.app/1 native";assert(RVAuthUserAgentIdentity(otherAgent,NSBundle.mainBundle.bundleIdentifier)==otherAgent);
    assert(RVAuthUserAgentIdentity(nativeAgent,RVAuthOriginalBundle)==nativeAgent);
    id nonString=@1;assert(RVAuthUserAgentIdentity(nativeAgent,nil)==nativeAgent && RVAuthUserAgentIdentity(nonString,@"io.fixture.sideload")==nonString);
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://oauthaccountmanager.googleapis.com/v1/authadvice"]];
    request.HTTPMethod=@"POST";[request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    NSDictionary *body=@{@"client_id":RVAuthClientID,@"mediator_client_id":RVAuthLibraryClientID,@"package_name":RVAuthOriginalBundle,
        @"redirect_uri":[RVAuthScheme stringByAppendingString:@":/authCallback"],@"device_challenge_request":@"DEVICE_SECRET",
        @"client_state":@"STATE_SECRET",@"email":@"EMAIL_SECRET",@"account_list":@[@"TOKEN_SECRET"]};
    request.HTTPBody=[NSJSONSerialization dataWithJSONObject:body options:0 error:nil];NSData *nativeBody=request.HTTPBody;
    NSDictionary *requestSummary=RVAuthRequestSummary(request);Private(requestSummary);
    assert([requestSummary[@"body_client_matches"] boolValue] && [requestSummary[@"body_mediator_matches"] boolValue]
        && [requestSummary[@"body_package_matches"] boolValue] && [requestSummary[@"device_challenge_present"] boolValue]);
    GTMSessionFetcher *fetcher=[SSOService fetcherWithRequest:request configuration:config];
    assert(LastRequest==request && LastConfiguration==config && [fetcher.request.HTTPBody isEqual:nativeBody]);
    assert([[fetcher.request valueForHTTPHeaderField:@"User-Agent"] isEqual:adaptedAgent]);
    assert([[fetcher.request valueForHTTPHeaderField:@"Content-Type"] isEqual:@"application/json"]);
    config.clientID=@"unknown";fetcher=[SSOService fetcherWithRequest:request configuration:config];
    assert([[fetcher.request valueForHTTPHeaderField:@"User-Agent"] isEqual:nativeAgent]);config.clientID=RVAuthClientID;
    Settings[@"sideload_auth_identity"]=@NO;fetcher=[SSOService fetcherWithRequest:request configuration:config];
    assert([[fetcher.request valueForHTTPHeaderField:@"User-Agent"] isEqual:nativeAgent]);Settings[@"sideload_auth_identity"]=@YES;
    RVCompatible=NO;fetcher=[SSOService fetcherWithRequest:request configuration:config];assert([[fetcher.request valueForHTTPHeaderField:@"User-Agent"] isEqual:nativeAgent]);RVCompatible=YES;
    for (NSString *target in @[@"http://oauthaccountmanager.googleapis.com/v1/authadvice",@"https://oauthaccountmanager.googleapis.com.evil.test/v1/authadvice",
                              @"https://user:COOKIE_SECRET@oauthaccountmanager.googleapis.com/v1/authadvice",@"https://oauthaccountmanager.googleapis.com:8443/v1/authadvice"]) {
        request.URL=[NSURL URLWithString:target];fetcher=[SSOService fetcherWithRequest:request configuration:config];
        assert([[fetcher.request valueForHTTPHeaderField:@"User-Agent"] isEqual:nativeAgent]);Private(RVAuthRequestSummary(request));
    }
    NilFetcher=YES;assert(![SSOService fetcherWithRequest:request configuration:config]);NilFetcher=NO;
    request.URL=[NSURL URLWithString:@"https://oauthaccountmanager.googleapis.com/v1/authadvice"];request.HTTPBody=[@"TOKEN_SECRET" dataUsingEncoding:NSUTF8StringEncoding];
    assert([RVAuthRequestSummary(request)[@"body_class"] isEqual:@"unparsed"]);Private(RVAuthRequestSummary(request));
    request.HTTPBody=nil;request.HTTPBodyStream=[NSInputStream inputStreamWithData:nativeBody];
    assert([RVAuthRequestSummary(request)[@"body_class"] isEqual:@"streamed"]);Private(RVAuthRequestSummary(request));

    NSURL *url=[NSURL URLWithString:[NSString stringWithFormat:@"https://accounts.google.com/o/oauth2/v2/auth?client_id=%@&package_name=%@&redirect_uri=%@%%3A%%2FauthCallback&state=STATE_SECRET&code_challenge=VERIFIER_SECRET&code_challenge_method=S256&code=CODE_SECRET&login_hint=EMAIL_SECRET&token=TOKEN_SECRET&cookie=COOKIE_SECRET&device_challenge_request=DEVICE_SECRET#error=ERROR_SECRET",RVAuthClientID,RVAuthOriginalBundle,RVAuthScheme]];
    NSDictionary *summary=RVAuthURLSummary(url);assert([summary[@"client_matches"] boolValue] && [summary[@"package_matches"] boolValue]);
    NSURL *libraryURL=[NSURL URLWithString:[@"https://accounts.google.com/ServiceLogin?client_id=" stringByAppendingString:RVAuthLibraryClientID]];
    assert([RVAuthURLSummary(libraryURL)[@"client_kind"] isEqual:@"sso_library"] && ![RVAuthURLSummary(libraryURL)[@"client_matches"] boolValue]);
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
    assert([SSOKeychainCore secItemDelete:query]==CoreStatus && LastQuery==query);
    Private(RVAuthenticationReport());
    for (int i=0;i<100;i++) (void)config.applicationIdentifier;
    assert([RVAuthenticationReport()[@"recent_events"] count]==64);Private(RVAuthenticationReport());
    NSUInteger records=RVAuthEvents.count;Settings[@"diagnostics"]=@NO;(void)config.applicationIdentifier;
    assert(RVAuthEvents.count==records);
    assert(RVStatus.count==15); // Every production auth hook installed with its actual ABI.
    for (NSString *entry in RVStatus) assert([entry hasPrefix:@"Installed "]);
    puts("Authentication privacy, native private keychain, request identity and forwarding checks passed");
} return 0; }
