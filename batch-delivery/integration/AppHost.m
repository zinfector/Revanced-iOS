// Fresh capability host. No legacy RVPort implementation is linked.
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <Security/Security.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import <Network/Network.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include "AppBridge.h"

static NSDictionary *catalog,*resources,*snapshot;
static NSUserDefaults *preferences;
static NSMutableDictionary *admissions,*syncValues;
static NSHashTable<UIView *> *voteViews;
static __weak id player,core,watch;
static NSString *video,*cpn,*sessionNonce;
static NSDictionary *votes;
static NSUInteger epoch;
static NSString *startedVideo,*startedCPN;
static __weak id startedPlayer;
static BOOL configured,networkOnline,applyingRate;
static NSArray *segments;
static const void *VoteLabelKey=&VoteLabelKey;
static const void *VisibilityKey=&VisibilityKey;
static NSString *const Prefix=@"RVBHost.0_4_7.";

static NSMethodSignature *signature(id target,NSString *selector){return [target methodSignatureForSelector:NSSelectorFromString(selector)];}
static NSString *shape(NSMethodSignature *s){if(!s)return nil;NSMutableString *r=[NSMutableString stringWithUTF8String:s.methodReturnType];for(NSUInteger n=0;n<s.numberOfArguments;n++)[r appendString:[NSString stringWithUTF8String:[s getArgumentTypeAtIndex:n]]];return r;}
static BOOL accepts(id target,NSString *selector,NSString *types){return [shape(signature(target,selector)) isEqual:types];}
static id object(id target,NSString *selector){return accepts(target,selector,@"@@:")?((id(*)(id,SEL))objc_msgSend)(target,NSSelectorFromString(selector)):nil;}
static BOOL boolean(id target,NSString *selector){return accepts(target,selector,@"B@:")?((BOOL(*)(id,SEL))objc_msgSend)(target,NSSelectorFromString(selector)):NO;}
static double scalar(id target,NSString *selector){NSMethodSignature *s=signature(target,selector);NSString *t=shape(s);if([t isEqual:@"d@:"])return ((double(*)(id,SEL))objc_msgSend)(target,NSSelectorFromString(selector));if([t isEqual:@"f@:"])return ((float(*)(id,SEL))objc_msgSend)(target,NSSelectorFromString(selector));return NAN;}
static id field(id target,NSString *owner,NSString *name,NSString *type){Class c=NSClassFromString(owner);if(!c||![target isKindOfClass:c])return nil;Ivar iv=class_getInstanceVariable(c,name.UTF8String);return iv&&[type isEqual:[NSString stringWithUTF8String:ivar_getTypeEncoding(iv)]]?object_getIvar(target,iv):nil;}
static BOOL validVideo(id v){if(![v isKindOfClass:NSString.class]||[v length]!=11)return NO;return [v rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"].invertedSet].location==NSNotFound;}
static id setting(NSString *key){return [NSUserDefaults.standardUserDefaults objectForKey:[Prefix stringByAppendingString:key]]?:catalog[@"defaults"][key];}
static BOOL enabled(NSString *key){return [setting(key) boolValue];}
static UIViewController *presenter(void){if([watch isKindOfClass:UIViewController.class]&&((UIViewController *)watch).viewIfLoaded.window)return watch;for(UIWindowScene *scene in UIApplication.sharedApplication.connectedScenes)if([scene isKindOfClass:UIWindowScene.class])for(UIWindow *w in scene.windows)if(w.isKeyWindow)return w.rootViewController;return nil;}
static UIView *overlay(void){id v=object(object(player,@"playerView"),@"overlayView");return [v isKindOfClass:UIView.class]?v:nil;}
static BOOL owned(void){return NSThread.isMainThread&&player&&core&&field(player,@"YTPlayerViewController",@"_playbackController",@"@\"YTLocalPlaybackController\"")==core&&[object(core,@"contentVideoID") isEqual:video];}
static BOOL ordinary(void){if(!owned()||!accepts(core,@"isPlayingAd",@"B@:")||boolean(core,@"isPlayingAd"))return NO;id response=object(object(object(core,@"contentPlaybackData"),@"playerResponse"),@"playerData");return accepts(response,@"isLivePlayback",@"B@:")&&!boolean(response,@"isLivePlayback");}
static id findWatch(id candidate){NSMutableArray *nodes=[NSMutableArray arrayWithObject:candidate];NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];for(NSUInteger i=0;i<nodes.count&&i<32;i++){id n=nodes[i];if([seen containsObject:n])continue;[seen addObject:n];NSString *name=NSStringFromClass([n class]);if([name hasPrefix:@"YTReel"]||[name containsString:@"InlineMuted"])return nil;if([n isKindOfClass:NSClassFromString(@"YTWatchViewController")]&&object(n,@"playerViewController")==candidate)return n;for(NSString *edge in @[@"parentViewController",@"parentResponder",@"eventsDelegate",@"UIDelegate",@"watchViewController"]){id child=object(n,edge);if(child&&![seen containsObject:child]&&nodes.count<32)[nodes addObject:child];}}return nil;}
static BOOL seek(double millis){if(!ordinary()||!isfinite(millis)||millis<0)return NO;double duration=scalar(player,@"currentVideoTotalMediaTime");if(!isfinite(duration)||duration<=0||millis>duration*1000)return NO;Class t=NSClassFromString(@"YTSingleVideoTime");NSString *sel=@"seekToTime:toleranceBefore:toleranceAfter:seekSource:";if(!accepts(t,@"timeWithTime:CPN:",@"@@:d@")||!accepts(core,sel,@"v@:@ddi"))return NO;NSString *currentCPN=object(object(core,@"contentVideoCurrentTime"),@"CPN");id time=((id(*)(id,SEL,double,id))objc_msgSend)(t,NSSelectorFromString(@"timeWithTime:CPN:"),millis/1000,currentCPN);if(!time)return NO;((void(*)(id,SEL,id,double,double,int))objc_msgSend)(core,NSSelectorFromString(sel),time,0,0,0);return YES;}
static void refreshVotes(void);
static void applyViewEffects(void){if(!configured||!owned())return;UIView *root=overlay();if(!root)return;NSMutableArray *pending=[NSMutableArray arrayWithObject:root];for(NSUInteger i=0;i<pending.count&&i<512;i++){UIView *v=pending[i];NSString *key=catalog[@"view_effects"][NSStringFromClass([v class])];if(key){BOOL hide=decision(key,NO);NSNumber *baseline=objc_getAssociatedObject(v,VisibilityKey);if(hide){if(!baseline)objc_setAssociatedObject(v,VisibilityKey,@(v.hidden),OBJC_ASSOCIATION_RETAIN_NONATOMIC);v.hidden=YES;}else if(baseline){v.hidden=baseline.boolValue;objc_setAssociatedObject(v,VisibilityKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);}}for(UIView *child in v.subviews)if(pending.count<512)[pending addObject:child];}}
static void synchronizeSettings(void){
 for(NSDictionary *entry in catalog[@"settings"]){NSString *native=entry[@"native"],*key=entry[@"source"],*type=entry[@"type"];id value=setting(native);if(!value)continue;if([type isEqual:@"speeds"]){value=[value componentsJoinedByString:@"\n"];type=@"string";}if([syncValues[key] isEqual:value])continue;syncValues[key]=value;RVBAppSetPreference(key,type,value);}
 id selected=setting(@"sponsor_categories"),behaviours=setting(@"sponsor_behaviors");for(NSDictionary *category in catalog[@"categories"]){NSString *name=category[@"native"],*key=category[@"source"];NSString *value=[selected containsObject:name]?(behaviours[name]?:@"skip"):@"ignore";NSDictionary *convert=@{@"skip_once":@"skip-once",@"manual":@"manual-skip",@"seekbar":@"seekbar-only"};value=convert[value]?:value;if(![syncValues[key] isEqual:value]){syncValues[key]=value;RVBAppSetPreference(key,@"string",value);}}
}
static id invoke(NSString *op,NSArray *args){
 if([op isEqual:@"network.connected"]){@synchronized(preferences){return @(networkOnline);}}
 if([op isEqual:@"player.seek_ms"]){__block BOOL result=NO;void(^action)(void)=^{result=seek([args[0] doubleValue]);};if(NSThread.isMainThread)action();else dispatch_sync(dispatch_get_main_queue(),action);return @(result);}
 if([op isEqual:@"player.set_rate"]){float rate=[args[0] floatValue];if(!NSThread.isMainThread||!ordinary()||!isfinite(rate)||rate<.25||rate>4||!accepts(player,@"setPlaybackRate:",@"v@:f"))return nil;applyingRate=YES;@try{((void(*)(id,SEL,float))objc_msgSend)(player,NSSelectorFromString(@"setPlaybackRate:"),rate);}@finally{applyingRate=NO;}NSMutableDictionary *next=[snapshot mutableCopy];next[@"player.rate"]=@(rate);@synchronized(preferences){snapshot=next.copy;}return nil;}
 if([op isEqual:@"resource.integer"]&&[args[0] isEqual:@"fade_duration_fast"])return @200;
 return NSNull.null;
}
static NSString *resource(NSString *key,NSArray *args){NSMutableString *s=[(resources[key]?:key) mutableCopy];for(NSUInteger i=0;i<args.count;i++){NSString *value=[args[i] description];for(NSString *format in @[@"s",@"d",@"f"]){NSString *token=[NSString stringWithFormat:@"%%%lu$%@",(unsigned long)i+1,format];[s replaceOccurrencesOfString:token withString:value options:0 range:NSMakeRange(0,s.length)];}NSRange r=[s rangeOfString:@"%s"];if(r.location==NSNotFound)r=[s rangeOfString:@"%d"];if(r.location!=NSNotFound)[s replaceCharactersInRange:r withString:value];}return s;}
static BOOL decision(NSString *key,BOOL original){if(!configured||!enabled(key))return original;return RVBAppPolicy(key,original);}

// Runtime ABI admission compares types, not stale addresses or cross-platform offsets.
static BOOL hook(NSDictionary *entry,id(^make)(IMP,SEL)){
 Class cls=NSClassFromString(entry[@"class"]);if([entry[@"kind"] isEqual:@"+"])cls=object_getClass(cls);SEL sel=NSSelectorFromString(entry[@"selector"]);Method m=cls?class_getInstanceMethod(cls,sel):NULL;NSString *key=[NSString stringWithFormat:@"%@ %@",entry[@"class"],entry[@"selector"]];
 if(!m||![shape([NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(m)]) isEqual:entry[@"types"]]){admissions[key]=@"signature_mismatch_or_unavailable";return NO;}
 IMP original=method_getImplementation(m);id block=make(original,sel);if(!block){admissions[key]=@"unsupported_adapter_shape";return NO;}IMP replacement=imp_implementationWithBlock(block);if(!class_addMethod(cls,sel,replacement,method_getTypeEncoding(m)))class_replaceMethod(cls,sel,replacement,method_getTypeEncoding(m));admissions[key]=@"installed";return YES;
}
static id filterModel(id model,NSUInteger depth,NSHashTable *seen){
 if(!configured||!model||depth>8||seen.count>2048||[seen containsObject:model])return model;
 [seen addObject:model];NSDictionary *descriptor=catalog[@"messages"][NSStringFromClass([model class])];if(!descriptor)return model;
 for(NSDictionary *rule in descriptor[@"classify"]){NSString *presence=rule[@"presence"];if(boolean(model,presence)&&enabled(rule[@"feature"])&&RVBAppFilter(rule[@"family"],rule[@"semantic"]))return NSNull.null;}
 id copy=nil;for(NSDictionary *edge in descriptor[@"children"]){NSString *getter=edge[@"getter"],*setter=edge[@"setter"];id child=object(model,getter);if(!child||!accepts(model,setter,@"v@:@"))continue;id changed=child;
  if([edge[@"array"] boolValue]&&[child isKindOfClass:NSArray.class]&&[child count]<=4096){NSMutableArray *a=[NSMutableArray array];BOOL different=NO;for(id item in child){id mapped=filterModel(item,depth+1,seen);different|=mapped!=item;if(mapped!=NSNull.null)[a addObject:mapped];}if(different)changed=a;}
  else changed=filterModel(child,depth+1,seen);
  if(changed!=child&&[model conformsToProtocol:@protocol(NSCopying)]){if(!copy)copy=[model copy];((void(*)(id,SEL,id))objc_msgSend)(copy,NSSelectorFromString(setter),changed==NSNull.null?nil:changed);}
 }return copy?:model;
}

static void observeBinding(id candidate,id time){
 if(!NSThread.isMainThread){__weak id weak=candidate;dispatch_async(dispatch_get_main_queue(),^{observeBinding(weak,time);});return;}
 if(!configured||![candidate isKindOfClass:NSClassFromString(@"YTPlayerViewController")])return;
 id controller=field(candidate,@"YTPlayerViewController",@"_playbackController",@"@\"YTLocalPlaybackController\"");
 if(!controller||field(controller,@"YTLocalPlaybackController",@"_delegate",@"@\"<YTCorePlaybackControllerDelegate>\"")!=candidate)return;
 id owner=findWatch(candidate);if(!owner)return;
 NSString *nextVideo=object(controller,@"contentVideoID");if(!validVideo(nextVideo))return;
 id contentTime=object(controller,@"contentVideoCurrentTime");NSString *nextCPN=object(contentTime,@"CPN")?:object(controller,@"contentVideoCPN");NSString *eventCPN=object(time,@"CPN");if(eventCPN&&nextCPN&&![eventCPN isEqual:nextCPN])return;
 BOOL changed=startedPlayer!=candidate||![startedVideo isEqual:nextVideo]||![(startedCPN?:@"") isEqual:(nextCPN?:@"")];
 player=candidate;core=controller;watch=owner;video=nextVideo;cpn=nextCPN;
 synchronizeSettings();double position=scalar(time,@"time");if(!isfinite(position))position=scalar(contentTime,@"time");double duration=scalar(candidate,@"currentVideoTotalMediaTime"),rate=scalar(object(candidate,@"activePlaybackRateModel"),@"activeRate");
 @synchronized(preferences){snapshot=@{@"player.video_id":video,@"player.position_ms":@(isfinite(position)?MAX(0,position*1000):0),@"player.duration_ms":@(isfinite(duration)?MAX(0,duration*1000):0),@"player.rate":@(isfinite(rate)&&rate>0?rate:1),@"player.is_short":@NO,@"navigation.selected":@"HOME",@"navigation.search_active":@NO,@"engagement.description":@NO};}
 NSString *state=boolean(candidate,@"isPictureInPictureActive")?@"WATCH_WHILE_PICTURE_IN_PICTURE":boolean(owner,@"isFullscreen")?@"WATCH_WHILE_FULLSCREEN":@"WATCH_WHILE_MAXIMIZED";
 RVBAppPlayerState(state,NO,overlay().window!=nil);
 if(!ordinary()){RVBAppPlayerState(@"NONE",NO,NO);return;}
 if(changed){startedPlayer=candidate;startedVideo=video;startedCPN=cpn;epoch++;votes=nil;segments=@[];refreshVotes();RVBAppBeginVideo(video);NSString *requested=video;NSUInteger generation=epoch;if(enabled(@"return_dislikes"))RVBAppWatchVotes(video,^(NSDictionary *result){if(epoch==generation&&[video isEqual:requested]&&owned()){votes=result;refreshVotes();}});float restored=RVBAppRateOverride();if(restored>0)invoke(@"player.set_rate",@[@(restored)]);}
 if(isfinite(position)&&position>=0)RVBAppTick((int64_t)(position*1000));
 segments=RVBAppSegments();NSDictionary *updated=enabled(@"return_dislikes")?RVBAppVotes(video):nil;if(updated)votes=updated;refreshVotes();
 applyViewEffects();
}
static NSString *viewVoteRole(UIView *view){if(![view isKindOfClass:NSClassFromString(@"YTSlimVideoDetailsActionView")])return nil;id delegate=object(view,@"delegate");if(![delegate isKindOfClass:NSClassFromString(@"YTSlimVideoScrollableActionBarCellController")])return nil;NSString *owner=object(object(delegate,@"delegate"),@"videoId");if(![owner isEqual:video])return nil;NSMutableArray *pending=[NSMutableArray arrayWithObject:view];for(NSUInteger i=0;i<pending.count&&i<128;i++){UIView *v=pending[i];NSString *identifier=v.accessibilityIdentifier;if([identifier isEqual:@"id.video.like.button"])return @"likes";if([identifier isEqual:@"id.video.dislike.button"])return @"dislikes";for(UIView *child in v.subviews)if(pending.count<128)[pending addObject:child];}return nil;}
static void updateVote(UIView *v){UILabel *label=objc_getAssociatedObject(v,VoteLabelKey);NSString *role=viewVoteRole(v);NSNumber *count=role?votes[role]:nil;BOOL visible=enabled(@"return_dislikes")&&owned()&&ordinary()&&count&&v.window;if(!visible){label.hidden=YES;return;}NSString *text=RVBAppCountText(count.longLongValue);if(!text){label.hidden=YES;return;}if(!label){label=[UILabel new];label.userInteractionEnabled=NO;label.font=[UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];label.textColor=UIColor.labelColor;label.backgroundColor=UIColor.secondarySystemBackgroundColor;label.layer.cornerRadius=4;label.clipsToBounds=YES;label.textAlignment=NSTextAlignmentCenter;label.accessibilityIdentifier=[@"rvb.translated." stringByAppendingString:role];[v addSubview:label];objc_setAssociatedObject(v,VoteLabelKey,label,OBJC_ASSOCIATION_RETAIN_NONATOMIC);}label.text=text;label.accessibilityLabel=[NSString stringWithFormat:@"%@ estimated %@",count,role];CGSize size=[label sizeThatFits:CGSizeMake(240,24)];CGFloat width=MIN(MAX(24,size.width+8),MAX(24,v.bounds.size.width));label.frame=CGRectMake(MAX(0,v.bounds.size.width-width-4),MAX(0,v.bounds.size.height-20),width,18);label.hidden=NO;}
static void refreshVotes(void){for(UIView *v in voteViews)updateVote(v);}

static NSDictionary *report(void){NSDictionary *s;@synchronized(preferences){s=snapshot?:@{};}return @{@"schema":@1,@"version":@"0.4.7-translated",@"session":sessionNonce,@"native_feature_code_reused":@NO,@"snapshot":s,@"owner_bound":@(player&&core&&watch),@"epoch":@(epoch),@"votes":votes?:@{},@"segments":segments?:@[],@"admissions":admissions.copy?:@{},@"source_error":RVBAppError()?:@"",@"device_verified":@NO};}
static void diagnosticsServer(void){dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return;int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));struct sockaddr_in addr={0};addr.sin_len=sizeof(addr);addr.sin_family=AF_INET;addr.sin_port=htons(6179);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(bind(fd,(struct sockaddr *)&addr,sizeof(addr))||listen(fd,4)){close(fd);return;}for(;;){int client=accept(fd,NULL,NULL);if(client<0)break;struct timeval timeout={2,0};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));char input[8193]={0};ssize_t n=recv(client,input,8192,0);NSString *request=n>0?[[NSString alloc] initWithBytes:input length:n encoding:NSUTF8StringEncoding]:@"";NSString *token=[NSUserDefaults.standardUserDefaults stringForKey:@"RVBHost.debuggerToken"];BOOL allowed=enabled(@"diagnostics")&&[request hasPrefix:@"GET /state HTTP/1."]&&[request containsString:[NSString stringWithFormat:@"Authorization: Bearer %@\r\n",token]];__block NSData *body;if(allowed)dispatch_sync(dispatch_get_main_queue(),^{body=[NSJSONSerialization dataWithJSONObject:report() options:0 error:NULL];});else body=[@"{\"error\":\"unauthorized\"}" dataUsingEncoding:NSUTF8StringEncoding];NSMutableData *response=[[[NSString stringWithFormat:@"HTTP/1.1 %@\r\nContent-Type: application/json\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",allowed?@"200 OK":@"401 Unauthorized",(unsigned long)body.length] dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];[response appendData:body];const uint8_t *bytes=response.bytes;NSUInteger remaining=response.length;while(remaining){ssize_t sent=send(client,bytes,remaining,0);if(sent<=0)break;bytes+=sent;remaining-=sent;}close(client);}close(fd);});}

@interface RVBSettings : UITableViewController
@end
@implementation RVBSettings
-(void)viewDidLoad{[super viewDidLoad];self.title=@"Translated ReVanced";self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done)];}
-(void)done{[self dismissViewControllerAnimated:YES completion:nil];}
-(NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s{return [catalog[@"menu"] count]+4;}
-(UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)index{UITableViewCell *cell=[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];NSArray *menu=catalog[@"menu"];if(index.row<(NSInteger)menu.count){NSDictionary *entry=menu[index.row];cell.textLabel.text=entry[@"title"];UISwitch *control=[UISwitch new];control.accessibilityIdentifier=entry[@"key"];control.on=enabled(entry[@"key"]);[control addTarget:self action:@selector(toggle:) forControlEvents:UIControlEventValueChanged];cell.accessoryView=control;}else{NSArray *titles=@[@"Playback speed",@"Start Picture in Picture",@"Copy diagnostics",@"Copy USB debugger token"];cell.textLabel.text=titles[index.row-menu.count];if(index.row==(NSInteger)menu.count+3)cell.detailTextLabel.text=@"Loopback port 6179 · GET /state";}return cell;}
-(void)toggle:(UISwitch *)control{[NSUserDefaults.standardUserDefaults setBool:control.on forKey:[Prefix stringByAppendingString:control.accessibilityIdentifier]];synchronizeSettings();if([control.accessibilityIdentifier isEqual:@"sponsorblock"]&&owned())RVBAppResetSponsor(video);refreshVotes();applyViewEffects();}
-(void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)index{NSInteger action=index.row-[catalog[@"menu"] count];if(action==0){[self dismissViewControllerAnimated:YES completion:^{if(ordinary())RVBAppShowSpeedMenu();}];}else if(action==1){[self dismissViewControllerAnimated:YES completion:^{if(enabled(@"picture_in_picture")&&owned()&&boolean(player,@"isPictureInPicturePossible")&&accepts(player,@"switchToPip",@"v@:"))((void(*)(id,SEL))objc_msgSend)(player,NSSelectorFromString(@"switchToPip"));}];}else if(action==2){NSData *data=[NSJSONSerialization dataWithJSONObject:report() options:NSJSONWritingPrettyPrinted error:NULL];UIPasteboard.generalPasteboard.string=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];}else if(action==3)UIPasteboard.generalPasteboard.string=[NSUserDefaults.standardUserDefaults stringForKey:@"RVBHost.debuggerToken"];}
@end
@interface RVBEntrance : NSObject
-(void)attach;
-(void)open:(UITapGestureRecognizer *)gesture;
@end
@implementation RVBEntrance
-(void)attach{static const void *key=&key;for(UIWindowScene *scene in UIApplication.sharedApplication.connectedScenes)if([scene isKindOfClass:UIWindowScene.class])for(UIWindow *w in scene.windows)if(!objc_getAssociatedObject(w,key)){UITapGestureRecognizer *tap=[[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(open:)];tap.numberOfTouchesRequired=3;tap.numberOfTapsRequired=2;tap.cancelsTouchesInView=NO;[w addGestureRecognizer:tap];objc_setAssociatedObject(w,key,tap,OBJC_ASSOCIATION_RETAIN_NONATOMIC);}}
-(void)open:(UITapGestureRecognizer *)gesture{UIViewController *p=presenter();while(p.presentedViewController)p=p.presentedViewController;if(p)[p presentViewController:[[UINavigationController alloc] initWithRootViewController:[[RVBSettings alloc] initWithStyle:UITableViewStyleInsetGrouped]] animated:YES completion:nil];}
@end

static id makeHook(NSDictionary *e,IMP original,SEL sel){NSString *driver=e[@"driver"],*types=e[@"types"],*feature=e[@"feature"];
 if([driver isEqual:@"bool"]&&[types isEqual:@"B@:"])return ^BOOL(id target){BOOL value=((BOOL(*)(id,SEL))original)(target,sel);if(!configured||!enabled(feature))return value;if([feature isEqual:@"picture_in_picture"])return YES;BOOL active=decision(feature,NO);return [feature isEqual:@"background_playback"]?RVBAppPolicy(feature,value):active?[e[@"value"] boolValue]:value;};
 if([driver isEqual:@"ad_array"]&&[types isEqual:@"@@:"])return ^id(id target){id value=((id(*)(id,SEL))original)(target,sel);return decision(@"video_ads",NO)?[NSMutableArray array]:value;};
 if([driver isEqual:@"ad_count"]&&[types isEqual:@"Q@:"])return ^uint64_t(id target){return decision(@"video_ads",NO)?0:((uint64_t(*)(id,SEL))original)(target,sel);};
 if([driver isEqual:@"clock"]&&[types isEqual:@"v@:@@"])return ^(id target,id first,id time){((void(*)(id,SEL,id,id))original)(target,sel,first,time);observeBinding(target,time);};
 if([driver isEqual:@"loaded"]&&[types isEqual:@"v@:@@"])return ^(id target,id controller,id data){((void(*)(id,SEL,id,id))original)(target,sel,controller,data);observeBinding(target,object(controller,@"contentVideoCurrentTime"));};
 if([driver isEqual:@"rate"]&&[types isEqual:@"v@:f"])return ^(id target,float value){((void(*)(id,SEL,float))original)(target,sel,value);if(configured&&target==player&&!applyingRate&&ordinary()&&isfinite(value)&&value>=.25&&value<=4)RVBAppSelectRate(value);};
 if([driver isEqual:@"filter"]&&[types isEqual:@"v@:@"])return ^(id target,id model){id mapped=filterModel(model,0,[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality]);((void(*)(id,SEL,id))original)(target,sel,mapped==NSNull.null?nil:mapped);};
 if([driver isEqual:@"layout"]&&[types isEqual:@"v@:"])return ^(UIView *target){((void(*)(id,SEL))original)(target,sel);[voteViews addObject:target];updateVote(target);};
 if([driver isEqual:@"dislike_text"]&&[types isEqual:@"@@:"])return ^id(id target){id value=((id(*)(id,SEL))original)(target,sel);if(configured&&enabled(@"return_dislikes")&&[object(target,@"videoID") isEqual:video]&&votes[@"dislikes"])return RVBAppCountText([votes[@"dislikes"] longLongValue])?:value;return value;};
 if([driver isEqual:@"speed_menu"]&&[types isEqual:@"v@:@"])return ^(id target,id sender){if(configured&&enabled(@"custom_speed_menu")&&ordinary()){RVBAppShowSpeedMenu();return;}((void(*)(id,SEL,id))original)(target,sel,sender);};
 if([driver isEqual:@"identity"]&&[types isEqual:@"@@:"])return ^id(id target){return e[@"value"]==NSNull.null?nil:e[@"value"];};
 if([driver isEqual:@"identity_bool"]&&[types isEqual:@"B@:"])return ^BOOL(id target){return [e[@"value"] boolValue];};
 if([driver isEqual:@"keychain_query"]&&[types isEqual:@"@@:@@"])return ^id(id target,id a,id b){id result=((id(*)(id,SEL,id,id))original)(target,sel,a,b);if(![result isKindOfClass:NSDictionary.class])return result;NSMutableDictionary *safe=[result mutableCopy];[safe removeObjectForKey:(__bridge id)kSecAttrAccessGroup];return safe;};
 return nil;
}
static BOOL matchesImage(void){const struct mach_header_64 *header=(const void *)_dyld_get_image_header(0);if(!header||header->magic!=MH_MAGIC_64)return NO;const uint8_t *p=(const uint8_t *)(header+1);for(uint32_t i=0;i<header->ncmds;i++){const struct load_command *command=(const void *)p;if(command->cmdsize<8)return NO;if(command->cmd==LC_UUID){const struct uuid_command *u=(const void *)p;NSMutableString *value=[NSMutableString string];for(int n=0;n<16;n++)[value appendFormat:@"%02x",u->uuid[n]];return [value isEqual:catalog[@"app_uuid"]];}p+=command->cmdsize;}return NO;}
__attribute__((constructor)) static void start(void){@autoreleasepool{NSString *path=[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"RVBIntegration.json"];catalog=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:path]?:[NSData data] options:0 error:NULL];if(![catalog[@"app_version"] isEqual:[NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]]||!matchesImage())return;admissions=[NSMutableDictionary dictionary];syncValues=[NSMutableDictionary dictionary];voteViews=[NSHashTable weakObjectsHashTable];sessionNonce=NSUUID.UUID.UUIDString;
 // The signing identity remains owned by SideStore. No credentials are read by diagnostics.
 if(![NSUserDefaults.standardUserDefaults stringForKey:@"RVBHost.debuggerToken"])[NSUserDefaults.standardUserDefaults setObject:NSUUID.UUID.UUIDString forKey:@"RVBHost.debuggerToken"];
 for(NSDictionary *e in catalog[@"hooks"])hook(e,^id(IMP original,SEL sel){return makeHook(e,original,sel);});
 dispatch_async(dispatch_get_main_queue(),^{preferences=[[NSUserDefaults alloc] initWithSuiteName:@"org.revanced.translated.batch0_4_7"];resources=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"RVBResources.json"]] options:0 error:NULL]?:@{};
  RVBAppConfigure(preferences,^id(NSString *key){@synchronized(preferences){return snapshot[key];}},^id(NSString *op,NSArray *args){return invoke(op,args);},^NSString *(NSString *key,NSArray *args){return resource(key,args);},^id{return presenter();},^id{return overlay();});synchronizeSettings();configured=YES;
  nw_path_monitor_t monitor=nw_path_monitor_create();nw_path_monitor_set_update_handler(monitor,^(nw_path_t p){@synchronized(preferences){networkOnline=nw_path_get_status(p)==nw_path_status_satisfied;}});nw_path_monitor_set_queue(monitor,dispatch_get_global_queue(QOS_CLASS_UTILITY,0));nw_path_monitor_start(monitor);objc_setAssociatedObject(preferences,@selector(start),monitor,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  static RVBEntrance *entrance;entrance=[RVBEntrance new];[[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note){[entrance attach];}];[entrance attach];diagnosticsServer();
 });}}
