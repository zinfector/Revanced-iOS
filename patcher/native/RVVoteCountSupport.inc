// SPDX-License-Identifier: MIT
// Shared response and sizing contracts for owned vote-count text.
#include <CoreFoundation/CoreFoundation.h>
static NSNumber *RVVoteCountNumber(id value) {
    if (![value isKindOfClass:NSNumber.class] || CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID()) return nil;
    double number=[value doubleValue];
    return isfinite(number) && number>=0 && number<=1e15 && floor(number)==number ? value : nil;
}
static NSDictionary *RVVoteCountResponse(id json,NSString *video) {
    if (![json isKindOfClass:NSDictionary.class] || ![video isKindOfClass:NSString.class] ||
        ![json[@"id"] isKindOfClass:NSString.class] || ![json[@"id"] isEqual:video]) return nil;
    id deleted=json[@"deleted"];
    if (deleted && (![deleted isKindOfClass:NSNumber.class] || [deleted boolValue])) return nil;
    NSNumber *down=RVVoteCountNumber(json[@"dislikes"]),*up=RVVoteCountNumber(json[@"likes"]);
    if (!down) return nil;
    return up ? @{@"dislikes":down,@"likes":up} : @{@"dislikes":down};
}
typedef struct { NSInteger unit; double value; } RVVoteTextDimension;
static BOOL RVVoteFullTextFits(double width,double measured) {
    return isfinite(width) && isfinite(measured) && width>0 && measured>0 && width+.5>=measured;
}
static BOOL RVVoteReserveTextSize(id style,double width,double height) {
    if (!isfinite(width) || !isfinite(height) || width<=0 || height<=0 || width>512 || height>256 ||
        !RVCan(style,"setWidth:",@"v@:{?=qd}") || !RVCan(style,"setMinWidth:",@"v@:{?=qd}") ||
        !RVCan(style,"setHeight:",@"v@:{?=qd}")) return NO;
    // These are the verified AS dimension setters. Only our text node is sized.
    RVVoteTextDimension w={1,width},h={1,height};
    ((void(*)(id,SEL,RVVoteTextDimension))objc_msgSend)(style,sel_registerName("setWidth:"),w);
    ((void(*)(id,SEL,RVVoteTextDimension))objc_msgSend)(style,sel_registerName("setMinWidth:"),w);
    ((void(*)(id,SEL,RVVoteTextDimension))objc_msgSend)(style,sel_registerName("setHeight:"),h);
    return YES;
}
