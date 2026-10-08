// SPDX-License-Identifier: MIT
// Shared bounded protobuf fields for feed classification and presentation copies.
@interface RVVoteField : NSObject
@property(nonatomic) uint32_t number;
@property(nonatomic) unsigned int wire;
@property(nonatomic) uint64_t scalar;
@property(nonatomic,strong) NSData *value;
@property(nonatomic,strong) NSData *raw;
@end
@implementation RVVoteField @end
static BOOL RVVoteReadVarint(const uint8_t *bytes,NSUInteger length,NSUInteger *offset,uint64_t *value) {
    uint64_t result=0;
    for (unsigned int i=0;i<10;i++) {
        if (*offset>=length) return NO;uint8_t next=bytes[(*offset)++];
        if (i==9 && (next&0xfe)) return NO;
        result|=(uint64_t)(next&0x7f)<<(7*i);
        if (!(next&0x80)) { *value=result;return YES; }
    }return NO;
}
static NSArray<RVVoteField *> *RVVoteFields(NSData *data) {
    if (![data isKindOfClass:NSData.class] || data.length>2*1024*1024) return nil;
    const uint8_t *bytes=data.bytes;NSUInteger offset=0;
    NSMutableArray *fields=[NSMutableArray array];
    while (offset<data.length) {
        if (fields.count>=4096) return nil;
        NSUInteger start=offset;uint64_t tag=0,length=0;
        if (!RVVoteReadVarint(bytes,data.length,&offset,&tag) || !(tag>>3) || (tag>>3)>0x1fffffff) return nil;
        RVVoteField *field=[RVVoteField new];field.number=(uint32_t)(tag>>3);field.wire=(unsigned int)(tag&7);
        if (field.wire==0) { uint64_t scalar=0;if (!RVVoteReadVarint(bytes,data.length,&offset,&scalar)) return nil;field.scalar=scalar; }
        else if (field.wire==2) {
            if (!RVVoteReadVarint(bytes,data.length,&offset,&length) || length>data.length-offset) return nil;
            field.value=[data subdataWithRange:NSMakeRange(offset,(NSUInteger)length)];offset+=(NSUInteger)length;
        } else if (field.wire==1 || field.wire==5) {
            NSUInteger fixed=field.wire==1 ? 8 : 4;if (fixed>data.length-offset) return nil;offset+=fixed;
        } else return nil; // Group fields are not rewritten by this adapter.
        field.raw=[data subdataWithRange:NSMakeRange(start,offset-start)];[fields addObject:field];
    }return fields;
}
static void RVVoteWriteVarint(NSMutableData *data,uint64_t value) {
    do { uint8_t byte=(uint8_t)(value&0x7f);value>>=7;if (value) byte|=0x80;[data appendBytes:&byte length:1]; } while (value);
}
static NSData *RVVoteMessageField(uint32_t number,NSData *value) {
    NSMutableData *data=[NSMutableData data];RVVoteWriteVarint(data,((uint64_t)number<<3)|2);
    RVVoteWriteVarint(data,value.length);[data appendData:value];return data;
}
static NSData *RVVoteScalarField(uint32_t number,uint64_t value) {
    NSMutableData *data=[NSMutableData data];RVVoteWriteVarint(data,(uint64_t)number<<3);RVVoteWriteVarint(data,value);return data;
}
static RVVoteField *RVVoteOne(NSArray<RVVoteField *> *fields,uint32_t number,unsigned int wire) {
    RVVoteField *found=nil;
    for (RVVoteField *field in fields) if (field.number==number) { if (found || field.wire!=wire) return nil;found=field; }
    return found;
}
static BOOL RVVoteSingularFields(NSArray<RVVoteField *> *fields,NSArray<NSNumber *> *numbers) {
    if (!fields) return NO;
    NSMutableSet *seen=[NSMutableSet set];
    for (RVVoteField *field in fields) {
        if ([numbers containsObject:@(field.number)]) {
            if ([seen containsObject:@(field.number)]) return NO;[seen addObject:@(field.number)];
        }
    }
    return YES;
}
