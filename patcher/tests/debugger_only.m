#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdio.h>
#include "../native/RVDebugOnly.h"
#include "../native/RVFeatures.h"
int main(int argc,const char **argv) {
    @autoreleasepool {
        assert(argc==3 && RVDebuggerOnly());
        NSDictionary *catalog=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:@(argv[1])] options:0 error:nil];
        NSDictionary *profile=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:@(argv[2])] options:0 error:nil];
        assert(catalog && profile);
        NSMutableSet *supported=[NSMutableSet setWithArray:profile[@"default_enabled"]];[supported addObjectsFromArray:profile[@"optional_features"]];
        NSSet *forced=[NSSet setWithArray:profile[@"forced_features"]];
        assert([supported isEqual:[NSSet setWithArray:RV_BUNDLE_SUPPORTED]]);
        assert([forced isEqual:[NSSet setWithArray:RV_BUNDLE_FORCED]]);
        NSMutableSet *admitted=[NSMutableSet set],*expected=[NSMutableSet set];
        for (NSDictionary *entry in catalog[@"contracts"]) {
            BOOL should=[entry[@"features"] containsObject:@"core"];
            for (NSString *key in entry[@"features"]) should |= [supported containsObject:key];
            if (should) [expected addObject:entry[@"id"]];
            BOOL accepts=RVDebugObserverContract(entry[@"class_name"],entry[@"selector"],entry[@"abi"],[entry[@"kind"] isEqual:@"+"]);
            assert(accepts==should);if (accepts) [admitted addObject:entry[@"id"]];
        }
        assert(admitted.count>8 && [admitted isEqual:expected]);
        assert(!RVDebugObserverContract(@"UnknownClass",@"unknown",@"v@:",NO));
        for (NSString *key in RVFeatureKeys) {
            BOOL mandatory=[forced containsObject:key],allows=[supported containsObject:key];
            assert(RVDebugBaselineFeature(key)==allows);
            assert([RVDebugOnlySetting(key,@{key:@YES},RVFeatureKeys,nil) boolValue]==allows);
            assert([RVDebugOnlySetting(key,@{key:@NO},RVFeatureKeys,nil) boolValue]==mandatory);
            assert([RVDebugOnlySetting(key,@{key:@NO},RVFeatureKeys,@YES) boolValue]==allows);
            assert([RVDebugOnlySetting(key,@{key:@YES},RVFeatureKeys,@NO) boolValue]==mandatory);
        }
        assert([RVDebugOnlySetting(@"diagnostics",@{@"diagnostics":@NO},RVFeatureKeys,@NO) boolValue]);
        assert(![RVDebugOnlySetting(@"dearrow_format_custom_titles",@{@"dearrow_format_custom_titles":@YES},RVFeatureKeys,@YES) boolValue]);
        assert([RVDebugOnlySetting(@"default_speed",@{@"default_speed":@1.0},RVFeatureKeys,@1.5) doubleValue]==1.5);
        printf("Major tweak isolation checks passed: %lu supported features, %lu admitted contracts\n",(unsigned long)supported.count,(unsigned long)admitted.count);
    }
    return 0;
}
