#import <Foundation/Foundation.h>
@class IOSObjectArray;
typedef id (^RVBRoleRead)(NSString *key,id fallback);
typedef id (^RVBOperation)(NSString *operation,IOSObjectArray *arguments);
typedef NSString *(^RVBResource)(NSString *key,IOSObjectArray *arguments);
// Configure before the first source class is initialized; source statics live for the process.
void RVBConfigure(NSUserDefaults *preferences,RVBRoleRead read,RVBOperation invoke,RVBResource resources);
BOOL RVBReady(void);
