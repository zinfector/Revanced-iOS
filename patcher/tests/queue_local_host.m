// Foundation integration test for the shipped MDX-to-native-queue bridge.
#define main QueueUpsellPriorMain
#include "queue_upsell_host.m"
#undef main

@interface YTIRequestMdxDeviceDiscoveryCommand : GPBMock
@property(nonatomic,strong) NSArray *requestPollingStrategiesArray;
@end
@implementation YTIRequestMdxDeviceDiscoveryCommand @end
@interface YTIMdxDeviceDiscoveryPollingStrategy : GPBMock
@property(nonatomic) int discoveryStrategy;
@property(nonatomic,strong) NSString *discoveryParams;
@end
@implementation YTIMdxDeviceDiscoveryPollingStrategy @end
@interface YTIWatchEndpoint : NSObject
@property(nonatomic,strong) NSString *videoId;
@end
@implementation YTIWatchEndpoint @end
@interface YTICommand (LocalFixture)
- (id)watchEndpoint;
- (void)setWatchEndpoint:(id)value;
@end
@implementation YTICommand (LocalFixture)
- (id)watchEndpoint { return self.command; }
- (void)setWatchEndpoint:(id)value { self.command=value; }
@end
@interface YTIFormattedString : NSObject
+ (id)formattedStringWithString:(NSString *)value;
@end
@implementation YTIFormattedString
+ (id)formattedStringWithString:(NSString *)value { return value; }
@end
@interface YTIPlaylistPanelVideoRenderer : NSObject <NSCopying>
@property(nonatomic,strong) NSString *videoId,*playlistSetVideoId;
@property(nonatomic,strong) id navigationEndpoint,title,thumbnail,indexText;
@property(nonatomic) bool selected;
@end
@implementation YTIPlaylistPanelVideoRenderer
- (id)copyWithZone:(NSZone *)zone {
    YTIPlaylistPanelVideoRenderer *copy=[[[self class] allocWithZone:zone] init];
    copy.videoId=self.videoId;copy.playlistSetVideoId=self.playlistSetVideoId;
    copy.navigationEndpoint=self.navigationEndpoint;copy.title=self.title;
    copy.thumbnail=self.thumbnail;copy.indexText=self.indexText;copy.selected=self.selected;return copy;
}
@end
@interface YTIPlaylistPanelRenderer_PlaylistPanelVideoSupportedRenderers : NSObject
@property(nonatomic,strong) id playlistPanelVideoRenderer;
@end
@implementation YTIPlaylistPanelRenderer_PlaylistPanelVideoSupportedRenderers @end
static BOOL RVFixturePanelUnavailable;
@interface YTIPlaylistPanelRenderer : NSObject
@property(nonatomic,strong) NSMutableArray *contentsArray;
@property(nonatomic,strong) NSString *playlistId,*title;
@property(nonatomic,strong) id titleText;
@property(nonatomic) NSUInteger totalVideos,currentIndex,localCurrentIndex;
@property(nonatomic) bool isEditable;
@end
@implementation YTIPlaylistPanelRenderer
@synthesize contentsArray=_contentsArray;
- (id)init { if ((self=[super init])) _contentsArray=[NSMutableArray array];return self; }
- (NSMutableArray *)contentsArray { return RVFixturePanelUnavailable ? nil : _contentsArray; }
@end
@interface YTQueueItem : NSObject
@property(nonatomic,strong) id videoRenderer;
+ (id)queueItemWithPlaylistPanelVideoRenderer:(id)video;
@end
@implementation YTQueueItem
+ (id)queueItemWithPlaylistPanelVideoRenderer:(id)video {
    YTQueueItem *item=[YTQueueItem new];item.videoRenderer=video;return item;
}
@end
@interface RVFixturePlayback : NSObject
@property(nonatomic,strong) NSString *contentVideoID;
@end
@implementation RVFixturePlayback @end
@interface YTQueueController : NSObject
@property(nonatomic,strong) NSMutableArray *playbackQueueItems,*insertions;
@property(nonatomic,strong) NSString *nowPlayingVideoID;
@property(nonatomic) bool isMDXRemoteQueue;
@property(nonatomic) BOOL throwOnInsert;
@property(nonatomic) NSUInteger modifications,nowPlayingIndex,indexSets;
- (NSUInteger)queueCount;
- (void)recordUserQueueModification;
- (void)insertQueueItems:(NSArray *)items atIndex:(NSUInteger)index;
@end
@implementation YTQueueController
@synthesize nowPlayingIndex=_nowPlayingIndex;
- (id)init { if ((self=[super init])) { _playbackQueueItems=[NSMutableArray array];_insertions=[NSMutableArray array]; }return self; }
- (NSUInteger)queueCount { return self.playbackQueueItems.count; }
- (void)recordUserQueueModification { self.modifications++; }
- (void)setNowPlayingIndex:(NSUInteger)index { _nowPlayingIndex=index;self.indexSets++; }
- (void)insertQueueItems:(NSArray *)items atIndex:(NSUInteger)index {
    if (self.throwOnInsert) @throw [NSException exceptionWithName:@"FixtureInsertFailure" reason:nil userInfo:nil];
    [self.insertions addObject:@(index)];
    [self.playbackQueueItems insertObjects:items atIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(index,items.count)]];
}
@end
@interface YTPlaylistPanelController : NSObject
@property(nonatomic,strong) id playlistPanelRenderer;
@property(nonatomic) NSUInteger refreshes;
- (void)refreshPlaylistCollectionViewController;
- (void)updateViewForCurrentVideo;
@end
@implementation YTPlaylistPanelController
- (void)refreshPlaylistCollectionViewController { self.refreshes++; }
- (void)updateViewForCurrentVideo {}
@end
@interface YTCommandRouter : NSObject
@property(nonatomic) NSUInteger calls;
- (bool)handleCommand:(id)command entry:(id)entry fromView:(id)view sender:(id)sender;
- (bool)handleCommand:(id)command entry:(id)entry fromView:(id)view sender:(id)sender completionBlock:(void(^)(id,id))completion;
@end
@implementation YTCommandRouter
- (bool)handleCommand:(id)command entry:(id)entry fromView:(id)view sender:(id)sender { self.calls++;return false; }
- (bool)handleCommand:(id)command entry:(id)entry fromView:(id)view sender:(id)sender completionBlock:(void(^)(id,id))completion {
    self.calls++;if (completion) completion(@"original",nil);return false;
}
@end

static NSString *RVLocalToken(NSUInteger operation,NSString *video) {
    uint8_t header[]={0x1a,0x15,0x08,(uint8_t)operation,0x12,0x11,0x0a,0x0f,0x0a,0x0b},tail[]={0x18,1};
    NSMutableData *bytes=[NSMutableData dataWithBytes:header length:sizeof(header)];
    [bytes appendData:[video dataUsingEncoding:NSASCIIStringEncoding]];[bytes appendBytes:tail length:sizeof(tail)];
    return [bytes base64EncodedStringWithOptions:0];
}
static YTICommand *RVLocalCommand(NSUInteger operation,NSString *video) {
    YTIMdxDeviceDiscoveryPollingStrategy *strategy=[YTIMdxDeviceDiscoveryPollingStrategy new];
    strategy.discoveryStrategy=4;strategy.discoveryParams=RVLocalToken(operation,video);
    YTIRequestMdxDeviceDiscoveryCommand *payload=[YTIRequestMdxDeviceDiscoveryCommand new];
    payload.requestPollingStrategiesArray=@[strategy];return Typed(payload,@"mdx-queue-fixture");
}
static NSArray *RVLocalIDs(NSArray *items,BOOL native) {
    NSMutableArray *ids=[NSMutableArray array];
    for (id item in items) [ids addObject:RVObject(native?RVObject(item,"videoRenderer"):item,"videoId") ?: @"missing"];
    return ids;
}
static void RVLocalReset(YTQueueController *controller,YTPlaylistPanelController *panel,RVFixturePlayback *playback) {
    RVCompatible=YES;RVFixturePanelUnavailable=NO;RVQueueLocalItems=nil;
    RVQueueLocalController=controller;RVQueueLocalPanelController=panel;RVCurrentPlayback=playback;
    controller.isMDXRemoteQueue=false;controller.throwOnInsert=NO;
    controller.indexSets=0;
    [controller.playbackQueueItems removeAllObjects];[controller.insertions removeAllObjects];
    playback.contentVideoID=@"Current0001";controller.nowPlayingVideoID=playback.contentVideoID;
    [controller.playbackQueueItems addObject:[YTQueueItem queueItemWithPlaylistPanelVideoRenderer:RVQueueMakeVideo(playback.contentVideoID,nil)]];
    RVQueueLocalSeed();
}
int main(void) {
    @autoreleasepool {
        YTQueueController *controller=[YTQueueController new];
        YTPlaylistPanelController *panel=[YTPlaylistPanelController new];RVFixturePlayback *playback=[RVFixturePlayback new];
        RVLocalReset(controller,panel,playback);
        YTICommand *next=RVLocalCommand(2,@"NextVideo01"),*last=RVLocalCommand(3,@"LastVideo01");
        assert([RVQueueMDXOperation(next)[@"play_next"] boolValue]);
        assert(![RVQueueMDXOperation(last)[@"play_next"] boolValue]);
        assert(RVQueueLocalHandle(last,nil));assert(RVQueueLocalHandle(next,nil));
        NSArray *expected=@[@"Current0001",@"NextVideo01",@"LastVideo01"];
        assert([RVLocalIDs(RVQueueLocalItems,NO) isEqual:expected]);
        assert([RVLocalIDs(controller.playbackQueueItems,YES) isEqual:expected]);
        assert(([controller.insertions isEqual:@[@1,@1]] && panel.refreshes==2));
        id rendered=panel.playlistPanelRenderer;assert([RVObject(rendered,"playlistId") isEqual:@"RVPortQueue"]);
        assert([RVObject(rendered,"contentsArray") count]==3);
        // Unrelated discovery and a real casting session retain their original path.
        NSArray *before=[RVQueueLocalItems copy];
        assert(!RVQueueLocalHandle(RVLocalCommand(1,@"NextVideo01"),nil));
        controller.isMDXRemoteQueue=true;assert(!RVQueueLocalHandle(next,nil));controller.isMDXRemoteQueue=false;
        assert([RVQueueLocalItems isEqual:before] && controller.insertions.count==2);
        YTIRequestMdxDeviceDiscoveryCommand *multiple=(id)[next.extensionValues allValues][0];
        NSArray *one=multiple.requestPollingStrategiesArray;multiple.requestPollingStrategiesArray=@[one[0],one[0]];
        assert(!RVQueueMDXOperation(next));multiple.requestPollingStrategiesArray=one;
        YTIRequestMdxDeviceDiscoveryCommand *invalidPayload=(id)[RVLocalCommand(1,@"NextVideo01").extensionValues allValues][0];
        YTICommand *ambiguous=[YTICommand new];ambiguous.extensionsCurrentlySet=@[@"invalid",@"valid"];
        ambiguous.extensionValues=@{@"invalid":invalidPayload,@"valid":multiple};
        assert(!RVQueueMDXOperation(ambiguous));
        ambiguous.extensionsCurrentlySet=@[@"queue",@"other"];
        ambiguous.extensionValues=@{@"queue":multiple,@"other":[YTIYPCOffersEndpoint new]};
        assert(!RVQueueMDXOperation(ambiguous));
        // An adapter failure must not publish a queue operation or mutate either model.
        RVFixturePanelUnavailable=YES;assert(!RVQueueLocalHandle(next,nil));RVFixturePanelUnavailable=NO;
        assert([RVQueueLocalItems isEqual:before]);
        assert([RVLocalIDs(controller.playbackQueueItems,YES) isEqual:expected]);
        // A native exception after acceptance must never replay the discovery path.
        controller.throwOnInsert=YES;assert(RVQueueLocalHandle(next,nil));controller.throwOnInsert=NO;
        // A real server playlist is respected by the response adapter.
        YTIPlaylistPanelRenderer *server=[YTIPlaylistPanelRenderer new];server.playlistId=@"PLserver";
        assert(RVQueueLocalParsedPanel(server,nil)==server);
        // A playing single video may not yet have any native queue items.
        RVLocalReset(controller,panel,playback);[controller.playbackQueueItems removeAllObjects];RVQueueLocalItems=nil;
        assert(RVQueueLocalHandle(next,nil));
        assert(([RVLocalIDs(controller.playbackQueueItems,YES) isEqual:@[@"Current0001",@"NextVideo01"]]));
        assert(([controller.insertions isEqual:@[@0]] && controller.nowPlayingIndex==0 && controller.indexSets==1));
        // Exercise the actual installed router hook and its completion contract.
        RVLocalReset(controller,panel,playback);RVInstallQueueLocal();
        YTCommandRouter *router=[YTCommandRouter new];__block NSUInteger completions=0;
        assert([router handleCommand:last entry:nil fromView:nil sender:nil completionBlock:^(id first,id second) {
            assert(first==nil && second==nil);completions++;
        }]);
        assert(completions==1 && router.calls==0 && controller.insertions.count==1);
        assert(![router handleCommand:[YTICommand new] entry:nil fromView:nil sender:nil completionBlock:^(id first,id second) {
            assert([first isEqual:@"original"]);completions++;
        }]);
        assert(completions==2 && router.calls==1);
        // Account changes, including incognito/nil transitions, discard local state.
        RVQueueLocalAccountBound=NO;RVQueueLocalBind(controller,@"fixture-account-A");
        assert(RVQueueLocalItems.count>0);
        RVQueueLocalBind(controller,nil);assert(RVQueueLocalItems==nil);
        RVQueueLocalItems=[NSMutableArray arrayWithObject:RVQueueMakeVideo(@"Current0001",nil)];
        RVQueueLocalBind(controller,@"fixture-account-B");assert(RVQueueLocalItems==nil);
        puts("{\"ok\":true,\"suite\":\"queue-local-native-bridge\",\"test_groups\":6,\"device_validation\":false}");
    }
    return 0;
}
