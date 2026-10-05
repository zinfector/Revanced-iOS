// SPDX-License-Identifier: MIT
// Local presentation policy. Always start from the pristine owned source.
#import <NaturalLanguage/NaturalLanguage.h>
#import <CoreImage/CoreImage.h>
static NSString *RVPlainText(id value) {
    if (!value) return nil;
    NSString *simple=RVAdResolvedObject(value,"simpleText");
    if ([simple isKindOfClass:NSString.class] && simple.length<=1024) return simple;
    id runs=RVAdResolvedObject(value,"runsArray");if (![runs isKindOfClass:NSArray.class] || [runs count]>128) return nil;
    NSMutableString *text=[NSMutableString string];
    for (id run in runs) {
        NSString *part=RVAdResolvedObject(run,"text");if (![part isKindOfClass:NSString.class] || part.length>1024-text.length) return nil;[text appendString:part];
    }return text.length ? text : nil;
}
static NSString *RVDAFormatTitle(NSString *source,BOOL custom) {
    if (!source.length || source.length>1024) return source;
    NSString *text=source;
    if (RVEnabled(@"dearrow_remove_emojis")) {
        NSMutableString *clean=[NSMutableString string];
        [text enumerateSubstringsInRange:NSMakeRange(0,text.length) options:NSStringEnumerationByComposedCharacterSequences usingBlock:^(NSString *part,NSRange range,NSRange enclosing,BOOL *stop) {
            NSData *bytes=[part dataUsingEncoding:NSUTF32LittleEndianStringEncoding];uint32_t scalar=0;
            if (bytes.length>=4) memcpy(&scalar,bytes.bytes,4);
            NSDictionary *letters=@{@0x1f170:@"A",@0x1f171:@"B",@0x1f17e:@"O",@0x1f17f:@"P",@0x1f18e:@"AB"};
            if (letters[@(scalar)]) { [clean appendString:letters[@(scalar)]];return; }
            BOOL emoji=(scalar>=0x1f000 && scalar<=0x1faff) || [part rangeOfString:@"\ufe0f"].location!=NSNotFound || [part rangeOfString:@"\u20e3"].location!=NSNotFound;
            // Keep ordinary mathematical symbols and text-presentation glyphs.
            if (!emoji) [clean appendString:part];
        }];
        text=[clean precomposedStringWithCompatibilityMapping];
        text=[[text componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] componentsJoinedByString:@" "];
        NSRegularExpression *spaces=[NSRegularExpression regularExpressionWithPattern:@" +" options:0 error:nil];
        text=[spaces stringByReplacingMatchesInString:text options:0 range:NSMakeRange(0,text.length) withTemplate:@" "];
        text=[text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        if (!text.length) text=source; // Never remove an emoji-only title entirely.
    }
    NSString *mode=RVSetting(@"dearrow_title_format") ?: @"disabled";
    if ([mode isEqual:@"disabled"] || !RVEnabled(custom ? @"dearrow_format_custom_titles" : @"dearrow_format_original_titles")) return text;
    if (!RVEnabled(@"dearrow_capitalize_non_english") && ![mode isEqual:@"lower_case"]) {
        NLLanguageRecognizer *recognizer=[NLLanguageRecognizer new];[recognizer processString:source];
        NSDictionary *languages=[recognizer languageHypothesesWithMaximum:1];
        if (![recognizer.dominantLanguage isEqual:NLLanguageEnglish] || [languages[NLLanguageEnglish] doubleValue]<.65) return text;
    }
    NSLocale *locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    if ([mode isEqual:@"lower_case"]) return [text lowercaseStringWithLocale:locale];
    NSRegularExpression *words=[NSRegularExpression regularExpressionWithPattern:@"[\\p{L}\\p{N}][\\p{L}\\p{M}\\p{N}'’._-]*" options:0 error:nil];
    NSArray<NSTextCheckingResult *> *matches=[words matchesInString:text options:0 range:NSMakeRange(0,text.length)];
    NSSet *minor=[NSSet setWithArray:@[@"a",@"an",@"the",@"and",@"as",@"at",@"but",@"by",@"for",@"from",@"in",@"nor",@"of",@"on",@"or",@"per",@"to",@"up",@"via",@"vs",@"with"]];
    NSSet *acronyms=[NSSet setWithArray:@[@"AI",@"API",@"CPU",@"GPU",@"VR",@"AR",@"USA",@"UK",@"NASA",@"HD",@"UHD",@"FPS",@"PS5",@"PC",@"TV",@"USB",@"EU",@"DNA",@"RNA",@"RGB",@"SSD",@"RAM",@"GPT",@"AMD",@"RTX"]];
    NSMutableString *result=[text mutableCopy];
    for (NSInteger i=(NSInteger)matches.count-1;i>=0;i--) {
        NSRange range=matches[i].range;NSString *word=[text substringWithRange:range],*lower=[word lowercaseStringWithLocale:locale],*next=lower;
        BOOL first=i==0,last=(NSUInteger)i==matches.count-1;
        NSString *between=first ? @"" : [text substringWithRange:NSMakeRange(NSMaxRange(matches[i-1].range),range.location-NSMaxRange(matches[i-1].range))];
        BOOL boundary=first || [between rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@":.!?\n"]].location!=NSNotFound;
        BOOL capital=[mode isEqual:@"capitalize_words"] || ([mode isEqual:@"title_case"] && (first || last || boundary || ![minor containsObject:lower])) || ([mode isEqual:@"sentence_case"] && boundary) || ([mode isEqual:@"first_letter_uppercase"] && first);
        if ([mode isEqual:@"first_letter_uppercase"] && !first) next=word;
        BOOL mixed=[word rangeOfCharacterFromSet:NSCharacterSet.lowercaseLetterCharacterSet].location!=NSNotFound && [word rangeOfCharacterFromSet:NSCharacterSet.uppercaseLetterCharacterSet].location!=NSNotFound;
        NSDictionary *names=@{@"youtube":@"YouTube",@"dearrow":@"DeArrow",@"revanced":@"ReVanced",@"iphone":@"iPhone",@"ipad":@"iPad",@"tiktok":@"TikTok",@"javascript":@"JavaScript",@"github":@"GitHub"};
        if (names[lower]) next=names[lower];
        else if ([acronyms containsObject:word] || (mixed && ![[word substringFromIndex:1] isEqual:[[word substringFromIndex:1] lowercaseStringWithLocale:locale]])) next=word;
        else if (capital && next.length) { NSRange letter=[next rangeOfComposedCharacterSequenceAtIndex:0];next=[next stringByReplacingCharactersInRange:letter withString:[[next substringWithRange:letter] uppercaseStringWithLocale:locale]]; }
        [result replaceCharactersInRange:range withString:next];
    }return result;
}
static UIImage *RVDAColorImage(UIImage *image,NSInteger saturation) {
    if (!image || saturation>=100) return image;
    CIImage *source=[[CIImage alloc] initWithImage:image];if (!source) return image;
    CIFilter *filter=[CIFilter filterWithName:@"CIColorControls"];
    [filter setValue:source forKey:kCIInputImageKey];[filter setValue:@(MAX(0,saturation)/100.0) forKey:kCIInputSaturationKey];
    static CIContext *context;static dispatch_once_t once;dispatch_once(&once,^{ context=[CIContext contextWithOptions:@{kCIContextUseSoftwareRenderer:@YES}]; });
    CGImageRef rendered=[context createCGImage:filter.outputImage fromRect:source.extent];
    if (!rendered) return image;UIImage *result=[UIImage imageWithCGImage:rendered scale:image.scale orientation:image.imageOrientation];CGImageRelease(rendered);return result;
}
static UIImage *RVDABlankImage(void) {
    static UIImage *image;static dispatch_once_t once;dispatch_once(&once,^{
        UIGraphicsImageRendererFormat *format=UIGraphicsImageRendererFormat.defaultFormat;format.scale=1;format.opaque=YES;
        image=[[[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(320,180) format:format] imageWithActions:^(UIGraphicsImageRendererContext *context) { [[UIColor colorWithWhite:.1 alpha:1] setFill];[context fillRect:CGRectMake(0,0,320,180)]; }];
    });return image;
}
static UIImage *RVDACoverImage(UIImage *avatar) {
    UIGraphicsImageRendererFormat *format=UIGraphicsImageRendererFormat.defaultFormat;format.scale=1;format.opaque=YES;
    return [[[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(320,180) format:format] imageWithActions:^(UIGraphicsImageRendererContext *context) {
        [[UIColor colorWithWhite:.1 alpha:1] setFill];[context fillRect:CGRectMake(0,0,320,180)];[avatar drawInRect:CGRectMake(100,30,120,120)];
    }];
}
