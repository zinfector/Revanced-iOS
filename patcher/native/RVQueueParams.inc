// SPDX-License-Identifier: MIT
// Strict parser for the queue-shaped MDX discovery token observed on YouTube
// 21.39.4. Wire schema, established from the affected device:
//   3 { 1: operation, 2 { 1 { 1: video_id, 3: 1 } } }
// Operations 2/3 mean next/last. Other discovery/cast token shapes are rejected.
// The token is private input; neither it nor its video ID is logged here.
typedef struct { const uint8_t *bytes; NSUInteger length; } RVQueueParamSpan;

static BOOL RVQueueParamVarint(RVQueueParamSpan span,NSUInteger *cursor,uint64_t *value) {
    uint64_t result=0;
    for (NSUInteger index=0;index<10;index++) {
        if (*cursor>=span.length) return NO;
        uint8_t byte=span.bytes[(*cursor)++];
        if (index==9 && byte>1) return NO;
        result|=(uint64_t)(byte&0x7f)<<(index*7);
        if (!(byte&0x80)) {
            if (index && !byte) return NO; // Reject noncanonical/overlong encodings.
            *value=result;return YES;
        }
    }
    return NO;
}

static BOOL RVQueueParamMessage(RVQueueParamSpan span,NSUInteger stage,
                               NSUInteger *budget,uint64_t *operation,
                               RVQueueParamSpan *video) {
    if (stage>3 || !span.length || span.length>4096) return NO;
    NSUInteger cursor=0,seen=0;RVQueueParamSpan child={NULL,0};
    while (cursor<span.length) {
        if (!*budget) return NO;
        --*budget;
        uint64_t tag=0,value=0;
        if (!RVQueueParamVarint(span,&cursor,&tag)) return NO;
        uint64_t number=tag>>3;NSUInteger wire=(NSUInteger)(tag&7);
        if (!number || number>3 || (seen&((NSUInteger)1<<number))) return NO;
        seen|=(NSUInteger)1<<number;
        BOOL scalar=(stage==1 && number==1) || (stage==3 && number==3);
        BOOL message=(stage==0 && number==3) || (stage==1 && number==2)
            || (stage==2 && number==1) || (stage==3 && number==1);
        if (scalar) {
            if (wire!=0 || !RVQueueParamVarint(span,&cursor,&value)) return NO;
            if (stage==1) {
                if (value!=2 && value!=3) return NO;
                *operation=value;
            } else if (value!=1) return NO;
        } else if (message) {
            if (wire!=2 || !RVQueueParamVarint(span,&cursor,&value)
                || value>span.length-cursor) return NO;
            RVQueueParamSpan contents={span.bytes+cursor,(NSUInteger)value};
            cursor+=(NSUInteger)value;
            if (stage==3) {
                if (contents.length!=11) return NO;
                for (NSUInteger index=0;index<contents.length;index++) {
                    uint8_t byte=contents.bytes[index];
                    if (!((byte>='A'&&byte<='Z') || (byte>='a'&&byte<='z')
                        || (byte>='0'&&byte<='9') || byte=='_' || byte=='-')) return NO;
                }
                *video=contents;
            } else child=contents;
        } else return NO;
    }
    const NSUInteger required[]={8,6,2,10};
    if (seen!=required[stage]) return NO;
    return stage==3 || RVQueueParamMessage(child,stage+1,budget,operation,video);
}

static NSDictionary *RVQueueDiscoveryParams(NSString *encoded) {
    if (![encoded isKindOfClass:NSString.class] || !encoded.length || encoded.length>16384) return nil;
    NSString *normal=[encoded stringByRemovingPercentEncoding];
    if (!normal.length || normal.length>8192) return nil;
    normal=[[normal stringByReplacingOccurrencesOfString:@"-" withString:@"+"]
        stringByReplacingOccurrencesOfString:@"_" withString:@"/"];
    NSCharacterSet *invalid=[[NSCharacterSet characterSetWithCharactersInString:
        @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="] invertedSet];
    if ([normal rangeOfCharacterFromSet:invalid].location!=NSNotFound) return nil;
    NSRange padding=[normal rangeOfString:@"="];
    if (padding.location!=NSNotFound) {
        NSUInteger count=normal.length-padding.location;
        if (count>2 || normal.length%4 || ![[normal substringFromIndex:padding.location]
            isEqualToString:(count==1?@"=":@"==")]) return nil;
    } else {
        NSUInteger remainder=normal.length%4;
        if (remainder==1) return nil;
        if (remainder) normal=[normal stringByPaddingToLength:normal.length+4-remainder
            withString:@"=" startingAtIndex:0];
    }
    NSData *data=[[NSData alloc] initWithBase64EncodedString:normal options:0];
    if (!data.length || data.length>4096 || ![[data base64EncodedStringWithOptions:0] isEqualToString:normal]) return nil;
    RVQueueParamSpan root={data.bytes,data.length},video={NULL,0};
    NSUInteger budget=64;uint64_t operation=0;
    if (!RVQueueParamMessage(root,0,&budget,&operation,&video)) return nil;
    NSString *videoID=[[NSString alloc] initWithBytes:video.bytes length:video.length encoding:NSASCIIStringEncoding];
    return videoID ? @{@"video_id":videoID,@"play_next":@(operation==2)} : nil;
}
