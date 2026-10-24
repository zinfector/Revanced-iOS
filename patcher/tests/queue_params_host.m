// Compile with Foundation and -fobjc-arc -Wall -Wextra -Werror.
#import <Foundation/Foundation.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "../native/RVQueueParams.inc"

static NSData *RVFixture(NSUInteger operation) {
    // Synthetic ID; this fixture preserves the wire shape captured on device.
    uint8_t bytes[]={0x1a,0x15,0x08,(uint8_t)operation,0x12,0x11,0x0a,0x0f,
        0x0a,0x0b,'A','b','C','d','E','f','G','h','_','-','1',0x18,0x01};
    return [NSData dataWithBytes:bytes length:sizeof(bytes)];
}
static NSString *RVEncode(NSData *data) { return [data base64EncodedStringWithOptions:0]; }
static void RVReject(NSData *data) { assert(RVQueueDiscoveryParams(RVEncode(data))==nil); }
static NSMutableData *RVMutate(NSData *data,NSUInteger index,uint8_t byte) {
    NSMutableData *copy=[data mutableCopy];((uint8_t *)copy.mutableBytes)[index]=byte;return copy;
}
int main(void) {
    @autoreleasepool {
        NSData *next=RVFixture(2),*last=RVFixture(3);
        NSDictionary *result=RVQueueDiscoveryParams(RVEncode(next));
        assert([result[@"video_id"] isEqualToString:@"AbCdEfGh_-1"] && [result[@"play_next"] boolValue]);
        assert(![RVQueueDiscoveryParams(RVEncode(last))[@"play_next"] boolValue]);
        NSString *unpadded=[RVEncode(next) stringByReplacingOccurrencesOfString:@"=" withString:@""];
        assert(RVQueueDiscoveryParams(unpadded));
        NSString *escaped=[RVEncode(last) stringByReplacingOccurrencesOfString:@"=" withString:@"%3D"];
        assert(RVQueueDiscoveryParams(escaped));
        NSString *urlsafe=[[RVEncode(next) stringByReplacingOccurrencesOfString:@"+" withString:@"-"]
            stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
        assert(RVQueueDiscoveryParams(urlsafe));
        // Protobuf field order is flexible even though the accepted schema is exact.
        NSMutableData *reordered=[[next subdataWithRange:NSMakeRange(0,2)] mutableCopy];
        [reordered appendData:[next subdataWithRange:NSMakeRange(4,19)]];
        [reordered appendData:[next subdataWithRange:NSMakeRange(2,2)]];
        assert([RVQueueDiscoveryParams(RVEncode(reordered))[@"video_id"] isEqualToString:@"AbCdEfGh_-1"]);
        // Every truncation, including a length-delimited child ending early.
        for (NSUInteger length=0;length<next.length;length++) RVReject([next subdataWithRange:NSMakeRange(0,length)]);
        for (NSUInteger operation=0;operation<8;operation++) if (operation!=2 && operation!=3) RVReject(RVFixture(operation));
        RVReject(RVMutate(next,0,0x12)); // A different discovery family.
        RVReject(RVMutate(next,1,0x7f)); // Invalid outer length.
        RVReject(RVMutate(next,2,0x0a)); // Operation encoded with wrong wire type.
        RVReject(RVMutate(next,4,0x1a)); // Wrong payload path.
        RVReject(RVMutate(next,9,0x0a)); // Wrong video length.
        RVReject(RVMutate(next,10,'!')); // Invalid ID character.
        RVReject(RVMutate(next,22,2)); // Unobserved field-3 variant.
        NSMutableData *trailing=[next mutableCopy];uint8_t extra[]={0x08,2};[trailing appendBytes:extra length:sizeof(extra)];RVReject(trailing);
        NSMutableData *duplicate=[next mutableCopy];((uint8_t *)duplicate.mutableBytes)[1]+=2;
        uint8_t conflicting[]={0x08,3};[duplicate appendBytes:conflicting length:sizeof(conflicting)];RVReject(duplicate);
        NSMutableData *duplicateID=[next mutableCopy];uint8_t idfield[]={0x0a,0x0b,'X','b','C','d','E','f','G','h','_','-','1'};
        [duplicateID appendBytes:idfield length:sizeof(idfield)];
        uint8_t *duplicateBytes=duplicateID.mutableBytes;duplicateBytes[1]+=13;duplicateBytes[5]+=13;duplicateBytes[7]+=13;RVReject(duplicateID);
        NSMutableData *overflow=[NSMutableData dataWithBytes:(uint8_t[]){0x1a,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x02} length:11];RVReject(overflow);
        NSMutableData *overlong=[next mutableCopy];uint8_t expanded[]={0x95,0};
        [overlong replaceBytesInRange:NSMakeRange(1,1) withBytes:expanded length:2];RVReject(overlong);
        assert(!RVQueueDiscoveryParams(@"%%%"));assert(!RVQueueDiscoveryParams(@"A"));
        assert(!RVQueueDiscoveryParams(@"AA==\n"));assert(!RVQueueDiscoveryParams(@"AB=="));
        assert(!RVQueueDiscoveryParams(@"AA=A"));assert(!RVQueueDiscoveryParams(@"AAAA===="));
        assert(!RVQueueDiscoveryParams([@"A" stringByPaddingToLength:16385 withString:@"A" startingAtIndex:0]));
        // A familiar ID byte pattern in unrelated bytes cannot be scanned out.
        NSMutableData *unrelated=[[@"prefix" dataUsingEncoding:NSASCIIStringEncoding] mutableCopy];
        [unrelated appendData:next];RVReject(unrelated);
        puts("{\"ok\":true,\"suite\":\"queue-discovery-params\",\"test_groups\":5,\"device_validation\":false}");
    }
    return 0;
}
