// SPDX-License-Identifier: MIT
// YouTube 21.39.4: verified YTIIcon enums, YTUIResources factories and catalog names.
// See profiles/settings-menu-icons-evidence.json. Resolver IDs are not iconType values.
#include "RVSettingsBrandIcons.inc"
typedef struct {
    const char *key;
    int iconType; // 0 means no verified enum route; use the native resource instead.
    const char *factory;
    const char *asset;
} RVSettingsMenuGlyph;
static const RVSettingsMenuGlyph RVSettingsMenuGlyphs[]={
    {"ads",0x18f,"slashCircleLeftOutline","yt_outline_slash_circle_left_24pt"},
    {"player",0x445,"playArrowOutlineWithColor:","youtube_outline/play_arrow_24pt"},
    {"video",0x46c,"videoQualityHdOutlineWithColor:","youtube_outline/hd_24pt"},
    {"shorts",0x301,"youtubeShortsOutline24WithColor:","youtube_outline/youtube_shorts_24pt"},
    {"navigation",0x309,"compassOutline","yt_outline_compass_24pt"},
    {"miniplayer",0,NULL,"yt_outline_picture_in_picture_24pt"},
    {"seekbar",0,NULL,"youtube_outline/hand_swipe_left_24pt"},
    {"layout",0,NULL,"youtube_outline/palette_24pt"},
    {"thumbnails",0x16f,NULL,"youtube_outline/image_24pt"},
    {"dearrow",0x23,"pencilOutlineWithColor:","youtube_outline/pencil_24pt"},
    {"links",0x12a,"linkOutlineWithColor:","yt_outline_link_24pt"},
    {"ryd",0x33,"thumbDownOutlineWithColor:","youtube_outline/thumb_down_24pt"},
    {"sponsorblock",0x1cc,"skipNextFill","yt_fill_skip_next_24pt"},
    {"authentication",0,"keyOutline","youtube_outline/key_24pt"},
    {"advanced",0x2c,"gearOutlineWithColor:","yt_outline_gear_24pt"},
    {"tool.0",0,NULL,"youtube_outline/toolbox_24pt"},
    {"tool.1",0,"arrowInOutline","yt_outline_arrow_in_24pt"},
    {"tool.2",0,NULL,"youtube_outline/copy_24pt"},
    {"tool.3",0,"arrowCircleOutline","yt_outline_arrow_circle_24pt"},
    {"tool.4",0x95,"barGraphBoxVerticalOutline","yt_outline_bar_graph_box_vertical_24pt"},
    {"tool.5",0x31,"infoCircleOutlineWithColor:","yt_outline_info_circle_24pt"},
};
static NSString *RVSettingsMenuIconKey(NSDictionary *row) {
    // Only root menu actions have group/tool keys; search and preference rows stay unadorned.
    if (row[@"group"]) return row[@"group"];
    if (row[@"tool"]) return [NSString stringWithFormat:@"tool.%@",row[@"tool"]];
    return nil;
}
static UIImage *RVSettingsMenuIconImage(id icon,UIColor *color) {
    if (!RVCan(icon,"iconImageWithColor:",@"@@:@")) return nil;
    id image=((id(*)(id,SEL,id))objc_msgSend)(icon,sel_registerName("iconImageWithColor:"),color);
    return [image isKindOfClass:UIImage.class] && [image size].width>0 && [image size].height>0 ? image : nil;
}

// The same protocol as YTIIcon, with ordinary monochrome tinting in updateColors.
@interface RVSettingsMenuImageIcon : NSObject
@property(nonatomic,strong) UIImage *image;
- (UIImage *)iconImageWithColor:(UIColor *)color;
- (BOOL)hasIconType;
- (int)iconType;
@end
@implementation RVSettingsMenuImageIcon
- (UIImage *)iconImageWithColor:(UIColor *)color { return [self.image imageWithTintColor:color renderingMode:UIImageRenderingModeAlwaysTemplate]; }
- (BOOL)hasIconType { return NO; }
- (int)iconType { return 0; }
@end

static id RVSettingsCreateMenuIcon(const RVSettingsMenuGlyph *glyph) {
    if (glyph->iconType) {
        id icon=RVObject(NSClassFromString(@"YTIIcon"),"message");
        // RVCan invokes respondsToSelector first, allowing GPB to resolve its accessors.
        if (RVCan(icon,"setIconType:",@"v@:i") && RVCan(icon,"hasIconType",@"B@:") && RVCan(icon,"iconType",@"i@:")) {
            ((void(*)(id,SEL,int))objc_msgSend)(icon,sel_registerName("setIconType:"),glyph->iconType);
            if (RVSettingsMenuIconImage(icon,UIColor.blackColor)) return icon;
        }
    }
    UIImage *image=nil;Class resources=NSClassFromString(@"YTUIResources");
    if (glyph->factory) {
        BOOL colored=strchr(glyph->factory,':')!=NULL;
        if (RVCan(resources,glyph->factory,colored ? @"@@:@" : @"@@:")) {
            id result=colored ? ((id(*)(id,SEL,id))objc_msgSend)(resources,sel_registerName(glyph->factory),UIColor.blackColor)
                : ((id(*)(id,SEL))objc_msgSend)(resources,sel_registerName(glyph->factory));
            if ([result isKindOfClass:UIImage.class]) image=result;
        }
    }
    if (!image) {
        // YTUIResources owns Innertube aliases; the remaining glyphs are in app Assets.car.
        // Match YTAppIconSupportProviderImpl's app resource bundle, not the injected dylib.
        Class owner=NSClassFromString(@"YTResources");
        NSBundle *bundle=owner ? [NSBundle bundleForClass:owner] : NSBundle.mainBundle;
        image=[UIImage imageNamed:@(glyph->asset) inBundle:bundle compatibleWithTraitCollection:nil];
    }
    if (image.size.width<=0 || image.size.height<=0) return nil;
    CGFloat scale=MIN(24/image.size.width,24/image.size.height);
    CGSize size=CGSizeMake(image.size.width*scale,image.size.height*scale);
    UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(24,24)];
    RVSettingsMenuImageIcon *icon=[RVSettingsMenuImageIcon new];
    icon.image=[[renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        (void)context;[image drawInRect:CGRectMake((24-size.width)/2,(24-size.height)/2,size.width,size.height)];
    }] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    return icon;
}
static id RVSettingsMenuIcon(NSDictionary *row) {
    NSString *key=RVSettingsMenuIconKey(row);if (!key) return nil;
    static NSMutableDictionary *icons;static dispatch_once_t once;
    dispatch_once(&once,^{ icons=[NSMutableDictionary dictionary]; });
    // Settings rendering and measurement run on the main thread. Cache successes and misses.
    if (!icons[key]) {
        id icon=RVSettingsBrandIconForMenu(key);
        // Prefer the supplied service logo, retaining the native glyph as a decode fallback.
        for (NSUInteger index=0;!icon && index<sizeof(RVSettingsMenuGlyphs)/sizeof(*RVSettingsMenuGlyphs);index++) {
            const RVSettingsMenuGlyph *glyph=&RVSettingsMenuGlyphs[index];
            if (![key isEqualToString:@(glyph->key)]) continue;
            icon=RVSettingsCreateMenuIcon(glyph);
            if (!icon) RVLog([NSString stringWithFormat:@"Settings icon unavailable: %@",key]);
            break;
        }
        icons[key]=icon ?: NSNull.null;
    }
    return icons[key]==NSNull.null ? nil : icons[key];
}
