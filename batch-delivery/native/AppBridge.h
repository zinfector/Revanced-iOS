#import <Foundation/Foundation.h>
// Application boundary. Feature algorithms remain in the generated Java code.
typedef id (^RVBAppRead)(NSString *key);
typedef id (^RVBAppInvoke)(NSString *operation,NSArray *arguments);
typedef NSString *(^RVBAppResource)(NSString *key,NSArray *arguments);
void RVBAppConfigure(NSUserDefaults *preferences,RVBAppRead read,RVBAppInvoke invoke,RVBAppResource resource,id(^presenter)(void),id(^overlay)(void));
void RVBAppSetPreference(NSString *key,NSString *type,id value);
void RVBAppPlayerState(NSString *state,BOOL shorts,BOOL controlsVisible);
void RVBAppBeginVideo(NSString *video);
void RVBAppTick(int64_t millis);
void RVBAppResetSponsor(NSString *video);
BOOL RVBAppPolicy(NSString *kind,BOOL original);
BOOL RVBAppFilter(NSString *family,NSString *semanticIdentifier);
void RVBAppSelectRate(float rate);
float RVBAppRateOverride(void);
void RVBAppShowSpeedMenu(void);
NSDictionary *RVBAppVotes(NSString *video);
void RVBAppWatchVotes(NSString *video,void(^completion)(NSDictionary *votes));
NSString *RVBAppVoteText(NSString *video,NSString *original);
NSString *RVBAppCountText(int64_t count);
void RVBAppVote(int32_t vote);
NSArray *RVBAppSegments(void);
NSString *RVBAppError(void);
