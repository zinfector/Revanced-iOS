#import "AppBridge.h"
#import "Operations.h"
#import "UIKitOperations.h"
#include "IOSClass.h"
#include "IOSObjectArray.h"
#include "IOSPrimitiveArray.h"
#include "NSString+JavaString.h"
#include "java/lang/Boolean.h"
#include "java/lang/Integer.h"
#include "java/lang/Long.h"
#include "java/lang/Float.h"
#include "java/lang/Double.h"
#include "java/lang/UnsupportedOperationException.h"
#include "java/lang/reflect/Field.h"
#include "java/lang/reflect/Method.h"
#include "NSObject+JavaObject.h"
#include "java/lang/CharSequence.h"
#include "rv/batch/Events.h"
#include "app/revanced/extension/youtube/shared/PlayerControlsVisibility.h"
#include "app/revanced/extension/youtube/patches/DisablePlayerPopupPanelsPatch.h"
#include "app/revanced/extension/youtube/patches/DisableResumingStartupShortsPlayerPatch.h"
#include "app/revanced/extension/youtube/patches/HideAutoplayPreviewPatch.h"
#include "app/revanced/extension/youtube/returnyoutubedislike/ReturnYouTubeDislike.h"
#include "app/revanced/extension/youtube/returnyoutubedislike/requests/RYDVoteData.h"
#include "app/revanced/extension/youtube/sponsorblock/SegmentPlaybackController.h"
#include "app/revanced/extension/youtube/sponsorblock/objects/SponsorSegment.h"
#include "app/revanced/extension/youtube/sponsorblock/objects/SegmentCategory.h"
#include <string.h>

static NSUserDefaults *appPrefs;
static NSString *lastError;
static void failed(NSException *e){@synchronized(AppRevancedExtensionYoutubeSponsorblockSegmentPlaybackController.class){lastError=e.description;}NSLog(@"[TranslatedBatch] %@",e);}
NSString *RVBAppError(void){@synchronized(AppRevancedExtensionYoutubeSponsorblockSegmentPlaybackController.class){return lastError;}}
static id unbox(id v){if([v isKindOfClass:JavaLangBoolean.class])return @([v booleanValue]);if([v isKindOfClass:JavaLangInteger.class])return @([v intValue]);if([v isKindOfClass:JavaLangLong.class])return @([v longLongValue]);if([v isKindOfClass:JavaLangFloat.class])return @([v floatValue]);if([v isKindOfClass:JavaLangDouble.class])return @([v doubleValue]);return v?:NSNull.null;}
static NSArray *array(IOSObjectArray *a){NSMutableArray *v=[NSMutableArray array];for(int i=0;a&&i<a->size_;i++)[v addObject:unbox(IOSObjectArray_Get(a,i))];return v;}
static id box(id v,id fallback){
 if(!v||v==NSNull.null)return fallback;
 if([fallback isKindOfClass:JavaLangBoolean.class])return JavaLangBoolean_valueOfWithBoolean_([v boolValue]);
 if([fallback isKindOfClass:JavaLangInteger.class])return JavaLangInteger_valueOfWithInt_([v intValue]);
 if([fallback isKindOfClass:JavaLangLong.class])return JavaLangLong_valueOfWithLong_([v longLongValue]);
 if([fallback isKindOfClass:JavaLangFloat.class])return JavaLangFloat_valueOfWithFloat_([v floatValue]);
 if([fallback isKindOfClass:JavaLangDouble.class])return JavaLangDouble_valueOfWithDouble_([v doubleValue]);
 return v;
}
void RVBAppConfigure(NSUserDefaults *p,RVBAppRead read,RVBAppInvoke invoke,RVBAppResource resource,id(^presenter)(void),id(^overlay)(void)){
 appPrefs=p;
 #if TARGET_OS_IOS
 RVBConfigureUIKit(^UIViewController *{return presenter();},^UIView *{return overlay();});
 #endif
 RVBConfigure(p,^(NSString *k,id f){return box(read(k),f);},^(NSString *op,IOSObjectArray *args){
  id v=invoke(op,array(args));
  if([op isEqual:@"player.seek_ms"]||[op isEqual:@"network.connected"])return JavaLangBoolean_valueOfWithBoolean_([v boolValue]);
  if([op isEqual:@"resource.id"]||[op isEqual:@"resource.integer"])return JavaLangInteger_valueOfWithInt_([v intValue]);
  if(v==NSNull.null)@throw [[JavaLangUnsupportedOperationException alloc] initWithNSString:op];
  return v;
 },^(NSString *k,IOSObjectArray *args){return resource(k,array(args));});
}
void RVBAppSetPreference(NSString *key,NSString *type,id value){
 if([type isEqual:@"f32"]){float f=[value floatValue];uint32_t bits;memcpy(&bits,&f,4);value=@(bits);}
 if([type isEqual:@"f64"]){double d=[value doubleValue];uint64_t bits;memcpy(&bits,&d,8);value=@(bits);}
 [appPrefs setObject:@{@"type":type,@"value":value} forKey:key];
}
void RVBAppPlayerState(NSString *state,BOOL shorts,BOOL visible){@try{RVBEvents_playerStateWithNSString_withBoolean_(state,shorts);[AppRevancedExtensionYoutubeSharedPlayerControlsVisibility setFromStringWithNSString:visible?@"PLAYER_CONTROLS_VISIBILITY_SHOWN":@"PLAYER_CONTROLS_VISIBILITY_HIDDEN"];}@catch(NSException *e){failed(e);}}
void RVBAppBeginVideo(NSString *video){@try{RVBEvents_beginVideoWithNSString_(video);}@catch(NSException *e){failed(e);}}
void RVBAppTick(int64_t millis){@try{RVBEvents_timeChangedWithLong_(millis);}@catch(NSException *e){failed(e);}}
void RVBAppResetSponsor(NSString *video){@try{RVBFFE3ED3FSegmentPlaybackController_initialize__WithRVBC99F70F0VideoInformation_PlaybackController_(nil);[AppRevancedExtensionYoutubeSponsorblockSegmentPlaybackController setCurrentVideoIdWithNSString:video];}@catch(NSException *e){failed(e);}}
BOOL RVBAppPolicy(NSString *kind,BOOL original){@try{
 if([kind isEqual:@"video_ads"])return !RVBEvents_showVideoAds();
 if([kind isEqual:@"background_playback"])return RVBEvents_allowBackgroundWithBoolean_(original);
 if([kind isEqual:@"hide_info_cards"])return RVBEvents_hideInfoCards();
 if([kind isEqual:@"hide_end_cards"])return RVBEvents_hideEndScreenCards();
 if([kind isEqual:@"hide_autoplay_preview"])return [AppRevancedExtensionYoutubePatchesHideAutoplayPreviewPatch hideAutoplayPreview];
 if([kind isEqual:@"disable_shorts_resume"])return [AppRevancedExtensionYoutubePatchesDisableResumingStartupShortsPlayerPatch disableResumingStartupShortsPlayer];
 if([kind isEqual:@"disable_popup_panels"])return [AppRevancedExtensionYoutubePatchesDisablePlayerPopupPanelsPatch disablePlayerPopupPanels];
 return original;
 }@catch(NSException *e){failed(e);return original;}}
BOOL RVBAppFilter(NSString *family,NSString *identifier){@try{return RVBEvents_filterWithNSString_withNSString_withNSString_withNSString_withByteArray_(family,identifier,@"",identifier,[IOSByteArray arrayWithLength:0]);}@catch(NSException *e){failed(e);return NO;}}
void RVBAppSelectRate(float rate){@try{RVBEvents_rateSelectedWithFloat_(rate);}@catch(NSException *e){failed(e);}}
float RVBAppRateOverride(void){@try{return RVBEvents_rateOverride();}@catch(NSException *e){failed(e);return -2;}}
void RVBAppShowSpeedMenu(void){@try{RVBEvents_showSpeedMenu();}@catch(NSException *e){failed(e);}}
NSDictionary *RVBAppVotes(NSString *video){@try{
 AppRevancedExtensionYoutubeReturnyoutubedislikeReturnYouTubeDislike *fetch=[AppRevancedExtensionYoutubeReturnyoutubedislikeReturnYouTubeDislike getFetchForVideoIdWithNSString:video];
 if(![fetch fetchCompleted])return nil;
 AppRevancedExtensionYoutubeReturnyoutubedislikeRequestsRYDVoteData *data=[fetch getFetchDataWithLong:0];
 return data?@{@"likes":@([data getLikeCount]),@"dislikes":@([data getDislikeCount])}:nil;
 }@catch(NSException *e){failed(e);return nil;}}
NSString *RVBAppVoteText(NSString *video,NSString *original){@try{return [RVBEvents_voteTextWithNSString_withJavaLangCharSequence_withBoolean_(video,original,NO) description];}@catch(NSException *e){failed(e);return nil;}}
void RVBAppWatchVotes(NSString *video,void(^completion)(NSDictionary *votes)){
 dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{@autoreleasepool{NSDictionary *votes=nil;@try{AppRevancedExtensionYoutubeReturnyoutubedislikeRequestsRYDVoteData *d=[[AppRevancedExtensionYoutubeReturnyoutubedislikeReturnYouTubeDislike getFetchForVideoIdWithNSString:video] getFetchDataWithLong:10000];if(d)votes=@{@"likes":@([d getLikeCount]),@"dislikes":@([d getDislikeCount])};}@catch(NSException *e){failed(e);}dispatch_async(dispatch_get_main_queue(),^{completion(votes);});}});
}
void RVBAppVote(int32_t vote){@try{RVBEvents_voteWithInt_(vote);}@catch(NSException *e){failed(e);}}
NSArray *RVBAppSegments(void){@try{
 // Read-only metadata access, never binary offsets or a duplicate segment policy.
 JavaLangReflectField *field=[RVBFFE3ED3FSegmentPlaybackController_class_() getDeclaredField:@"segments"];
 [field setAccessibleWithBoolean:YES];IOSObjectArray *segments=[field getWithId:nil];NSMutableArray *out=[NSMutableArray array];
 for(int i=0;segments&&i<segments->size_;i++){AppRevancedExtensionYoutubeSponsorblockObjectsSponsorSegment *s=IOSObjectArray_Get(segments,i);[out addObject:@{@"start":@(s->start_/1000.0),@"end":@(s->end_/1000.0),@"category":s->category_->keyValue_,@"UUID":s->UUID_?:@"",@"action":[s->category_->keyValue_ isEqual:@"poi_highlight"]?@"poi":@"skip"}];}
 return out;
 }@catch(NSException *e){failed(e);return @[];}}
NSString *RVBAppCountText(int64_t count){@try{
 JavaLangReflectMethod *method=[RVBE62C953EReturnYouTubeDislike_class_() getDeclaredMethod:@"formatDislikeCount" parameterTypes:[IOSObjectArray arrayWithObjects:(id[]){[IOSClass longClass]} count:1 type:IOSClass_class_()]];
 [method setAccessibleWithBoolean:YES];return (NSString *)[method invokeWithId:nil withNSObjectArray:[IOSObjectArray arrayWithObjects:(id[]){JavaLangLong_valueOfWithLong_(count)} count:1 type:NSObject_class_()]];
 }@catch(NSException *e){failed(e);return nil;}}
