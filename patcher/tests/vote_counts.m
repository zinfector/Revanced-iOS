// Executes the production response/sizing helpers with native Objective-C calls.
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include "../native/RVABI.h"
static BOOL RVCan(id object,const char *name,NSString *signature) {
    char normalized[4097];Method method=class_getInstanceMethod(object_getClass(object),sel_registerName(name));
    return method && RVABINormalize(method_getTypeEncoding(method),normalized,sizeof(normalized)) && [@(normalized) isEqual:signature];
}
#include "../native/RVVoteCountSupport.inc"
@interface VoteStyle : NSObject
@property(nonatomic) RVVoteTextDimension width,minWidth,height;
@property(nonatomic) NSUInteger writes;
@end
@implementation VoteStyle
- (void)setWidth:(RVVoteTextDimension)value { _width=value;_writes++; }
- (void)setMinWidth:(RVVoteTextDimension)value { _minWidth=value;_writes++; }
- (void)setHeight:(RVVoteTextDimension)value { _height=value;_writes++; }
@end
@interface WrongStyle : NSObject
- (void)setWidth:(double)value;
@end
@implementation WrongStyle
- (void)setWidth:(double)value { (void)value;assert(0); }
@end
int main(void) {
    @autoreleasepool {
        NSString *video=@"fr0WIIZxvx0";
        NSData *data=[@"{\"id\":\"fr0WIIZxvx0\",\"likes\":14897,\"dislikes\":265,\"deleted\":false}" dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary *json=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        NSDictionary *counts=RVVoteCountResponse(json,video);
        assert([counts[@"dislikes"] isEqual:@265] && [counts[@"likes"] isEqual:@14897]);
        assert(!RVVoteCountResponse(json,@"other_video"));
        assert(!RVVoteCountResponse(@[],video));
        assert(!RVVoteCountResponse(@{@"id":video,@"dislikes":@YES},video));
        assert(!RVVoteCountResponse(@{@"id":video,@"dislikes":@265,@"deleted":NSNull.null},video));
        assert(!RVVoteCountResponse(@{@"id":video,@"dislikes":@265,@"deleted":@YES},video));
        assert(!RVVoteCountResponse(@{@"id":video,@"dislikes":@(-1)},video));
        assert(!RVVoteCountNumber(@1.5) && !RVVoteCountNumber(@(NAN)) && !RVVoteCountNumber(@(INFINITY)));
        assert(!RVVoteCountNumber(@1e16) && !RVVoteCountNumber(@"265") && !RVVoteCountNumber(NSNull.null));
        counts=RVVoteCountResponse(@{@"id":video,@"dislikes":@0,@"likes":@NO},video);
        assert([counts[@"dislikes"] isEqual:@0] && !counts[@"likes"]);
        VoteStyle *style=[VoteStyle new];
        // Regression: a nine-point cell cannot represent a full 27-point count.
        assert(!RVVoteFullTextFits(9,27) && RVVoteFullTextFits(28,27));
        assert(!RVVoteFullTextFits(NAN,27) && !RVVoteFullTextFits(9,0));
        assert(RVVoteReserveTextSize(style,28,17));
        assert(style.width.unit==1 && style.width.value==28 && style.minWidth.value==28 && style.height.value==17);
        assert(RVVoteReserveTextSize(style,9,17)); // A later one-digit count may shrink.
        assert(style.width.value==9 && style.minWidth.value==9 && style.writes==6);
        assert(!RVVoteReserveTextSize(style,-1,17) && !RVVoteReserveTextSize(style,513,17));
        assert(!RVVoteReserveTextSize(style,28,NAN) && style.writes==6);
        assert(!RVVoteReserveTextSize([WrongStyle new],28,17));
        puts("Vote count response, full-text sizing and ABI checks passed");
    }return 0;
}
