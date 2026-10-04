// SPDX-License-Identifier: MIT
// Native adapter implementation based on the local 21.39.4 metadata analysis.
// No fixed function addresses and no external hooking framework.
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import <mach-o/dyld.h>
#import <mach-o/loader.h>
#include <math.h>
#include <CommonCrypto/CommonDigest.h>
#include <netdb.h>
#include <stdatomic.h>
#import <Network/Network.h>
#import <AVFoundation/AVFoundation.h>
#import <MediaPlayer/MediaPlayer.h>
#include "RVFeatures.h"
#include "RVIntervals.h"
#include "RVImageHeader.h"

static void RVExtraObserve(id controller);
static void RVExtraSetPlayer(id player,id controller);
static BOOL RVExtraReject(NSData *data);
static void RVSegmentsChanged(void);

static NSDictionary *RVConfig;
static NSMutableArray<NSString *> *RVStatus;
static NSString *const RVPreferencePrefix = @"RVPort.";
static const void *RVSessionKey = &RVSessionKey;
static const void *RVGestureKey = &RVGestureKey;
static BOOL RVCompatible;
static atomic_int RVNetworkKind;
static void RVObserveNetwork(void) {
    static nw_path_monitor_t monitor;
    monitor=nw_path_monitor_create();
    nw_path_monitor_set_update_handler(monitor,^(nw_path_t path) {
        int kind=0;
        if (nw_path_get_status(path)==nw_path_status_satisfied)
            kind=nw_path_uses_interface_type(path,nw_interface_type_cellular) ? 2 : nw_path_uses_interface_type(path,nw_interface_type_wifi) ? 1 : 3;
        atomic_store_explicit(&RVNetworkKind,kind,memory_order_relaxed);
    });
    nw_path_monitor_set_queue(monitor,dispatch_get_global_queue(QOS_CLASS_UTILITY,0));nw_path_monitor_start(monitor);
}
static NSString *RVQualityPreference(void) {
    int kind=atomic_load_explicit(&RVNetworkKind,memory_order_relaxed);
    return kind==1 ? @"RVPort.lastQuality.wifi" : kind==2 ? @"RVPort.lastQuality.cellular" : @"RVPort.lastQuality";
}

static void RVLog(NSString *message) {
    @synchronized(RVStatus) { [RVStatus addObject:message]; }
    if ([RVConfig[@"diagnostics"] boolValue]) NSLog(@"[RVPort] %@",message);
}
static id RVSetting(NSString *key) {
    id override=[[NSUserDefaults standardUserDefaults] objectForKey:[RVPreferencePrefix stringByAppendingString:key]];
    return override ?: RVConfig[key];
}
static BOOL RVEnabled(NSString *key) { return RVCompatible && [RVSetting(key) boolValue]; }
#include "RVRuntime.inc"
#include "RVAuthentication.inc"

static BOOL RVAds(NSString *strategy) { return RVEnabled(@"video_ads") && [RVSetting(@"ad_strategy") isEqual:strategy]; }
static void RVInstallAds(void) {
    for (NSString *field in @[@"playerAdsArray",@"adPlacementsArray",@"adSlotsArray"]) {
        RVHook(@"YTIPlayerResponse",field,@"@@:",NO,^id(IMP original,SEL selector) {
            return ^id(id object) {
                if (RVAds(@"response")) return [NSMutableArray array];
                return ((id(*)(id,SEL))original)(object,selector);
            };
        });
    }
    for (NSString *selector in @[@"isMonetized",@"hasPrerollAds"]) {
        RVHook(@"YTIPlayerResponse",selector,@"B@:",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id object) {
                return RVAds(@"response") ? NO : ((BOOL(*)(id,SEL))original)(object,sel);
            };
        });
    }
    RVHook(@"YTAdsControlFlowManagerImpl",@"handleActivationForTriggerBundles:",@"v@:@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id bundles) { if (!RVAds(@"trigger")) ((void(*)(id,SEL,id))original)(object,sel,bundles); };
    });
    RVHook(@"YTLocalPlaybackController",@"createAdsPlaybackCoordinator",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) { return RVAds(@"coordinator") ? nil : ((id(*)(id,SEL))original)(object,sel); };
    });
    NSDictionary *gates=@{@"YTIPlayabilityStatus":@"isPlayableInBackground",@"MLVideo":@"playableInBackground",
        @"YTPlaybackData":@"isPlayableInBackground",@"YTBackgroundabilityPolicyImpl":@"isBackgroundableByUserSettings"};
    for (NSString *cls in gates) {
        RVHook(cls,gates[cls],@"B@:",NO,^id(IMP original,SEL sel) {
            return ^BOOL(id object) { return RVEnabled(@"background_playback") ? YES : ((BOOL(*)(id,SEL))original)(object,sel); };
        });
    }
}

@interface RVPlaybackSession : NSObject
@property(nonatomic,copy) NSString *videoID;
@property(nonatomic) NSUInteger generation;
@property(nonatomic,strong) NSArray<NSDictionary *> *segments;
@property(nonatomic,strong) NSArray<NSDictionary *> *rawSegments;
@property(nonatomic,strong) NSMutableSet<NSString *> *skippedOnce;
@property(nonatomic) BOOL highlightApplied;
@property(nonatomic,copy) NSString *categoryKey;
@property(nonatomic) double lastSkippedStart;
@property(nonatomic) double lastSkippedEnd;
@property(nonatomic) BOOL hasLastSkip;
@property(nonatomic) double ignoreSegmentEnd;
@property(nonatomic,strong) NSURLSessionDataTask *task;
@property(nonatomic) NSTimeInterval lastRequest;
@property(nonatomic) NSTimeInterval lastSeekWallTime;
@property(nonatomic) double lastSeekTarget;
@property(nonatomic) double speed;
@property(nonatomic) BOOL speedApplied;
@property(nonatomic) BOOL programmaticSpeed;
@property(nonatomic) BOOL seeking;
@property(nonatomic,strong) NSString *cpn;
@end
@implementation RVPlaybackSession
@end

static RVPlaybackSession *RVSession(id controller) {
    if (!controller) return nil;
    RVPlaybackSession *session=objc_getAssociatedObject(controller,RVSessionKey);
    if (!session) { session=[RVPlaybackSession new]; session.segments=@[];objc_setAssociatedObject(controller,RVSessionKey,session,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
    return session;
}
static NSString *RVSponsorBehavior(NSString *category) {
    id configured=RVSetting(@"sponsor_behaviors");
    NSString *value=[configured isKindOfClass:NSDictionary.class] ? configured[category] : nil;
    return [value isKindOfClass:NSString.class] ? value : @"skip";
}
static NSString *RVSegmentKey(NSDictionary *segment) {
    NSString *uuid=segment[@"UUID"];
    return uuid.length ? uuid : [NSString stringWithFormat:@"%@/%@/%@",segment[@"category"],segment[@"start"],segment[@"end"]];
}
static NSArray<NSDictionary *> *RVSponsorRanges(RVPlaybackSession *session,BOOL automatic) {
    size_t capacity=MIN(session.rawSegments.count,512),count=0;
    RVInterval *input=calloc(capacity ?: 1,sizeof(RVInterval)),*output=calloc(capacity ?: 1,sizeof(RVInterval));
    if (!input || !output) { free(input);free(output);return @[]; }
    for (NSDictionary *range in session.rawSegments) {
        if (![range[@"action"] isEqual:@"skip"]) continue;
        NSString *behavior=RVSponsorBehavior(range[@"category"]);
        if ([behavior isEqual:@"ignore"] || [behavior isEqual:@"seekbar-only"]) continue;
        if (automatic && ![@[@"skip",@"skip-once"] containsObject:behavior]) continue;
        if (automatic && [behavior isEqual:@"skip-once"] && [session.skippedOnce containsObject:RVSegmentKey(range)]) continue;
        if (count>=capacity) break;
        input[count++]=(RVInterval){[range[@"start"] doubleValue],[range[@"end"] doubleValue]};
    }
    size_t merged=RVMergeIntervals(input,count,output,capacity);
    NSMutableArray *result=[NSMutableArray arrayWithCapacity:merged];
    for (size_t i=0;i<merged;i++) [result addObject:@{@"start":@(output[i].start),@"end":@(output[i].end)}];
    free(input);free(output);return result;
}
static void RVRecordSponsorSkip(RVPlaybackSession *session,double start,double end) {
    if (!session.skippedOnce) session.skippedOnce=[NSMutableSet set];
    for (NSDictionary *range in session.rawSegments) if ([range[@"action"] isEqual:@"skip"] &&
        [RVSponsorBehavior(range[@"category"]) isEqual:@"skip-once"] && [range[@"start"] doubleValue]>=start && [range[@"end"] doubleValue]<=end)
        [session.skippedOnce addObject:RVSegmentKey(range)];
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    [defaults setInteger:[defaults integerForKey:@"RVPort.SB.skipRequests"]+1 forKey:@"RVPort.SB.skipRequests"];
    double previous=[defaults doubleForKey:@"RVPort.SB.requestedSkipSeconds"];
    if (isfinite(start) && isfinite(end) && end>start) [defaults setDouble:(isfinite(previous) ? previous : 0)+end-start forKey:@"RVPort.SB.requestedSkipSeconds"];
}

static void RVFetchSegments(RVPlaybackSession *session) {
    NSTimeInterval now=[NSDate timeIntervalSinceReferenceDate];
    if (!RVEnabled(@"sponsorblock")) { if (session.task) { [session.task cancel];session.task=nil;session.generation++; } return; }
    NSMutableArray *categories=[NSMutableArray array];
    for (NSString *category in RVSetting(@"sponsor_categories")) if (![RVSponsorBehavior(category) isEqual:@"ignore"]) [categories addObject:category];
    NSData *categoryData=[NSJSONSerialization dataWithJSONObject:categories options:0 error:nil];
    NSString *key=[[NSString alloc] initWithData:categoryData encoding:NSUTF8StringEncoding];
    if (![session.categoryKey isEqual:key]) {
        [session.task cancel];session.task=nil;session.rawSegments=@[];session.segments=@[];session.categoryKey=key;session.generation++;session.lastRequest=0;
    }
    if (!categories.count || session.task || session.rawSegments.count || now-session.lastRequest<30) return;
    session.lastRequest=now;
    NSURLComponents *components=[NSURLComponents componentsWithString:@"https://sponsor.ajay.app/api/skipSegments"];
    components.queryItems=@[[NSURLQueryItem queryItemWithName:@"videoID" value:session.videoID],
        [NSURLQueryItem queryItemWithName:@"categories" value:[[NSString alloc] initWithData:categoryData encoding:NSUTF8StringEncoding]],
        [NSURLQueryItem queryItemWithName:@"actionTypes" value:@"[\"skip\",\"poi\"]"]];
    NSUInteger generation=session.generation;
    __weak RVPlaybackSession *weakSession=session;
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:components.URL];request.timeoutInterval=10;
    session.task=[[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData *data,NSURLResponse *response,NSError *error) {
        id json=nil;
        if (!error && [(NSHTTPURLResponse *)response statusCode]==200 && data.length<1024*1024)
            json=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        NSMutableArray *accepted=[NSMutableArray array];
        if ([json isKindOfClass:[NSArray class]]) {
            for (id record in json) {
                if (![record isKindOfClass:[NSDictionary class]]) continue;
                id interval=record[@"segment"];
                if (![interval isKindOfClass:[NSArray class]] || [interval count]!=2 ||
                    ![interval[0] isKindOfClass:[NSNumber class]] || ![interval[1] isKindOfClass:[NSNumber class]]) continue;
                double start=[interval[0] doubleValue],end=[interval[1] doubleValue];
                NSString *action=[record[@"actionType"] isKindOfClass:NSString.class] ? record[@"actionType"] : @"skip";
                BOOL point=[action isEqual:@"poi"] && [record[@"category"] isEqual:@"poi_highlight"];
                if (accepted.count<512 && (point || [action isEqual:@"skip"]) && RVIntervalValid((RVInterval){start,end},point,[RVSetting(@"sponsor_min_duration") doubleValue]) &&
                    [record[@"category"] isKindOfClass:[NSString class]] &&
                    [RVSetting(@"sponsor_categories") containsObject:record[@"category"]])
                    [accepted addObject:@{@"start":@(start),@"end":@(end),@"category":record[@"category"],@"action":action,
                        @"UUID":[record[@"UUID"] isKindOfClass:[NSString class]] ? record[@"UUID"] : @""}];
            }
        }
        [accepted sortUsingComparator:^NSComparisonResult(NSDictionary *a,NSDictionary *b) { return [a[@"start"] compare:b[@"start"]]; }];
        dispatch_async(dispatch_get_main_queue(),^{
            RVPlaybackSession *live=weakSession;
            if (!live || live.generation!=generation) return;
            live.task=nil;live.rawSegments=accepted;live.segments=RVSponsorRanges(live,NO);
            RVSegmentsChanged();
        });
    }];
    [session.task resume];
}

static void RVObservePlayback(id controller,id timeObject) {
    if (![NSThread isMainThread]) {
        __weak id weakController=controller;
        dispatch_async(dispatch_get_main_queue(),^{ id current=weakController;if (current) RVObservePlayback(current,timeObject); });
        return;
    }
    RVExtraObserve(controller);
    NSString *videoID=RVObject(controller,"contentVideoID");
    if (![videoID isKindOfClass:[NSString class]] || !videoID.length || !RVBool(controller,"isPlayingContentVideo") || RVBool(controller,"isPlayingAd")) return;
    RVPlaybackSession *session=RVSession(controller);
    id contentTime=RVObject(controller,"contentVideoCurrentTime");
    NSString *cpn=RVObject(contentTime,"CPN");
    if (![session.videoID isEqual:videoID] || (cpn && ![session.cpn isEqual:cpn])) {
        [session.task cancel];session.task=nil;session.videoID=videoID;session.cpn=cpn;session.generation++;
        session.segments=@[];session.lastRequest=0;session.lastSeekWallTime=0;session.speedApplied=NO;
        session.rawSegments=@[];session.lastSkippedEnd=0;session.hasLastSkip=NO;session.ignoreSegmentEnd=0;
        session.skippedOnce=[NSMutableSet set];session.highlightApplied=NO;
    }
    double seconds=RVDouble(contentTime ?: timeObject,"time");
    if (!isfinite(seconds) || seconds<0) return;
    id playbackData=RVObject(controller,"contentPlaybackData");
    id response=RVObject(playbackData,"playerResponse");
    // Fail closed for live/unknown response ownership.
    if (!response || !RVCan(response,"isLivePlayback",@"B@:") || RVBool(response,"isLivePlayback")) return;
    RVFetchSegments(session);
    if (RVEnabled(@"sponsorblock") && !RVEnabled(@"sponsorblock_manual") && !session.seeking) {
        if (!session.highlightApplied) for (NSDictionary *point in session.rawSegments) {
            if (![point[@"action"] isEqual:@"poi"] || ![@[@"skip",@"skip-once"] containsObject:RVSponsorBehavior(point[@"category"])]) continue;
            double targetSeconds=[point[@"start"] doubleValue];
            if (seconds>=targetSeconds) { session.highlightApplied=YES;break; }
            Class times=NSClassFromString(@"YTSingleVideoTime");
            if (!RVCan(times,"timeWithTime:CPN:",@"@@:d@") || !RVCan(controller,"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi")) break;
            id target=((id(*)(id,SEL,double,id))objc_msgSend)(times,sel_registerName("timeWithTime:CPN:"),targetSeconds,session.cpn);
            if (!target) break;
            session.highlightApplied=YES;session.lastSkippedStart=seconds;session.lastSkippedEnd=targetSeconds;session.hasLastSkip=YES;
            ((void(*)(id,SEL,id,double,double,int))objc_msgSend)(controller,sel_registerName("seekToTime:toleranceBefore:toleranceAfter:seekSource:"),target,0,0,0);return;
        }
        for (NSDictionary *segment in RVSponsorRanges(session,YES)) {
            double start=[segment[@"start"] doubleValue],end=[segment[@"end"] doubleValue];
            if (seconds<start || seconds>=end-0.08) continue;
            if (seconds<session.ignoreSegmentEnd && end<=session.ignoreSegmentEnd) continue;
            NSTimeInterval wall=[NSDate timeIntervalSinceReferenceDate];
            if (wall-session.lastSeekWallTime<2 && fabs(end-session.lastSeekTarget)<0.1) break;
            Class timeClass=NSClassFromString(@"YTSingleVideoTime");
            if (!RVCan(timeClass,"timeWithTime:CPN:",@"@@:d@") || !RVCan(controller,"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi")) break;
            id target=((id(*)(id,SEL,double,id))objc_msgSend)(timeClass,sel_registerName("timeWithTime:CPN:"),end,session.cpn);
            if (!target) break;
            session.seeking=YES;session.lastSeekWallTime=wall;session.lastSeekTarget=end;
            session.lastSkippedStart=start;session.lastSkippedEnd=end;
            session.hasLastSkip=YES;
            RVRecordSponsorSkip(session,start,end);
            ((void(*)(id,SEL,id,double,double,int))objc_msgSend)(controller,sel_registerName("seekToTime:toleranceBefore:toleranceAfter:seekSource:"),target,0,0,0);
            session.seeking=NO;break;
        }
    }
}

static BOOL RVDataMatches(NSData *data,NSArray<NSString *> *patterns) {
    for (NSString *pattern in patterns) {
        NSData *needle=[pattern dataUsingEncoding:NSUTF8StringEncoding];
        if ([data rangeOfData:needle options:0 range:NSMakeRange(0,data.length)].location!=NSNotFound) return YES;
    }
    return NO;
}
static BOOL RVIsPromoted(id item) {
    return RVBool(item,"hasPromotedVideoRenderer") || RVBool(item,"hasCompactPromotedVideoRenderer") || RVBool(item,"hasPromotedVideoInlineMutedRenderer");
}
static id RVFilterModel(id model) {
    if (!RVEnabled(@"feed_ads")) return model;
    NSArray *contents=RVObject(model,"contentsArray");
    if (![contents isKindOfClass:[NSArray class]] || !RVCan(model,"setContentsArray:",@"v@:@") || ![model conformsToProtocol:@protocol(NSCopying)]) return model;
    NSMutableArray *filtered=[NSMutableArray array];
    for (id item in contents) {
        if (RVIsPromoted(item)) continue;
        id section=RVObject(item,"itemSectionRenderer");
        if (section) {
            id newSection=RVFilterModel(section);
            if (newSection!=section && [item conformsToProtocol:@protocol(NSCopying)] && RVCan(item,"setItemSectionRenderer:",@"v@:@")) {
                id copy=[item copy];RVSetObject(copy,"setItemSectionRenderer:",newSection);[filtered addObject:copy];continue;
            }
        }
        [filtered addObject:item];
    }
    if ([filtered isEqualToArray:contents]) return model;
    id copy=[model copy];RVSetObject(copy,"setContentsArray:",filtered);return copy;
}
static void RVInstallFeed(void) {
    RVHook(@"YTIElementRenderer",@"elementData",@"@@:",NO,^id(IMP original,SEL sel) {
        return ^id(id object) {
            id data=((id(*)(id,SEL))original)(object,sel);
            if (![data isKindOfClass:[NSData class]] || [data length]>2*1024*1024) return data;
            BOOL reject=(RVEnabled(@"feed_ads") && RVDataMatches(data,RVSetting(@"feed_patterns"))) ||
                (RVEnabled(@"hide_shorts") && RVDataMatches(data,RVSetting(@"shorts_patterns"))) || RVExtraReject(data);
            static _Thread_local BOOL makingEmpty;
            if (!reject || makingEmpty) return data;
            makingEmpty=YES;
            id empty=RVObject(NSClassFromString(@"YTIElementRenderer"),"emptyCellElementRenderer");
            id result=RVObject(empty,"elementData");
            makingEmpty=NO;
            return [result isKindOfClass:[NSData class]] ? result : data;
        };
    });
    for (NSString *cls in @[@"YTSectionListViewController",@"YTInnerTubeCollectionViewController"]) {
        RVHook(cls,@"loadWithModel:",@"v@:@",NO,^id(IMP original,SEL sel) {
            return ^(id object,id model) { ((void(*)(id,SEL,id))original)(object,sel,RVFilterModel(model)); };
        });
    }
    RVHook(@"YTReelContentModel",@"makeContentModelForEntry:",@"@@:@",YES,^id(IMP original,SEL sel) {
        return ^id(id cls,id entry) {
            id model=((id(*)(id,SEL,id))original)(cls,sel,entry);
            return RVEnabled(@"shorts_ads") && RVInt(model,"videoType")==3 ? nil : model;
        };
    });
}

static id RVLimitFormats(id formats) {
    int cap=[RVSetting(@"default_quality") intValue];
    int kind=atomic_load_explicit(&RVNetworkKind,memory_order_relaxed);
    id configured=kind==1 ? RVSetting(@"wifi_quality") : kind==2 ? RVSetting(@"cellular_quality") : nil;
    if ([configured isKindOfClass:NSNumber.class] && [configured intValue]>=0) cap=[configured intValue];
    if (RVEnabled(@"remember_quality")) {
        NSNumber *saved=[[NSUserDefaults standardUserDefaults] objectForKey:RVQualityPreference()] ?: [[NSUserDefaults standardUserDefaults] objectForKey:@"RVPort.lastQuality"];
        if (saved) cap=saved.intValue;
    }
    if (![formats isKindOfClass:[NSArray class]]) return formats;
    NSMutableArray *accepted=[NSMutableArray array];
    for (id format in formats) {
        if (cap && !RVCan(format,"singleDimensionResolution",@"i@:")) return formats;
        int resolution=cap ? RVInt(format,"singleDimensionResolution") : 0;
        if (cap && resolution>cap) continue;
        NSString *mime=RVObject(format,"MIMEType");
        if (RVEnabled(@"disable_vp9") && [mime isKindOfClass:[NSString class]] &&
            ([mime.lowercaseString containsString:@"vp9"] || [mime.lowercaseString containsString:@"vp09"])) continue;
        if (RVEnabled(@"disable_hdr") && RVCan(format,"transferCharacteristics",@"C@:")) {
            unsigned char transfer=((unsigned char(*)(id,SEL))objc_msgSend)(format,sel_registerName("transferCharacteristics"));
            if (transfer==16 || transfer==18) continue; // ITU-T H.273: PQ and HLG.
        }
        [accepted addObject:format];
    }
    // Never leave the player with no selectable format.
    return accepted.count ? accepted : formats;
}
static void RVInstallPlayer(void) {
    RVHook(@"YTLocalPlaybackController",@"singleVideo:currentVideoTimeDidChange:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id controller,id video,id time) {
            ((void(*)(id,SEL,id,id))original)(controller,sel,video,time);
            RVObservePlayback(controller,time);
        };
    });
    RVHook(@"YTPlayerViewController",@"playbackController:didLoadContentPlaybackData:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id controller,id data) {
            ((void(*)(id,SEL,id,id))original)(object,sel,controller,data);
            RVExtraSetPlayer(object,controller);
            dispatch_async(dispatch_get_main_queue(),^{
                RVPlaybackSession *session=RVSession(object);
                double value=[RVSetting(@"default_speed") doubleValue];
                if (RVEnabled(@"remember_speed")) {
                    NSNumber *last=[[NSUserDefaults standardUserDefaults] objectForKey:@"RVPort.lastSpeed"];
                    if (last) value=last.doubleValue;
                }
                if ((!RVEnabled(@"remember_speed") && value==1.0) || !isfinite(value) || value<0.25 || value>4 || !RVCan(object,"setPlaybackRate:",@"v@:f")) return;
                session.programmaticSpeed=YES;
                ((void(*)(id,SEL,float))objc_msgSend)(object,sel_registerName("setPlaybackRate:"),(float)value);
                session.programmaticSpeed=NO;session.speedApplied=YES;
            });
        };
    });
    RVHook(@"YTPlayerViewController",@"setPlaybackRate:",@"v@:f",NO,^id(IMP original,SEL sel) {
        return ^(id object,float rate) {
            ((void(*)(id,SEL,float))original)(object,sel,rate);
            if (RVEnabled(@"remember_speed") && !RVSession(object).programmaticSpeed && isfinite(rate) && rate>=0.25 && rate<=4)
                [[NSUserDefaults standardUserDefaults] setFloat:rate forKey:@"RVPort.lastSpeed"];
        };
    });
    for (NSString *cls in @[@"MLPersistentVideoQualitySettingFormatConstraint",@"MLQuickMenuVideoQualitySettingFormatConstraint"]) {
        RVHook(cls,@"filterFormats:",@"@@:@",NO,^id(IMP original,SEL sel) {
            return ^id(id object,id formats) { return RVLimitFormats(((id(*)(id,SEL,id))original)(object,sel,formats)); };
        });
    }
}

#include "RVExtras.inc"

@interface RVSettingsController : UITableViewController
@property(nonatomic,strong) NSArray<NSString *> *keys;
@property(nonatomic,strong) NSArray<NSString *> *labels;
@end
@implementation RVSettingsController
- (void)viewDidLoad {
    [super viewDidLoad];self.title=@"ReVanced Port";
    self.keys=RVFeatureKeys;
    self.labels=RVFeatureLabels;
    self.navigationItem.leftBarButtonItem=[[UIBarButtonItem alloc] initWithTitle:@"Video tools" style:UIBarButtonItemStylePlain target:self action:@selector(tools)];
    self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done)];
}
- (void)done { [self dismissViewControllerAnimated:YES completion:nil]; }
- (void)tools { RVShowTools(self); }
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return 3; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { return section==0 ? self.keys.count : section==1 ? 3 : 1; }
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section { return section==0 ? @"Features" : section==1 ? @"Playback" : @"Diagnostics"; }
- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    return section==0 ? @"Changes apply immediately. Reopen the video to rebuild its player response. Experimental features need device testing. Restart after changing authentication settings." : nil;
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)index {
    UITableViewCell *cell=[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    if (index.section==0) {
        cell.textLabel.text=self.labels[index.row];UISwitch *toggle=[UISwitch new];toggle.tag=index.row;
        toggle.on=RVEnabled(self.keys[index.row]);toggle.enabled=RVCompatible;
        [toggle addTarget:self action:@selector(toggled:) forControlEvents:UIControlEventValueChanged];cell.accessoryView=toggle;
    } else if (index.section==1) {
        cell.accessoryType=UITableViewCellAccessoryDisclosureIndicator;
        if (index.row==0) { cell.textLabel.text=@"Default speed";cell.detailTextLabel.text=[NSString stringWithFormat:@"%.2fx",[RVSetting(@"default_speed") doubleValue]]; }
        if (index.row==1) { cell.textLabel.text=@"Resolution cap";int q=[RVSetting(@"default_quality") intValue];cell.detailTextLabel.text=q ? [NSString stringWithFormat:@"%dp",q] : @"Auto"; }
        if (index.row==2) { cell.textLabel.text=@"Ad strategy";cell.detailTextLabel.text=RVSetting(@"ad_strategy"); }
    } else { cell.textLabel.text=RVCompatible ? @"Show hook diagnostics" : @"Unsupported app: features disabled";cell.accessoryType=UITableViewCellAccessoryDisclosureIndicator; }
    return cell;
}
- (void)toggled:(UISwitch *)toggle {
    [[NSUserDefaults standardUserDefaults] setBool:toggle.on forKey:[RVPreferencePrefix stringByAppendingString:self.keys[toggle.tag]]];
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)index {
    [tableView deselectRowAtIndexPath:index animated:YES];
    if (index.section==0) return;
    if (index.section==2) {
        NSString *text;@synchronized(RVStatus) { text=[RVStatus componentsJoinedByString:@"\n"]; }
        UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Hook diagnostics" message:text preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Copy diagnostic report" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            NSMutableDictionary *effective=[NSMutableDictionary dictionary];for (NSString *key in RVConfig) effective[key]=RVSetting(key) ?: NSNull.null;
            NSArray *hooks;@synchronized(RVStatus) { hooks=[RVStatus copy]; }
            NSDictionary *report=@{@"patcher_version":@"0.3.1",@"profile_accepted":@(RVCompatible),@"youtube_version":NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"] ?: @"",@"ios_version":UIDevice.currentDevice.systemVersion,@"device_model":UIDevice.currentDevice.model,@"effective_config":effective,@"hooks":hooks,@"authentication":RVAuthenticationReport()};
            NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
            if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        }]];
        [alert addAction:[UIAlertAction actionWithTitle:@"Copy patch configuration" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            NSMutableDictionary *effective=[NSMutableDictionary dictionary];for (NSString *key in RVConfig) effective[key]=RVSetting(key) ?: NSNull.null;
            NSData *data=[NSJSONSerialization dataWithJSONObject:effective options:NSJSONWritingPrettyPrinted error:nil];
            if (data) UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        }]];
        [alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleCancel handler:nil]];[self presentViewController:alert animated:YES completion:nil];return;
    }
    NSArray *values;NSString *key;
    if (index.row==0) { values=@[@0.25,@0.5,@0.75,@1.0,@1.25,@1.5,@1.75,@2.0,@2.5,@3.0,@4.0];key=@"default_speed"; }
    else if (index.row==1) { values=@[@0,@144,@240,@360,@480,@720,@1080,@1440,@2160];key=@"default_quality"; }
    else { values=@[@"response",@"trigger",@"coordinator"];key=@"ad_strategy"; }
    UIAlertController *picker=[UIAlertController alertControllerWithTitle:@"Choose setting" message:index.row==2 ? @"Trigger and coordinator strategies are separate experiments." : nil preferredStyle:UIAlertControllerStyleAlert];
    for (id value in values) {
        [picker addAction:[UIAlertAction actionWithTitle:[value description] style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            [[NSUserDefaults standardUserDefaults] setObject:value forKey:[RVPreferencePrefix stringByAppendingString:key]];
            [self.tableView reloadData];
        }]];
    }
    [picker addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];[self presentViewController:picker animated:YES completion:nil];
}
@end

@interface RVSettingsEntrance : NSObject
- (void)attach;
- (void)open:(UILongPressGestureRecognizer *)gesture;
@end
@implementation RVSettingsEntrance
- (void)attach {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) continue;
        for (UIWindow *window in ((UIWindowScene *)scene).windows) {
            if (objc_getAssociatedObject(window,RVGestureKey)) continue;
            UILongPressGestureRecognizer *gesture=[[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(open:)];
            gesture.numberOfTouchesRequired=3;gesture.minimumPressDuration=1;gesture.cancelsTouchesInView=NO;
            [window addGestureRecognizer:gesture];objc_setAssociatedObject(window,RVGestureKey,gesture,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }
    }
}
- (void)open:(UILongPressGestureRecognizer *)gesture {
    if (gesture.state!=UIGestureRecognizerStateBegan) return;
    UIViewController *top=((UIWindow *)gesture.view).rootViewController;
    while (top.presentedViewController) top=top.presentedViewController;
    if (!top || [top isKindOfClass:[RVSettingsController class]]) return;
    RVSettingsController *settings=[[RVSettingsController alloc] initWithStyle:UITableViewStyleInsetGrouped];
    [top presentViewController:[[UINavigationController alloc] initWithRootViewController:settings] animated:YES completion:nil];
}
@end

static BOOL RVCheckIdentity(void) {
    NSDictionary *info=NSBundle.mainBundle.infoDictionary;
    // Installation tools may reassign the bundle ID. Main-image UUID is the
    // executable identity; preserve that check while allowing re-signing.
    if (![info[@"CFBundleShortVersionString"] isEqual:@"21.39.4"] || ![info[@"CFBundleExecutable"] isEqual:@"YouTube"]) return NO;
    const struct mach_header_64 *header=(const struct mach_header_64 *)_dyld_get_image_header(0);
    if (!header || header->magic!=MH_MAGIC_64) return NO;
    const uint8_t expected[16]={0x1f,0x61,0x66,0x5a,0x5d,0x4a,0x35,0x66,0x8b,0x77,0xda,0x82,0x1e,0x1d,0x6c,0x17};
    const uint8_t *cursor=(const uint8_t *)(header+1),*end=cursor+header->sizeofcmds;
    for (uint32_t i=0;i<header->ncmds && cursor+sizeof(struct load_command)<=end;i++) {
        const struct load_command *command=(const struct load_command *)cursor;
        if (command->cmdsize<8 || cursor+command->cmdsize>end) return NO;
        if (command->cmd==LC_UUID && command->cmdsize>=sizeof(struct uuid_command))
            return memcmp(((const struct uuid_command *)command)->uuid,expected,16)==0;
        cursor+=command->cmdsize;
    }
    return NO;
}
__attribute__((constructor)) static void RVStart(void) {
    @autoreleasepool {
        RVStatus=[NSMutableArray array];
        NSString *path=[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"RVPort.json"];
        NSData *data=[NSData dataWithContentsOfFile:path];
        id config=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        RVConfig=[config isKindOfClass:[NSDictionary class]] ? config : @{};
        RVCompatible=RVCheckIdentity() && [RVConfig[@"schema"] intValue]==1;
        RVLog(RVCompatible ? @"YouTube 21.39.4 profile accepted" : @"App identity/config mismatch: hooks disabled");
        if (RVCompatible) { RVInstallAuthentication();RVObserveNetwork();RVInstallAds();RVInstallFeed();RVInstallPlayer();RVInstallExtras(); }
        dispatch_async(dispatch_get_main_queue(),^{
            static RVSettingsEntrance *entrance;entrance=[RVSettingsEntrance new];
            [[NSNotificationCenter defaultCenter] addObserver:entrance selector:@selector(attach) name:UIApplicationDidBecomeActiveNotification object:nil];
            [entrance attach];
        });
    }
}
