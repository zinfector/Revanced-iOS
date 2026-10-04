// SPDX-License-Identifier: MIT
// Shared by the iOS adapter and the macOS authentication regression harness.
#pragma once
#import <Foundation/Foundation.h>
#import <Security/Security.h>

static NSString *const RVAuthOriginalBundle = @"com.google.ios.youtube";
static NSString *const RVAuthClientID = @"755541669657-kbosfavg7pk7sr3849c3tf657hpi5jpd.apps.googleusercontent.com";
static NSString *const RVAuthScheme = @"com.google.sso.755541669657-kbosfavg7pk7sr3849c3tf657hpi5jpd";

static BOOL RVAuthTupleMatches(id client,id scheme) {
    return [client isKindOfClass:NSString.class] && [scheme isKindOfClass:NSString.class]
        && [client isEqual:RVAuthClientID] && [scheme isEqual:RVAuthScheme];
}
static NSDictionary *RVAuthURLSummary(id value) {
    if (![value isKindOfClass:NSURL.class] || [(NSURL *)value absoluteString].length>16384)
        return @{@"url_present":@NO};
    NSURLComponents *parts=[NSURLComponents componentsWithURL:value resolvingAgainstBaseURL:NO];
    if (!parts) return @{@"url_present":@NO};
    NSString *host=parts.host.lowercaseString ?: @"",*scheme=parts.scheme.lowercaseString ?: @"";
    BOOL google=[scheme isEqual:@"https"] && !parts.user && !parts.password
        && (!parts.port || parts.port.integerValue==443)
        && ([host isEqual:@"accounts.google.com"] || [host isEqual:@"oauth2.googleapis.com"]);
    BOOL callback=[scheme isEqual:RVAuthScheme] && [parts.path isEqual:@"/authCallback"] && !parts.host;
    NSString *pathClass=callback ? @"callback" : !google ? @"other" :
        [parts.path hasPrefix:@"/o/oauth2/"] ? @"oauth" : [parts.path hasPrefix:@"/signin/"] ? @"signin" : @"other_google";
    // Only fixed field names, booleans and enum labels leave this function.
    // Query/fragment values, arbitrary hosts/paths and raw errors never leave it.
    NSMutableDictionary *summary=[@{@"url_present":@YES,@"google_endpoint":@(google),
        @"callback_matches":@(callback),@"path_class":pathClass,@"fragment_present":@(parts.fragment.length>0)} mutableCopy];
    if (!google && !callback) return summary;
    NSDictionary *presence=@{@"client_id":@"client_id_present",@"package_name":@"package_name_present",
        @"redirect_uri":@"redirect_uri_present",@"state":@"state_present",@"code_challenge":@"challenge_present",
        @"code":@"code_present",@"authorization_code":@"authorization_code_present",@"error":@"error_present"};
    for (NSString *key in presence.allValues) summary[key]=@NO;
    summary[@"client_matches"]=@NO;summary[@"package_matches"]=@NO;summary[@"redirect_scheme_matches"]=@NO;
    summary[@"challenge_method"]=@"absent";summary[@"oauth_error"]=@"absent";
    NSSet *errors=[NSSet setWithArray:@[@"disallowed_useragent",@"invalid_request",@"redirect_uri_mismatch",
        @"access_denied",@"invalid_client",@"unauthorized_client",@"deleted_client",@"org_internal",
        @"admin_policy_enforced",@"invalid_grant"]];
    NSMutableSet *seen=[NSMutableSet set];
    for (NSURLQueryItem *item in parts.queryItems) {
        NSString *key=item.name;
        if (presence[key]) summary[presence[key]]=@YES;
        // Duplicate identity fields are ambiguous; never mark them as matching.
        BOOL duplicate=[seen containsObject:key];[seen addObject:key];
        if ([key isEqual:@"client_id"]) summary[@"client_matches"]=@(!duplicate && [item.value isEqual:RVAuthClientID]);
        else if ([key isEqual:@"package_name"]) summary[@"package_matches"]=@(!duplicate && [item.value isEqual:RVAuthOriginalBundle]);
        else if ([key isEqual:@"redirect_uri"]) {
            NSURLComponents *redirect=[NSURLComponents componentsWithString:item.value ?: @""];
            summary[@"redirect_scheme_matches"]=@(!duplicate && [redirect.scheme isEqual:RVAuthScheme]);
        } else if ([key isEqual:@"code_challenge_method"])
            summary[@"challenge_method"]=duplicate ? @"ambiguous" : [item.value isEqual:@"S256"] ? @"S256" : [item.value isEqual:@"plain"] ? @"plain" : @"other";
        else if ([key isEqual:@"error"])
            summary[@"oauth_error"]=!duplicate && [errors containsObject:item.value ?: @""] ? item.value : @"other";
    }
    return summary;
}
static NSDictionary *RVAuthErrorSummary(id value) {
    if (![value isKindOfClass:NSError.class]) return @{@"error_present":@NO};
    NSError *error=value;
    NSString *domain=[error.domain isEqual:@"com.google.sso"] ? @"google_sso" :
        [error.domain isEqual:@"com.apple.AuthenticationServices.WebAuthenticationSession"] ? @"system_auth_session" :
        [error.domain isEqual:NSURLErrorDomain] ? @"url_loading" : @"other";
    return @{@"error_present":@YES,@"domain_class":domain,@"numeric_code":@(error.code)};
}

typedef struct {
    OSStatus (*add)(CFDictionaryRef,CFTypeRef *);
    OSStatus (*copy)(CFDictionaryRef,CFTypeRef *);
    OSStatus (*remove)(CFDictionaryRef);
} RVAuthKeychainAPI;

static NSString *RVAuthDiscoverGroup(RVAuthKeychainAPI api,OSStatus *status,OSStatus *cleanup) {
    // Random account identifies only this probe; no account items are searched.
    NSDictionary *identity=@{(__bridge id)kSecClass:(__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService:@"RVPort.AuthGroupProbe",(__bridge id)kSecAttrAccount:NSUUID.UUID.UUIDString};
    NSMutableDictionary *attributes=[identity mutableCopy];
    attributes[(__bridge id)kSecReturnAttributes]=@YES;
    attributes[(__bridge id)kSecAttrAccessible]=(__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    attributes[(__bridge id)kSecValueData]=[NSData data];
    CFTypeRef result=NULL;
    *cleanup=errSecSuccess;
    *status=api.add((__bridge CFDictionaryRef)attributes,&result);
    BOOL created=*status==errSecSuccess;
    if (*status==errSecDuplicateItem) {
        if (result) { CFRelease(result);result=NULL; }
        NSMutableDictionary *query=[identity mutableCopy];query[(__bridge id)kSecReturnAttributes]=@YES;
        *status=api.copy((__bridge CFDictionaryRef)query,&result);
    }
    id found=CFBridgingRelease(result);
    id group=[found isKindOfClass:NSDictionary.class] ? found[(__bridge id)kSecAttrAccessGroup] : nil;
    if (*status==errSecSuccess && (![group isKindOfClass:NSString.class] || ![group length] || [group containsString:@"*"])) {
        *status=errSecParam;group=nil;
    }
    if (created) *cleanup=api.remove((__bridge CFDictionaryRef)identity);
    return *status==errSecSuccess ? [group copy] : nil;
}
