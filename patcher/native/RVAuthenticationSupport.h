// SPDX-License-Identifier: MIT
// Shared by the iOS adapter and the macOS authentication regression harness.
#pragma once
#import <Foundation/Foundation.h>
#import <Security/Security.h>

static NSString *const RVAuthOriginalBundle = @"com.google.ios.youtube";
static NSString *const RVAuthClientID = @"755541669657-kbosfavg7pk7sr3849c3tf657hpi5jpd.apps.googleusercontent.com";
static NSString *const RVAuthScheme = @"com.google.sso.755541669657-kbosfavg7pk7sr3849c3tf657hpi5jpd";

static NSString *const RVAuthLibraryClientID = @"936475272427.apps.googleusercontent.com";
static NSString *const RVAuthTestLibraryClientID = @"77906438180.apps.googleusercontent.com";

static BOOL RVAuthProductionURL(id value) {
    if (![value isKindOfClass:NSURL.class]) return NO;
    NSURLComponents *parts=[NSURLComponents componentsWithURL:value resolvingAgainstBaseURL:NO];
    return [parts.scheme.lowercaseString isEqual:@"https"] && !parts.user && !parts.password
        && (!parts.port || parts.port.integerValue==443)
        && [@[@"accounts.google.com",@"oauth2.googleapis.com",@"oauthaccountmanager.googleapis.com"] containsObject:parts.host.lowercaseString ?: @""];
}
static id RVAuthUserAgentIdentity(id value,id installedID) {
    // The native GTM formatter places the bundle ID before /version. Replace
    // only that exact token; preserve the native library/platform UA suffix.
    if (![value isKindOfClass:NSString.class] || ![installedID isKindOfClass:NSString.class] || ![installedID length]
        || [installedID isEqual:RVAuthOriginalBundle]) return value;
    NSString *prefix=[installedID stringByAppendingString:@"/"];
    if (![value hasPrefix:prefix] || [value length]>4096) return value;
    return [RVAuthOriginalBundle stringByAppendingString:[value substringFromIndex:[installedID length]]];
}

static BOOL RVAuthTupleMatches(id client,id scheme) {
    return [client isKindOfClass:NSString.class] && [scheme isKindOfClass:NSString.class]
        && [client isEqual:RVAuthClientID] && [scheme isEqual:RVAuthScheme];
}
static NSDictionary *RVAuthURLSummary(id value) {
    if (![value isKindOfClass:NSURL.class] || [(NSURL *)value absoluteString].length>16384)
        return @{@"url_present":@NO};
    NSURLComponents *parts=[NSURLComponents componentsWithURL:value resolvingAgainstBaseURL:NO];
    if (!parts) return @{@"url_present":@NO};
    NSString *scheme=parts.scheme.lowercaseString ?: @"";
    BOOL google=RVAuthProductionURL(value);
    BOOL callback=[scheme isEqual:RVAuthScheme] && [parts.path isEqual:@"/authCallback"] && !parts.host;
    NSString *pathClass=callback ? @"callback" : !google ? @"other" :
        [parts.path isEqual:@"/v1/authadvice"] ? @"auth_advice" : [parts.path hasPrefix:@"/o/oauth2/"] ? @"oauth" : [parts.path hasPrefix:@"/signin/"] ? @"signin" : @"other_google";
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
    summary[@"challenge_method"]=@"absent";summary[@"oauth_error"]=@"absent";summary[@"client_kind"]=@"absent";
    NSSet *errors=[NSSet setWithArray:@[@"disallowed_useragent",@"invalid_request",@"redirect_uri_mismatch",
        @"access_denied",@"invalid_client",@"unauthorized_client",@"deleted_client",@"org_internal",
        @"admin_policy_enforced",@"invalid_grant"]];
    NSMutableSet *seen=[NSMutableSet set];
    for (NSURLQueryItem *item in parts.queryItems) {
        NSString *key=item.name;
        if (presence[key]) summary[presence[key]]=@YES;
        // Duplicate identity fields are ambiguous; never mark them as matching.
        BOOL duplicate=[seen containsObject:key];[seen addObject:key];
        if ([key isEqual:@"client_id"]) {
            summary[@"client_matches"]=@(!duplicate && [item.value isEqual:RVAuthClientID]);
            summary[@"client_kind"]=duplicate ? @"ambiguous" : [item.value isEqual:RVAuthClientID] ? @"youtube" :
                [item.value isEqual:RVAuthLibraryClientID] ? @"sso_library" : [item.value isEqual:RVAuthTestLibraryClientID] ? @"sso_test_library" : @"other";
        }
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

static NSDictionary *RVAuthRequestSummary(id value) {
    if (![value isKindOfClass:NSURLRequest.class]) return @{@"request_present":@NO};
    NSURLRequest *request=value;NSMutableDictionary *summary=[RVAuthURLSummary(request.URL) mutableCopy];
    summary[@"request_present"]=@YES;
    NSURLComponents *parts=[NSURLComponents componentsWithURL:request.URL resolvingAgainstBaseURL:NO];
    // Never inspect or export account lists or credential fields. Only the
    // known auth-advice JSON envelope produces these fixed boolean checks.
    if (!RVAuthProductionURL(request.URL) || ![parts.host.lowercaseString isEqual:@"oauthaccountmanager.googleapis.com"]
        || ![parts.path isEqual:@"/v1/authadvice"]) return summary;
    NSData *body=request.HTTPBody;
    if (!body.length || body.length>256*1024) { summary[@"body_class"]=request.HTTPBodyStream ? @"streamed" : body.length ? @"oversized" : @"absent";return summary; }
    id object=[NSJSONSerialization JSONObjectWithData:body options:0 error:nil];
    if (![object isKindOfClass:NSDictionary.class]) { summary[@"body_class"]=@"unparsed";return summary; }
    summary[@"body_class"]=@"json_object";
    summary[@"body_client_matches"]=@([object[@"client_id"] isEqual:RVAuthClientID]);
    summary[@"body_mediator_matches"]=@([object[@"mediator_client_id"] isEqual:RVAuthLibraryClientID]);
    summary[@"body_package_matches"]=@([object[@"package_name"] isEqual:RVAuthOriginalBundle]);
    NSURLComponents *redirect=[object[@"redirect_uri"] isKindOfClass:NSString.class] ? [NSURLComponents componentsWithString:object[@"redirect_uri"]] : nil;
    summary[@"body_redirect_scheme_matches"]=@([redirect.scheme isEqual:RVAuthScheme]);
    summary[@"device_challenge_present"]=@(object[@"device_challenge_request"]!=nil);
    summary[@"client_state_present"]=@(object[@"client_state"]!=nil);
    return summary;
}
