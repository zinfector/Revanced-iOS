// SPDX-License-Identifier: MIT
// Embedded source logo; no filesystem/bundle lookup is needed after injection.
#include "../build/RVSettingsIconData.h"

// Shared with the supplied service logos: crop transparent padding, preserve color,
// and fit the original aspect ratio into YouTube's 24-point settings icon slot.
static UIImage *RVSettingsSizedIconImage(UIImage *source) {
    CGImageRef image=source.CGImage;if (!image) return nil;
    size_t width=CGImageGetWidth(image),height=CGImageGetHeight(image);
    if (!width || !height || width>4096 || height>4096) return nil;
    NSMutableData *pixels=[NSMutableData dataWithLength:width*height*4];
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGContextRef context=CGBitmapContextCreate(pixels.mutableBytes,width,height,8,width*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);if (!context) return nil;
    CGContextDrawImage(context,CGRectMake(0,0,width,height),image);
    CGImageRef raster=CGBitmapContextCreateImage(context);CGContextRelease(context);if (!raster) return nil;
    const unsigned char *bytes=pixels.bytes;size_t left=width,top=height,right=0,bottom=0;
    for (size_t y=0;y<height;y++) for (size_t x=0;x<width;x++) if (bytes[(y*width+x)*4+3]>16) {
        left=MIN(left,x);right=MAX(right,x);top=MIN(top,y);bottom=MAX(bottom,y);
    }
    if (left>right || top>bottom) { CGImageRelease(raster);return nil; }
    // Crop the exact raster inspected above, including its row orientation.
    CGImageRef cropped=CGImageCreateWithImageInRect(raster,CGRectMake(left,top,right-left+1,bottom-top+1));CGImageRelease(raster);
    if (!cropped) return nil;UIImage *glyph=[UIImage imageWithCGImage:cropped];CGImageRelease(cropped);
    CGFloat scale=MIN(24/glyph.size.width,24/glyph.size.height);CGSize size=CGSizeMake(glyph.size.width*scale,glyph.size.height*scale);
    UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(24,24)];
    return [[renderer imageWithActions:^(UIGraphicsImageRendererContext *unused) {
        [glyph drawInRect:CGRectMake((24-size.width)/2,(24-size.height)/2,size.width,size.height)];
    }] imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
}
static UIImage *RVSettingsLogoImage(void) {
    static UIImage *logo;static dispatch_once_t once;
    dispatch_once(&once,^{
        UIImage *source=[UIImage imageWithData:[[NSData alloc] initWithBase64EncodedString:RVSettingsIconBase64 options:0]];
        logo=RVSettingsSizedIconImage(source);
    });return logo;
}

// YTSettingsCell accepts the icon protocol without requiring a YTIIcon instance.
@interface RVSettingsLogoIcon : NSObject
- (UIImage *)iconImageWithColor:(UIColor *)color;
- (BOOL)hasIconType;
- (int)iconType;
@end
@implementation RVSettingsLogoIcon
- (UIImage *)iconImageWithColor:(UIColor *)color { return RVSettingsLogoImage(); }
- (BOOL)hasIconType { return YES; }
// In 21.39.4, the native multicolor predicate explicitly accepts 0x415.
// This keeps updateColors on iconImageWithColor: instead of forcing a template.
- (int)iconType { return 0x415; }
@end
static id RVSettingsEntryIcon(void) {
    static RVSettingsLogoIcon *icon;static dispatch_once_t once;
    dispatch_once(&once,^{ if (RVSettingsLogoImage()) icon=[RVSettingsLogoIcon new]; });return icon;
}
