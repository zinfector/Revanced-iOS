// SPDX-License-Identifier: MIT
// Shared Objective-C ABI guards and class-local hook installation.
#include "RVAdaptive.inc"
static BOOL RVCan(id object, const char *name, NSString *signature) {
    if (!object) return NO;
    SEL selector=sel_registerName(name);
    if (![object respondsToSelector:selector]) return NO;
    return [RVSignature(class_getInstanceMethod(object_getClass(object),selector)) isEqualToString:signature];
}
static id RVObject(id object,const char *name) {
    if (!RVCan(object,name,@"@@:")) return nil;
    return ((id(*)(id,SEL))objc_msgSend)(object,sel_registerName(name));
}
static BOOL RVBool(id object,const char *name) {
    if (!RVCan(object,name,@"B@:")) return NO;
    return ((BOOL(*)(id,SEL))objc_msgSend)(object,sel_registerName(name));
}
static int RVInt(id object,const char *name) {
    if (!RVCan(object,name,@"i@:")) return 0;
    return ((int(*)(id,SEL))objc_msgSend)(object,sel_registerName(name));
}
static double RVDouble(id object,const char *name) {
    if (!RVCan(object,name,@"d@:")) return NAN;
    return ((double(*)(id,SEL))objc_msgSend)(object,sel_registerName(name));
}
static void RVSetObject(id object,const char *name,id value) {
    if (RVCan(object,name,@"v@:@")) ((void(*)(id,SEL,id))objc_msgSend)(object,sel_registerName(name),value);
}

typedef id (^RVReplacementFactory)(IMP,SEL);
static BOOL RVHook(NSString *className,NSString *selectorName,NSString *signature,BOOL classMethod,RVReplacementFactory factory) {
    if (!RVAdaptivePermit(className,selectorName,signature,classMethod)) return NO;
    Class cls=NSClassFromString(className);
    if (!cls) { RVLog([@"Missing class: " stringByAppendingString:className]);return NO; }
    SEL selector=NSSelectorFromString(selectorName);
    Class target=classMethod ? object_getClass(cls) : cls;
    Method method=RVAdaptiveMethod(className,selectorName,classMethod);
    if (![RVSignature(method) isEqualToString:signature]) {
        RVLog([NSString stringWithFormat:@"Skipped %@ %@: ABI %@, expected %@",className,selectorName,RVSignature(method),signature]);
        RVAdaptiveRollback();return NO;
    }
    IMP original=method_getImplementation(method);
    IMP replacement=NULL;
    @try { replacement=imp_implementationWithBlock(factory(original,selector)); }
    @catch (NSException *exception) { RVAdaptiveRollback();return NO; }
    if (!replacement) { RVAdaptiveRollback();return NO; }
    [RVAdaptiveLedger addObject:@{@"class":className,@"selector":selectorName,@"class_method":@(classMethod),
        @"original":[NSValue valueWithPointer:original],@"encoding":@(method_getTypeEncoding(method))}];
    // class_replaceMethod creates a class-local override when the method is inherited.
    class_replaceMethod(target,selector,replacement,method_getTypeEncoding(method));
    RVLog([NSString stringWithFormat:@"Installed %@ %@",className,selectorName]);return YES;
}

