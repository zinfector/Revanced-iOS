// SPDX-License-Identifier: MIT
// Supplied service logos are embedded, so injection needs no extra bundle assets.
#include "RVSettingsBrandIconData.h"

// Reuse the root logo adapter's verified multicolor contract (iconType 0x415).
// This is a color-preservation marker only; no YTIIcon resolver is called for it.
@interface RVSettingsBrandIcon : RVSettingsLogoIcon
@property(nonatomic,strong) UIImage *image;
@end
@implementation RVSettingsBrandIcon
- (UIImage *)iconImageWithColor:(UIColor *)color { (void)color;return self.image; }
@end

static id RVSettingsBrandIconForMenu(NSString *key) {
    UIImage *image=nil;NSString *encoded=nil;
    if ([key isEqualToString:@"dearrow"]) image=RVSettingsDeArrowBrandImage();
    else if ([key isEqualToString:@"ryd"]) encoded=RVSettingsRYDBrandBase64;
    else if ([key isEqualToString:@"sponsorblock"]) encoded=RVSettingsSponsorBrandBase64;
    else return nil;
    if (encoded) image=RVSettingsSizedIconImage([UIImage imageWithData:[[NSData alloc] initWithBase64EncodedString:encoded options:0]]);
    if (!image) { RVLog([NSString stringWithFormat:@"Supplied settings logo unavailable: %@",key]);return nil; }
    RVSettingsBrandIcon *icon=[RVSettingsBrandIcon new];icon.image=image;return icon;
}
