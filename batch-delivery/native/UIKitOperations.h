#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
@class IOSObjectArray;
#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
void RVBConfigureUIKit(UIViewController *(^presenter)(void),UIView *(^overlay)(void));
#endif
id RVBUIKit(NSString *operation,IOSObjectArray *arguments,BOOL *handled);
