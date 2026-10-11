#import "Operations.h"
#import "HTTP.h"
#include "jni.h"
#include "IOSObjectArray.h"
#include "java/lang/Boolean.h"
#include "java/lang/Float.h"
#include "java/lang/Double.h"
#include "java/lang/Integer.h"
#include "java/lang/Long.h"
#include "java/lang/Runnable.h"
#include "java/lang/IllegalStateException.h"
#include "java/lang/UnsupportedOperationException.h"
#include "rv/batch/NativeOperations.h"
#include <dispatch/dispatch.h>
#include <string.h>

static NSUserDefaults *prefs;
static RVBRoleRead roleRead;
static RVBOperation operation;
static RVBResource resources;
static void fail(NSString *s){@throw [[JavaLangIllegalStateException alloc] initWithNSString:s];}
BOOL RVBReady(void){return prefs && operation && resources && roleRead;}
void RVBConfigure(NSUserDefaults *p,RVBRoleRead r,RVBOperation o,RVBResource s){
 if(!NSThread.isMainThread||!p||!r||!o||!s||RVBReady())fail(@"Configure the batch once on main, before source initialization");
 prefs=p;roleRead=[r copy];operation=[o copy];resources=[s copy];RVBNativeOperations_install();
}
static void ready(void){if(!RVBReady())fail(@"Batch operations are not configured");}
JNIEXPORT jobject JNICALL Java_rv_batch_NativeOperations_read(JNIEnv *env,jobject self,jstring key,jobject fallback){
 ready();if([key hasPrefix:@"player."]||[key hasPrefix:@"navigation."]||[key hasPrefix:@"engagement."])return roleRead(key,fallback);
 id v=[prefs objectForKey:key];if(!v)return fallback;
 if(![v isKindOfClass:NSDictionary.class])fail(@"Batch preference has no typed envelope");
 NSString *t=v[@"type"];id n=v[@"value"];
 if([fallback isKindOfClass:JavaLangBoolean.class]&&[t isEqual:@"bool"])return JavaLangBoolean_valueOfWithBoolean_([n boolValue]);
 if([fallback isKindOfClass:JavaLangInteger.class]&&[t isEqual:@"i32"])return JavaLangInteger_valueOfWithInt_([n intValue]);
 if([fallback isKindOfClass:JavaLangLong.class]&&[t isEqual:@"i64"])return JavaLangLong_valueOfWithLong_([n longLongValue]);
 if([fallback isKindOfClass:JavaLangFloat.class]&&[t isEqual:@"f32"]){uint32_t b=[n unsignedIntValue];float f;memcpy(&f,&b,4);return JavaLangFloat_valueOfWithFloat_(f);}
 if([fallback isKindOfClass:JavaLangDouble.class]&&[t isEqual:@"f64"]){uint64_t b=[n unsignedLongLongValue];double f;memcpy(&f,&b,8);return JavaLangDouble_valueOfWithDouble_(f);}
 if([fallback isKindOfClass:NSString.class]&&[t isEqual:@"string"]&&[n isKindOfClass:NSString.class])return n;
 fail([@"Preference type mismatch: " stringByAppendingString:key]);return nil;
}
JNIEXPORT void JNICALL Java_rv_batch_NativeOperations_write(JNIEnv *env,jobject self,jstring key,jobject value){
 ready();NSString *type;id n;
 if([value isKindOfClass:JavaLangBoolean.class]){type=@"bool";n=@([value booleanValue]);}
 else if([value isKindOfClass:JavaLangInteger.class]){type=@"i32";n=@([value intValue]);}
 else if([value isKindOfClass:JavaLangLong.class]){type=@"i64";n=@([value longLongValue]);}
 else if([value isKindOfClass:JavaLangFloat.class]){type=@"f32";float f=[value floatValue];uint32_t b;memcpy(&b,&f,4);n=@(b);}
 else if([value isKindOfClass:JavaLangDouble.class]){type=@"f64";double f=[value doubleValue];uint64_t b;memcpy(&b,&f,8);n=@(b);}
 else if([value isKindOfClass:NSString.class]){type=@"string";n=value;}
 else fail(@"Unsupported preference value");
 [prefs setObject:@{@"type":type,@"value":n} forKey:key];
}
JNIEXPORT jlong JNICALL Java_rv_batch_NativeOperations_wallMillis(JNIEnv *env,jobject self){return (jlong)(NSDate.date.timeIntervalSince1970*1000.0);}
JNIEXPORT void JNICALL Java_rv_batch_NativeOperations_post(JNIEnv *env,jobject self,jobject runnable,jlong delay,jboolean main){
 ready();if(!runnable)fail(@"Null scheduled callback");id<JavaLangRunnable> retained=runnable;
 int64_t ns=delay<=0?0:delay>INT64_MAX/NSEC_PER_MSEC?INT64_MAX:delay*NSEC_PER_MSEC;
 dispatch_queue_t q=main?dispatch_get_main_queue():dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0);
 dispatch_after(dispatch_time(DISPATCH_TIME_NOW,ns),q,^{@autoreleasepool{[retained run];}});
}
JNIEXPORT jstring JNICALL Java_rv_batch_NativeOperations_resource(JNIEnv *env,jobject self,jstring key,jobjectArray args){ready();return resources(key,args);}
JNIEXPORT jobject JNICALL Java_rv_batch_NativeOperations_invoke(JNIEnv *env,jobject self,jstring op,jobjectArray args){
 ready();if([op isEqual:@"thread.is_main"])return JavaLangBoolean_valueOfWithBoolean_(NSThread.isMainThread);
 if([op isEqual:@"log"]){NSLog(@"[RVB] %@",IOSObjectArray_Get(args,0));return nil;}
 BOOL handled=NO;id result=RVBHTTP(op,args,&handled);if(handled)return result;
 return operation(op,args);
}
