#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdio.h>
#include "../native/RVDebugOnly.h"
#include "../native/RVFeatures.h"
int main(int argc,const char **argv) {
    @autoreleasepool {
        assert(argc==2 && RVDebuggerOnly());
        NSData *data=[NSData dataWithContentsOfFile:@(argv[1])];
        NSDictionary *catalog=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        assert(catalog);
        NSMutableSet *allowed=[NSMutableSet set];
        for (NSDictionary *entry in catalog[@"contracts"]) {
            if (RVDebugObserverContract(entry[@"class_name"],entry[@"selector"],entry[@"abi"],[entry[@"kind"] isEqual:@"+"])) [allowed addObject:entry[@"id"]];
        }
        assert(allowed.count==8);
        for (NSString *key in RVFeatureKeys) {
            BOOL login=[@[@"sideload_auth_identity",@"sideload_auth_keychain"] containsObject:key];
            assert([RVDebugOnlySetting(key,@{key:@YES},RVFeatureKeys) boolValue]==login);
            assert([RVDebugOnlySetting(key,@{key:@NO},RVFeatureKeys) boolValue]==login);
            assert([RVDebugOnlySetting(key,@{},RVFeatureKeys) boolValue]==login);
        }
        assert([RVDebugOnlySetting(@"diagnostics",@{@"diagnostics":@NO},RVFeatureKeys) boolValue]);
        assert(![RVDebugOnlySetting(@"dearrow_format_custom_titles",@{@"dearrow_format_custom_titles":@YES},RVFeatureKeys) boolValue]);
        assert([RVDebugOnlySetting(@"default_speed",@{@"default_speed":@1.0},RVFeatureKeys) doubleValue]==1.0);
        assert(!RVDebugObserverContract(@"YTPlayerViewController",@"playbackController:didLoadContentPlaybackData:",@"v@:@",NO));
        assert(!RVDebugObserverContract(@"YTWatchViewController",@"viewDidAppear:",@"v@:B",YES));
        assert(!RVDebugObserverContract(@"YTRollingNumberView",@"setUpdatedCount:updatedCountNumber:font:fontAttributes:color:skipAnimation:",@"v@:@@@@@B",NO));
        printf("Debugger/login isolation checks passed: %lu feature keys, %lu admitted observer/login contracts\n",(unsigned long)[RVFeatureKeys count],(unsigned long)allowed.count);
    }
    return 0;
}
