#include "Operations.h"
#include "jni.h"
#include "java/lang/Runnable.h"
#include "java/lang/IllegalStateException.h"
#include <math.h>
#include <string.h>
#include <dispatch/dispatch.h>

static NSUserDefaults *preferences;
static RVSTToastPresenter toastPresenter;
static void fail(NSString *message) { @throw [[JavaLangIllegalStateException alloc] initWithNSString:message]; }
void RVSTConfigureOperations(NSUserDefaults *domain, RVSTToastPresenter toast) {
    if (![NSThread isMainThread] || !domain || !toast) fail(@"Configure operations on main with a preference domain and toast presenter");
    if (preferences) fail(@"Operations already configured; source state is application lifetime");
    preferences = domain; toastPresenter = [toast copy];
}
BOOL RVSTOperationsReady(void) { return preferences != nil && toastPresenter != nil; }
void RVSTRequireEventContext(void) {
    if (!RVSTOperationsReady()) fail(@"Native operations are not configured");
    if (![NSThread isMainThread]) fail(@"Reviewed patch events require main-thread placement");
}
static void requirePreferences(void) { if (!preferences) fail(@"Preference provider is not configured"); }

JNIEXPORT jboolean JNICALL Java_rv_platform_NativeBridge_readBoolean(JNIEnv *env, jclass cls, jstring key, jboolean fallback) {
    requirePreferences(); id value = [preferences objectForKey:key];
    if (!value) return fallback;
    if (![value isKindOfClass:[NSDictionary class]] || ![value[@"type"] isEqual:@"bool"] || ![value[@"value"] isKindOfClass:[NSNumber class]]) fail(@"Boolean preference schema mismatch");
    return [value[@"value"] boolValue];
}
JNIEXPORT jfloat JNICALL Java_rv_platform_NativeBridge_readFloat(JNIEnv *env, jclass cls, jstring key, jfloat fallback) {
    requirePreferences(); id value = [preferences objectForKey:key];
    if (!value) return fallback;
    if (![value isKindOfClass:[NSDictionary class]] || ![value[@"type"] isEqual:@"f32"] || ![value[@"bits"] isKindOfClass:[NSNumber class]]) fail(@"Float preference schema mismatch");
    uint32_t bits = [value[@"bits"] unsignedIntValue]; jfloat result; memcpy(&result, &bits, sizeof result); return result;
}
JNIEXPORT void JNICALL Java_rv_platform_NativeBridge_writeFloat(JNIEnv *env, jclass cls, jstring key, jfloat value) {
    requirePreferences(); uint32_t bits; memcpy(&bits, &value, sizeof bits);
    [preferences setObject:@{@"type":@"f32", @"bits":@(bits)} forKey:key];
}
JNIEXPORT jlong JNICALL Java_rv_platform_NativeBridge_wallMillis(JNIEnv *env, jclass cls) {
    return (jlong)floor([NSDate timeIntervalSinceReferenceDate] * 1000.0 + 978307200000.0);
}
JNIEXPORT void JNICALL Java_rv_platform_NativeBridge_postMain(JNIEnv *env, jclass cls, jobject runnable, jlong millis) {
    if (!runnable) fail(@"Null queued callback");
    // Android Handler treats a negative delay as zero. Keep enqueue behavior.
    int64_t nanos = millis <= 0 ? 0 : (millis > INT64_MAX / NSEC_PER_MSEC ? INT64_MAX : millis * NSEC_PER_MSEC);
    id<JavaLangRunnable> retained = runnable;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, nanos), dispatch_get_main_queue(), ^{ [retained run]; });
}
JNIEXPORT void JNICALL Java_rv_platform_NativeBridge_showToast(JNIEnv *env, jclass cls, jstring text) {
    RVSTRequireEventContext(); toastPresenter(text);
}
JNIEXPORT jboolean JNICALL Java_rv_platform_NativeBridge_debugEnabled(JNIEnv *env, jclass cls) { return NO; }
JNIEXPORT void JNICALL Java_rv_platform_NativeBridge_log(JNIEnv *env, jclass cls, jstring text) { NSLog(@"[RVST] %@", text); }
