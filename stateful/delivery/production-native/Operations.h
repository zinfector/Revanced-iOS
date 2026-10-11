#pragma once
#import <Foundation/Foundation.h>
// One operation provider configuration for every generated patch, not a policy.
typedef void (^RVSTToastPresenter)(NSString *message);
void RVSTConfigureOperations(NSUserDefaults *preferences, RVSTToastPresenter toast);
void RVSTRequireEventContext(void);
BOOL RVSTOperationsReady(void);
