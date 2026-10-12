package rv.executable;

/*-[
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include "IOSArray.h"
#include "IOSObjectArray.h"
#include "IOSClass.h"
#include "java/lang/IllegalStateException.h"

static NSMutableArray *RVEArrayMetadataEvents;
static IOSClass *RVECanonicalArrayClass(id object, SEL selector) {
  // Exact pinned IOSArray layout/API; no guessing based on a class name.
  return IOSClass_arrayOf([(IOSArray *)object elementType]);
}
static NSString *RVEArrayImage(const void *pointer) {
  Dl_info info={0};
  return dladdr(pointer,&info) && info.dli_fname ?
    [[NSString stringWithUTF8String:info.dli_fname] lastPathComponent] : @"unknown";
}
static BOOL RVEArrayOwns(const void *pointer) {
  Dl_info own={0}, other={0};
  return dladdr((const void *)&RVECanonicalArrayClass,&own) && dladdr(pointer,&other)
    && own.dli_fbase && own.dli_fbase==other.dli_fbase;
}
static BOOL RVEArrayGetterABI(Method method) {
  if(!method || method_getNumberOfArguments(method)!=2)return NO;
  char *ret=method_copyReturnType(method), *selfType=method_copyArgumentType(method,0), *selType=method_copyArgumentType(method,1);
  BOOL valid=ret && !strcmp(ret,"@") && selfType && !strcmp(selfType,"@") && selType && !strcmp(selType,":");
  free(ret);free(selfType);free(selType);return valid;
}
static void RVEEnsureArrayMetadata(id object) {
  if(!object || ![object isKindOfClass:[IOSArray class]])
    @throw create_JavaLangIllegalStateException_initWithNSString_(@"Array boundary received a non-IOSArray");
  @synchronized([IOSArray class]) {
    Class cls=object_getClass(object);
    IOSClass *component=[(IOSArray *)object elementType];
    Method method=class_getInstanceMethod(cls,@selector(java_getClass));
    if(!component || ![component isKindOfClass:[IOSClass class]] || !RVEArrayGetterABI(method))
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Invalid native array component/getter ABI");
    IOSClass *expected=IOSClass_arrayOf(component);
    IOSClass *actual=[object java_getClass];
    BOOL consistent=actual==expected && [actual isArray] && [actual getComponentType]==component;
    IMP previous=method_getImplementation(method);
    NSMutableDictionary *event=[@{@"nativeClass":NSStringFromClass(cls),
      @"superclass":NSStringFromClass(class_getSuperclass(cls)) ?: @"",
      @"classImage":RVEArrayImage((__bridge const void *)cls),@"getterImage":RVEArrayImage((const void *)previous),
      @"returnedClass":actual ? NSStringFromClass(object_getClass(actual)) : @"nil",
      @"isArray":@([actual isArray]),@"factoryClass":expected ? NSStringFromClass(object_getClass(expected)) : @"nil",
      @"factoryIsArray":@([expected isArray]),@"phase":consistent ? @"metadata_ready" : @"metadata_mismatch",@"repaired":@NO} mutableCopy];
    if(!RVEArrayMetadataEvents)RVEArrayMetadataEvents=[NSMutableArray array];
    if((!consistent || !RVEArrayMetadataEvents.count) && RVEArrayMetadataEvents.count<16)[RVEArrayMetadataEvents addObject:event];
    Class arrayClass=objc_getClass("IOSArrayClass");
    if(!arrayClass || ![expected isKindOfClass:arrayClass] || ![expected isArray] || [expected getComponentType]!=component){
      event[@"phase"]=@"array_class_factory_inconsistent";
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Pinned array-class factory is inconsistent");
    }
    if(consistent)return;
    // Mutate only a class owned by this payload, with its pinned superclass and
    // component-class runtime in the same image. Never override NSObject/JRE
    // types owned by the app or another embedded runtime.
    if(!RVEArrayOwns((__bridge const void *)cls) || !RVEArrayOwns((__bridge const void *)[IOSArray class])
       || !RVEArrayOwns((__bridge const void *)arrayClass)
       || !RVEArrayOwns((__bridge const void *)object_getClass(component)) || !RVEArrayOwns((const void *)previous)) {
      event[@"phase"]=@"foreign_runtime_ownership";
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Array runtime belongs to another image; repair refused");
    }
    // Create a concrete-class override rather than modifying an inherited Method.
    // The replacement computes the real component-specific class; it does not
    // force isArray or weaken component assignment/cast checks.
    const char *encoding=method_getTypeEncoding(method);
    if(!class_addMethod(cls,@selector(java_getClass),(IMP)RVECanonicalArrayClass,encoding))
      class_replaceMethod(cls,@selector(java_getClass),(IMP)RVECanonicalArrayClass,encoding);
    actual=[object java_getClass];
    if(actual!=expected || ![actual isArray] || [actual getComponentType]!=component) {
      class_replaceMethod(cls,@selector(java_getClass),previous,encoding);
      event[@"phase"]=@"repair_failed";
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Native array metadata repair failed");
    }
    event[@"phase"]=@"canonical_array_getter_installed";event[@"repaired"]=@YES;
  }
}
]-*/

/** Reusable pinned-backend admission at the source/native array boundary. */
public final class ArrayInterop {
    private ArrayInterop() {}
    public static void prepare() { ensure(new Object[0]); }
    public static boolean isArray(Object object) {
        if(object==null)return false;
        try { if(!isNativeArray(object))return false;ensureNative(object);return true; }
        catch(UnsatisfiedLinkError absentBackend){return object.getClass().isArray();}
    }
    private static native boolean isNativeArray(Object object) /*-[ return [object isKindOfClass:[IOSArray class]]; ]-*/;
    public static void ensure(Object array) {
        try { ensureNative(array); }
        catch(UnsatisfiedLinkError absentBackend) {
            if(array==null || !array.getClass().isArray())throw new IllegalArgumentException("Not a Java array");
        }
    }
    private static native void ensureNative(Object array) /*-[ RVEEnsureArrayMetadata(array); ]-*/;
    public static String snapshotJSON(){try{return snapshotNative();}catch(UnsatisfiedLinkError absentBackend){return "[]";}}
    private static native String snapshotNative() /*-[
      @synchronized([IOSArray class]) {
        NSData *data=[NSJSONSerialization dataWithJSONObject:RVEArrayMetadataEvents ?: @[] options:0 error:NULL];
        return data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"[]";
      }
    ]-*/;
}
