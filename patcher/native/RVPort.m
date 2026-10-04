// SPDX-License-Identifier: MIT
// Native adapter implementation based on the local 21.39.4 metadata analysis.
// No fixed function addresses and no external hooking framework.
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
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
static BOOL RVOrdinaryContent(id controller);
static NSString *RVContentGate(id controller);
static BOOL RVEnabled(NSString *key);
static __weak id RVCurrentPlayer;
static __weak id RVCurrentPlayback;
static NSMutableDictionary<NSString *,NSNumber *> *RVSponsorCounts;
static NSMutableDictionary<NSString *,id> *RVSponsorState;
static void RVSponsorCount(NSString *key) {
    if (!RVEnabled(@"diagnostics")) return;
    if (!RVSponsorCounts) RVSponsorCounts=[NSMutableDictionary dictionary];
    RVSponsorCounts[key]=@([RVSponsorCounts[key] unsignedLongLongValue]+1);
}
static void RVSponsorValue(NSString *key,id value) {
    if (!RVEnabled(@"diagnostics")) return;
    if (!RVSponsorState) RVSponsorState=[NSMutableDictionary dictionary];
    RVSponsorState[key]=value;
}

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
// The player's native clock observes its active video's event center directly.
// Resolve the same core controller rather than associating a second session with
// the player facade (whose seek method takes a double, not YTSingleVideoTime).
static id RVPlaybackForPlayer(id player) {
    Class playerClass=NSClassFromString(@"YTPlayerViewController");
    Class localClass=NSClassFromString(@"YTLocalPlaybackController");
    if (!playerClass || !localClass || ![player isKindOfClass:playerClass]) return nil;
    Ivar field=class_getInstanceVariable(playerClass,"_playbackController");
    const char *type=field ? ivar_getTypeEncoding(field) : NULL;
    if (!type || strcmp(type,"@\"<YTCorePlaybackController>\"")!=0) return nil;
    id controller=object_getIvar(player,field);
    return [controller isKindOfClass:localClass] ? controller : nil;
}
static id RVContentResponse(id controller) {
    // YTPlaybackData.playerResponse is a YTPlayerResponse wrapper. Its
    // playerData is the YTIPlayerResponse that implements isLivePlayback.
    id wrapper=RVObject(RVObject(controller,"contentPlaybackData"),"playerResponse");
    Class wrapperClass=NSClassFromString(@"YTPlayerResponse");
    Class responseClass=NSClassFromString(@"YTIPlayerResponse");
    if (!wrapperClass || !responseClass || ![wrapper isKindOfClass:wrapperClass]) return nil;
    id response=RVObject(wrapper,"playerData");
    return [response isKindOfClass:responseClass] ? response : nil;
}
#include "RVAuthentication.inc"
#include "RVMiniplayer.inc"

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
@property(nonatomic) BOOL segmentsFetched;
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
    NSString *key=[NSString stringWithFormat:@"%@/%@",[[NSString alloc] initWithData:categoryData encoding:NSUTF8StringEncoding],RVSetting(@"sponsor_min_duration")];
    if (![session.categoryKey isEqual:key]) {
        [session.task cancel];session.task=nil;session.rawSegments=@[];session.segments=@[];session.segmentsFetched=NO;session.categoryKey=key;session.generation++;session.lastRequest=0;
        RVSegmentsChanged();
    }
    if (!categories.count || session.task || session.segmentsFetched || now-session.lastRequest<30) return;
    session.lastRequest=now;
    RVSponsorCount(@"fetch_started");
    RVSponsorValue(@"fetch_state",@"requesting");
    NSURLComponents *components=[NSURLComponents componentsWithString:@"https://sponsor.ajay.app/api/skipSegments"];
    components.queryItems=@[[NSURLQueryItem queryItemWithName:@"videoID" value:session.videoID],
        [NSURLQueryItem queryItemWithName:@"categories" value:[[NSString alloc] initWithData:categoryData encoding:NSUTF8StringEncoding]],
        [NSURLQueryItem queryItemWithName:@"actionTypes" value:@"[\"skip\",\"poi\"]"]];
    NSUInteger generation=session.generation;
    __weak RVPlaybackSession *weakSession=session;
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:components.URL];request.timeoutInterval=10;
    session.task=[[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData *data,NSURLResponse *response,NSError *error) {
        id json=nil;
        NSInteger status=[response isKindOfClass:NSHTTPURLResponse.class] ? [(NSHTTPURLResponse *)response statusCode] : 0;
        if (!error && status==200 && data.length<1024*1024)
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
            if (!live || live.generation!=generation) { RVSponsorCount(@"stale_response");return; }
            RVSponsorCount(@"fetch_completed");
            RVSponsorValue(@"http_status",@(status));
            RVSponsorValue(@"network_error_code",@(error.code));
            RVSponsorValue(@"fetch_state",error ? @"network_error" : status==404 ? @"no_segments" : [json isKindOfClass:NSArray.class] ? @"loaded" : @"invalid_response");
            RVSponsorValue(@"accepted_segments",@(accepted.count));
            live.segmentsFetched=!error && (status==404 || [json isKindOfClass:NSArray.class]);
            live.task=nil;live.rawSegments=accepted;live.segments=RVSponsorRanges(live,NO);
            RVSegmentsChanged();
        });
    }];
    [session.task resume];
}

static void RVObservePlayback(id controller,id timeObject,NSString *source) {
    if (![NSThread isMainThread]) {
        __weak id weakController=controller;
        dispatch_async(dispatch_get_main_queue(),^{ id current=weakController;if (current) RVObservePlayback(current,timeObject,source); });
        return;
    }
    RVSponsorCount(source);
    if (!controller || controller!=RVCurrentPlayback || RVPlaybackForPlayer(RVCurrentPlayer)!=controller) {
        RVSponsorCount(@"unowned_clock");return;
    }
    RVExtraObserve(controller);
    NSString *videoID=RVObject(controller,"contentVideoID");
    NSString *gate=RVContentGate(controller);
    if (gate) { RVSponsorValue(@"playback_gate",gate);return; }
    RVPlaybackSession *session=RVSession(controller);
    id contentTime=RVObject(controller,"contentVideoCurrentTime");
    NSString *cpn=RVObject(contentTime,"CPN") ?: RVObject(controller,"contentVideoCPN");
    if (![session.videoID isEqual:videoID] || (cpn && ![session.cpn isEqual:cpn])) {
        [session.task cancel];session.task=nil;session.videoID=videoID;session.cpn=cpn;session.generation++;
        session.segments=@[];session.lastRequest=0;session.lastSeekWallTime=0;session.speedApplied=NO;
        session.rawSegments=@[];session.segmentsFetched=NO;session.lastSkippedEnd=0;session.hasLastSkip=NO;session.ignoreSegmentEnd=0;
        session.skippedOnce=[NSMutableSet set];session.highlightApplied=NO;
        RVSponsorCount(@"video_changed");RVSponsorValue(@"fetch_state",@"not_requested");
        RVSponsorValue(@"accepted_segments",@0);RVSegmentsChanged();
    }
    RVFetchSegments(session);
    NSString *eventCPN=RVObject(timeObject,"CPN");
    if (eventCPN && cpn && ![eventCPN isEqual:cpn]) { RVSponsorValue(@"playback_gate",@"stale_clock");return; }
    double seconds=RVDouble(timeObject,"time");
    if (!isfinite(seconds) || seconds<0) seconds=RVDouble(contentTime,"time");
    if (!isfinite(seconds) || seconds<0) { RVSponsorValue(@"playback_gate",@"invalid_time");return; }
    RVSponsorValue(@"content_time_seconds",@(seconds));
    RVSponsorValue(@"playback_gate",RVBool(controller,"isPlayingContentVideo") ? @"ready" : @"not_playing");
    // Fetch/paint while paused or buffering; only an active content clock seeks.
    if (!RVBool(controller,"isPlayingContentVideo")) return;
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
            RVSponsorCount(@"seek_requested");RVSponsorValue(@"seek_target_seconds",@(targetSeconds));
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
            RVSponsorCount(@"seek_requested");RVSponsorValue(@"seek_target_seconds",@(end));
            ((void(*)(id,SEL,id,double,double,int))objc_msgSend)(controller,sel_registerName("seekToTime:toleranceBefore:toleranceAfter:seekSource:"),target,0,0,0);
            session.seeking=NO;break;
        }
    }
}

static void RVObservePlayerClock(id player,id time) {
    if (!NSThread.isMainThread) {
        __weak id weakPlayer=player;
        dispatch_async(dispatch_get_main_queue(),^{ if (weakPlayer) RVObservePlayerClock(weakPlayer,time); });return;
    }
    if (player!=RVCurrentPlayer) { RVSponsorCount(@"unowned_player_clock");return; }
    id controller=RVPlaybackForPlayer(player);
    RVExtraSetPlayer(player,controller);
    RVObservePlayback(controller,time,@"player_clock");
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
            if (video==RVObject(controller,"activeVideo")) RVObservePlayback(controller,time,@"local_clock");
        };
    });
    RVHook(@"YTPlayerViewController",@"potentiallyMutatedSingleVideo:currentVideoTimeDidChange:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id player,id video,id time) {
            ((void(*)(id,SEL,id,id))original)(player,sel,video,time);
            // Data-load binds the player; clock updates never take ownership of
            // a different preview/preloaded player.
            RVObservePlayerClock(player,time);
        };
    });
    RVHook(@"YTPlayerViewController",@"playbackController:didLoadContentPlaybackData:",@"v@:@@",NO,^id(IMP original,SEL sel) {
        return ^(id object,id controller,id data) {
            ((void(*)(id,SEL,id,id))original)(object,sel,controller,data);
            RVExtraSetPlayer(object,controller);
            RVObservePlayback(RVPlaybackForPlayer(object),nil,@"data_load");
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

static NSDictionary *RVSponsorReport(void) {
    id controller=RVCurrentPlayback;
    RVPlaybackSession *session=RVSession(controller);
    id wrapper=RVObject(RVObject(controller,"contentPlaybackData"),"playerResponse");
    id response=RVContentResponse(controller);
    return @{@"schema":@2,@"enabled":@(RVEnabled(@"sponsorblock")),@"markers_enabled":@(RVEnabled(@"sponsorblock_markers")),
        @"automatic_skipping_enabled":@(RVEnabled(@"sponsorblock") && !RVEnabled(@"sponsorblock_manual")),
        @"controller_bound":@(controller && controller==RVPlaybackForPlayer(RVCurrentPlayer)),
        @"video_id_valid":@(RVVideoIDValid(RVObject(controller,"contentVideoID"))),
        @"ordinary_content":@(RVOrdinaryContent(controller)),@"playing_content":@(RVBool(controller,"isPlayingContentVideo")),
        @"playing_ad":@(RVBool(controller,"isPlayingAd")),@"overlay_bound":@(RVPlayerOverlay()!=nil),
        @"response_present":@(wrapper!=nil),@"response_unwrapped":@(response!=nil),
        @"ordinary_content_gate":RVContentGate(controller) ?: @"ready",
        @"response_path":@"contentPlaybackData.playerResponse.playerData",
        @"live_flag_known":@(RVCan(response,"isLivePlayback",@"B@:")),
        @"seek_abi_matches":@(RVCan(controller,"seekToTime:toleranceBefore:toleranceAfter:seekSource:",@"v@:@ddi")),
        @"request_in_flight":@(session.task!=nil),@"response_cached":@(session.segmentsFetched),
        @"segment_count":@(session.rawSegments.count),@"automatic_range_count":@(RVSponsorRanges(session,YES).count),
        @"counts":RVSponsorCounts.copy ?: @{},@"state":RVSponsorState.copy ?: @{},@"device_playback_verified":@NO};
}

#include "RVSettingsBridge.inc"
#include "RVSettingsUI.inc"

@interface RVSettingsEntrance : NSObject
- (void)attach;
- (void)open:(UILongPressGestureRecognizer *)gesture;
@end
@implementation RVSettingsEntrance
- (void)attach {
    if (RVCompatible) RVRefreshShortcuts();
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
    UIViewController *visible=[top isKindOfClass:UINavigationController.class] ? ((UINavigationController *)top).visibleViewController : top;
    if (!top || [visible isKindOfClass:RVSettingsController.class] || [visible isKindOfClass:RVPreferenceTextController.class]) return;
    RVSettingsController *settings=[[RVSettingsController alloc] initWithStyle:UITableViewStyleInsetGrouped];settings.modalEntry=YES;
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
        if (RVCompatible) { RVInstallAuthentication();RVInstallMiniplayer();RVInstallSettingsBridge();RVObserveNetwork();RVInstallAds();RVInstallFeed();RVInstallPlayer();RVInstallExtras(); }
        dispatch_async(dispatch_get_main_queue(),^{
            static RVSettingsEntrance *entrance;entrance=[RVSettingsEntrance new];
            [[NSNotificationCenter defaultCenter] addObserver:entrance selector:@selector(attach) name:UIApplicationDidBecomeActiveNotification object:nil];
            [entrance attach];
        });
    }
}
